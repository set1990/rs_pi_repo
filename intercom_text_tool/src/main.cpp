#include <iostream>
#include <cstdlib>
#include <chrono>
#include "async_logger.h"
#include "libipc.h"



int main()
{

	while(true)
	{
		std::this_thread::sleep_for(std::chrono::seconds(10));
	}
    return 0;
}
