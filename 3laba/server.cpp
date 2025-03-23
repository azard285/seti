#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_CLIENTS 10

std::ofstream data_file;
std::mutex data_mutex;
std::condition_variable data_cv;
bool data_ready = false;
std::string all_data;

struct thread_data {
    int client_socket;
    sockaddr_in client_address;
    int client_address_len;
};

void* handle_client(void* arg) {
    thread_data* data = (thread_data*)arg;
    int client_socket = data->client_socket;
    sockaddr_in client_address = data->client_address;
    int client_address_len = data->client_address_len;

    char buffer[1024];
    ssize_t bytes_received;

    while ((bytes_received = recv(client_socket, buffer, sizeof(buffer), 0)) > 0) {
        buffer[bytes_received] = '\0';

        {
            std::unique_lock<std::mutex> lock(data_mutex);
            all_data += "Received from " + std::string(inet_ntoa(client_address.sin_addr)) + ":" + std::to_string(ntohs(client_address.sin_port)) + ": " + buffer + "\n";
            data_ready = true;
        }
        data_cv.notify_one();

        std::cout << "Received from " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << ": " << buffer << std::endl;
    }

    if (bytes_received == 0) {
        std::cout << "Client " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << " disconnected." << std::endl;
    } else if (bytes_received < 0) {
        std::cerr << "Error receiving data from client " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << std::endl;
    }

    close(client_socket);
    delete data;
    pthread_exit(NULL);
}

void data_writer() {
    while (true) {
        std::unique_lock<std::mutex> lock(data_mutex);
        data_cv.wait(lock, [] { return data_ready; });
        data_file << all_data << std::endl;
        all_data.clear();
        data_ready = false;
        lock.unlock();
    }
}

int main() {
    int server_socket;
    struct sockaddr_in server_address;
    int port = 0;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        std::cerr << "Error creating socket" << std::endl;
        return 1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        std::cerr << "Error binding socket" << std::endl;
        close(server_socket);
        return 1;
    }

    sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(server_socket, (struct sockaddr *)&sin, &len) == -1) {
        std::cerr << "Error getting socket name" << std::endl;
        close(server_socket);
        return 1;
    }
    port = ntohs(sin.sin_port);

    if (listen(server_socket, MAX_CLIENTS) < 0) {
        std::cerr << "Error listening on socket" << std::endl;
        close(server_socket);
        return 1;
    }

    std::cout << "Server listening on port " << port << std::endl;

    data_file.open("server.dat", std::ios::app);
    if (!data_file.is_open()) {
        std::cerr << "Error opening data file" << std::endl;
        close(server_socket);
        return 1;
    }

    std::thread writer_thread(data_writer);

    while (true) {
        struct sockaddr_in client_address;
        int client_address_len = sizeof(client_address);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_address, (socklen_t*)&client_address_len);
        if (client_socket < 0) {
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }

        std::cout << "Accepted connection from " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << std::endl;

        thread_data* data = new thread_data;
        data->client_socket = client_socket;
        data->client_address = client_address;
        data->client_address_len = client_address_len;

        pthread_t thread_id;
        if(pthread_create(&thread_id, NULL, handle_client, (void*)data) < 0){
            std::cerr << "Could not create thread" << std::endl;
            close(client_socket);
            delete data;
            continue;
        }

        pthread_detach(thread_id);
    }

    close(server_socket);
    data_file.close();
    writer_thread.join();

    return 0;
}