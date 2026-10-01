/**
 * @file hardware_manager.h
 * @brief Static lifecycle manager for underlying hardware resources and libraries.
 * @details Provides static initialization and cleanup methods to manage global
 *          hardware peripherals (such as pigpio) across the application lifecycle.
 */

#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

/**
 * @class HardwareManager
 * @brief Utility class for hardware subsystem initialization and termination.
 * @details Pure static class that cannot be instantiated. Responsible for
 *          setting up and safely tearing down global hardware dependencies.
 */
class HardwareManager {
public:
    HardwareManager() = delete;
    HardwareManager(const HardwareManager&) = delete;
    HardwareManager& operator=(const HardwareManager&) = delete;

    /**
     * @brief Initializes global hardware resources and low-level libraries.
     */
    static void initialize();

    /**
     * @brief Terminates hardware resources and releases underlying library handles.
     */
    static void terminate();

private:
    static bool is_initialized;
};



#endif // HARDWARE_MANAGER_H
