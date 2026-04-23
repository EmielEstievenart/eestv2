#include "can_bus_data_provider.hpp"

#include "can_signals.hpp"

#include <chrono>
#include <random>

CanBusDataProvider::CanBusDataProvider(CanManager& can_manager) : _can_manager(can_manager)
{
}

CanBusDataProvider::~CanBusDataProvider()
{
    stop();
}

void CanBusDataProvider::start()
{
    if (_running.exchange(true))
    {
        return;
    }

    _worker = std::thread(&CanBusDataProvider::run, this);
}

void CanBusDataProvider::stop()
{
    if (!_running.exchange(false))
    {
        return;
    }

    if (_worker.joinable())
    {
        _worker.join();
    }
}

void CanBusDataProvider::run()
{
    std::mt19937 generator(std::random_device {}());
    std::uniform_int_distribution<int> signal_distribution(0, static_cast<int>(CanSignal::count) - 1);
    std::uniform_int_distribution<int> value_distribution(0, 1);

    while (_running)
    {
        const auto signal = static_cast<CanSignal>(signal_distribution(generator));
        const auto value  = value_distribution(generator);

        _can_manager.update_signal_value(signal, value);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
