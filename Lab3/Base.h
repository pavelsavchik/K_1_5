#pragma once
#define _SCL_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <vector>
#include <thread>
#include <mutex>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment (lib, "Ws2_32.lib")

#define TIMEOUT 5000
#define SERVER_TIMEOUT TIMEOUT - 2000
#define TIME_STRUCT DWORD
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <std::string.h>

#define TIMEOUT 5000000
#define SERVER_TIMEOUT TIMEOUT - 2000000
#define DWORD unsigned long
#define fpos_t int64_t
#define Sleep sleep
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#define SD_BOTH SHUT_RDWR
#define TIME_STRUCT timeval
#define ADDR_ANY INADDR_ANY
#endif

#define DELAY 1
#define PATH_DELIM '\\'
#define PATH_DELIM_LINUX '/'
#define METADATA_DELIM '\r'
#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT 22222
#define BUFFER_SIZE 65000
#define UDP_NUMBER_SIZE 4
#define UDP_BUFFER_SIZE (BUFFER_SIZE - UDP_NUMBER_SIZE)
#define PACKAGE_COUNT 1000
#define PACKAGES_TILL_DROP 50000
#define REQUEST_ALL_PACKAGES 0
#define ACK "ACK"

struct Package {
	char *data;
	size_t size;
	Package() { }
	Package(char *data, size_t size) : data(data), size(size) {	}
};

class Base
{
	void CheckRecvResult(int result);
	Package* PeekRawData(SOCKET socket);
protected:
	
	static char *buffer;
	unsigned int port;
	SOCKET _tcp_socket;
	static SOCKET _udp_socket;

	void virtual OpenFile(std::fstream *file, std::string fileName) = 0;

	void CreateTCPSocket();
	SOCKET CreateUDPSocket();

	void SetReceiveTimeout(SOCKET socket);
	void SetReceiveTimeout(SOCKET socket, TIME_STRUCT timeout);
	void SetSendTimeout(SOCKET socket);
	void SetSendTimeout(SOCKET socket, TIME_STRUCT timeout);
	TIME_STRUCT GetTimeout(unsigned time = TIMEOUT);

	fpos_t GetNumber(char* data, fpos_t startPosition);
	static void AddNumberToDatagram(char *buffer, fpos_t startPosition, fpos_t number);

	sockaddr_in* CreateAddressInfo(std::string address, unsigned int port);

	void SendMessageTo(SOCKET socket, std::string message, sockaddr *to);
	void SendMessage(SOCKET socket, std::string message);
	static size_t SendRawData(SOCKET socket, const char *data, size_t size);
	static size_t SendRawDataTo(SOCKET socket, const char* data, size_t size, sockaddr *to);

	std::string ReceiveMessage(SOCKET socket);
	std::string ReceiveMessageFrom(SOCKET socket, sockaddr *from);
	Package* ReceiveRawData(SOCKET socket, int size = BUFFER_SIZE, int flags = NULL);
	Package* ReceiveRawDataFrom(SOCKET socket, sockaddr *from, int size = BUFFER_SIZE, int flags = NULL);
public:
	explicit Base(unsigned int port = DEFAULT_PORT);
	void virtual Close();
	virtual ~Base();
};

class ConnectionInterrupted : public std::exception
{
	fpos_t position;
public:
	ConnectionInterrupted(fpos_t pos): position(pos) {}
	fpos_t GetProgress() {
		return position;
	}
};

#define EX_SENDING_FAILED "Sending failed."
#define EX_CONNECTION_CLOSED "Connection closed."
#define EX_CONNECTION_INTERRUPTED "Connection interrupted."
#define EX_FILE_ERROR "Can't open/create file."
#define EX_SOCKET_ERROR "Can't create socket."
#define EX_SET_TIMEOUT_ERROR "Can't set the timeout."
#define EX_CONNECTION_ERROR "Can't connect."
#define EX_BIND_ERROR "Bind failed."
#define EX_FILE_NOT_FOUND "File not found."
#define EX_SERVER_ERROR "Server error."
#define EX_LISTEN_ERROR "Listen failed."
#define EX_ACCEPT_ERROR "Accept failed."
#define EX_SENDING_DONE "Sending done."
#define EX_NO_NEW_CLIENTS "There is no new clients."
#define EX_SET_NONBLOCK_ERROR "Can't set to nonblock mode."
#define EX_SET_BLOCKING_ERROR "Cam't set to block mode."
#define EX_WRONG_IP "Wrong IP."
