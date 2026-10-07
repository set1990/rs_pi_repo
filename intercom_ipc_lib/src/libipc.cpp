#include "libipc.h"
#include <thread>
#include <unistd.h>
#include <cstring>


namespace {
	constexpr const char* SERVER_PATH = "/tmp/intercom_dgram_server.sock";
	constexpr std::size_t BUFFER_SIZE = 1024;
}


IpcConnectorSerwer::IpcConnectorSerwer()
{
	 server_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	 if(server_fd == -1) error = true;
	 unlink(SERVER_PATH);
	 sockaddr_un server_addr{};
	 server_addr.sun_family = AF_UNIX;
	 std::strncpy(server_addr.sun_path, SERVER_PATH, sizeof(server_addr.sun_path) - 1); // @suppress("Invalid arguments")
	 if (bind(server_fd, (struct sockaddr*)(&server_addr), sizeof(server_addr)) == -1) error = true; // @suppress("Invalid arguments")
}	

IpcConnectorSerwer::~IpcConnectorSerwer()
{
    close(server_fd);
    unlink(SERVER_PATH);
}

void IpcConnectorSerwer::msg_handler(std::function<msg_package(msg_package& msg)> callback)
{
    msg_package msg_in;
    msg_package msg_out;
    sockaddr_un client_addr{};
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE]{};

    ssize_t bytes_read = recvfrom(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)(&client_addr), &client_len); // @suppress("Invalid arguments")

    if(bytes_read > 0)
    {
    	msg_in.type = msg_type(buffer[0]);
        std::memcpy(msg_in.payload, &buffer[1], PAYLOAD_SIZE); // @suppress("Invalid arguments")
        msg_out = callback(msg_in);
        std::memset(buffer, 0, sizeof(buffer)); // @suppress("Invalid arguments")
        buffer[0] = char(msg_out.type);
        std::memcpy(&buffer[1], msg_out.payload, PAYLOAD_SIZE); // @suppress("Invalid arguments")

         if(client_addr.sun_path[0] != '\0')
         {
        	 if(msg_out.type == msg_type::CONNECT)
        	 {
        		 clients_map[msg_in.payload[0]].push_back(client_addr);
        	 }
        	 else sendto(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)(&client_addr), client_len); // @suppress("Invalid arguments")
         }
    }
}

void IpcConnectorSerwer::send_to_clients(msg_package msg)
{
	char buffer[BUFFER_SIZE]{};
    buffer[0] = char(msg.type);
    std::memcpy(&buffer[1], msg.payload, PAYLOAD_SIZE); // @suppress("Invalid arguments")
    auto it = clients_map.find(msg.payload[0]);
    if (it != clients_map.end()) {
        for (const auto& addres : it->second) {
        	sendto(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)(&addres), sizeof(addres)); // @suppress("Invalid arguments")
        }
    }

}

msg_package IpcConnectorSerwer::wait_for_answer(uint8_t expect_number)
{
	msg_package msg_out;
	std::unique_lock<std::mutex> lock(mtx);
	cv.wait(lock, [this, expect_number] { return (this->answer.payload[0] == expect_number);});
	msg_out = answer;
	return msg_out;
}

void IpcConnectorSerwer::notify_answer(msg_package answer_in)
{
	std::lock_guard<std::mutex> lock(mtx);
	answer = answer_in;
	cv.notify_all();
}

IpcConnectorClient::IpcConnectorClient()
{
    client_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (client_fd == -1) error = true;
    client_path = "/tmp/intercom_dgram_client_" + std::to_string(getpid()) + ".sock";
    unlink(client_path.c_str());
    sockaddr_un client_addr{};
    client_addr.sun_family = AF_UNIX;
    std::strncpy(client_addr.sun_path, client_path.c_str(), sizeof(client_addr.sun_path) - 1); // @suppress("Invalid arguments")
    if (bind(client_fd, (struct sockaddr*)(&client_addr), sizeof(client_addr)) == -1) error = true; // @suppress("Invalid arguments")
    server_addr.sun_family = AF_UNIX;
    std::strncpy(server_addr.sun_path, SERVER_PATH, sizeof(server_addr.sun_path) - 1); // @suppress("Invalid arguments")
}

IpcConnectorClient::~IpcConnectorClient()
{
    close(client_fd);
    unlink(client_path.c_str());
}

msg_package IpcConnectorClient::send_msg(msg_package& msg_out)
{
	msg_package msg_back;
	msg_back.type = msg_type::EMPTY;
	char buffer[BUFFER_SIZE]{};
	buffer[0] = char(msg_out.type);
	std::memcpy(&buffer[1], msg_out.payload, PAYLOAD_SIZE); // @suppress("Invalid arguments")
    sendto(client_fd, buffer, PAYLOAD_SIZE+1, 0, (struct sockaddr*)(&server_addr), sizeof(server_addr)); // @suppress("Invalid arguments")
    std::memset(buffer, 0, sizeof(buffer)); // @suppress("Invalid arguments")
    ssize_t bytes_read = recvfrom(client_fd, buffer, sizeof(buffer) - 1, 0, nullptr, nullptr); // @suppress("Invalid arguments")

    if (bytes_read > 0)
    {
    	msg_back.type =msg_type(buffer[0]);
    	std::memcpy(msg_back.payload, &buffer[1], PAYLOAD_SIZE); // @suppress("Invalid arguments")
    }
	return msg_back;
}
