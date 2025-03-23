// client.cpp
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: client <server_ip> <port>" << std::endl;
        return 1;
    }

    const char* server_ip = argv[1];
    int port = std::stoi(argv[2]);
    int num_iterations = 5; // Число итераций отправки данных
    int socket_desc;
    struct sockaddr_in server_addr;
    char buffer[1024];

    // Создаем сокет
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        std::cerr << "Error creating socket" << std::endl;
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        close(socket_desc);
        return 1;
    }


    // Подключаемся к серверу
    if (connect(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error connecting to server" << std::endl;
        close(socket_desc);
        return 1;
    }

    std::cout << "Connected to server " << server_ip << ":" << port << std::endl;

    // Отправляем данные в цикле
    for (int i = 1; i <= num_iterations; ++i) {
        std::string message = "Message " + std::to_string(i) + " from client.";
        strcpy(buffer, message.c_str());

        // Отправляем данные на сервер
        if (send(socket_desc, buffer, strlen(buffer), 0) < 0) {
            std::cerr << "Error sending data" << std::endl;
            break;
        }

        std::cout << "Sent: " << message << std::endl;

        // Задержка i секунд
        std::this_thread::sleep_for(std::chrono::seconds(i));
    }

    // Закрываем сокет
    close(socket_desc);

    return 0;
}