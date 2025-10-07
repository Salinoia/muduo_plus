#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <vector>

constexpr int MAX_CONN = 1000000;  // 目标连接数
constexpr int BATCH    = 1000;     // 每轮发起连接数量
constexpr int INTERVAL_US = 10000; // 每轮间隔（10ms）

int make_socket_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int create_connection(const char* ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;
    make_socket_nonblocking(sockfd);

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server.sin_addr);

    int ret = connect(sockfd, (sockaddr*)&server, sizeof(server));
    if (ret == 0 || (ret < 0 && errno == EINPROGRESS)) {
        return sockfd;
    } else {
        close(sockfd);
        return -1;
    }
}

int main() {
    const char* server_ip = "172.17.15.130";
    int port = 8000;

    std::vector<int> sockets;
    sockets.reserve(MAX_CONN);

    int success = 0, failed = 0;
    while (success < MAX_CONN) {
        for (int i = 0; i < BATCH && success < MAX_CONN; ++i) {
            int fd = create_connection(server_ip, port);
            if (fd >= 0) {
                sockets.push_back(fd);
                ++success;
            } else {
                ++failed;
            }
        }

        std::cout << "\rEstablished: " << success << " | Failed: " << failed << std::flush;
        usleep(INTERVAL_US);
    }

    std::cout << "\nAll connections established. Sleeping...\n";
    while (true) sleep(10);  // 保持连接不断开

    for (int fd : sockets) close(fd);
    return 0;
}
