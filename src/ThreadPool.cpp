#include "ThreadPool.h"
#include "Logger.h"
#include <exception>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>

ThreadPool::ThreadPool(size_t numThreads) : stop_(false) {
  for (size_t i = 0; i < numThreads; ++i) {
    // workers_.emplace_back(
    //     [this](){
    //         workerLoop();
    //     }
    // );
    workers_.emplace_back(&ThreadPool::workerLoop, this);
  }
}
ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lock(queuedMutex_);
    stop_ = true;
  }
  cv.notify_all();
  for (auto &w : workers_)
    w.join(); // not const because join modifies the object
}
// template<typename F>
// void ThreadPool::submit(F&& f) {
void ThreadPool::submit(Task task) {
  {
    std::lock_guard<std::mutex> lock(queuedMutex_);
    if (stop_)
      // throw std::runtime_error("ThreadPool stopped");
      Logger::warning("Task submitted after thread pool stopped");
    tasks_.push(std::move(task));
  }
  cv.notify_one();
}
void ThreadPool::workerLoop() {
  Logger::debug("Worker thread started");
  // std::cout << "Worker started\n";
  while (true) {
    Task task;
    {
      std::unique_lock<std::mutex> lock(queuedMutex_);
      cv.wait(lock, [this]() { return stop_ || !tasks_.empty(); });
      if (stop_ && tasks_.empty()) {
        // std::cout << "Worker exiting\n";
        Logger::debug("Worker thread exiting");
        return;
      }
      task = std::move(tasks_.front());
      tasks_.pop();
    }
    try {
      task();
    } catch (const std::exception &e) {
      // std::cerr << e.what() << "\n";
      Logger::error(std::string("Task failed: ") + e.what());
    } catch (...) {
      Logger::error("Task failed: unknown exception");
      // std::cerr << "Unknown exception\n";
    }
  }
}