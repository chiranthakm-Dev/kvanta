#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <iostream>

int main(int argc, char** argv) {
    CLI::App app{"MemDB - Redis-inspired in-memory key-value store"};

    int port = 6399;
    app.add_option("--port", port, "Port to listen on");

    std::string max_memory = "256mb";
    app.add_option("--maxmemory", max_memory, "Maximum memory limit");

    std::string wal_dir = "/tmp/memdb-wal";
    app.add_option("--wal-dir", wal_dir, "WAL directory");

    CLI11_PARSE(app, argc, argv);

    spdlog::info("Starting MemDB on port {}", port);
    std::cout << "MemDB server starting..." << std::endl;

    // TODO: initialize stores, server, etc.

    return 0;
}
