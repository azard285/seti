
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <iomanip>  

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <cstring> 
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>


const int SMTP_PORT = 465; 
const int BUFFER_SIZE = 4096;
const std::string SMTP_SERVER = "smtp.gmail.com"; 


class SMTPException : public std::runtime_error {
public:
    explicit SMTPException(const std::string& message) : std::runtime_error(message) {}
};


std::string read_response(SSL *ssl) {
    char buffer[BUFFER_SIZE] = {0};
    int bytes_received = SSL_read(ssl, buffer, BUFFER_SIZE - 1);
    if (bytes_received < 0) {
        throw SMTPException("Ошибка при чтении ответа от сервера: " + std::to_string(SSL_get_error(ssl, bytes_received)));
    }
    return std::string(buffer, bytes_received);
}


void send_command(SSL *ssl, const std::string& command) {
    std::cout << "Клиент: " << command; 
    if (SSL_write(ssl, command.c_str(), command.length()) <= 0) {
        throw SMTPException("Ошибка при отправке команды на сервер: " + std::to_string(SSL_get_error(ssl, 0)));
    }
}



void check_response_code(const std::string& response, int expected_code) {
    int code;
    std::stringstream ss(response);
    ss >> code;

    if (code != expected_code) {
        std::cerr << "Ошибка: " << response << std::endl;
        throw SMTPException("Неожиданный код ответа от сервера: " + std::to_string(code) + " Response: " + response);
    }
}


std::string base64_encode_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw SMTPException("Не удалось открыть файл: " + filename);
    }

    std::string file_content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());

    const std::string base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

    std::string encoded_string;
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    for (size_t pos = 0; pos < file_content.length(); pos++) {
        char_array_3[i++] = file_content[pos];
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++)
                encoded_string += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i)
    {
        for (int j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (int j = 0; (j < i + 1); j++)
            encoded_string += base64_chars[char_array_4[j]];

        while((i++ < 3))
            encoded_string += '=';

    }

    return encoded_string;
}

std::string base64_encode(const std::string& input) {
    const std::string base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

    std::string encoded_string;
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    for (size_t pos = 0; pos < input.length(); pos++) {
        char_array_3[i++] = input[pos];
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++)
                encoded_string += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i)
    {
        for (int j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (int j = 0; (j < i + 1); j++)
            encoded_string += base64_chars[char_array_4[j]];

        while((i++ < 3))
            encoded_string += '=';

    }

    return encoded_string;
}


int main() {
    std::string sender_email, recipient_email, subject, body, attachment_filename, app_password;

    
    std::cout << "Введите ваш email адрес (Gmail) (по умолчанию pavlenkopasa382@gmail.com): ";
    std::getline(std::cin, sender_email);

    if(sender_email.empty())
    {
        sender_email = "pavlenkopasa382@gmail.com";
    }

    std::cout << "Введите пароль приложения (Gmail) (не нужно если не вводили свою почту): ";
    std::getline(std::cin, app_password);
    if(app_password.empty())
    {
        app_password = "jwbvhuezjvvsawqh"; 
    }

    std::cout << "Введите email адрес получателя (по умолчанию iv221s21@gmail.com): ";
    std::getline(std::cin, recipient_email);
    if(recipient_email.empty())
    {
        recipient_email = "iv221s21@gmail.com";
    }
    
    std::cout << "Введите тему письма: ";
    std::cin.ignore(); 
    std::getline(std::cin, subject);

    std::cout << "Введите текст письма: ";
    std::getline(std::cin, body);

    std::cout << "Введите имя файла для вложения (или оставьте пустым, чтобы не вкладывать файл): ";
    std::getline(std::cin, attachment_filename);


    
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        throw SMTPException("Не удалось создать SSL context");
    }


    int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        std::cerr << "Не удалось создать сокет." << std::endl;
        return 1;
    }

    struct hostent *server = gethostbyname(SMTP_SERVER.c_str());
    if (server == nullptr) {
        std::cerr << "Не удалось разрешить имя хоста." << std::endl;
        close(socket_desc);
        SSL_CTX_free(ctx);
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SMTP_PORT);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);


    if (connect(socket_desc, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Не удалось подключиться к серверу." << std::endl;
        close(socket_desc);
        SSL_CTX_free(ctx);
        return 1;
    }

    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        ERR_print_errors_fp(stderr);
        close(socket_desc);
        SSL_CTX_free(ctx);
        return 1;
    }

    SSL_set_fd(ssl, socket_desc);

    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        close(socket_desc);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        return 1;
    }

    try {
        
        std::string response = read_response(ssl);
        std::cout << "Сервер: " << response << std::endl;
        check_response_code(response, 220);

        
        send_command(ssl, "EHLO example.com\r\n");
        response = read_response(ssl);
        std::cout << "Сервер: " << response << std::endl;
        check_response_code(response, 250);

        
        send_command(ssl, "AUTH LOGIN\r\n");
        response = read_response(ssl);
        std::cout << "Сервер: " << response << std::endl;
        check_response_code(response, 334);

        
        std::string encoded_username = base64_encode(sender_email);
        send_command(ssl, encoded_username + "\r\n");
        response = read_response(ssl);
        std::cout << "Сервер: " << response << std::endl;
        check_response_code(response, 334);

        
        std::string encoded_password = base64_encode(app_password);
        send_command(ssl, encoded_password + "\r\n");
        response = read_response(ssl);
        std::cout << "Сервер: " << response << std::endl;
        check_response_code(response, 235);

        
        send_command(ssl, "MAIL FROM:<" + sender_email + ">\r\n");
        response = read_response(ssl);
        std::cout << "Сервер: " << response << std::endl;
        check_response_code(response, 250);

        
        send_command(ssl, "RCPT TO:<" + recipient_email + ">\r\n");
        response = read_response(ssl);
        std::cout << "Сервер: " << response << std::endl;
        check_response_code(response, 250);

        
        send_command(ssl, "DATA\r\n");
        response = read_response(ssl);
        std::cout << "Сервер: " << response << std::endl;
        check_response_code(response, 354);

        
        std::string message;
        message += "Date: Mon, 15 Nov 2023 14:30:00 +0000\r\n"; 
        message += "From: <" + sender_email + ">\r\n";
        message += "To: <" + recipient_email + ">\r\n";
        message += "Subject: " + subject + "\r\n";
        message += "MIME-Version: 1.0\r\n";

        
        if (!attachment_filename.empty()) {
            std::string encoded_file = base64_encode_file(attachment_filename);

            
            std::string boundary = "----=_NextPart_001_" + std::to_string(time(0)); 
            message += "Content-Type: multipart/mixed; boundary=\"" + boundary + "\"\r\n\r\n";

            
            message += "--" + boundary + "\r\n";
            message += "Content-Type: text/plain; charset=UTF-8\r\n";
            message += "Content-Transfer-Encoding: 8bit\r\n\r\n";
            message += body + "\r\n\r\n";

            
            message += "--" + boundary + "\r\n";
            message += "Content-Type: application/octet-stream; name=\"" + attachment_filename + "\"\r\n";
            message += "Content-Transfer-Encoding: base64\r\n";
            message += "Content-Disposition: attachment; filename=\"" + attachment_filename + "\"\r\n\r\n";
            message += encoded_file + "\r\n\r\n";

            message += "--" + boundary + "--\r\n"; 
        } else {
            
            message += "Content-Type: text/plain; charset=UTF-8\r\n";
            message += "Content-Transfer-Encoding: 8bit\r\n\r\n";
            message += body + "\r\n";
        }

        
        send_command(ssl, message + "\r\n.\r\n"); 
        response = read_response(ssl);
        std::cout << "Сервер: " << response << std::endl;
        check_response_code(response, 250);

        
        send_command(ssl, "QUIT\r\n");
        response = read_response(ssl);
        std::cout << "Сервер: " << response << std::endl;
        check_response_code(response, 221);

    } catch (const SMTPException& e) {
        std::cerr << "Ошибка SMTP: " << e.what() << std::endl;
        ERR_print_errors_fp(stderr);
    } catch (const std::exception& e) {
        std::cerr << "Общая ошибка: " << e.what() << std::endl;
        ERR_print_errors_fp(stderr);
    }

    
    SSL_shutdown(ssl);
    close(socket_desc);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
    ERR_free_strings();

    return 0;
}