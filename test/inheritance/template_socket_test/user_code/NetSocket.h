// NetSocket.h : sample of user-defined code

#ifndef NET_SOCKET_H
#define NET_SOCKET_H
#include "../../../../include/nodecpp/common.h"


#include "../../../../3rdparty/fmt/include/fmt/format.h"
#include "../../../../include/nodecpp/net.h"

#include <functional>


using namespace std;
using namespace nodecpp;
using namespace fmt;

class MyServer;

#if 0
class MySocket :public net::Socket {
	size_t recvSize = 0;
	size_t sentSize = 0;
	std::unique_ptr<uint8_t> ptr;
	size_t size = 64 * 1024;

public:
	MySocket() {}

	void onClose(bool hadError) override {
		print("onClose!\n");
	}

	void onConnect() override {
		print("onConnect!\n");
		ptr.reset(static_cast<uint8_t*>(malloc(size)));

		bool ok = true;
		while (ok) {
			ok = write(ptr.get(), size);
			sentSize += size;
		}
	}

	void onData(Buffer& buffer) override {
		print("onData!\n");
		recvSize += buffer.size();

		if (recvSize >= sentSize)
			end();
	}

	void onDrain() override {
		print("onDrain!\n");
	}

	void onEnd() override {
		print("onEnd!\n");
	}

	void onError(Error&) override {
		print("onError!\n");
	}
};
#endif

#ifdef USING_L_SOCKETS
class MySocketLambda :public net::Socket {
	size_t recvSize = 0;
	size_t sentSize = 0;
	std::unique_ptr<uint8_t> ptr;
	size_t size = 64 * 1024;

public:
	MySocketLambda() {
		//preset handlers
//		on(event::close, std::bind(&MySocketLambda::didClose, this, std::placeholders::_1));
//		on(event::close, std::bind(&MySocketLambda::didClose, this, std::placeholders::_1));
		on(event::close, [this](bool b) { this->didClose(b); });

//		on<event::Connect>(std::bind(&MySocketLambda::didConnect, this));
//		on<event::Data>(std::bind(&MySocket2::didData, this, std::placeholders::_1));
//		on<event::Drain>(std::bind(&MySocketLambda::didDrain, this));
//		on(event::end, std::bind(&MySocketLambda::didEnd, this));
		on(event::end, [this]() { this->didEnd(); });
//		on(event::error, std::bind(&MySocketLambda::didError, this));
		on(event::error, [this](Error&) { this->didError(); });
	}

	void didClose(bool hadError) {
		print("onClose!\n");
	}

	void didConnect() {
		print("onConnect!\n");
		ptr.reset(static_cast<uint8_t*>(malloc(size)));

		bool ok = true;
		while (ok) {
			ok = write(ptr.get(), size, [] { print("onDrain!\n"); });
			sentSize += size;
		}
	}

	void didData(Buffer& buffer) {
		print("onData!\n");
		recvSize += buffer.size();

		if (recvSize >= sentSize)
			end();
	}

	//void didDrain() {
	//	print("onDrain!\n");
	//}

	void didEnd() {
		print("onEnd!\n");
	}

	void didError() {
		print("onError!\n");
	}
};
#endif // USING_L_SOCKETS
/*
class MyServerSocket :public net::Socket {
	size_t count = 0;
	MyServer* server = nullptr;

public:
	MyServerSocket(MyServer* server) :server(server) {}

	void onClose(bool hadError) override;

	void onConnect() override {
		print("onConnect!\n");
	}

	void onData(Buffer& buffer) override {
		print("onData!\n");
		++count;
		write(buffer.begin(), buffer.size());
	}

	void onDrain() override {
		print("onDrain!\n");
	}

	void onEnd() override {
		print("onEnd!\n");
		const char buff[] = "goodbye!";
		write(reinterpret_cast<const uint8_t*>(buff), sizeof(buff));
		end();
	}

	void onError() override {
		print("onError!\n");
	}
};



class MyServer :public net::Server {
	list<unique_ptr<net::Socket>> socks;
public:


	void onClose(bool hadError) override {
		print("onClose!\n");
	}
	void onConnection(net::Socket* socket) override {
		print("onConnection!\n");
		unref();
		socks.emplace_back(socket);
	}
	void onListening() override {
		print("onListening!\n");
	}

	void onError() override {
		print("onError!\n");
	}
	MyServerSocket* makeSocket() override {
		return new MyServerSocket(this);
	}

	void closeMe(MyServerSocket* ptr) {
		for (auto it = socks.begin(); it != socks.end(); ++it) {
			if (it->get() == ptr) {
				socks.erase(it);
				return;
			}
		}
	}

};

inline
void MyServerSocket::onClose(bool hadError) {
	print("onClose!\n");
	if (server)
		server->closeMe(this);
}
*/

#ifndef NET_CLIENT_ONLY
#ifdef USING_L_SOCKETS
class MyServerMember;

class MyServerSocketMember {
	unique_ptr<net::Socket> socket;
	size_t count = 0;
	MyServerMember* server = nullptr;

public:
	MyServerSocketMember(net::Socket* socket, MyServerMember* server) 
		:socket(socket), server(server)
	{
		socket->on(event::close, [this](bool b) { this->didClose(b); });
		socket->on(event::data, [this](Buffer& buffer) { this->didData(buffer); });
		socket->on(event::end, [this]() { this->didEnd(); });
		socket->on(event::error, [this](Error&) { this->didError(); });
	}

	void didClose(bool hadError);

	void didData(Buffer& buffer) {
		print("onData!\n");
		++count;
		socket->write(buffer.begin(), buffer.size());
	}

	void didEnd() {
		print("onEnd!\n");
		const char buff[] = "goodbye!";
		socket->write(reinterpret_cast<const uint8_t*>(buff), sizeof(buff));
		socket->end();
	}

	void didError() {
		print("onError!\n");
	}
};

class MyServerMember 
{
	unique_ptr<net::Server> server;
	list<unique_ptr<MyServerSocketMember>> socks;
public:

	MyServerMember() :server(new net::Server()) {}

	void listen(uint16_t port, const char* ip, int backlog) {
		server->on(event::error, [](Error&) { print("onError!\n"); });
		server->on(event::close, [](bool b) { print("onClose!\n"); });
		server->on(event::connection, [this](net::Socket* socket) {
			print("onConnection!\n");
			this->server->unref();
			socks.emplace_back(new MyServerSocketMember(socket, this));
		});
		server->listen(port, ip, backlog, []() { print("onListening!\n"); });
	}
	//MyServerSocket* makeSocket() override {
	//	return new MyServerSocket(server.get());
	//}

	void closeMe(MyServerSocketMember* ptr) {
		for (auto it = socks.begin(); it != socks.end(); ++it) {
			if (it->get() == ptr) {
				socks.erase(it);
				return;
			}
		}
	}
};
#endif // NO_SERVER_STAFF

inline
void MyServerSocketMember::didClose(bool hadError) {
	print("onClose!\n");
	if (server)
		server->closeMe(this);
}
#endif // USING_L_SOCKETS

#ifdef USING_L_SOCKETS
class MySampleLambdaOneNode : public NodeBase
{
#ifndef NET_CLIENT_ONLY
	MyServerMember srv;
#endif // NO_SERVER_STAFF
//	unique_ptr<MySocketLambda> cli;
	MySocketLambda* cli;
public:
	MySampleLambdaOneNode() : cli( new MySocketLambda() )
	{
		printf( "MySampleLambdaOneNode::MySampleLambdaOneNode()\n" );
	}

	virtual void main()
	{
		printf( "MySampleLambdaOneNode::main()\n" );

#ifndef NET_CLIENT_ONLY
		srv.listen(2000, "127.0.0.1", 5);
#endif // NO_SERVER_STAFF

		cli->on(event::data, [this](Buffer& buffer) { this->cli->didData(buffer); });

	//	cli->once<event::Connect>([this]() { fmt::print( "welcome lambda-based solution [1]!\n" ); });
		cli->once(event::connect, [this]() { fmt::print( "welcome lambda-based solution [2]!\n" ); });
		cli->once(event::Connect::name, [this]() { fmt::print( "welcome lambda-based solution [3]!\n" ); });
		cli->once("connect", [this]() { fmt::print( "welcome lambda-based solution [3.1]!\n" ); });

	//	cli->once<event::Close>([this](bool) { fmt::print( "close lambda-based solution [1]!\n" ); return true; });
		cli->once(event::close, [this](bool) { fmt::print( "close lambda-based solution [2]!\n" ); return true; });
		cli->once(event::Close::name, [this](bool) { fmt::print( "close lambda-based solution [3]!\n" ); return true; });
		cli->once("close", [this](bool) { fmt::print( "close lambda-based solution [3.1]!\n" ); return true; });

		cli->connect(2000, "127.0.0.1", [this] {cli->didConnect(); });
	}
};
#endif // USING_L_SOCKETS

#ifdef USING_O_SOCKETS
class MySampleInheritanceOneNode : public NodeBase
{
	size_t recvSize = 0;
	size_t sentSize = 0;
	std::unique_ptr<uint8_t> ptr;
	size_t size = 64 * 1024;
	bool letOnDrain = false;

	using SocketIdType = int;

#ifndef NET_CLIENT_ONLY
	MyServerMember srv;
#endif // NO_SERVER_STAFF
//	unique_ptr<MySocketLambda> cli;
//	MySocketLambda* cli;
public:
	MySampleInheritanceOneNode() : sockNN1_3( this )/*, sockNN1_4( this )*/
	{
		printf( "MySampleInheritanceOneNode::MySampleInheritanceOneNode()\n" );
	}

	virtual void main()
	{
		printf( "MySampleLambdaOneNode::main()\n" );

#ifndef NET_CLIENT_ONLY
		srv.listen(2000, "127.0.0.1", 5);
#endif // NO_SERVER_STAFF

		*( sockNN1_3.getExtra() ) = 17;
//		*( sockNN1_4.getExtra() ) = 71;
		sockNN1_3.connect(2000, "127.0.0.1");
//		sockNN1_4.connect(2008, "127.0.0.1");
	}

	void onConnect(const void* extra) 
	{ 
		printf( "MySampleInheritanceOneNode::onConnect()\n" ); 

		ptr.reset(static_cast<uint8_t*>(malloc(size)));

		bool ok = true;
		while (ok) {
//			ok = write(ptr.get(), size, [] { print("onDrain!\n"); });
			ok = sockNN1_3.write(ptr.get(), size);
			letOnDrain = !ok;
			sentSize += size;
		}
	}
	void onClose(const void* extra,bool) { printf( "MySampleInheritanceOneNode::onClose()\n" ); }
	void onData(const void* extra, nodecpp::Buffer& buffer) 
	{ 
		printf( "MySampleInheritanceOneNode::onData()\n" );  
		recvSize += buffer.size();

		if (recvSize >= sentSize)
			sockNN1_3.end();
	}
	void onDrain(const void* extra) 
	{
		if ( letOnDrain )
			printf( "MySampleInheritanceOneNode::onDrain()\n" ); 
	}
	void onError(const void* extra, nodecpp::Error&) { printf( "MySampleInheritanceOneNode::onError()\n" ); }
	void onEnd(const void* extra) { printf( "MySampleInheritanceOneNode::onEnd()\n" ); }

	void onWhateverConnect_2(const SocketIdType* extra) 
	{
		NODECPP_ASSERT( extra != nullptr );
		printf( "MySampleInheritanceOneNode::onWhateverConnect_2(), extra = %d\n", *extra );

		ptr.reset(static_cast<uint8_t*>(malloc(size)));

		bool ok = true;
		while (ok) {
//			ok = write(ptr.get(), size, [] { print("onDrain!\n"); });
			ok = sockNN1_3.write(ptr.get(), size);
			letOnDrain = !ok;
			sentSize += size;
		}
	}
	void onWhateverClose_2(const SocketIdType* extra, bool)
	{
		NODECPP_ASSERT( extra != nullptr );
		printf( "MySampleInheritanceOneNode::onWhateverClose_2(), extra = %d\n", *extra );
	}
	void onWhateverData_2(const SocketIdType* extra, nodecpp::Buffer& buffer)
	{
		NODECPP_ASSERT( extra != nullptr );
		printf( "MySampleInheritanceOneNode::onWhateverData_2(), extra = %d\n", *extra );
		recvSize += buffer.size();

		if (recvSize >= sentSize)
			sockNN1_3.end();
	}
	void onWhateverDrain_2(const SocketIdType* extra)
	{
		NODECPP_ASSERT( extra != nullptr );
		if ( letOnDrain )
			printf( "MySampleInheritanceOneNode::onWhateverDrain_2(), extra = %d\n", *extra );
	}
	void onWhateverError_2(const SocketIdType* extra, nodecpp::Error&)
	{
		NODECPP_ASSERT( extra != nullptr );
		printf( "MySampleInheritanceOneNode::onWhateverError_2(), extra = %d\n", *extra );
	}
	void onWhateverEnd_2(const SocketIdType* extra)
	{
		NODECPP_ASSERT( extra != nullptr );
		printf( "MySampleInheritanceOneNode::onWhateverEnd_2(), extra = %d\n", *extra );
	}


	nodecpp::net::SocketN<MySampleInheritanceOneNode,SocketIdType,
		nodecpp::net::OnConnect<&MySampleInheritanceOneNode::onWhateverConnect_2>,
		nodecpp::net::OnClose<&MySampleInheritanceOneNode::onWhateverClose_2>,
		nodecpp::net::OnData<&MySampleInheritanceOneNode::onWhateverData_2>,
		nodecpp::net::OnDrain<&MySampleInheritanceOneNode::onWhateverDrain_2>,
		nodecpp::net::OnError<&MySampleInheritanceOneNode::onWhateverError_2>,
		nodecpp::net::OnEnd<&MySampleInheritanceOneNode::onWhateverEnd_2>
	> sockNN1_3/*, sockNN1_4*/;
};
#endif // USING_O_SOCKETS

class MySampleTNode; // forward declaration; needed in SocketO-derived classes

template<class Extra>
class MySocketO : public net::SocketO
{
	MySampleTNode* myNode = nullptr;
	Extra myId;

public:
	MySocketO( MySampleTNode* myNode_ ) : myNode( myNode_ ) {}
	void onClose(bool hadError) override { myNode->onClose( &myId, hadError ); }
	void onConnect() override { myNode->onConnect( &myId ); }
	void onData(Buffer& buffer) override { myNode->onData( &myId, buffer ); }
	void onDrain() override { myNode->onDrain( &myId ); }
	void onEnd() override { myNode->onEnd( &myId ); }
	void onError(Error& err) override { myNode->onError( &myId, err ); }
};

class MySampleTNode : public NodeBase
{
	size_t recvSize = 0;
	size_t sentSize = 0;
	std::unique_ptr<uint8_t> ptr;
	size_t size = 64 * 1024;
	bool letOnDrain = false;

	using SocketIdType = int;

	MySocketO<SocketIdType> sockO_1;

#ifndef NET_CLIENT_ONLY
	MyServerMember srv;
#endif // NO_SERVER_STAFF
//	unique_ptr<MySocketLambda> cli;
//	MySocketLambda* cli;
public:
	MySampleTNode() : sockO_1( this ), sockNN_1(this), sockNN_2(this)
	{
		printf( "MySampleTNode::MySampleTNode()\n" );
	}

	virtual void main()
	{
		printf( "MySampleLambdaOneNode::main()\n" );

#ifndef NET_CLIENT_ONLY
		srv.listen(2000, "127.0.0.1", 5);
#endif // NO_SERVER_STAFF

		*( sockNN_1.getExtra() ) = 17;
//		*( sockNN1_4.getExtra() ) = 71;
		sockNN_1.connect(2000, "127.0.0.1");
//		sockNN1_4.connect(2008, "127.0.0.1");
	}

	void onConnect(const void* extra) 
	{ 
		printf( "MySampleTNode::onConnect()\n" ); 

		ptr.reset(static_cast<uint8_t*>(malloc(size)));

		bool ok = true;
		while (ok) {
//			ok = write(ptr.get(), size, [] { print("onDrain!\n"); });
			ok = sockNN_1.write(ptr.get(), size);
			letOnDrain = !ok;
			sentSize += size;
		}
	}
	void onClose(const void* extra,bool) { printf( "MySampleTNode::onClose()\n" ); }
	void onData(const void* extra, nodecpp::Buffer& buffer) 
	{ 
		printf( "MySampleTNode::onData()\n" );  
		recvSize += buffer.size();

		if (recvSize >= sentSize)
			sockNN_1.end();
	}
	void onDrain(const void* extra) 
	{
		if ( letOnDrain )
			printf( "MySampleTNode::onDrain()\n" ); 
	}
	void onError(const void* extra, nodecpp::Error&) { printf( "MySampleTNode::onError()\n" ); }
	void onEnd(const void* extra) { printf( "MySampleTNode::onEnd()\n" ); }

	
	void onWhateverConnect(const SocketIdType* extra) 
	{
		NODECPP_ASSERT( extra != nullptr );
		printf( "MySampleTNode::onWhateverConnect(), extra = %d\n", *extra );

		ptr.reset(static_cast<uint8_t*>(malloc(size)));

		bool ok = true;
		while (ok) {
//			ok = write(ptr.get(), size, [] { print("onDrain!\n"); });
			ok = sockNN_1.write(ptr.get(), size);
			letOnDrain = !ok;
			sentSize += size;
		}
	}
	void onWhateverClose(const SocketIdType* extra, bool)
	{
		NODECPP_ASSERT( extra != nullptr );
		printf( "MySampleTNode::onWhateverClose(), extra = %d\n", *extra );
	}
	void onWhateverData(const SocketIdType* extra, nodecpp::Buffer& buffer)
	{
		NODECPP_ASSERT( extra != nullptr );
		printf( "MySampleTNode::onWhateverData(), extra = %d\n", *extra );
		recvSize += buffer.size();

		if (recvSize >= sentSize)
			sockNN_1.end();
	}
	void onWhateverDrain(const SocketIdType* extra)
	{
		NODECPP_ASSERT( extra != nullptr );
		if ( letOnDrain )
			printf( "MySampleTNode::onWhateverDrain(), extra = %d\n", *extra );
	}
	void onWhateverError(const SocketIdType* extra, nodecpp::Error&)
	{
		NODECPP_ASSERT( extra != nullptr );
		printf( "MySampleTNode::onWhateverError(), extra = %d\n", *extra );
	}
	void onWhateverEnd(const SocketIdType* extra)
	{
		NODECPP_ASSERT( extra != nullptr );
		printf( "MySampleTNode::onWhateverEnd(), extra = %d\n", *extra );
	}


	void onWhateverConnect_2(const SocketIdType* extra) 
	{
		NODECPP_ASSERT( extra != nullptr );
		printf( "MySampleTNode::onWhateverConnect_2(), extra = %d\n", *extra );

		ptr.reset(static_cast<uint8_t*>(malloc(size)));

		bool ok = true;
		while (ok) {
//			ok = write(ptr.get(), size, [] { print("onDrain!\n"); });
			ok = sockNN_1.write(ptr.get(), size);
			letOnDrain = !ok;
			sentSize += size;
		}
	}
	void onWhateverClose_2(const SocketIdType* extra, bool)
	{
		NODECPP_ASSERT( extra != nullptr );
		printf( "MySampleTNode::onWhateverClose_2(), extra = %d\n", *extra );
	}
	void onWhateverData_2(const SocketIdType* extra, nodecpp::Buffer& buffer)
	{
		NODECPP_ASSERT( extra != nullptr );
		printf( "MySampleTNode::onWhateverData_2(), extra = %d\n", *extra );
		recvSize += buffer.size();

		if (recvSize >= sentSize)
			sockNN_1.end();
	}
	void onWhateverDrain_2(const SocketIdType* extra)
	{
		NODECPP_ASSERT( extra != nullptr );
		if ( letOnDrain )
			printf( "MySampleTNode::onWhateverDrain_2(), extra = %d\n", *extra );
	}
	void onWhateverError_2(const SocketIdType* extra, nodecpp::Error&)
	{
		NODECPP_ASSERT( extra != nullptr );
		printf( "MySampleTNode::onWhateverError_2(), extra = %d\n", *extra );
	}
	void onWhateverEnd_2(const SocketIdType* extra)
	{
		NODECPP_ASSERT( extra != nullptr );
		printf( "MySampleTNode::onWhateverEnd_2(), extra = %d\n", *extra );
	}

/*	using Initializer = SocketTInitializer<
		nodecpp::net::OnConnectT<&MySampleTNode::onWhateverConnect>,
		nodecpp::net::OnCloseT<&MySampleTNode::onWhateverClose>,
		nodecpp::net::OnDataT<&MySampleTNode::onWhateverData>,
		nodecpp::net::OnDrainT<&MySampleTNode::onWhateverDrain>,
		nodecpp::net::OnErrorT<&MySampleTNode::onWhateverError>,
		nodecpp::net::OnEndT<&MySampleTNode::onWhateverEnd>
	>;*/

	using SockType_1 = nodecpp::net::SocketT<MySampleTNode,SocketIdType,
		nodecpp::net::OnConnectT<&MySampleTNode::onWhateverConnect>,
		nodecpp::net::OnCloseT<&MySampleTNode::onWhateverClose>,
		nodecpp::net::OnDataT<&MySampleTNode::onWhateverData>,
		nodecpp::net::OnDrainT<&MySampleTNode::onWhateverDrain>,
		nodecpp::net::OnErrorT<&MySampleTNode::onWhateverError>,
		nodecpp::net::OnEndT<&MySampleTNode::onWhateverEnd>
	>;
	SockType_1 sockNN_1;

	using SockType_2 = nodecpp::net::SocketT<MySampleTNode,SocketIdType,
		nodecpp::net::OnConnectT<&MySampleTNode::onWhateverConnect_2>,
		nodecpp::net::OnCloseT<&MySampleTNode::onWhateverClose_2>,
		nodecpp::net::OnDataT<&MySampleTNode::onWhateverData_2>,
		nodecpp::net::OnDrainT<&MySampleTNode::onWhateverDrain_2>,
		nodecpp::net::OnErrorT<&MySampleTNode::onWhateverError_2>,
		nodecpp::net::OnEndT<&MySampleTNode::onWhateverEnd_2>
	>;
	SockType_2 sockNN_2;

	using EmitterType = nodecpp::net::SocketTEmitter<net::SocketO, SockType_1, SockType_2>;
//	using EmitterType = nodecpp::net::SocketTEmitter<SockType_1>;
};

#endif // NET_SOCKET_H
