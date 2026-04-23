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
    std::lock_guard<std::recursive_mutex> lock(_mutex);

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

    _pending_initial_notifies.clear();
}

void CanManager::add_notify(CanNotify& notify)
{
    if (notify._manager != nullptr)
    {
        notify._manager->remove_notify(notify);
    }

    std::lock_guard<std::recursive_mutex> lock(_mutex);

    for (const auto signal : notify.signal_ids())
    {
        const auto signal_index = static_cast<std::size_t>(signal);
        _notifies[signal_index].push_back(&notify);
    }

    notify._manager = this;
    _pending_initial_notifies.push_back(&notify);
}

void CanManager::remove_notify(CanNotify& notify)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    for (const auto signal : notify.signal_ids())
    {
        const auto signal_index = static_cast<std::size_t>(signal);
        auto& notifies          = _notifies[signal_index];
        notifies.erase(std::remove(notifies.begin(), notifies.end(), &notify), notifies.end());
    }

    _pending_initial_notifies.erase(
        std::remove(_pending_initial_notifies.begin(), _pending_initial_notifies.end(), &notify), _pending_initial_notifies.end());

    if (notify._manager == this)
    {
        notify._manager = nullptr;
    }
}

void CanManager::housekeeping()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    const auto pending_initial_notifies = std::move(_pending_initial_notifies);
    _pending_initial_notifies.clear();

    for (auto* notify : pending_initial_notifies)
    {
        if (notify == nullptr || notify->_manager != this)
        {
            continue;
        }

        for (const auto signal : notify->signal_ids())
        {
            if (notify->_manager != this)
            {
                break;
            }

            const auto signal_index = static_cast<std::size_t>(signal);
            notify->notify(signal, _signal_values[signal_index]);
        }
    }
}

void CanManager::update_signal_value(CanSignal signal, int value)
{
    const auto signal_index = static_cast<std::size_t>(signal);
    std::vector<CanNotify*> notifies;

    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _signal_values[signal_index] = value;
    notifies                     = _notifies[signal_index];

    for (auto* notify : notifies)
    {
        notify->notify(signal, value);
    }
}
