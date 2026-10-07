#ifndef LIBIPC_H
#define LIBIPC_H

#include <iostream>
#include <string>
#include <functional>
#include <cstdint>
#include <map>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <sys/un.h>
#include <sys/socket.h>

constexpr std::size_t PAYLOAD_SIZE = 8;

enum class msg_type : uint8_t
{
	CONNECT,
	CALL_REQ,
	REQ_ACCEPT,
	REQ_REJECT,
	CALL_END,
	ACTION,
	EMPTY,
};

struct msg_package
{
	msg_type type;
	char payload[PAYLOAD_SIZE];
};

class IpcConnectorSerwer
{
public:
	IpcConnectorSerwer();
	~IpcConnectorSerwer();
	void msg_handler(std::function<msg_package(msg_package&)> callback);
	void send_to_clients(msg_package msg);
	msg_package wait_for_answer(uint8_t expect_number);
	void notify_answer(msg_package answer);
private:
	std::map<std::uint8_t, std::vector<sockaddr_un>> clients_map;
	int server_fd;
	bool error = false;
	std::mutex mtx;
	std::condition_variable cv;
	msg_package answer;
};

class IpcConnectorClient
{
public:
	IpcConnectorClient();
	~IpcConnectorClient();
	msg_package send_msg(msg_package&);
private:
	int client_fd;
	bool error = false;
	std::string client_path;
	sockaddr_un server_addr{};
};



#endif /* LIBIPC_H  */
