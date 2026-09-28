#include "physical_controller.h"


PhysicalController::PhysicalController() {
    std::cout << "PhysicalController initialized." << std::endl;

}

PhysicalController::~PhysicalController() {
    std::cout << "PhysicalController destroyed." << std::endl;

}


void PhysicalController::open_door() {
    std::cout << "Opening the door..." << std::endl;

}


void PhysicalController::wait_for_request(std::function<void()> callback) {
    std::cout << "Waiting for a request..." << std::endl;
    if (callback) {
        callback();
    }
}


void PhysicalController::accept_request() {
    std::cout << "Accepting request..." << std::endl;

}


void PhysicalController::decline_request() {
    std::cout << "Declining request..." << std::endl;

}


void PhysicalController::end_call() {
    std::cout << "Ending call..." << std::endl;

}
