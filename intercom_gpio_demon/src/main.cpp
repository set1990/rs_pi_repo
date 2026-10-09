#include <iostream>
#include <cstdlib>
#include <chrono>
#include <csignal>
#include <atomic>
#include "pigpio.h"
#include "async_logger.h"
#include "physical_controller.h"
#include "hardware_manager.h"
#include "libipc.h"

std::atomic<bool> running(true);

void signalHandler(int signum)
{
	running = false;
}

int main()
{
	int sig;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, nullptr);

	AsyncLogger::instance().load_config_ini("logger_GPIO.ini");
	LOG_INFO("Start application");
	HardwareManager::initialize();
	IpcConnectorSerwer server;
	std::vector<std::shared_ptr<PhysicalController>> controllers;
	server.run_msg_handler([&controllers,&server](msg_package& msg_in)->msg_package{
		LOG_INFO("Connect callback run");
		msg_package msg_out;
		msg_out.type = msg_type::EMPTY;

		auto controlptr = PhysicalController::create_controller(msg_in.payload[NUMBER_RING],
																msg_in.payload[NUMBER_PIN_INPUT],
																msg_in.payload[NUMBER_PIN_OTPUT0],
																msg_in.payload[NUMBER_PIN_OTPUT1],
																msg_in.payload[NUMBER_PIN_OTPUT2]);
		if(controlptr != nullptr)
		{
			LOG_INFO("GPIO controller created");
			controlptr->wait_for_request([&server](std::shared_ptr<PhysicalController> controller){
				LOG_INFO("Start GPIO callback");
				msg_package msg;
				msg.type = msg_type::CALL_REQ;
				msg.payload[0] = controller->get_expects();
				LOG_INFO("Send info to client");
				server.send_to_clients(msg);
				msg = server.wait_for_answer(controller->get_expects());
				if(msg.type==msg_type::REQ_ACCEPT)
				{
					LOG_INFO("REQ_ACCEPT received");
					controller->accept_request();
					do{
						LOG_INFO("Wait for open or end");
						msg = server.wait_for_answer(controller->get_expects());
						if(msg.type == msg_type::ACTION) controller->open_door();
					}while(msg.type == msg_type::CALL_END);
					LOG_INFO("Finish call");
					controller->end_call();
				}
			});
			controllers.push_back(controlptr);
			msg_out.type = msg_type::CONNECT;
		}
		else
		{

			if(PhysicalController::add_expect_if_exist(controllers, msg_in.payload[NUMBER_RING],
													                msg_in.payload[NUMBER_PIN_INPUT],
																    msg_in.payload[NUMBER_PIN_OTPUT0],
																    msg_in.payload[NUMBER_PIN_OTPUT1],
																    msg_in.payload[NUMBER_PIN_OTPUT2]))
			{
				LOG_INFO("Add expect number to exist");
				msg_out.type = msg_type::CONNECT;
			}
		}
		LOG_INFO("Finish connect");
		return msg_out;});

	std::signal(SIGINT,[](int){running = false;});
	sigwait(&set, &sig);
	server.stop();
	LOG_INFO("Finish APP Good Bay!");
    return 0;
}
