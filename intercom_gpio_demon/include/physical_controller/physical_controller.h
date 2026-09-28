#ifndef PHYSICAL_CONTROLLER_H
#define PHYSICAL_CONTROLLER_H

#include <functional>
#include <iostream>

class PhysicalController
{
public:
    PhysicalController();
    ~PhysicalController();

    void open_door();
    void wait_for_request(std::function<void()> callback = nullptr);
    void accept_request();
    void decline_request();
    void end_call();

private:
};

#endif // PHYSICAL_CONTROLLER_H
