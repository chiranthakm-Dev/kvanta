#pragma once

#include "thread_pool.h"
#include <string>
#include <functional>

class TcpServer {
public:
    TcpServer(int port, std::function<void(int)> handler);
    ~TcpServer();
    void start();
    void stop();

private:
    int server_fd_;
    int port_;
    std::function<void(int)> connection_handler_;
    ThreadPool thread_pool_;
    bool running_;
};
