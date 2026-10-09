#include <iostream>
#include <cstdlib>
#include <chrono>
#include "async_logger.h"
#include "libipc.h"

 constexpr uint8_t RING = 6;
 constexpr uint8_t PIN_INPUT  = 25;
 constexpr uint8_t PIN_OTPUT0 = 12;
 constexpr uint8_t PIN_OTPUT1 = 23;
 constexpr uint8_t PIN_OTPUT2 = 24;

int main()
{
	char val;
	std::cout<<"start"<<std::endl;
	msg_package msg;
	msg_package msg_out;
	IpcConnectorClient client;
	msg.type = msg_type::CONNECT;
	msg.payload[NUMBER_RING] = RING;
	msg.payload[NUMBER_PIN_INPUT] = PIN_INPUT;
	msg.payload[NUMBER_PIN_OTPUT0] = PIN_OTPUT0;
	msg.payload[NUMBER_PIN_OTPUT1] = PIN_OTPUT1;
	msg.payload[NUMBER_PIN_OTPUT2] = PIN_OTPUT2;
	std::cout<<"send"<<std::endl;
	msg_out = client.send_msg(msg);
	std::cout<<"received type:"<<static_cast<int>(msg_out.type)<<" number:"<<msg_out.payload[0]<<std::endl;
	if(msg_out.type == msg_type::CALL_REQ)
	{
	   std::cout<<"Y accept or N decline: "<<std::endl;
	   std::cin >> val;
	   if(val=='Y')
	   {
		   std::cout<<"ok"<<std::endl;
		   msg.type = msg_type::REQ_ACCEPT;
		   msg_out = client.send_msg(msg);
		   do
		   {
			   std::cout<<"Y open or N end call: "<<std::endl;
			   std::cin >> val;
			   if(val=='Y') msg.type = msg_type::ACTION;
			   if(val=='N') msg.type = msg_type::CALL_END;
			   msg_out = client.send_msg(msg);
		   }
		   while(msg.type !=msg_type::CALL_END);

	   }
	   else
	   {
		   msg.type == msg_type::REQ_REJECT;
		   msg_out = client.send_msg(msg);
	   }
	}
}
