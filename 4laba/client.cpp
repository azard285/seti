#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

int main(int argc, char *argv[]) {
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " server_ip server_port delay" << endl;
        return 1;
    }

    const char* server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int delay = atoi(argv[3]);

    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    string message;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    for (int i = 0; i < 10; ++i) {
        message = to_string(delay);
        send(sock, message.c_str(), message.length(), 0);
        cout << "Client (delay " << delay << ") sent: " << message << endl;
        this_thread::sleep_for(chrono::seconds(delay));
    }

    close(sock);
    return 0;
}