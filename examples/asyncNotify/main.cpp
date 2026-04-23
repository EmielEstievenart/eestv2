#include "can_bus_data_provider.hpp"
#include "can_manager.hpp"
#include "can_notify.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <magic_enum/magic_enum.hpp>
#include <thread>

namespace
{
std::atomic<bool> running {true};

void handle_signal(int)
{
    running = false;
}
}

int main()
{
    std::signal(SIGINT, handle_signal);

    CanManager can_manager;
    CanBusDataProvider data_provider(can_manager);
    CanNotify notify({CanSignal::engine_running, CanSignal::brake_pressed}, [](CanSignal signal, int value) { std::cout << "Signal " << magic_enum::enum_name(signal) << " updated to " << value << '\n'; });

    can_manager.add_notify(notify);
    data_provider.start();

    std::cout << "CAN data provider running. Press Ctrl+C to stop." << '\n';

    while (running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    data_provider.stop();

    return 0;
}
