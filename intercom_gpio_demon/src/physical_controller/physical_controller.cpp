#include <mutex>
#include <chrono>
#include "physical_controller.h"
#include "async_logger.h"
#include "pigpio.h"
#include <unistd.h>
#include <algorithm>
#include <fstream>

namespace {
	constexpr uint32_t START_ACCEPT_PAUSE_MS = 1;
	constexpr uint32_t ACCEPT_REQ_PAUSE_MS   = 60;
	constexpr uint32_t OPEN_DOOR_PAUSE_US    = 8500;
}

std::shared_ptr<PhysicalController> PhysicalController::create_controller(uint8_t expect_number,
																		  uint8_t pin_input,
																		  uint8_t pin_out0,
																		  uint8_t pin_out1,
																		  uint8_t pin_out2)
{
	LOG_INFO("PhysicalController instance created");

	if(std::any_of(cache_expect.begin(), cache_expect.end(), [expect_number](uint8_t number) {return number == expect_number;}))
	{
		return nullptr;
	}

	for (uint8_t pin : {pin_input, pin_out0, pin_out1, pin_out2})
	{
	    if(std::any_of(cache_pins.begin(), cache_pins.end(), [pin](const pins& p) { return p.pin_input == pin ||
	    																		           p.pin_out0 == pin  ||
																				           p.pin_out1 == pin  ||
																				           p.pin_out2 == pin;}))
	    {
	    	return nullptr;
	    }
	}

	cache_expect.push_back(expect_number);
	cache_pins.push_back({pin_input, pin_out0, pin_out1, pin_out2});
	return std::shared_ptr<PhysicalController>(new PhysicalController(expect_number, pin_input, pin_out0, pin_out1, pin_out2)); // @suppress("Symbol is not resolved")
}

bool PhysicalController::add_expect_if_exist(std::vector<std::shared_ptr<PhysicalController>> ptr_in_v,
		                                     uint8_t expect_number, uint8_t pin_input, uint8_t pin_out0, uint8_t pin_out1, uint8_t pin_out2)
{
	for(auto ptr : ptr_in_v)
	{
		return (add_expect_if_exist(ptr, expect_number, {pin_input, pin_out0, pin_out1, pin_out2}));
	}
	return false;
}

bool PhysicalController::add_expect_if_exist(std::shared_ptr<PhysicalController> ptr_in, uint8_t expect_number, pins pin_in)
{
	LOG_DEBUG("Check compatibility");
	if(ptr_in->check_pins(pin_in))
	{
		LOG_DEBUG("Pins compatibility");
		if(!(ptr_in->check_expects(expect_number))) ptr_in->add_expect(expect_number);
		return true;
	}
	else return false;
}

void PhysicalController::add_expect(uint8_t expect_number)
{
	LOG_DEBUG("Added expect to exist");
	expect.push_back(expect_number);
}

PhysicalController::PhysicalController(uint8_t expect_number, uint8_t pin_input, uint8_t pin_out0, uint8_t pin_out1, uint8_t pin_out2):
		config_pins{pin_input, pin_out0, pin_out1, pin_out2}
{
	LOG_DEBUG("Start PhysicalController constructor");
	expect.push_back(expect_number);
    gpioSetMode(config_pins.pin_input, PI_INPUT);
    gpioSetMode(config_pins.pin_out1, PI_OUTPUT);
    gpioSetMode(config_pins.pin_out2, PI_OUTPUT);
    gpioSetPullUpDown(config_pins.pin_input, PI_PUD_OFF);
    gpioWrite(config_pins.pin_out1, PI_LOW);
    gpioWrite(config_pins.pin_out2, PI_LOW);
    if(pin_out0!=0)
    {
    	LOG_DEBUG("Connection to line");
        gpioSetMode(config_pins.pin_out0, PI_OUTPUT);
        gpioWrite(config_pins.pin_out0, PI_HIGH);
    }
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
    if(config_pins.pin_out0!=0)
    {
    	gpioWrite(config_pins.pin_out0, PI_LOW);
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
    	    	//LOG_DEBUG(("time window close" + std::to_string(self->req_number)));
		        if(self->check_expects(self->req_number))
    	        {
    	    		LOG_DEBUG("cheesed expect number");
    	    		self->find_number = self->req_number;
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

bool PhysicalController::check_pins(pins pins_to_check)
{
	 return((config_pins == pins_to_check) ? true : false);
}

bool PhysicalController::check_expects(uint8_t expect_in)
{
	return std::any_of(expect.begin(), expect.end(), [expect_in](uint8_t number) {return number == expect_in;});
}

uint8_t PhysicalController::get_expects()
{
	std::lock_guard<std::mutex> lock(mtx);
	return find_number;
}

void PhysicalController::wait_for_request(std::function<void(std::shared_ptr<PhysicalController>)> callback, uint32_t window_ms)
{
		LOG_INFO("start wait to request");
		std::lock_guard<std::mutex> lock(mtx);
		interface_callbeck = callback;
		timout = window_ms;
		start_alert();
}

void PhysicalController::accept_request()
{
	LOG_INFO("accept_request call");
	std::this_thread::sleep_for(std::chrono::seconds(START_ACCEPT_PAUSE_MS));
	gpioWrite(config_pins.pin_out1, PI_HIGH);
	std::this_thread::sleep_for(std::chrono::milliseconds(ACCEPT_REQ_PAUSE_MS));
    gpioWrite(config_pins.pin_out2, PI_HIGH);
    gpioWrite(config_pins.pin_out1, PI_LOW);
}

void PhysicalController::open_door()
{
	LOG_INFO("open_door call");
    gpioWrite(config_pins.pin_out2, PI_LOW);
    std::this_thread::sleep_for(std::chrono::microseconds(OPEN_DOOR_PAUSE_US));
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
	LOG_DEBUG("GPIO alert activate");
	gpioSetAlertFuncEx(config_pins.pin_input, &PhysicalController::gpioCallbackEx, this);
}

inline void PhysicalController::stop_alert()
{
	LOG_DEBUG("GPIO alert deactivate");
	gpioSetAlertFuncEx(config_pins.pin_input, nullptr, nullptr);
}
