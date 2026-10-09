#include "libipc.h"
#include <unistd.h>
#include <cstring>
#include <poll.h>
#include <cerrno>
#include <sys/eventfd.h>
#include "async_logger.h"

namespace {
	constexpr const char* SERVER_PATH = "/tmp/intercom_dgram_server.sock";
	constexpr std::size_t BUFFER_SIZE = 1024;
}


IpcConnectorSerwer::IpcConnectorSerwer()
{
	 LOG_INFO("Server start");
	 stop_fd = eventfd(0, EFD_CLOEXEC);
	 if (stop_fd == -1) error = true;
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
	stop();
    close(server_fd);
    close(stop_fd);
    unlink(SERVER_PATH);
}

void IpcConnectorSerwer::run_msg_handler(std::function<msg_package(msg_package& msg)> callback_for_connect)
{
	if (worker.joinable()) return;
	worker = std::thread ([this,callback_for_connect]() {
	    msg_package msg_in;
	    msg_package msg_out;
	    sockaddr_un client_addr{};
	    socklen_t client_len = sizeof(client_addr);
	    char buffer[BUFFER_SIZE]{};

	    pollfd fds[2] = {
	        { server_fd, POLLIN, 0 },
	        { stop_fd,   POLLIN, 0 },
	    };

	    while(true)
	    {
	    	int r = poll(fds, 2, -1);
	    	if (r < 0)
	    	{
	    		if (errno == EINTR) continue;
	    		break;
	    	}
	    	if (fds[1].revents & POLLIN) break;
	    	if (fds[0].revents & (POLLERR | POLLNVAL)) break;
	    	if (!(fds[0].revents & POLLIN)) continue;

	    	LOG_DEBUG("Start receive");
		    ssize_t bytes_read = recvfrom(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)(&client_addr), &client_len); // @suppress("Invalid arguments")

		    if(bytes_read > 0)
		    {
		    	msg_in.type = msg_type(buffer[0]);
		        std::memcpy(msg_in.payload, &buffer[1], PAYLOAD_SIZE); // @suppress("Invalid arguments")
				LOG_DEBUG("Check type");
				msg_out.type = msg_in.type;
				switch (msg_in.type)
				{
				case msg_type::CONNECT:
					LOG_DEBUG("Call CONNECT callback");
					msg_out = callback_for_connect(msg_in);
					break;
				case msg_type::REQ_ACCEPT:
				case msg_type::REQ_REJECT:
				case msg_type::ACTION:
				case msg_type::CALL_END:
					//LOG_DEBUG(("Notify abswer" + std::to_string((int)msg_in.type)));
					notify_answer(msg_in);
					break;
				case msg_type::EMPTY:
				default:
					break;
				}
		        std::memset(buffer, 0, sizeof(buffer)); // @suppress("Invalid arguments")
		        buffer[0] = char(msg_out.type);
		        std::memcpy(&buffer[1], msg_out.payload, PAYLOAD_SIZE); // @suppress("Invalid arguments")
		        LOG_DEBUG("Check address");
		         if(client_addr.sun_path[0] != '\0')
		         {
		        	 LOG_DEBUG("Check msg type" );
		        	 if(msg_out.type == msg_type::CONNECT)
		        	 {
		        		 LOG_DEBUG("ADD client to cache");
		        		 clients_map[msg_in.payload[0]].push_back(client_addr);
		        	 }
		        	 else
		        	 {
		        		 LOG_DEBUG("Send Back");
		        		 sendto(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)(&client_addr), client_len); // @suppress("Invalid arguments")
		        	 }
		         }
		    }
	    }
	});
}

void IpcConnectorSerwer::send_to_clients(msg_package msg)
{
	char buffer[BUFFER_SIZE]{};
    buffer[0] = char(msg.type);
    std::memcpy(&buffer[1], msg.payload, PAYLOAD_SIZE); // @suppress("Invalid arguments")
    LOG_DEBUG(("serch client no. " + std::to_string(msg.payload[0])));
    auto it = clients_map.find(msg.payload[0]);
    if(it != clients_map.end()) {
        for (const auto& addres : it->second) {
        	LOG_DEBUG("sendto call");
        	sendto(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)(&addres), sizeof(addres)); // @suppress("Invalid arguments")
        }
    }

}

msg_package IpcConnectorSerwer::wait_for_answer(uint8_t expect_number)
{
	msg_package msg_out;
	std::unique_lock<std::mutex> lock(mtx);
	cv.wait(lock, [this, expect_number] { return stopping || (this->answer.payload[0] == expect_number);});

	if(stopping) msg_out.type = msg_type::EMPTY;
	else msg_out = answer;
	answer.type = msg_type::EMPTY;
	answer.payload[0] = 0;
	return msg_out;
}

void IpcConnectorSerwer::notify_answer(msg_package answer_in)
{
	std::lock_guard<std::mutex> lock(mtx);
	answer = answer_in;
	cv.notify_all();
}

void IpcConnectorSerwer::stop()
{
    if (stopping.exchange(true)) return;
    uint64_t one = 1;
    (void)write(stop_fd, &one, sizeof one); // @suppress("Invalid arguments")
    { std::lock_guard<std::mutex> lock(mtx); }
    cv.notify_all();
    if (worker.joinable()) worker.join();
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

msg_package IpcConnectorClient::wait_for_msg()
{
	msg_package msg_back;
	msg_back.type = msg_type::EMPTY;
	char buffer[BUFFER_SIZE]{};
	ssize_t bytes_read = recvfrom(client_fd, buffer, sizeof(buffer) - 1, 0, nullptr, nullptr); // @suppress("Invalid arguments")

    if (bytes_read > 0)
    {
    	msg_back.type =msg_type(buffer[0]);
    	std::memcpy(msg_back.payload, &buffer[1], PAYLOAD_SIZE); // @suppress("Invalid arguments")
    }
	return msg_back;
}

