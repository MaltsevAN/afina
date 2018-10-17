#ifndef AFINA_THREADPOOL_H
#define AFINA_THREADPOOL_H

#include <afina/logging/Service.h>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace Afina {

/**
 * # Thread pool
 */
class Executor;
void perform(Executor *executor);
class Executor {

public:
    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadppol is stopped
        kStopped
    };

    Executor(const std::string name, size_t low_watermark, size_t hight_watermark, size_t max_queue_size,
             int idle_time);
    ~Executor() = default;

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false);

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);
        {

        } {
            std::unique_lock<std::mutex> lock(this->_tasks_mutex);
            if (_state != State::kRun || _tasks.size() == _max_queue_size) {
                return false;
            }
            {
                if (!number_free_thread) {
                    {
                        std::unique_lock<std::mutex> threads_lock(_threads_mutex);
                        if (_threads.size() < _hight_watermark) {
                            _threads.emplace_back(std::thread(&perform, this));
                        }
                    }
                }
            }
            // Enqueue new task
            _tasks.push(exec);
        }
        _empty_condition.notify_one();
        return true;
    }

private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;

    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    friend void perform(Executor *executor);

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex _threads_mutex;

    std::mutex _tasks_mutex;
    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable _empty_condition;

    /**
     * Vector of actual threads that perorm execution
     */
    std::vector<std::thread> _threads;

    /**
     * Task queue
     */
    std::queue<std::function<void()>> _tasks;

    /**
     * Flag to stop bg threads
     */
    State _state;

    const size_t _low_watermark;
    const size_t _hight_watermark;
    const size_t _max_queue_size;
    const int _idle_time;
    std::atomic_size_t number_free_thread;
};

} // namespace Afina

#endif // AFINA_THREADPOOL_H
