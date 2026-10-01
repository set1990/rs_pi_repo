#include <mutex>
#include <chrono>
#include "physical_controller.h"
#include "async_logger.h"
#include "pigpio.h"


std::shared_ptr<PhysicalController> PhysicalController::create_controller(uint8_t pin_input, uint8_t pin_out1, uint8_t pin_out2)
{
	LOG_INFO("PhysicalController instance created");
	return std::shared_ptr<PhysicalController>(new PhysicalController(pin_input, pin_out1, pin_out2)); // @suppress("Symbol is not resolved")
}

PhysicalController::PhysicalController(uint8_t pint_input, uint8_t pint_out1, uint8_t pint_out2): config_pins{pint_input, pint_out1, pint_out2}
{
    gpioSetMode(config_pins.pin_input, PI_INPUT);
    gpioSetPullUpDown(config_pins.pin_input, PI_PUD_OFF);
    worker_thread = std::thread(&PhysicalController::worker_loop, this);
}

PhysicalController::~PhysicalController()
{
	gpioSetAlertFuncEx(config_pins.pin_input, nullptr, nullptr);

	{
		std::lock_guard<std::mutex> lock(queue_mtx);
	    stop_worker = true;
	}
	queue_cv.notify_all();

	if (worker_thread.joinable())
	{
		worker_thread.join();
	}
}

void PhysicalController::worker_loop()
{
    while (true)
    {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(queue_mtx);
            queue_cv.wait(lock, [this]() {
                return stop_worker || !task_queue.empty();
            });

            if (stop_worker && task_queue.empty()) break;

            task = std::move(task_queue.front());
            task_queue.pop();
        }

        if (task) task();
    }
}

void PhysicalController::enqueue_task(std::function<void()> task)
{
    {
    	std::lock_guard<std::mutex> lock(queue_mtx);
        task_queue.push(std::move(task));
    }
    queue_cv.notify_one();
}

void PhysicalController::open_door()
{
    std::cout << "Opening the door..." << std::endl;
}

void PhysicalController::gpioCallbackEx(int gpio, int level, uint32_t tick, void* userdata)
{
	auto* self = static_cast<PhysicalController*>(userdata);
    if(self)
    {
    	std::function<void(std::shared_ptr<PhysicalController>)> callback_to_run = nullptr;

    	{
    		std::lock_guard<std::mutex> lock(self->mtx);
    	    if(level == 1)
    	    {
    	    	self->req_number++;
    	    	gpioSetWatchdog(self->config_pins.pin_input, self->timout);
    	    }
    	    else if (level == 2)
    	    {
    	    	if (self->req_number == self->expect)
    	        {
    	            callback_to_run = self->interface_callbeck;
    	        }
    	        self->req_number = 0;
    	        gpioSetWatchdog(self->config_pins.pin_input, 0);
    	    }
    	}
    	if (callback_to_run)
    	{
    		auto self_ptr = self->shared_from_this();
    		self->enqueue_task([self_ptr, callback_to_run]() {callback_to_run(self_ptr);});
    	}
    }
}

void PhysicalController::wait_for_request(uint8_t expect_number, std::function<void(std::shared_ptr<PhysicalController>)> callback, uint8_t window_ms)
{
	if(expect_number>0)
	{
		std::lock_guard<std::mutex> lock(mtx);
		expect = expect_number;
		interface_callbeck = callback;
		timout = window_ms;
		gpioSetAlertFuncEx(config_pins.pin_input, &PhysicalController::gpioCallbackEx, this);
	}
}

void PhysicalController::accept_request()
{
    std::cout << "Accepting request..." << std::endl;
}


void PhysicalController::decline_request()
{
    std::cout << "Declining request..." << std::endl;
}


void PhysicalController::end_call()
{
    std::cout << "Ending call..." << std::endl;
}
