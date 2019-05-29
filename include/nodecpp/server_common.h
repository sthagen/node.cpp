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
*     * Neither the name of the OLogN Technologies AG nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL OLogN Technologies AG BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* -------------------------------------------------------------------------------*/

#ifndef SERVER_COMMON_H
#define SERVER_COMMON_H

#include "common.h"
#include "event.h"
#include "net_common.h"

namespace nodecpp {

	namespace net {

		class ServerBase
		{
			public:
				nodecpp::safememory::soft_this_ptr<ServerBase> myThis;
			public:
			class DataForCommandProcessing {
			public:
				size_t index;
				bool refed = false;
				short fdEvents = 0;
				//SOCKET osSocket = INVALID_SOCKET;
				unsigned long long osSocket = 0;

				DataForCommandProcessing() {}
				DataForCommandProcessing(const DataForCommandProcessing& other) = delete;
				DataForCommandProcessing& operator=(const DataForCommandProcessing& other) = delete;

				DataForCommandProcessing(DataForCommandProcessing&& other) = default;
				DataForCommandProcessing& operator=(DataForCommandProcessing&& other) = default;

				Address localAddress;

				awaitable_handle_data ahd_listen;
				struct awaitable_connection_handle_data : public awaitable_handle_data
				{
					soft_ptr<SocketBase> sock;
				};
				awaitable_connection_handle_data ahd_connection;

				void (*userDefListenHandler)(void*, size_t, nodecpp::net::Address) = nullptr;
				void (*userDefConnectionHandler)(void*, nodecpp::safememory::soft_ptr<net::SocketBase>) = nullptr;
				void (*userDefCloseHandler)(void*, bool) = nullptr;
				void (*userDefErrorHandler)(void*, Error&) = nullptr;

				void *userDefListenHandlerObjectPtr = nullptr;
				void *userDefConnectionHandlerObjectPtr = nullptr;
				void *userDefCloseHandlerObjectPtr = nullptr;
				void *userDefErrorHandlerObjectPtr = nullptr;

				template<class T>
				using userListenMemberHandler = void (T::*)(size_t, nodecpp::net::Address);

//				template<class ObjectT, class MemberFnT>
				template<class ObjectT, userListenMemberHandler<ObjectT> MemberFnT>
				static void listenHandler( void* objPtr, size_t id, nodecpp::net::Address addr)
				{
					((reinterpret_cast<ObjectT*>(objPtr))->*MemberFnT)(id, addr);
				}

				template<class ObjectT, userListenMemberHandler<ObjectT> memmberFn>
				void registerListenHandler(ObjectT* object )
//				static void registerListenHandler(ObjectT* object )
				{
//					userDefListenHandler = &listenHandler<ObjectT, userListenMemberHandler<ObjectT>>;
					userDefListenHandler = &listenHandler<ObjectT, memmberFn>;
					//listenHandler<ObjectT, memmberFn>( object, 5, nodecpp::net::Address() );
					//void* objPtr = object; 	((reinterpret_cast<ObjectT*>(objPtr))->*memmberFn)(5, nodecpp::net::Address());
					userDefListenHandlerObjectPtr = object;
				}
			};
			DataForCommandProcessing dataForCommandProcessing;

		protected:
//			uint16_t localPort = 0;

//			size_t id = 0;
			enum State { UNINITIALIZED = 0, LISTENING, CLOSED } state = UNINITIALIZED;

		protected:
			void registerServerByID(NodeBase* node, soft_ptr<net::ServerBase> t, int typeId);

		public:
			NodeBase* node = nullptr;

		public:
			ServerBase() {}
			~ServerBase() {reportBeingDestructed();}

			const Address& address() const { return dataForCommandProcessing.localAddress; }
			void close();

			bool listening() const { return state == LISTENING; }
			void ref();
			void unref();
			void reportBeingDestructed();

			void listen(uint16_t port, const char* ip, int backlog);

		};

		//TODO don't use naked pointers, think
		//template<class T>
		//T* createServer() {
		//	return new T();
		//}
		//template<class T>
		//T* createServer(std::function<void()> cb) {
		//	auto svr = new T();
		//	svr->on<event::Connect>(cb);
		//	return svr;
		//}

		//template<class T>
		//T* createConnection(uint16_t port, const char* host) {
		//	auto cli = new T();
		//	cli->appConnect(port, host);
		//	return cli;
		//}

		//template<class T>
		//T* createConnection(uint16_t port, const char* host, std::function<void()> cb) {
		//	auto cli = new T();
		//	cli->appConnect(port, host, std::move(cb));
		//	return cli;
		//}

	} //namespace net
} //namespace nodecpp

#endif //SERVER_COMMON_H
