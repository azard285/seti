#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <csignal>
#include <sstream>

#define PORT 0
#define BACKLOG 10

void handle_client(int sockfd);
void reaper(int sig);

int main() {
    int sockfd, new_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        std::cerr << "Ошибка создания сокета" << std::endl;
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    memset(&(server_addr.sin_zero), '\0', 8);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
        std::cerr << "Ошибка привязки сокета" << std::endl;
        exit(1);
    }

    socklen_t addrlen = sizeof(server_addr);
    if (getsockname(sockfd, (struct sockaddr *)&server_addr, &addrlen) == -1) {
        std::cerr << "Ошибка получения имени сокета" << std::endl;
        exit(1);
    }

    std::cout << "Сервер запущен на порту: " << ntohs(server_addr.sin_port) << std::endl;

    if (listen(sockfd, BACKLOG) == -1) {
        std::cerr << "Ошибка прослушивания сокета" << std::endl;
        exit(1);
    }

    signal(SIGCHLD, reaper); //когда пораждается поток поднимается сигнал. порождается когда нет дочерный процесс создается

    std::cout << "Ожидание соединений..." << std::endl;

    while (1) {
        sin_size = sizeof(struct sockaddr_in);
        if ((new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size)) == -1) {
            std::cerr << "Ошибка принятия соединения" << std::endl;
            continue;
        }

        inet_ntop(AF_INET, &(client_addr.sin_addr), s, sizeof(s));
        std::cout << "Получено соединение от: " << s << std::endl;

        if (!fork()) {
            close(sockfd);
            handle_client(new_fd);
            close(new_fd);
            exit(0);
        }
        close(new_fd);
    }

    return 0;
}

void handle_client(int sockfd) {
    int numbytes;
    char buf[256];

    while (1) {
        if ((numbytes = recv(sockfd, buf, sizeof(buf) - 1, 0)) == -1) {
            std::cerr << "Ошибка получения данных от клиента" << std::endl;
            break;
        }

        if (numbytes > 0) {
            buf[numbytes] = '\0';
            std::cout << "Получено от клиента: " << buf << std::endl;
        } else {
            std::cout << "Клиент закрыл соединение." << std::endl;
            break;
        }
    }
}

void reaper(int sig) {
    int status;
    while (wait3(&status, WNOHANG, (struct rusage *)0) >= 0);
}
