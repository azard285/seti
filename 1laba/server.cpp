#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Ошибка при создании сокета");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(0); 

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Ошибка привязки");
        close(sockfd);
        return 1;
    }

    getsockname(sockfd, (struct sockaddr*)&server_addr, &client_addr_len);
    int port = ntohs(server_addr.sin_port);
    std::cout << "Сервер запущен на порту: " << port << std::endl;

    while (true) {
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &client_addr_len);
        buffer[n] = '\0';

        std::cout << "Получено сообщение от клиента [" << inet_ntoa(client_addr.sin_addr) << ":" 
                  << ntohs(client_addr.sin_port) << "]: " << buffer << std::endl;

        std::string response = "Преобразованное сообщение: " + std::string(buffer);
        sendto(sockfd, response.c_str(), response.size(), 0, (struct sockaddr*)&client_addr, client_addr_len);
    }

    close(sockfd);
    return 0;
}
