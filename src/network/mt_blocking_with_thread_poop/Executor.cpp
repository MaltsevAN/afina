//
// Created by alexmal on 17.10.18.
//

#include <algorithm>
#include <chrono>
#include <iostream>

#include <afina/Executor.h>
#include <afina/logging/Service.h>

namespace Afina {
void perform(Executor *executor) {
    for (;;) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(executor->_executor_mutex);
            executor->number_free_thread++;
            if (executor->_empty_condition.wait_for(lock, std::chrono::milliseconds(executor->_idle_time), [executor] {
                    return executor->_state != Executor::State::kRun || !executor->_tasks.empty();
                })) {
                executor->number_free_thread--;
                if (executor->_state != Executor::State::kRun && executor->_tasks.empty()) {
                    auto id = std::this_thread::get_id();
                    executor->del_thread(id);
                    return;
                }
                task = std::move(executor->_tasks.front());
                executor->_tasks.pop();
            } else {
                executor->number_free_thread--;
                if (executor->_threads.size() > executor->_low_watermark) {
                    auto id = std::this_thread::get_id();
                    executor->del_thread(id);
                    return;
                }
                continue;
            }
        }
        task();
    }
};

Executor::Executor(const std::string name, size_t low_watermark, size_t hight_watermark, size_t max_queue_size,
                   int idle_time)
    : _low_watermark(low_watermark), _hight_watermark(hight_watermark), _max_queue_size(max_queue_size),
      _idle_time(idle_time) {
    {
        std::unique_lock<std::mutex> lock(this->_executor_mutex);
        number_free_thread = 0;
        _state = State ::kRun;
        _threads.reserve(_hight_watermark);
        for (int i = 0; i < _low_watermark; i++) {
            _threads.emplace_back(std::thread(&perform, this));
        }
    }
}

void Executor::Stop(bool await) {
    {
        std::unique_lock<std::mutex> lock(_executor_mutex);
        _state = Executor::State::kStopping;
        _empty_condition.notify_all();
        _stop_condition.wait(lock, [this] { return _threads.empty(); });
        _state = Executor::State::kStopped;

    }



}

void Executor::del_thread(std::thread::id id) {
    auto iter = std::find_if(_threads.begin(), _threads.end(), [=](std::thread &t) { return (t.get_id() == id); });
    if (iter != _threads.end()) {
        iter->detach();
        _threads.erase(iter);
    }
    if (_threads.empty()) {
        _stop_condition.notify_one();
    }
}

} // namespace Afina
