#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

class HardwareManager {
public:
    HardwareManager() = delete;
    HardwareManager(const HardwareManager&) = delete;
    HardwareManager& operator=(const HardwareManager&) = delete;
    static void initialize();
    static void terminate();

private:
    static bool is_initialized;
};



#endif // HARDWARE_MANAGER_H
