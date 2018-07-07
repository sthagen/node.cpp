// NetSocket.cpp : Defines the entry point for the console application.
//


#include "../../3rdparty/fmt/include/fmt/format.h"
#include "../../include/nodecpp/net.h"
#include "../../include/nodecpp/loop.h"


using namespace std;
using namespace nodecpp;
using namespace fmt;

class MyServer;

class MySocket :public net::Socket {
	size_t count = 0;

public:
	MySocket() {}

	void onClose(bool hadError) override {
		print("onClose!\n");
	}

	void onConnect() override {
		print("onConnect!\n");
		const char buff[] = "hello world!";
		write(reinterpret_cast<const uint8_t*>(buff), sizeof(buff));
	}

	void onData(Buffer buffer) override {
		print("onData!\n");
		++count;
		if(count <= 3)
			write(buffer.begin(), buffer.size());

		if (count == 3)
			end();
	}

	void onDrain() override {
		print("onDrain!\n");
	}

	void onEnd() override {
		print("onEnd!\n");
	}

	void onError() override {
		print("onError!\n");
	}
};


class MyServerSocket :public net::Socket {
	size_t count = 0;
	MyServer* server = nullptr;

public:
	MyServerSocket(MyServer* server) :server(server) {}

	void onClose(bool hadError) override;

	void onConnect() override {
		print("onConnect!\n");
	}

	void onData(Buffer buffer) override {
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
	void onConnection(unique_ptr<net::Socket> socket) override {
		print("onConnection!\n");
		socks.push_back(std::move(socket));
	}
	void onListening() override {
		print("onListening!\n");
	}

	void onError() override {
		print("onError!\n");
	}
	std::unique_ptr<net::Socket> makeSocket() override {
		return std::unique_ptr<net::Socket>(new MyServerSocket(this));
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


int main()
{
	auto srv = net::createServer<MyServer>();
	srv->listen(2000, "127.0.0.1", 5);

	auto cli = net::createConnection<MySocket>(2000, "127.0.0.1");

	runLoop();

	return 0;
}


