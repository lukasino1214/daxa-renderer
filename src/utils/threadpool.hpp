#pragma once

#include <atomic>            
#include <chrono>            
#include <condition_variable>
#include <exception>         
#include <functional>        
#include <future>            
#include <iostream>          
#include <memory>            
#include <mutex>             
#include <queue>             
#include <thread>            
#include <type_traits>       
#include <utility>           
#include <vector>   

using concurrency_t = std::invoke_result_t<decltype(std::thread::hardware_concurrency)>;

namespace dare {
    class ThreadPool {
    public:
        ThreadPool(const concurrency_t thread_count_ = 0) : thread_count(determine_thread_count(thread_count_)), threads(std::make_unique<std::thread[]>(determine_thread_count(thread_count_))) {
            create_threads();
        }

        ~ThreadPool() {
            wait_for_tasks();
            destroy_threads();
        }

        template <typename F, typename... A>
        void push_task(F&& task, A&&... args) {
            std::function<void()> task_function = std::bind(std::forward<F>(task), std::forward<A>(args)...);
            {
                const std::scoped_lock tasks_lock(this->tasks_mutex);
                this->tasks.push(task_function);
            }
            this->tasks_total++;
            this->task_available_cv.notify_one();
        }

        template <typename F, typename... A, typename R = std::invoke_result_t<std::decay_t<F>, std::decay_t<A>...>>
        auto submit(F&& task, A&&... args) -> std::future<R> {
            std::function<R()> task_function = std::bind(std::forward<F>(task), std::forward<A>(args)...);
            std::shared_ptr<std::promise<R>> task_promise = std::make_shared<std::promise<R>>();
            push_task([task_function, task_promise] {
                try {
                    if constexpr (std::is_void_v<R>) {
                        std::invoke(task_function);
                        task_promise->set_value();
                    } else {
                        task_promise->set_value(std::invoke(task_function));
                    }
                } catch (...) {
                    try {
                        task_promise->set_exception(std::current_exception());
                    } catch (...) {
                        }
                    }
                });
            return task_promise->get_future();
        }

        auto get_tasks_queued() const -> size_t {
            const std::scoped_lock tasks_lock(this->tasks_mutex);
            return this->tasks.size();
        }

        auto get_tasks_running() const -> size_t {
            const std::scoped_lock tasks_lock(this->tasks_mutex);
            return this->tasks_total - this->tasks.size();
        }

        auto get_tasks_total() const -> size_t {
            return this->tasks_total;
        }

        auto get_thread_count() const -> concurrency_t
        {
            return this->thread_count;
        }

        auto is_paused() const -> bool {
            return this->paused;
        }

        void pause() {
            this->paused = true;
        }

        void unpause() {
            this->paused = false;
        }

        void wait_for_tasks() {
            this->waiting = true;
            std::unique_lock<std::mutex> tasks_lock(this->tasks_mutex);
            this->task_done_cv.wait(tasks_lock, [this] { return (this->tasks_total == (this->paused ? this->tasks.size() : 0)); });
            this->waiting = false;
        }

        void reset(const concurrency_t thread_count_ = 0) {
            const bool was_paused = this->paused;
            this->paused = true;
            wait_for_tasks();
            destroy_threads();
            this->thread_count = determine_thread_count(thread_count_);
            this->threads = std::make_unique<std::thread[]>(thread_count);
            this->paused = was_paused;
            create_threads();
        }

    private:
        void create_threads() {
            this->running = true;
            for (concurrency_t i = 0; i < this->thread_count; i++) {
                this->threads[i] = std::thread(&ThreadPool::worker, this);
            }
        }

        void destroy_threads(){
            this->running = false;
            this->task_available_cv.notify_all();
            for (concurrency_t i = 0; i < this->thread_count; i++) {
                this->threads[i].join();
            }
        }

        auto determine_thread_count(const concurrency_t thread_count_) -> concurrency_t {
            if (thread_count_ > 0) {
                return thread_count_;
            } else {
                if(std::thread::hardware_concurrency() > 0) {
                    return std::thread::hardware_concurrency();
                } else {
                    return 1;
                }
            }
        }

        void worker() {
            while (this->running) {
                std::function<void()> task;
                std::unique_lock<std::mutex> tasks_lock(this->tasks_mutex);
                task_available_cv.wait(tasks_lock, [this] { return !this->tasks.empty() || !this->running; });
                if (this->running && !this->paused) {
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                    tasks_lock.unlock();
                    task();
                    tasks_lock.lock();
                    this->tasks_total--;
                    if (this->waiting) {
                        this->task_done_cv.notify_one();
                    }
                }
            }
        }

        std::atomic<bool> paused = false;
        std::atomic<bool> running = false;
        std::atomic<bool> waiting = false;

        std::condition_variable task_available_cv = {};
        std::condition_variable task_done_cv = {};

        std::queue<std::function<void()>> tasks = {};

        std::atomic<size_t> tasks_total = 0;

        mutable std::mutex tasks_mutex = {};

        concurrency_t thread_count = 0;

        std::unique_ptr<std::thread[]> threads = nullptr;
    };
}