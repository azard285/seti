#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>

// Константы
const std::string SERVER_ADDRESS = "pop.gmail.com"; // Замените на адрес вашего POP3 сервера
const int SERVER_PORT = 995; // POP3S (SSL) порт. Используйте 110 для обычного POP3.
const int BUFFER_SIZE = 4096;
const int MAX_RETRIES = 3; // Максимальное количество попыток переподключения

// Функция для подключения к серверу
int connect_to_server(const std::string& server_address, int server_port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
    std::cerr << "Invalid address/ Address not supported" << std::endl;
    close(sock);
    return -1;
}


    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    return sock;
}


// Функция для отправки команды на сервер
std::string send_command(int sock, const std::string& command) {
    std::string command_with_newline = command + "\r\n";
    if (send(sock, command_with_newline.c_str(), command_with_newline.length(), 0) < 0) {
        perror("send");
        return "";
    }
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0); // -1 чтобы было место для нуль-терминатора
    if (bytes_received < 0) {
        perror("recv");
        return "";
    }
    buffer[bytes_received] = '\0'; // Обеспечиваем нуль-терминацию
    return std::string(buffer);
}


// Base64 decoding function
std::string base64_decode(const std::string& encoded_string) {
    BIO *bio, *b64;
    char *buffer = nullptr;
    size_t length = encoded_string.length();
    std::string decoded_string;

    bio = BIO_new_mem_buf(encoded_string.c_str(), length);
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Do not use newlines to flush buffer

    int decoded_length = 0;
    char* decoded = new char[length];
    while ((decoded_length = BIO_read(bio, decoded, length)) > 0) {
        decoded_string.append(decoded, decoded_length);
    }

    BIO_free_all(bio);
    delete[] decoded;
    return decoded_string;
}



// Функция для сохранения вложения в файл
bool save_attachment(const std::string& filename, const std::string& content) {
    std::ofstream outfile(filename, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "Error opening file for attachment: " << filename << std::endl;
        return false;
    }
    outfile.write(content.c_str(), content.length());
    outfile.close();
    std::cout << "Attachment saved to: " << filename << std::endl;
    return true;
}


// Функция для парсинга email и извлечения тела и вложений
void parse_email(const std::string& email_content) {
    std::stringstream email_stream(email_content);
    std::string line;
    bool in_body = false;
    std::string body;
    std::string boundary;
    bool found_boundary = false;

    // Поиск boundary
    while (std::getline(email_stream, line)) {
        if (line.find("Content-Type:") != std::string::npos && line.find("multipart/mixed;") != std::string::npos) {
            size_t pos = line.find("boundary=");
            if (pos != std::string::npos) {
                boundary = line.substr(pos + 9); // 9 = strlen("boundary=")
                // Remove quotes, if any
                if (boundary.front() == '"' && boundary.back() == '"') {
                    boundary = boundary.substr(1, boundary.length() - 2);
                }
                found_boundary = true;
                std::cout << "Found boundary: " << boundary << std::endl;
                break;
            }
        }
    }

    if (!found_boundary) {
        std::cout << "Simple email without attachments." << std::endl;
         email_stream.seekg(0, std::ios::beg);
         in_body = true;
         while (std::getline(email_stream, line)) {
                if (in_body) {
                    body += line + "\n";
                }
                if (line.empty() || line == "\r") {
                    in_body = true;
                }
            }
             std::cout << "Email Body:\n" << body << std::endl;
        return;
    }


    std::string attachment_content;
    std::string attachment_filename;
    bool in_attachment = false;

    email_stream.seekg(0, std::ios::beg); // Reset stream to the beginning

    while (std::getline(email_stream, line)) {
        if (line.find(boundary) != std::string::npos) {
            in_attachment = true;
            attachment_filename = "";
            attachment_content = "";
        } else if (in_attachment) {
            if (line.find("Content-Disposition: attachment;") != std::string::npos) {
                size_t filename_pos = line.find("filename=");
                if (filename_pos != std::string::npos) {
                    attachment_filename = line.substr(filename_pos + 9); //9 = strlen("filename=")
                    // Remove quotes, if any
                    if (attachment_filename.front() == '"' && attachment_filename.back() == '"') {
                        attachment_filename = attachment_filename.substr(1, attachment_filename.length() - 2);
                    }
                    std::cout << "Found attachment filename: " << attachment_filename << std::endl;
                }
            } else if (line.find("Content-Transfer-Encoding: base64") != std::string::npos) {
                // Read attachment content
                std::string encoded_content;
                while (std::getline(email_stream, line)) {
                    if (line.find(boundary) != std::string::npos) {
                        break;
                    }
                    encoded_content += line;
                }

                attachment_content = base64_decode(encoded_content);
                if (!attachment_filename.empty()) {
                    save_attachment(attachment_filename, attachment_content);
                } else {
                    std::cout << "Attachment found, but filename is missing." << std::endl;
                }
                in_attachment = false;


            } else {
                 if (line.empty() || line == "\r") {
                    in_body = true;
                }
                if(in_body){
                    body+= line + "\n";
                }
            }
        }else {
             if (line.empty() || line == "\r") {
                    in_body = true;
                }
                if(in_body){
                    body+= line + "\n";
                }

        }
    }
     std::cout << "Email Body:\n" << body << std::endl;
}




int main() {
    std::string username, password;
    int sock = -1;
    int retries = 0;

    std::cout << "Enter username: ";
    std::cin >> username;
    std::cout << "Enter password: ";
    std::cin >> password;

    while (sock == -1 && retries < MAX_RETRIES) {
        sock = connect_to_server(SERVER_ADDRESS, SERVER_PORT);
        if (sock == -1) {
            std::cerr << "Failed to connect. Retrying in 5 seconds..." << std::endl;
            retries++;
            sleep(5); // Ждем 5 секунд перед повторной попыткой
        }
    }

    if (sock == -1) {
        std::cerr << "Failed to connect after " << MAX_RETRIES << " retries. Exiting." << std::endl;
        return 1;
    }


    std::string response = send_command(sock, "USER " + username);
    std::cout << "Server response: " << response << std::endl;
    if (response.substr(0, 3) != "+OK") {
        std::cerr << "Authentication failed (USER)." << std::endl;
        close(sock);
        return 1;
    }

    response = send_command(sock, "PASS " + password);
    std::cout << "Server response: " << response << std::endl;
    if (response.substr(0, 3) != "+OK") {
        std::cerr << "Authentication failed (PASS)." << std::endl;
        close(sock);
        return 1;
    }


    response = send_command(sock, "STAT");
    std::cout << "Server response: " << response << std::endl;

    // Получаем количество писем
    std::stringstream ss(response);
    std::string ok, num_messages_str, total_size_str;
    ss >> ok >> num_messages_str >> total_size_str;

    int num_messages = std::stoi(num_messages_str);

    std::cout << "You have " << num_messages << " messages." << std::endl;


    for (int i = 1; i <= num_messages; ++i) {
        std::cout << "Fetching message " << i << std::endl;
        response = send_command(sock, "RETR " + std::to_string(i));
        if (response.substr(0, 3) == "+OK") {
            // Extract the email content
            std::string email_content;
            std::string line;
            std::stringstream response_stream(response);

            // Skip the first line (+OK ...)
            std::getline(response_stream, line);

            // Read the rest of the email
            while (std::getline(std::cin, line)) {
                if (line == ".") {
                    break;
                }
                email_content += line + "\n";
            }

            parse_email(email_content);

            // Delete the message (optional)
            // send_command(sock, "DELE " + std::to_string(i));
        } else {
            std::cerr << "Error retrieving message " << i << std::endl;
        }
    }

    send_command(sock, "QUIT");
    close(sock);

    return 0;
}