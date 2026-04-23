#pragma once

#include "can_manager.hpp"

#include <atomic>
#include <thread>

class CanBusDataProvider
{
public:
    explicit CanBusDataProvider(CanManager& can_manager);
    ~CanBusDataProvider();

    CanBusDataProvider(const CanBusDataProvider&)            = delete;
    CanBusDataProvider& operator=(const CanBusDataProvider&) = delete;

    void start();
    void stop();

private:
    void run();

    CanManager& _can_manager;
    std::atomic<bool> _running {false};
    std::thread _worker;
};
