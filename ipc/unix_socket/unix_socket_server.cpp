#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main() {
    const char* socket_path = "/tmp/unix_socket_example";
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    unlink(socket_path); // Remove any previous socket

    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 1);

    std::cout << "Server waiting for connection..." << std::endl;
    int client_fd = accept(server_fd, nullptr, nullptr);

    char buf[100];
    ssize_t n = read(client_fd, buf, sizeof(buf));
    if (n > 0) {
        std::cout << "Received: " << buf << std::endl;
    }

    close(client_fd);
    close(server_fd);
    unlink(socket_path);
    return 0;
}
