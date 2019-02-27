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

#ifndef SERVER_T_H
#define SERVER_T_H

#include "server_t_base.h"
#include "../../src/infrastructure.h"

namespace nodecpp {

	namespace net {

		template<auto x>
		struct OnCloseST {};

		template<auto x>
		struct OnConnectionST {};

		template<auto x>
		struct OnListeningST {};

		template<auto x>
		struct OnErrorST {};

		template<typename ... args>
		struct ServerTInitializer;

		//partial template specializations:
		template<auto F, typename ... args>
		struct ServerTInitializer<OnCloseST<F>, args...>
		: public ServerTInitializer<args...> {
			static constexpr auto onClose = F;
		};

		template<auto F, typename ... args>
		struct ServerTInitializer<OnConnectionST<F>, args...>
		: public ServerTInitializer<args...> {
			static constexpr auto onConnection = F;
		};

		template<auto F, typename ... args>
		struct ServerTInitializer<OnListeningST<F>, args...>
		: public ServerTInitializer<args...> {
			static constexpr auto onListening = F;
		};

		template<auto F, typename ... args>
		struct ServerTInitializer<OnErrorST<F>, args...>
		: public ServerTInitializer<args...> {
			static constexpr auto onError = F;
		};

		template<>
		struct ServerTInitializer<> {
			static constexpr auto onConnection = nullptr;
			static constexpr auto onClose = nullptr;
			static constexpr auto onListening = nullptr;
			static constexpr auto onError = nullptr;
		};

		template<class Node, class Socket, class Extra>
		class ServerTUserBase : public ServerTBase
		{
		public:
			using userIdType = Extra;
			using userNodeType = Node;
			using SocketType = Socket;

		protected:
			Extra extra;

		public:
			ServerTUserBase(Node* node_) {this->node = node_;}
			Extra* getExtra() { return &extra; }
			const Extra* getExtra() const { return &extra; }

		};

		template<class Node, class Socket>
		class ServerTUserBase<Node, Socket, void> : public ServerTBase
		{
			public:
				using userIdType = void;
				using userNodeType = Node;
				using SocketType = Socket;

		public:
			ServerTUserBase() {}
			void* getExtra() { return nullptr; }
			const void* getExtra() const { return nullptr; }

		};

		template<class Node, class Socket, class Initializer, class Extra>
		class ServerT2 : public ServerTUserBase<Node, Socket, Extra>
		{
		public:
			using Handlers = Initializer;

		public:
			ServerT2(Node* node_) :ServerTUserBase<Node, Socket, Extra>( node_ ) {}
		};

/*		template<class Node, class Socket, class Initializer>
		class ServerT2<Node, Socket, Initializer, void> : public ServerTBase
		{
			public:
				using userIdType = void;
				using userNodeType = Node;
				using Handlers = Initializer;
				using SocketType = Socket;

		public:
			ServerT2() {}
			void* getExtra() { return nullptr; }
			const void* getExtra() const { return nullptr; }

		};*/

		template<class Node, class Socket, class Extra, class ... Handlers>
		class ServerT : public ServerT2<Node, Socket, ServerTInitializer<Handlers...>, Extra>
		{
			//int idType1; // we will try to get rid of it later
			protected:
				MultiOwner<typename Socket::StorableType> socketList;
		public:
			ServerT(Node* node) : ServerT2<Node, Socket, ServerTInitializer<Handlers...>, Extra>(node) {
				int typeId = Node::EmitterTypeForServer::getTypeIndex( this ); 
				nodecpp::safememory::soft_ptr<ServerT> me = this->myThis.template getSoftPtr<ServerT>(this);
				this->registerServerByID(this->node, me, typeId);
			}
			
			soft_ptr<Socket> makeSocket(OpaqueSocketData& sdata) {
//				SocketForserver* sock = socketList.getNewSocket(sdata);
					//net::Socket* sock_ = new Socket(sdata);
					owning_ptr<Socket> sock_ = make_owning<Socket>(static_cast<Node*>(this->node), sdata);
					soft_ptr<Socket> retSock( sock_ );
				this->socketList.add( std::move(sock_) );
				return retSock;
			}
			void removeSocket( soft_ptr<typename Socket::StorableType> sock ) {
//				socketList.removeAndDelete( static_cast<SocketForserver*>(sock) );
				this->socketList.removeAndDelete( sock );
			}
			size_t getSockCount() {return this->socketList.getCount();}
			//SocketBase* makeSocket(OpaqueSocketData& sdata) { return new Socket( static_cast<Node*>(this->node), sdata ); }

		};

	} //namespace net
} //namespace nodecpp

#endif //SERVER_T_H