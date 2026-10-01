#include <iostream>
#include <cstdlib>
#include "pigpio.h"
#include "async_logger.h"
#include "physical_controller.h"
#include "hardware_manager.h"


int main()
{
	AsyncLogger::instance().load_config_ini();
	HardwareManager::initialize();
	auto controller_gpio = PhysicalController::create_controller(25,24,28);
	controller_gpio->wait_for_request(6,[](std::shared_ptr<PhysicalController> controller){controller->accept_request();});

	LOG_ERROR("Log działa");
    std::cout << "Hello Raspberry Pi Zero 2 W!" << std::endl;
    return 0;
}
