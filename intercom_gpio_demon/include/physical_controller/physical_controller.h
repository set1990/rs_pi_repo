#ifndef PHYSICAL_CONTROLLER_H
#define PHYSICAL_CONTROLLER_H

#include <iostream>
#include <thread>
#include <functional>
#include <memory>

struct pins
{
	uint8_t pin_input;
    uint8_t pin_out1;
    uint8_t pin_out2;
};

class PhysicalController: public std::enable_shared_from_this<PhysicalController>
{
public:
    static std::shared_ptr<PhysicalController> create_controller(uint8_t pin_input, uint8_t pin_out1, uint8_t pin_out2);
    void open_door();
    void wait_for_request(uint8_t expect_number = 0, std::function<void(std::shared_ptr<PhysicalController>)> callback = nullptr, uint8_t window_ms = 200);
    void accept_request();
    void decline_request();
    void end_call();

    PhysicalController(const PhysicalController&) = delete;
    PhysicalController& operator=(const PhysicalController&) = delete;

    ~PhysicalController();

private:
    std::mutex mtx;
	uint8_t timout = 0;
	uint8_t req_number = 0;
	uint8_t expect = 0;
	pins config_pins;
	bool is_registered = false;
	std::function<void(std::shared_ptr<PhysicalController>)> interface_callbeck = nullptr;
	static void gpioCallbackEx(int gpio, int level, uint32_t tick, void* userdata);
    PhysicalController(uint8_t pint_input, uint8_t pint_out1, uint8_t pint_out2);
};

#endif /* PHYSICAL_CONTROLLER_H  */
