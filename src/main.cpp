#include <bits/types/struct_iovec.h>
#include <liburing.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <coroutine>
#include <iostream>
#include <string>

#include "IoUring.hpp"
#include "Task.hpp"
#include "debug.hpp"

constexpr int PORT = 8080;

Task<int> print_int(IoUring& io_uring, int value) {
    constexpr int NUM = 10;
    for (int i = 0; i < NUM; i++) {
        std::string text = "Value: " + std::to_string(value + i) + "\n";
        co_await WriteAwaitable(io_uring, STDOUT_FILENO, text.data(), text.size(), 0);
    }
    co_return value;
}

Task<int> echo_client(IoUring& io_uring, int fd) {
    char buffer[4096];
    int index = 0;
    while (true) {
        auto readed = co_await ReadAwaitable(io_uring, fd, buffer, 4096, 0);
        println("Readded: " << readed);
        if (readed <= 0) {
            break;
        }
        println("Readded str: '" << std::string_view(buffer, readed) << "'");
        std::string header = std::to_string(++index) + ": ";

        std::array<iovec, 2> vs;
        vs[0].iov_base = header.data();
        vs[0].iov_len = header.size();
        vs[1].iov_base = buffer;
        vs[1].iov_len = readed;

        auto writted = co_await WritevAllAwaitable(io_uring, fd, vs.data(), vs.size(), 0);
        println("Writted: " << writted);
        if (writted <= 0) {
            break;
        }
    }
    int ret = co_await CloseAwaitable(io_uring, fd);
    if (ret < 0) {
        print_error("Closing fd " << fd << " " << ret);
    }
    println("Close connection " << fd);

    co_return 0;
}

Task<int> echo_server(IoUring& io_uring) {
    // 1. Create the socket (IPv4, TCP)
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    // 2. Enable SO_REUSEADDR
    int enable = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        close(listen_fd);
        throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");
    }

    // 3. Bind to address and port
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
    addr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(listen_fd);
        throw std::runtime_error("Failed to bind to port " + std::to_string(PORT));
    }

    // 4. Start listening
    if (listen(listen_fd, SOMAXCONN) < 0) {
        close(listen_fd);
        throw std::runtime_error("Failed to listen");
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    while (true) {
        int client_fd = co_await AcceptAwaitable(io_uring, listen_fd, nullptr, nullptr, 0);
        if (client_fd < 0) {
            println("Writted: " << client_fd);
            co_return client_fd;
        }

        echo_client(io_uring, client_fd);
    }
}

int main() {
    IoUring io_uring;

    auto task = echo_server(io_uring);

    while (!task.done()) {
        io_uring.progress_one();
    }

    return 0;
}