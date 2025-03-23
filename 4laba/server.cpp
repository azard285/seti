#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>

using namespace std;

int main() {
    int server_fd, new_socket, port;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    char buffer[1024] = {0};
    vector<int> client_sockets;

    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(0); 

    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    
    addrlen = sizeof(address);
    if (getsockname(server_fd, (struct sockaddr *)&address, &addrlen) == -1) {
        perror("getsockname");
        exit(EXIT_FAILURE);
    }
    port = ntohs(address.sin_port);

    cout << "Server listening on port " << port << endl;

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    fd_set readfds;
    int max_sd = server_fd;

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);

        max_sd = server_fd;
        for (int sd : client_sockets) {
            FD_SET(sd, &readfds);
            if (sd > max_sd) {
                max_sd = sd;
            }
        }

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            perror("select error");
        }

        
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            cout << "New connection, socket fd is " << new_socket << ", ip is " << inet_ntoa(address.sin_addr) << ", port " << ntohs(address.sin_port) << endl;

            client_sockets.push_back(new_socket);
        }

        
        for (auto it = client_sockets.begin(); it != client_sockets.end(); ) {
            int sd = *it;
            if (FD_ISSET(sd, &readfds)) {
                int valread = read(sd, buffer, 1024);
                if (valread == 0) {
                    
                    getpeername(sd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
                    cout << "Host disconnected, ip " << inet_ntoa(address.sin_addr) << ", port " << ntohs(address.sin_port) << endl;

                    close(sd);
                    it = client_sockets.erase(it); 
                } else {
                    buffer[valread] = '\0'; 
                    cout << "Client " << sd << ": " << buffer << endl;
                    it++; 
                }
            } else {
                it++; 
            }
        }
    }

    close(server_fd);
    return 0;
}