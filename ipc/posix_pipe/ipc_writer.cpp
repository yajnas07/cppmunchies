#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {
    const char* fifo_path = "/tmp/myfifo";
    mkfifo(fifo_path, 0666); // Create FIFO

    int fd = open(fifo_path, O_WRONLY);
    const char* msg = "Hello from writer!";
    write(fd, msg, strlen(msg)+1);
    close(fd);

    std::cout << "Message sent!" << std::endl;
    return 0;
}