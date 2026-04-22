#include <thread>
#include <atomic>
#include <chrono>

class ExpiryManager {
public:
    ExpiryManager() : running_(true) {
        thread_ = std::thread([this] { sweep_loop(); });
    }

    ~ExpiryManager() {
        running_ = false;
        if (thread_.joinable()) thread_.join();
    }

private:
    void sweep_loop() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            // TODO: sweep expired keys
        }
    }

    std::thread thread_;
    std::atomic<bool> running_;
};
