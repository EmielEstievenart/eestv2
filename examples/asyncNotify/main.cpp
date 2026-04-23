#include "can_bus_data_provider.hpp"
#include "can_manager.hpp"
#include "can_notify.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <thread>
#include <vector>

namespace
{
std::atomic<bool> running {true};

void handle_signal(int)
{
    running = false;
}

class Container
{
public:
    Container(CanManager& can_manager, int value) :
        _can_manager(can_manager),
        _value(value),
        _notify(
            {CanSignal::engine_running},
            [this](CanSignal signal, int value)
            {
                _value += value;
                std::cout << "Container " << _value << " received " << magic_enum::enum_name(signal) << " = " << value << '\n';
            })
    {
        _can_manager.add_notify(_notify);
    }

private:
    CanManager& _can_manager;
    int _value;
    CanNotify _notify;
};
}

int main()
{
    std::signal(SIGINT, handle_signal);

    CanManager can_manager;
    CanBusDataProvider data_provider(can_manager);
    CanNotify notify({CanSignal::engine_running, CanSignal::brake_pressed}, [](CanSignal signal, int value) { std::cout << "Signal " << magic_enum::enum_name(signal) << " updated to " << value << '\n'; });
    std::atomic<bool> pause_dispatch {false};
    std::atomic<bool> dispatch_paused {false};
    CanNotify slow_engine_notify(
        {CanSignal::engine_running},
        [&](CanSignal, int)
        {
            if (pause_dispatch.exchange(false))
            {
                dispatch_paused = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });

    can_manager.add_notify(notify);
    can_manager.add_notify(slow_engine_notify);
    data_provider.start();

    std::cout << "CAN data provider running. Press Ctrl+C to stop." << '\n';

    while (running)
    {
        std::vector<std::unique_ptr<Container>> containers;
        for (int index = 0; index < 4; ++index)
        {
            containers.push_back(std::make_unique<Container>(can_manager, index));
        }

        dispatch_paused = false;
        pause_dispatch  = true;
        std::thread dispatch_thread([&can_manager] { can_manager.update_signal_value(CanSignal::engine_running, 1); });

        while (running && !dispatch_paused)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        containers.clear();
        dispatch_thread.join();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    data_provider.stop();

    return 0;
}
