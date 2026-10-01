#include <iostream>
#include <cstdlib>
#include <chrono>
#include "pigpio.h"
#include "async_logger.h"
#include "physical_controller.h"
#include "hardware_manager.h"

constexpr uint8_t PIN_INPUT  = 25;
constexpr uint8_t PIN_OTPUT1 = 23;
constexpr uint8_t PIN_OTPUT2 = 24;

int main()
{
	AsyncLogger::instance().load_config_ini();
	HardwareManager::initialize();
	auto controller_gpio = PhysicalController::create_controller(PIN_INPUT,PIN_OTPUT1,PIN_OTPUT2);
	controller_gpio->wait_for_request(6,[](std::shared_ptr<PhysicalController> controller){
		controller->accept_request();
		std::this_thread::sleep_for(std::chrono::seconds(10));
		controller->open_door();
		std::this_thread::sleep_for(std::chrono::seconds(10));
		controller->end_call();
	});
	while(true)
	{
		std::this_thread::sleep_for(std::chrono::seconds(10));
	}
    return 0;
}
