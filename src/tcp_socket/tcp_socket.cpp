/* -------------------------------------------------------------------------------
* Copyright (c) 2018, OLogN Technologies AG
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the <organization> nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* -------------------------------------------------------------------------------*/

#include "tcp_socket.h"
#include "../../include/nodecpp/common.h"
#include "../infrastructure.h"


#ifdef _MSC_VER

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <time.h> //TIME_UTC
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

#define CLOSE_SOCKET( x ) closesocket( x )
typedef int ssize_t;

#else // _MSC_VER

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h> // for close() for socket
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/tcp.h>

#define CLOSE_SOCKET( x ) close( x )

#endif // _MSC_VER

//mb: TODO make enum
#define COMMLAYER_RET_FAILED 0
#define COMMLAYER_RET_OK 1
#define COMMLAYER_RET_PENDING 2

using namespace std;

/////////////////////////////////////////////     COMMUNICATION     ///////////////////////////////////////////


const int tcpListenBacklogSize = 3;
static bool netInitialize();

static bool netInitialized = netInitialize();

bool isNetInitialized() { return netInitialized; }

inline bool isErrorWouldBlock(int error)
{
#if defined _MSC_VER || defined __MINGW32__
	return WSAEWOULDBLOCK == error;
#else
	return errno == EINPROGRESS || error == EAGAIN || error == EWOULDBLOCK;
#endif
}



inline int getSockError()
{
#if defined _MSC_VER || defined __MINGW32__
	return WSAGetLastError();
#else
	return errno;
#endif
}

bool netInitialize()
{
#ifdef _MSC_VER
	// do Windows magic
	WSADATA wsaData;
	int iResult;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		NODECPP_TRACE("WSAStartup failed with error: {}", iResult);
		return false;
	}
	NODECPP_TRACE("WSAStartup success");
#endif
	return true;
}

static
void internal_close(SOCKET sock)
{
	NODECPP_TRACE("internal_close() on sock {}", sock);
	CLOSE_SOCKET(sock);
}

class SocketRiia
{
	SOCKET s;
public:
	SocketRiia(SOCKET s) :s(s) {}

	SocketRiia(const SocketRiia&) = delete;
	SocketRiia& operator=(const SocketRiia&) = delete;

	SocketRiia(SocketRiia&&) = default;
	SocketRiia& operator=(SocketRiia&&) = default;

	~SocketRiia() { if (s != INVALID_SOCKET) internal_close(s); }

	SOCKET get() const noexcept { return s; }
	SOCKET release() noexcept { SOCKET tmp = s; s = INVALID_SOCKET; return tmp; }
	explicit operator bool() const noexcept { return s != INVALID_SOCKET; }

};
static
void internal_shutdown_send(SOCKET sock)
{
#ifdef _MSC_VER
	int how = SD_SEND;
#else
	int how = SHUT_WR;
#endif

	NODECPP_TRACE("internal_shutdown_send() on sock {}", sock);

	int res = shutdown(sock, how);
	if (0 != res)
	{
		int error = getSockError();
		NODECPP_TRACE("shutdown on sock {} failed; error {}", sock, error);
	}
}


static
bool internal_async_socket(SOCKET sock)
{
#if defined _MSC_VER || defined __MINGW32__
	unsigned long one = 1;
	int res2 = ioctlsocket(sock, FIONBIO, &one);
#else
	int one = 1;
	int res2 = ioctl(sock, FIONBIO, &one);
#endif
	if (0 != res2)
	{
		int error = getSockError();
		NODECPP_TRACE("async on sock {} failed; error {}", sock, error);
		return false;
	}

	return true;
}

static
bool internal_tcp_no_delay_socket(SOCKET sock, bool noDelay)
{
#if defined _MSC_VER || defined __MINGW32__
	DWORD value = static_cast<DWORD>(noDelay);
    int result = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *) &value, sizeof(value));
#else
	int value = static_cast<int>(noDelay);
    int result = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *) &value, sizeof(value));
#endif
	
	if (0 != result)
	{
		int error = getSockError();
		NODECPP_TRACE("TCP_NODELAY on sock {} failed; error {}", sock, error);
//		internal_close(sock);
		return false;
	}

	return true;
}



static
bool internal_socket_keep_alive(SOCKET sock, bool enable)
{
#if defined _MSC_VER || defined __MINGW32__
	DWORD value = static_cast<DWORD>(enable);
	int result = setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&value, sizeof(value));
#else
	int value = static_cast<int>(enable);
	int result = setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&value, sizeof(value));
#endif

	if (0 != result)
	{
		int error = getSockError();
		NODECPP_TRACE("TCP_NODELAY on sock {} failed; error {}", sock, error);
		//		internal_close(sock);
		return false;
	}

	return true;
}

static
bool internal_linger_zero_socket(SOCKET sock)
{
	linger value;
	value.l_onoff = 1;
	value.l_linger = 0;
#if defined _MSC_VER || defined __MINGW32__
    int result = setsockopt(sock, SOL_SOCKET, SO_LINGER, (char *) &value, sizeof(value));
#else
	int result = setsockopt(sock, SOL_SOCKET, SO_LINGER, (void *) &value, sizeof(value));
#endif
	
	if (0 != result)
	{
		int error = getSockError();
		NODECPP_TRACE("SO_LINGER on sock {} failed; error {}", sock, error);
		return false;
	}

	return true;
}


static
SOCKET internal_make_tcp_socket()
{
	SOCKET sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == sock)
	{
		int error = getSockError();
		NODECPP_TRACE("socket() failed; error {}", error);
		return INVALID_SOCKET;
	}

	NODECPP_TRACE("socket() success {}", sock);

	if (!internal_async_socket(sock))
	{
		internal_close(sock);
		return INVALID_SOCKET;
	}
	
	return sock;
}

static
bool internal_bind_socket(SOCKET sock, struct sockaddr_in& sa_self)
{
	int res = ::bind(sock, (struct sockaddr *)(&sa_self), sizeof(struct sockaddr_in));
	if (0 != res)
	{
		int error = getSockError();
		NODECPP_TRACE("bind() on sock {} failed; error {}", sock, error);
		return false;
	}

	return true;
}

static
uint8_t internal_connect_socket(struct sockaddr_in& sa_other, SOCKET sock)
{
	int res = connect(sock, (struct sockaddr *)(&sa_other), sizeof(struct sockaddr_in));
	if (0 != res)
	{
		int error = getSockError();
		if (isErrorWouldBlock(error))
		{
			NODECPP_TRACE("connect() on sock {} in progress", sock);
			return COMMLAYER_RET_PENDING;
		}
		else
		{
			NODECPP_TRACE("connect() on sock {} failed; error {}", sock, error);
			return COMMLAYER_RET_FAILED;
		}
	}
	else
		NODECPP_TRACE("connect() on sock {} completed", sock);

	return COMMLAYER_RET_OK;
}


static
bool internal_bind_socket(SOCKET sock, Ip4 ip, Port port)
{
	NODECPP_TRACE("internal_bind_socket() on sock {} to {}:{}", sock, ip.toStr(), port.toStr());
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(struct sockaddr_in));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = ip.getNetwork();
	sa.sin_port = port.getNetwork();

	return internal_bind_socket(sock, sa);
}

static
bool internal_listen_tcp_socket(SOCKET sock)
{
	int res = listen(sock, tcpListenBacklogSize);
	if (0 != res) {
		int error = getSockError();
		NODECPP_TRACE("listen() on sock {} failed; error {}", sock, error);
		return false;
	}
	NODECPP_TRACE("listen() on sock {} success", sock);

	return true;
}

static
uint8_t internal_connect_for_address(Ip4 peerIp, Port peerPort, SOCKET sock)
{
	NODECPP_TRACE("internal_connect_for_address() on sock {} to {}:{}", sock, peerIp.toStr(), peerPort.toStr());

	struct sockaddr_in saOther;
	memset(&saOther, 0, sizeof(struct sockaddr_in));
	saOther.sin_family = AF_INET;
	saOther.sin_addr.s_addr = peerIp.getNetwork();
	saOther.sin_port = peerPort.getNetwork();

	return internal_connect_socket(saOther, sock);
}

static
SOCKET internal_tcp_accept(Ip4& ip, Port& port, SOCKET sock)
{
	NODECPP_TRACE("internal_tcp_accept() on sock {}", sock);
	struct sockaddr_in sa;
	socklen_t sz = sizeof(struct sockaddr_in);
	memset(&sa, 0, sz);

	SOCKET outSock = accept(sock, (struct sockaddr *)&sa, &sz);
	if (INVALID_SOCKET == outSock)
	{
		int error = getSockError();
		NODECPP_TRACE("accept() on sock {} failed; error {}", error);

		return INVALID_SOCKET;
	}

	
	ip = Ip4::fromNetwork(sa.sin_addr.s_addr);
	port = Port::fromNetwork(sa.sin_port);
	NODECPP_TRACE("accept() new sock {} from {}:{}", outSock, ip.toStr(), port.toStr());

	if (!internal_async_socket(outSock))
	{
		internal_close(outSock);
		return INVALID_SOCKET;
	}
	return outSock;
}

static
bool internal_getsockopt_so_error(SOCKET sock)
{
	int result;
	socklen_t result_len = sizeof(result);

	int err = getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)(&result), &result_len);
	if (err != 0)
	{
		int error = getSockError();
		NODECPP_TRACE("getsockopt() SO_ERROR on sock {} failed; error {}", sock, error);
		return false;
	}

	if (result != 0)
	{
		NODECPP_TRACE("getsockopt() SO_ERROR on sock {} error {}", sock, result);
		return false;
	}

	return true;
}

static
uint8_t internal_send_packet(const uint8_t* data, uint32_t size, SOCKET sock, size_t& sentSize)
{
	const char* ptr = reinterpret_cast<const char*>(data); //windows uses char*, linux void*
	ssize_t bytes_sent = sendto(sock, ptr, size, 0, nullptr, 0);

	if (bytes_sent < 0)
	{
		sentSize = 0;
		int error = getSockError();
		if (isErrorWouldBlock(error))
		{
			NODECPP_TRACE("internal_send_packet() on sock {} size {} PENDING", sock, size, sentSize);

			return COMMLAYER_RET_PENDING;
		}
		else
		{
			NODECPP_TRACE("internal_send_packet() on sock {} size {} ERROR {}", sock, size, error);
			return COMMLAYER_RET_FAILED;
		}
	}
	else
	{
		sentSize = static_cast<size_t>(bytes_sent);
		if(sentSize == size)
		{
			NODECPP_TRACE("internal_send_packet() on sock {} size {} OK", sock, size);

			return COMMLAYER_RET_OK;
		}
		else
		{
			NODECPP_TRACE("internal_send_packet() on sock {} size {} PENDING sent {} ", sock, size, sentSize);
			
			return COMMLAYER_RET_PENDING;
		}
	}
}

static
uint8_t internal_get_packet_bytes2(SOCKET sock, uint8_t* buff, size_t buffSz, size_t& retSz, struct sockaddr_in& sa_other, socklen_t& fromlen)
{
	retSz = 0;
	
	NODECPP_ASSERT(buff);
	NODECPP_ASSERT(buffSz != 0);
	ssize_t ret = recvfrom(sock, (char*)buff, buffSz, 0, (struct sockaddr *)(&sa_other), &fromlen);

	if (ret < 0)
	{
		int error = getSockError();
		if (isErrorWouldBlock(error))
		{
			NODECPP_TRACE("internal_get_packet_bytes2() on sock {} PENDING", sock);
			return COMMLAYER_RET_PENDING;
		}
		else
		{
			NODECPP_TRACE("internal_get_packet_bytes2() on sock {} ERROR {}", sock, error);
			return COMMLAYER_RET_FAILED;
		}
	}

	retSz = static_cast<size_t>(ret);
	NODECPP_TRACE("internal_get_packet_bytes2() on sock {} size {} OK", sock, retSz);
	return COMMLAYER_RET_OK;
}

/* static */
Ip4 Ip4::parse(const char* ip)
{
	return Ip4(inet_addr(ip));
}

/* static */
Ip4 Ip4::fromNetwork(uint32_t ip)
{
	return Ip4(ip);
}


/* static */
Port Port::fromHost(uint16_t port)
{
	return Port(htons(port));
}

/* static */
Port Port::fromNetwork(uint16_t port)
{
	return Port(port);
}



NetSocketManager::NetSocketManager()
{
	//mb tcpSockets[0] is always invalid
	ioSockets.emplace_back(0);
}


size_t NetSocketManager::connect(net::Socket* ptr, const char* ip, uint16_t port)
{
	Ip4 peerIp = Ip4::parse(ip);
	Ip4 myIp = Ip4::parse(ip);

	Port peerPort = Port::fromHost(port);
	Port myPort = Port::fromHost(port);


	SocketRiia s(internal_make_tcp_socket());
	if (!s)
		return 0; //TODO

	uint8_t st = internal_connect_for_address(peerIp, peerPort, s.get());

	if (st != COMMLAYER_RET_PENDING && st != COMMLAYER_RET_OK)
		return 0; //TODO

	size_t id = addEntry(ptr);
	if (id == 0)
	{
		NODECPP_TRACE("Failed to add entry on NetSocketManager::connect");
		return 0;
	}

	auto& entry = getEntry(id);
	entry.osSocket = s.release();
	entry.connecting = true;

	return id;
}

void NetSocketManager::destroy(size_t id)
{
	auto& entry = getEntry(id);
	if (!entry.isValid())
	{
		NODECPP_TRACE("Unexpected id {} on NetSocketManager::destroy", id);
		return;
	}
	//	
	//	if(force)
	internal_linger_zero_socket(entry.osSocket);
	
	pendingCloseEvents.emplace_back(entry.osSocket, false);
}

void NetSocketManager::end(size_t id)
{
	auto& entry = getEntry(id);
	if (!entry.isValid())
	{
		NODECPP_TRACE("Unexpected id {} on NetSocketManager::end", id);
		return;
	}
	
	if (entry.writeBuffer.empty())
	{
		entry.localEnded = true;
		internal_shutdown_send(entry.osSocket);
		if (entry.remoteEnded)
			pendingCloseEvents.emplace_back(entry.index, false);
	}
	else
		entry.pendingLocalEnd = true;



}

void NetSocketManager::setKeepAlive(size_t id, bool enable)
{
	auto& entry = getEntry(id);
	if (!entry.isValid())
	{
		NODECPP_TRACE("Unexpected id {} on NetSocketManager::setKeepAlive", id);
		return;
	}

	if (!internal_socket_keep_alive(entry.osSocket, enable))
	{
		makeErrorEventAndClose(entry);
	}
}

void NetSocketManager::setNoDelay(size_t id, bool noDelay)
{
	auto& entry = getEntry(id);
	if (!entry.isValid())
	{
		NODECPP_TRACE("Unexpected id {} on NetSocketManager::setNoDelay", id);
		return;
	}

	if (!internal_tcp_no_delay_socket(entry.osSocket, noDelay))
	{
		makeErrorEventAndClose(entry);
	}
}

bool NetSocketManager::write(size_t id, const uint8_t* data, uint32_t size)
{
	auto& entry = getEntry(id);
	if (!entry.isValid())
	{
		NODECPP_TRACE("Unexpected StreamSocket {} on sendStreamSegment", id);
		return true;
	}

	if (entry.localEnded || entry.pendingLocalEnd)
	{
		NODECPP_TRACE("StreamSocket {} already ended", id);
		return true;
	}

	if (entry.writeBuffer.size() == 0)
	{
		size_t sentSize = 0;
		uint8_t res = internal_send_packet(data, size, entry.osSocket, sentSize);
		if (res == COMMLAYER_RET_FAILED)
		{
			makeErrorEventAndClose(entry);
			return false;
		}
		else if (sentSize == size)
		{
			return true;
		}
		else if (sentSize < size)
		{
			entry.writeBuffer.append(data + sentSize, size - sentSize);
			return false;
		}
		else
		{
			NODECPP_ASSERT(false, "Unreachable!");
			return false;
		}
	}
	else
	{
		entry.writeBuffer.append(data, size);
		return false;
	}
}

bool NetSocketManager::addAccepted(net::Socket* ptr, SOCKET sock)
{

	size_t ix = addEntry(ptr);
	if (ix == 0)
	{
		NODECPP_TRACE("Couldn't allocate new StreamSocket, closing {}", sock);
		internal_close(sock);
		return false; // TODO
	}

	auto& entry = getEntry(ix);
	entry.osSocket = sock;

	ptr->emitAccepted(ix);
	//entry.state = 

	return true;
}


size_t NetSocketManager::getPollFdSetSize() const
{
	return ioSockets.size();
}

/*
 * TODO: for performace reasons, the poll data should be cached inside NetSocketManager
 * and updated at the same time that StreamSocketEntry.
 * Avoid to have to write it all over again every time
 * 
 */
void NetSocketManager::setPollFdSet(pollfd* begin, const pollfd* end) const
{
	size_t sz = end - begin;
	assert(sz >= ioSockets.size());
	
	for (size_t i = 0; i != sz; ++i)
	{
		if(i < ioSockets.size() && ioSockets[i].isValid())
		{
			const auto& current = ioSockets[i];
			NODECPP_ASSERT(current.osSocket != INVALID_SOCKET);
			begin[i].fd = current.osSocket;
			begin[i].events = 0;

			if(!current.remoteEnded && !current.paused)
				begin[i].events |= POLLIN;
			if (current.connecting || !current.writeBuffer.empty())
				begin[i].events |= POLLOUT;
		}
		else
			begin[i].fd = INVALID_SOCKET;
	}
}

void NetSocketManager::getPendingEvent()
{
	// if there is an issue with a socket, we may need to close it,
	// and push an event here to notify autom later.

	for (auto& current : pendingCloseEvents)
	{
		auto& entry = getEntry(current.first);
		if (entry.isValid())
		{
			if (entry.osSocket != INVALID_SOCKET)
				internal_close(entry.osSocket);
			entry.getPtr()->emitClose(current.second);
		}
		entry = NetSocketEntry(current.first);
	}
	pendingCloseEvents.clear();
}


void NetSocketManager::checkPollFdSet(const pollfd* begin, const pollfd* end)
{
	assert(end - begin >= static_cast<ptrdiff_t>(ioSockets.size()));
	for (size_t i = 0; i != ioSockets.size(); ++i)
	{
		auto& current = ioSockets[i];
		if (begin[i].fd != INVALID_SOCKET)
		{
			if ((begin[i].revents & (POLLERR | POLLNVAL)) != 0) // check errors first
			{
				NODECPP_TRACE("POLLERR event at {}", begin[i].fd);
				internal_getsockopt_so_error(current.osSocket);
				makeErrorEventAndClose(current);
			}
			else {
				/*
					on Windows when the other end sends a FIN,
					POLLHUP goes up, if we still have data pending to read
					both POLLHUP and POLLIN go up. POLLHUP continues to be up
					for the socket as long as the socket lives.
					on Linux, when remote end sends a FIN, we get a zero size read
					after all pending data is read.
				*/

				if ((begin[i].revents & POLLIN) != 0)
				{
					if (!current.paused)
					{
						NODECPP_TRACE("POLLIN event at {}", begin[i].fd);
						processReadEvent(current);
					}
				}
				else if ((begin[i].revents & POLLHUP) != 0)
				{
					NODECPP_TRACE("POLLHUP event at {}", begin[i].fd);

					if (!current.remoteEnded)
					{
						current.remoteEnded = true;
						current.ptr->emitEnd();
						if (current.localEnded)
						{
							pendingCloseEvents.emplace_back(current.index, false);
							continue;
						}
					}
				}
				
				if ((begin[i].revents & POLLOUT) != 0)
				{
					NODECPP_TRACE("POLLOUT event at {}", begin[i].fd);
					processWriteEvent(current);
				}
			}
			//else if (begin[i].revents != 0)
			//{
			//	NODECPP_TRACE("Unexpected event at {}, value {:x}", begin[i].fd, begin[i].revents);
			//	internal_getsockopt_so_error(current.osSocket);
			//	makeErrorEventAndClose(current);
			//}
		}
	}
}

void NetSocketManager::processReadEvent(NetSocketEntry& current)
{
	auto res = getPacketBytes(current.recvBuffer, current.osSocket);
	if (res.first)
	{
		if (res.second.size() != 0)
		{
			current.ptr->emitData(std::move(res.second));
		}
		else if (!current.remoteEnded)
		{
			current.remoteEnded = true;
			current.ptr->emitEnd();
		}
		else
		{
			NODECPP_TRACE("Unexpected read on socket {}, already shutdown", current.osSocket);
		}
	}
	else
	{
		internal_getsockopt_so_error(current.osSocket);
		return makeErrorEventAndClose(current);
	}
}

void NetSocketManager::processWriteEvent(NetSocketEntry& current)
{
	if (current.connecting)
	{
		current.connecting = false;

#if defined _MSC_VER || defined __MINGW32__
		bool success = true;
#else
		bool success = internal_getsockopt_so_error(current.osSocket);
#endif					
		if (success)
			current.ptr->emitConnect();
		else
			makeErrorEventAndClose(current);
	}
	else if (!current.writeBuffer.empty())
	{
//		assert(current.writeBuffer.size() != 0);
		size_t sentSize = 0;
		uint8_t res = internal_send_packet(current.writeBuffer.begin(), current.writeBuffer.size(), current.osSocket, sentSize);
		if (res == COMMLAYER_RET_FAILED)
		{
			//			pendingCloseEvents.push_back(current.index);
			makeErrorEventAndClose(current);
		}
		else if (sentSize == current.writeBuffer.size())
		{
//			current.writeEvents = false;
			current.writeBuffer.clear();
			if (current.pendingLocalEnd)
			{
				internal_shutdown_send(current.osSocket);
				current.pendingLocalEnd = false;
				current.localEnded = true;

				if (current.remoteEnded)
					pendingCloseEvents.emplace_back(current.index, false);

			}
				
			current.ptr->emitDrain();
		}
		else
		{
			NODECPP_ASSERT(sentSize < current.writeBuffer.size());
//			current.writeEvents = true;
			current.writeBuffer.popFront(sentSize);
		}
	}
	else //ignore?
		NODECPP_ASSERT(false, "Not supported yet!");
}

size_t NetSocketManager::addEntry(net::Socket* ptr)
{
	for (size_t i = 1; i != ioSockets.size(); ++i) // skip ioSockets[0]
	{
		if (!ioSockets[i].isValid())
		{
			NetSocketEntry entry(i, ptr);
			ioSockets[i] = std::move(entry);
			return i;
		}
	}

	if (ioSockets.size() == MAX_SOCKETS)
	{
		return 0;
	}

	size_t ix = ioSockets.size();
	ioSockets.emplace_back(ix, ptr);
	return ix;
}

NetSocketEntry& NetSocketManager::getEntry(size_t id)
{
	return ioSockets.at(id);
}
const NetSocketEntry& NetSocketManager::getEntry(size_t id) const
{
	return ioSockets.at(id);
}


std::pair<bool, Buffer> NetSocketManager::getPacketBytes(Buffer& buff, SOCKET sock)
{
	socklen_t fromlen = sizeof(struct sockaddr_in);
	struct sockaddr_in sa_other;
	size_t sz = 0;
	uint8_t ret = internal_get_packet_bytes2(sock, buff.begin(), buff.capacity(), sz, sa_other, fromlen);

	if (ret != COMMLAYER_RET_OK)
		return make_pair(false, Buffer());

	Buffer res(sz);
	res.append(buff.begin(), sz);
	buff.clear();

	return make_pair(true, std::move(res));
}


void NetSocketManager::makeErrorEventAndClose(NetSocketEntry& entry)
{
	entry.getPtr()->emitError();
	pendingCloseEvents.emplace_back(entry.index, true);
}

NetServerManager::NetServerManager()
{
	//mb ioSockets[0] is always invalid
	ioSockets.emplace_back(0);
}

void NetServerManager::listen(net::Server* ptr, uint16_t port, const char* ip, int backlog)
{
	Ip4 myIp = Ip4::parse(ip);

	Port myPort = Port::fromHost(port);


	SocketRiia s(internal_make_tcp_socket());
	if (!s)
	{
		ptr->emitError();
		return;
	}

	if (!internal_bind_socket(s.get(), myIp, myPort))
	{
		ptr->emitError();
		return;
	}

	if (!internal_listen_tcp_socket(s.get()))
	{
		ptr->emitError();
		return;
	}

	size_t id = addEntry(ptr);
	if (id == 0)
	{
		NODECPP_TRACE("Failed to addEntry at NetServerManager::listen");
		ptr->emitError();
		return;
	}

	auto& entry = getEntry(id);
	entry.osSocket = s.release();

	net::Address addr;
	ptr->emitListening(id, std::move(addr));
}


void NetServerManager::close(size_t id)
{
	auto& entry = getEntry(id);
	if (!entry.isValid())
	{
		NODECPP_TRACE("Unexpected id {} on NetServerManager::close", id);
		return;
	}

	pendingCloseEvents.emplace_back(entry.index, false);
}

size_t NetServerManager::getPollFdSetSize() const
{
	return ioSockets.size();
}

/*
* TODO: for performace reasons, the poll data should be cached inside NetSocketManager
* and updated at the same time that StreamSocketEntry.
* Avoid to have to write it all over again every time
*
*/
void NetServerManager::setPollFdSet(pollfd* begin, const pollfd* end) const
{
	size_t sz = end - begin;
	assert(sz >= ioSockets.size());

	for (size_t i = 0; i != sz; ++i)
	{
		if (i < ioSockets.size() && ioSockets[i].isValid())
		{
			const auto& current = ioSockets[i];
			NODECPP_ASSERT(current.osSocket != INVALID_SOCKET);
			begin[i].fd = current.osSocket;
			begin[i].events = 0;

			bool f2 = true;
			if (f2)
				begin[i].events |= POLLIN;
		}
		else
			begin[i].fd = INVALID_SOCKET;
	}
}

void NetServerManager::getPendingEvent()
{
	// if there is an issue with a socket, we may need to close it,
	// and push an event here to notify later.

	for (auto& current : pendingCloseEvents)
	{
		auto& entry = getEntry(current.first);
		if (entry.isValid())
		{
			if (entry.osSocket != INVALID_SOCKET)
				internal_close(entry.osSocket);
			entry.getPtr()->emitClose(current.second);
		}
		entry = NetServerEntry(current.first);

	}
	pendingCloseEvents.clear();
}


void NetServerManager::checkPollFdSet(const pollfd* begin, const pollfd* end)
{
	assert(end - begin >= static_cast<ptrdiff_t>(ioSockets.size()));
	for (size_t i = 0; i != ioSockets.size(); ++i)
	{
		auto& current = ioSockets[i];
		if (begin[i].fd != INVALID_SOCKET)
		{
			if ((begin[i].revents & (POLLERR | POLLNVAL)) != 0) // check errors first
			{
				NODECPP_TRACE("POLLERR event at {}", begin[i].fd);
				internal_getsockopt_so_error(current.osSocket);
				makeErrorEventAndClose(current);
			}
			else if ((begin[i].revents & POLLIN) != 0)
			{
				NODECPP_TRACE("POLLIN event at {}", begin[i].fd);
				processAcceptEvent(current);
			}
			else if (begin[i].revents != 0)
			{
				NODECPP_TRACE("Unexpected event at {}, value {:x}", begin[i].fd, begin[i].revents);
				internal_getsockopt_so_error(current.osSocket);
				makeErrorEventAndClose(current);
			}
		}
	}
}


void NetServerManager::processAcceptEvent(NetServerEntry& entry)
{
	Ip4 remoteIp;
	Port remotePort;

	SocketRiia newSock(internal_tcp_accept(remoteIp, remotePort, entry.osSocket));
	if (!newSock)
		return;

	std::unique_ptr<net::Socket> ptr(entry.getPtr()->makeSocket());
	auto& man = getInfra().getNetSocket();

	bool ok = man.addAccepted(ptr.get(), newSock.release());

	if (!ok)
		return;

	entry.getPtr()->emitConnection(std::move(ptr));

	return;
}

size_t NetServerManager::addEntry(net::Server* ptr)
{
	for (size_t i = 1; i != ioSockets.size(); ++i) // skip ioSockets[0]
	{
		if (!ioSockets[i].isValid())
		{
			NetServerEntry entry(i, ptr);
			ioSockets[i] = std::move(entry);
			return i;
		}
	}

	if (ioSockets.size() == MAX_SOCKETS)
	{
		return 0;
	}

	size_t ix = ioSockets.size();
	ioSockets.emplace_back(ix, ptr);
	return ix;
}

NetServerEntry& NetServerManager::getEntry(size_t id)
{
	return ioSockets.at(id);
}

const NetServerEntry& NetServerManager::getEntry(size_t id) const
{
	return ioSockets.at(id);
}



void NetServerManager::makeErrorEventAndClose(NetServerEntry& entry)
{
	entry.getPtr()->emitError();
	pendingCloseEvents.emplace_back(entry.index, true);
}
