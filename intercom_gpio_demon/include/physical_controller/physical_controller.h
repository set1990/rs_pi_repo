/**
 * @file physical_controller.h
 * @brief Definition of the PhysicalController class handling GPIO signals using the pigpio library.
 * @details Handles GPIO input events asynchronously in a dedicated worker thread
 *          to prevent blocking pigpio's internal event distribution thread.
 */

#ifndef PHYSICAL_CONTROLLER_H
#define PHYSICAL_CONTROLLER_H

#include <iostream>
#include <thread>
#include <functional>
#include <memory>
#include <queue>
#include <condition_variable>

/**
 * @struct pins
 * @brief Holds the GPIO pin configuration used by the controller.
 */
struct pins
{
	uint8_t pin_input;
	uint8_t pin_out0;
    uint8_t pin_out1;
    uint8_t pin_out2;
};

/**
 * @class PhysicalController
 * @brief Manages the physical GPIO interface and pulse-counting logic.
 * @details Inherits from std::enable_shared_from_this to enable safe pass-by-value
 *          of shared_ptr instances to asynchronous background tasks.
 */
class PhysicalController: public std::enable_shared_from_this<PhysicalController>
{
public:

	/**
	 * @brief Factory method creating an instance of PhysicalController.
	 * @param pin_input Input pin number.
	 * @param pin_out0 connect disconnect intercom line pine.
	 * @param pin_out1 First output pin number.
	 * @param pin_out2 Second output pin number.
	 * @return std::shared_ptr to the newly created instance.
	 */
    static std::shared_ptr<PhysicalController> create_controller(uint8_t pin_input, uint8_t pin_out0, uint8_t pin_out1, uint8_t pin_out2);

    /**
     * @brief Configures pulse detection on the input pin.
     * @param expect_number Expected number of pulses to trigger the callback.
     * @param callback User callback function executed upon receiving the expected pulses.
     * @param window_ms Watchdog timeout duration [ms] between consecutive pulses.
     */
    void wait_for_request(uint8_t expect_number = 0, std::function<void(std::shared_ptr<PhysicalController>)> callback = nullptr, uint32_t window_ms = 10);

    void open_door();
    void accept_request();
    void decline_request();
    void end_call();

    PhysicalController(const PhysicalController&) = delete;
    PhysicalController& operator=(const PhysicalController&) = delete;

    /**
     * @brief Destructor. Unregisters pigpio callbacks and safely stops the worker thread.
     */
    ~PhysicalController();

private:
    std::mutex mtx;
    uint32_t timout = 0;
	uint8_t req_number = 0;
	uint8_t expect = 0;
	pins config_pins;
	bool is_registered = false;
	std::function<void(std::shared_ptr<PhysicalController>)> interface_callbeck = nullptr;

	std::thread worker_thread;
	std::queue<std::function<void()>> task_queue;
	std::mutex queue_mtx;
	std::condition_variable queue_cv;
	bool stop_worker = false;

	void worker_loop();
	void enqueue_task(std::function<void()> task);
	inline void start_alert();
	inline void stop_alert();

	/**
	 * @brief Static C-style callback function registered with the pigpio library.
	 * @param gpio GPIO pin number that triggered the event.
	 * @param level Pin state (0 = Low, 1 = High, 2 = Watchdog timeout).
	 * @param tick Event timestamp in microseconds.
	 * @param userdata Pointer to the PhysicalController instance (this).
	 */
	static void gpioCallbackEx(int gpio, int level, uint32_t tick, void* userdata);

	/**
	 * @brief Private constructor initializing pins and starting the worker thread.
	 * @param pint_input Input pin number.
	 * @param pint_out1 First output pin number.
	 * @param pint_out2 Second output pin number.
	 */
	PhysicalController(uint8_t pint_input, uint8_t pint_out0, uint8_t pint_out1, uint8_t pint_out2);
};

#endif /* PHYSICAL_CONTROLLER_H  */
