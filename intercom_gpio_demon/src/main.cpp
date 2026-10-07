#include <iostream>
#include <cstdlib>
#include <chrono>
#include "pigpio.h"
#include "async_logger.h"
#include "physical_controller.h"
#include "hardware_manager.h"
#include "libipc.h"

#define NUMBER_RING					0
#define NUMBER_PIN_INPUT			1
#define NUMBER_PIN_OTPUT0			2
#define NUMBER_PIN_OTPUT1			3
#define NUMBER_PIN_OTPUT2			4

constexpr uint8_t PIN_INPUT  = 25;
constexpr uint8_t PIN_OTPUT0 = 12;
constexpr uint8_t PIN_OTPUT1 = 23;
constexpr uint8_t PIN_OTPUT2 = 24;

int main()
{
	AsyncLogger::instance().load_config_ini();
	HardwareManager::initialize();
	IpcConnectorSerwer server;
	std::vector<std::shared_ptr<PhysicalController>> controllers;
	server.msg_handler([&controllers,&server](msg_package& msg_in)->msg_package{
		msg_package msg_out;
		switch (msg_in.type)
		{
		case msg_type::CONNECT:
			{
			auto controlptr = PhysicalController::create_controller(msg_in.payload[NUMBER_RING],
																    msg_in.payload[NUMBER_PIN_INPUT],
																	msg_in.payload[NUMBER_PIN_OTPUT0],
																	msg_in.payload[NUMBER_PIN_OTPUT1],
																	msg_in.payload[NUMBER_PIN_OTPUT2]);
			if(controlptr != nullptr)
			{
				controlptr->wait_for_request([&server](std::shared_ptr<PhysicalController> controller){
					msg_package msg;
					msg.type = msg_type::CALL_REQ;
					msg.payload[0] = controller->expect;
					server.send_to_clients(msg);
					msg = server.wait_for_answer(controller->expect);
					if(msg.type==msg_type::REQ_ACCEPT)
					{
						controller->accept_request();
						do{
							msg = server.wait_for_answer(controller->expect);
							if(msg.type==msg_type::ACTION)
							{
								controller->open_door();
							}
						}while(msg.type==msg_type::CALL_END);
						controller->end_call();
					}
				});
				controllers.push_back(controlptr);
			}}
			msg_out.type = msg_type::CONNECT;
			break;
		case msg_type::REQ_ACCEPT:
		case msg_type::REQ_REJECT:
		case msg_type::ACTION:
		case msg_type::CALL_END:
			server.notify_answer(msg_in);
			break;
		case msg_type::EMPTY:
		default:
			break;
		}
		return msg_out;
	});
	while(true)
	{
		std::this_thread::sleep_for(std::chrono::seconds(10));
	}
    return 0;
}
