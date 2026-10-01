#include <mutex>
#include <chrono>
#include "physical_controller.h"
#include "async_logger.h"
#include "pigpio.h"

namespace {
	constexpr uint32_t START_TASK_PAUSE_MS = 10;
	constexpr uint32_t ACCEPT_REQ_PAUSE_MS = 60;
	constexpr uint32_t OPEN_DOOR_PAUSE_MS  = 85;
}


std::shared_ptr<PhysicalController> PhysicalController::create_controller(uint8_t pin_input, uint8_t pin_out1, uint8_t pin_out2)
{
	LOG_INFO("PhysicalController instance created");
	return std::shared_ptr<PhysicalController>(new PhysicalController(pin_input, pin_out1, pin_out2)); // @suppress("Symbol is not resolved")
}

PhysicalController::PhysicalController(uint8_t pint_input, uint8_t pint_out1, uint8_t pint_out2): config_pins{pint_input, pint_out1, pint_out2}
{
    gpioSetMode(config_pins.pin_input, PI_INPUT);
    gpioSetMode(config_pins.pin_out1, PI_OUTPUT);
    gpioSetMode(config_pins.pin_out2, PI_OUTPUT);
    gpioSetPullUpDown(config_pins.pin_input, PI_PUD_OFF);
    gpioWrite(config_pins.pin_out1, PI_LOW);
    gpioWrite(config_pins.pin_out2, PI_LOW);
    worker_thread = std::thread(&PhysicalController::worker_loop, this);
}

PhysicalController::~PhysicalController()
{
	stop_alert();

	{
		std::lock_guard<std::mutex> lock(queue_mtx);
	    stop_worker = true;
	}
	queue_cv.notify_all();

	if (worker_thread.joinable())
	{
		worker_thread.join();
	}
	LOG_INFO("worker thread finish");
}

void PhysicalController::worker_loop()
{
	LOG_INFO("worker loop start");
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
        std::this_thread::sleep_for(std::chrono::milliseconds(START_TASK_PAUSE_MS));
        if (task)
        {
        	LOG_DEBUG("task start");
        	task();
        	LOG_DEBUG("task finish");
        }
    }
    LOG_INFO("worker loop finish");
}

void PhysicalController::enqueue_task(std::function<void()> task)
{
    {
    	std::lock_guard<std::mutex> lock(queue_mtx);
    	LOG_DEBUG("task added to queue");
        task_queue.push(std::move(task));
    }
    queue_cv.notify_one();
}

void PhysicalController::gpioCallbackEx(int gpio, int level, uint32_t tick, void* userdata)
{
	LOG_INFO("gpioCallbackEx call");
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
    	    	LOG_DEBUG("req_number increase");
    	    }
    	    else if (level == 2)
    	    {
    	    	LOG_DEBUG("time window close");
    	    	if (self->req_number == self->expect)
    	        {
    	    		LOG_DEBUG("cheesed expect number");
    	            callback_to_run = self->interface_callbeck;
    	        }
    	        self->req_number = 0;
    	        gpioSetWatchdog(self->config_pins.pin_input, PI_OFF);
    	    }
    	}
    	if (callback_to_run)
    	{
    		LOG_DEBUG("callback_to_run send to queue");
    		self->stop_alert();
    		auto self_ptr = self->shared_from_this();
    		self->enqueue_task([self_ptr, callback_to_run]() {
    			LOG_DEBUG("callback_to_run call");
    			callback_to_run(self_ptr);
    		});

    	}
    }
}

void PhysicalController::wait_for_request(uint8_t expect_number, std::function<void(std::shared_ptr<PhysicalController>)> callback, uint32_t window_ms)
{
	if(expect_number>0)
	{
		LOG_INFO("start wait to request");
		std::lock_guard<std::mutex> lock(mtx);
		expect = expect_number;
		interface_callbeck = callback;
		timout = window_ms;
		start_alert();
	}
}

void PhysicalController::accept_request()
{
	LOG_INFO("accept_request call");
    gpioWrite(config_pins.pin_out1, PI_HIGH);
    std::this_thread::sleep_for(std::chrono::milliseconds(ACCEPT_REQ_PAUSE_MS));
    gpioWrite(config_pins.pin_out2, PI_HIGH);
    gpioWrite(config_pins.pin_out1, PI_LOW);
}

void PhysicalController::open_door()
{
	LOG_INFO("open_door call");
    gpioWrite(config_pins.pin_out2, PI_LOW);
    std::this_thread::sleep_for(std::chrono::milliseconds(OPEN_DOOR_PAUSE_MS));
    gpioWrite(config_pins.pin_out2, PI_HIGH);
}

void PhysicalController::decline_request()
{
	LOG_INFO("decline_request call");
	start_alert();
}


void PhysicalController::end_call()
{
	LOG_INFO("end_call call");
    gpioWrite(config_pins.pin_out2, PI_LOW);
    start_alert();
}

inline void PhysicalController::start_alert()
{
	LOG_DEBUG("gpio alert activate");
	gpioSetAlertFuncEx(config_pins.pin_input, &PhysicalController::gpioCallbackEx, this);
}

inline void PhysicalController::stop_alert()
{
	LOG_DEBUG("gpio alert deactivate");
	gpioSetAlertFuncEx(config_pins.pin_input, nullptr, nullptr);
}
