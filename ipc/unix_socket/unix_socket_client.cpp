#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

int main() {
    const char* socket_path = "/tmp/unix_socket_example";
    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    connect(sock_fd, (sockaddr*)&addr, sizeof(addr));

    const char* msg = "Hello from client!";
    write(sock_fd, msg, strlen(msg)+1);

    close(sock_fd);
    return 0;
}
