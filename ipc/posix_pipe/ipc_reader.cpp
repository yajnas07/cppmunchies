#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {
    const char* fifo_path = "/tmp/myfifo";
    mkfifo(fifo_path, 0666); // Create FIFO

    int fd = open(fifo_path, O_RDONLY);
    char buf[100];
    read(fd, buf, sizeof(buf));
    close(fd);

    std::cout << "Received: " << buf << std::endl;
    return 0;
}