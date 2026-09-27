#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace imdj {

template <typename Request, typename Result>
class WorkerQueue {
public:
    using Work = std::function<Result(const Request&)>;

    explicit WorkerQueue(Work work) : work_(std::move(work)), worker_([this]() { run(); }) {}

    ~WorkerQueue()
    {
        running_.store(false);
        wake_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    WorkerQueue(const WorkerQueue&) = delete;
    WorkerQueue& operator=(const WorkerQueue&) = delete;

    void submit(Request request)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(std::move(request));
        }
        wake_.notify_one();
    }

    std::vector<Result> takeCompleted()
    {
        std::vector<Result> completed;
        std::lock_guard<std::mutex> lock(mutex_);
        completed.swap(completed_);

        return completed;
    }

private:
    void run()
    {
        for (;;) {
            Request request;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait(lock, [this]() { return !queue_.empty() || !running_.load(); });
                if (!running_.load() && queue_.empty()) {
                    return;
                }

                request = std::move(queue_.front());
                queue_.pop_front();
            }

            Result result = work_(request);
            std::lock_guard<std::mutex> lock(mutex_);
            completed_.push_back(std::move(result));
        }
    }

    Work work_;
    std::atomic<bool> running_{true};

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Request> queue_;
    std::vector<Result> completed_;
    std::thread worker_;
};

} // namespace imdj
