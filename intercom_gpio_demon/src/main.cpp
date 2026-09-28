#include <iostream>
#include <cstdlib>
#include "pigpio.h"
#include "async_logger.h"
#include "physical_controller.h"


int main()
{
	AsyncLogger::instance().load_config_ini();
	LOG_ERROR("Log działa");
    std::cout << "Hello Raspberry Pi Zero 2 W!" << std::endl;
    return 0;
}
