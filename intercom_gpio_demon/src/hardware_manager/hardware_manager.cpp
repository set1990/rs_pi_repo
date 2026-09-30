#include "hardware_manager.h"
#include "async_logger.h"
#include <pigpio.h>
#include <stdexcept>
#include <iostream>

bool HardwareManager::is_initialized = false;

void HardwareManager::initialize()
{
    if(!is_initialized)
    {
    	gpioCfgClock(1, PI_CLOCK_PWM, 0);
        if (gpioInitialise() >= 0)
        {
            is_initialized = true;
            std::cout << "Hardware system initialized.\n";
        }

    }
}

void HardwareManager::terminate()
{
    if(is_initialized)
    {
        gpioTerminate();
        is_initialized = false;
        std::cout << "Hardware system terminated.\n";
    }
}

