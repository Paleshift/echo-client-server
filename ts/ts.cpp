#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux__
#include <arpa/inet.h>
#include <sys/socket.h>
#endif // __linux
#ifdef WIN32
#include <winsock2.h>
#include "../mingw_net.h"
#endif // WIN32
#include <thread>
#include <vector>

#ifdef WIN32
void myerror(const char* msg) { fprintf(stderr, "%s %lu\n", msg, GetLastError()); }
#else
void myerror(const char* msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno); }
#endif

void usage() {
	printf("echo-server %s\n",
#include "../version.txt"
   );
	printf("\n");
	printf("syntax: echo-server <port> [-e[-b]]\n");
	printf("  -e : echo\n");
	printf("  -b : broadcast\n");
	printf("sample: echo-server 1234 -e -b\n");
}

std::vector <int> clients;
//Create a vector object, "clients".

struct Param {
	bool echo{false};
	bool broadcast{false};
	uint16_t port{0};
	uint32_t srcIp{0};

	bool parse(int argc, char* argv[]) {
		for (int i = 1; i < argc;) {
			if (strcmp(argv[i], "-e") == 0) {
				echo = true;
				i++;
				continue;
			}

			//+++++
			if (strcmp(argv[i], "-b") == 0) {
				broadcast = true;
				i++;
				continue;
			}
			//+++++

			if (i < argc) port = atoi(argv[i++]);
		}
		return port != 0;
	}
} param;

void recvThread(int sd) {
	printf("connected\n");
	fflush(stdout);
	static const int BUFSIZE = 65536;
	char buf[BUFSIZE];
	while (true) {
		ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
		if (res == 0 || res == -1) {
			fprintf(stderr, "recv return %zd", res);
			myerror(" ");
			break;
		}
		buf[res] = '\0';
		printf("%s", buf);
		fflush(stdout);
		if (param.echo) {
			res = ::send(sd, buf, res, 0);
			if (res == 0 || res == -1) {
				fprintf(stderr, "send return %zd", res);
				myerror(" ");
				break;
			}
		}

		//+++++
		if (param.broadcast) {
			for (const auto &elem : clients) {
			//Range-based "for" Statement(C++)
			//The "auto" keyword causes type inference to be used. Preferred.
			//Type inference by "const" reference("&").
			//Observes in-place. Preferred when no modify is needed.
			//Range-based "for" loop to iterate through the vector, observing in-place.
				if (elem == sd){
					continue;
					//Ignore below code and go to "for (const auto &elem : clients)"
				}

				res = ::send(elem, buf, res, 0);
				if (res == 0 || res == -1) {
					fprintf(stderr, "send return %zd", res);
					myerror(" ");
					break;
				}
			}
		}
		//+++++

	}
	printf("disconnected\n");
	fflush(stdout);
	::close(sd);
}

int main(int argc, char* argv[]) {
	if (!param.parse(argc, argv)) {
		usage();
		return -1;
	}

#ifdef WIN32
	WSAData wsaData;
	WSAStartup(0x0202, &wsaData);
#endif // WIN32

	//
	// socket
	//
	int sd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1) {
		myerror("socket");
		return -1;
	}

#ifdef __linux__
	//
	// setsockopt
	//
	{
		int optval = 1;
		int res = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
		if (res == -1) {
			myerror("setsockopt");
			return -1;
		}
	}
#endif // __linux

	//
	// bind
	//
	{
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = param.srcIp;
		addr.sin_port = htons(param.port);

		ssize_t res = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
		if (res == -1) {
			myerror("bind");
			return -1;
		}
	}

	//
	// listen
	//
	{
		int res = listen(sd, 5);
		if (res == -1) {
			myerror("listen");
			return -1;
		}
	}

	while (true) {
		struct sockaddr_in addr;
		socklen_t len = sizeof(addr);
		int newsd = ::accept(sd, (struct sockaddr *)&addr, &len);
		if (newsd == -1) {
			myerror("accept");
			break;
		}
		std::thread* t = new std::thread(recvThread, newsd);

		clients.push_back(newsd);

		t->detach();
	}
	::close(sd);
}
