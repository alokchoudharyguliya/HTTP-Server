#include <future>
#include<thread>
#include <condition_variable>
#include<iostream>
#include<functional>
// using Task=std::function<void()>;
using Task=std::packaged_task<void()>;
class ThreadPool{
    public:
        ThreadPool(size_t numThreads);
        void submit(Task task);
        // template<typename F>
        // auto submit(F&&task);
        ~ThreadPool();
    private:
        void workerLoop();
        std::condition_variable cv;
        std::mutex queuedMutex_;
        std::vector<std::thread>workers_;
        // std::queue<std::packaged_task<void()>> tasks_; // will have to be protected as two threads might simultaneously access queue, one push and other pop
        std::queue<Task>tasks_;
        bool stop_;
};
// template<typename F>
// auto submit(F&&task);