#include "can_manager.hpp"

#include "can_notify.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

CanManager::CanManager() : _notifies(signal_count)
{
}

CanManager::~CanManager()
{
    std::lock_guard<std::mutex> lock(_mutex);

    for (auto& notifies : _notifies)
    {
        for (auto* notify : notifies)
        {
            if (notify != nullptr && notify->_manager == this)
            {
                notify->_manager = nullptr;
            }
        }
    }
}

void CanManager::add_notify(CanNotify& notify)
{
    if (notify._manager != nullptr)
    {
        notify._manager->remove_notify(notify);
    }

    std::vector<std::pair<CanSignal, int>> current_values;

    {
        std::lock_guard<std::mutex> lock(_mutex);

        for (const auto signal : notify.signal_ids())
        {
            const auto signal_index = static_cast<std::size_t>(signal);
            _notifies[signal_index].push_back(&notify);
            current_values.emplace_back(signal, _signal_values[signal_index]);
        }

        notify._manager = this;
    }

    for (const auto& [signal, value] : current_values)
    {
        notify.notify(signal, value);
    }
}

void CanManager::remove_notify(CanNotify& notify)
{
    std::lock_guard<std::mutex> lock(_mutex);

    for (const auto signal : notify.signal_ids())
    {
        const auto signal_index = static_cast<std::size_t>(signal);
        auto& notifies          = _notifies[signal_index];
        notifies.erase(std::remove(notifies.begin(), notifies.end(), &notify), notifies.end());
    }

    if (notify._manager == this)
    {
        notify._manager = nullptr;
    }
}

void CanManager::update_signal_value(CanSignal signal, int value)
{
    const auto signal_index = static_cast<std::size_t>(signal);
    std::vector<CanNotify*> notifies;

    {
        std::lock_guard<std::mutex> lock(_mutex);
        _signal_values[signal_index] = value;
        notifies                     = _notifies[signal_index];
    }

    for (auto* notify : notifies)
    {
        notify->notify(signal, value);
    }
}
