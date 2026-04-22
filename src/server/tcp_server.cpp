#include "tcp_server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <iostream>

TcpServer::TcpServer(int port, std::function<void(int)> handler)
    : port_(port), connection_handler_(handler), thread_pool_(4), running_(false) {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        throw std::runtime_error("Socket creation failed");
    }
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        throw std::runtime_error("Bind failed");
    }
    if (listen(server_fd_, 10) < 0) {
        throw std::runtime_error("Listen failed");
    }
}

TcpServer::~TcpServer() {
    stop();
    close(server_fd_);
}

void TcpServer::start() {
    running_ = true;
    std::thread([this] {
        while (running_) {
            sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &addr_len);
            if (client_fd >= 0) {
                thread_pool_.submit([this, client_fd] {
                    connection_handler_(client_fd);
                    close(client_fd);
                });
            }
        }
    }).detach();
}

void TcpServer::stop() {
    running_ = false;
    close(server_fd_);
}
