#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <cstdlib>
#include <sstream>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"

int main(int argc, char *argv[]) {
    int sockfd, portno, n, i;
    struct sockaddr_in server_addr;
    struct hostent *server;
    char buffer[256];

    if (argc < 2) {
        std::cerr << "Использование: " << argv[0] << " <порт> <количество_итераций>" << std::endl;
        exit(1);
    }

    portno = atoi(argv[1]);
    int num_iterations = 10; 
    if (argc > 2) {
        num_iterations = atoi(argv[2]);
    }
    if (num_iterations <= 0) {
        std::cerr << "Количество итераций должно быть положительным числом." << std::endl;
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Ошибка открытия сокета");
        exit(1);
    }

    server = gethostbyname(SERVER_IP);
    if (server == NULL) {
        std::cerr << "Ошибка, нет такого хоста" << std::endl;
        exit(1);
    }

    server_addr = {};
    server_addr.sin_family = AF_INET;
    std::memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Ошибка подключения");
        exit(1);
    }

    for (i = 1; i <= num_iterations; i++) {
        std::memset(buffer, 0, sizeof(buffer)); 
        std::stringstream ss;
        ss << "Сообщение " << i << " от клиента";
        std::string message = ss.str();
        strncpy(buffer, message.c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = 0;
        n = write(sockfd, buffer, strlen(buffer));
        if (n < 0) {
            perror("Ошибка записи в сокет");
            exit(1);
        }

        std::cout << "Отправлено: " << buffer << std::endl;
        sleep(i);
    }

    close(sockfd);
    return 0;
}
