#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring> // memset
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/x509v3.h>
#include <cassert> // Добавлено для assert
#include <netdb.h> // Добавлено для getaddrinfo

using namespace std;

// Настройки POP3 сервера (измените на свои)
const string POP3_SERVER = "POP3_Server.gmail.com"; // Замените на свой сервер
const int POP3_PORT = 110;            // Обычно 110 (незащищенный) или 995 (SSL)
const string USERNAME = "pavlenkopasa382@gmail.com";   // Замените на свой email
const string PASSWORD = "vncx yujm hhwg jovx";   // Замените на свой пароль

// Функция для чтения ответа от сервера
string recv_response(SSL *ssl) {
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    int bytes_received = SSL_read(ssl, buffer, sizeof(buffer) - 1);
    if (bytes_received < 0) {
        cerr << "Ошибка при чтении ответа сервера: " << SSL_get_error(ssl, bytes_received) << endl;
        return "";
    }
    buffer[bytes_received] = '\0';
    return string(buffer);
}

// Функция для отправки команды серверу
bool send_command(SSL *ssl, const string& command) {
    string cmd = command + "\r\n";
    int bytes_sent = SSL_write(ssl, cmd.c_str(), cmd.length());
    if (bytes_sent < 0) {
        cerr << "Ошибка при отправке команды: " << SSL_get_error(ssl, bytes_sent) << endl;
        return false;
    }
    return true;
}

// Функция для сохранения вложения
bool save_attachment(const string& filename, const string& content) {
    ofstream file(filename, ios::binary);
    if (!file.is_open()) {
        cerr << "Ошибка при открытии файла для сохранения: " << filename << endl;
        return false;
    }
    file.write(content.c_str(), content.length());
    file.close();
    cout << "Вложение сохранено как: " << filename << endl;
    return true;
}

// Функция для декодирования base64 (требуется для вложений)
string base64_decode(const string& encoded_string) {
    BIO *bio, *b64;
    char *buffer = (char*)malloc(encoded_string.length());
    memset(buffer, 0, encoded_string.length());

    bio = BIO_new_mem_buf((char*)encoded_string.c_str(), -1);
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Не декодировать символы новой строки
    int length = BIO_read(bio, buffer, encoded_string.length());

    if (length != (int)encoded_string.length()) {
        cerr << "Ошибка при декодировании base64: неверная длина" << endl;
        free(buffer);
        BIO_free_all(bio);
        return ""; // Или выбросить исключение, в зависимости от логики обработки ошибок
    }
    buffer[length] = '\0';

    string decoded_string(buffer);
    BIO_free_all(bio);
    free(buffer);

    return decoded_string;
}

// Функция для получения письма и обработки вложений
bool get_email(SSL *ssl, int email_number) {
    // Команда RETR для получения письма
    if (!send_command(ssl, "RETR " + to_string(email_number))) return false;

    string response = recv_response(ssl);
    if (response.substr(0, 3) != "+OK") {
        cerr << "Ошибка при получении письма: " << response << endl;
        return false;
    }

    stringstream email_stream;
    string line;
    while (getline(cin, line)) { // Читаем письмо из stdin
        email_stream << line << "\n";
        if (line == ".") break; // Конец письма обозначается точкой
    }

    string email_content = email_stream.str();

    // Разбор email_content для обнаружения и сохранения вложений (очень упрощенно, предполагает Content-Type: multipart/mixed)
    size_t boundary_pos = email_content.find("boundary=");
    if (boundary_pos != string::npos) {
        string boundary = email_content.substr(boundary_pos + 10); // Предполагаем "boundary=" + 10 символов
        boundary = boundary.substr(0, boundary.find("\n") -1 ); // убираем лишнее и символ переноса строки
        boundary = boundary.substr(1, boundary.length() - 2); // Remove the double quotes from the boundary string

        size_t attachment_start = email_content.find("Content-Disposition: attachment");
        while (attachment_start != string::npos) {
            size_t filename_start = email_content.find("filename=", attachment_start);
            if (filename_start != string::npos) {
                string filename = email_content.substr(filename_start + 9); // Предполагаем "filename=" + 9 символов
                filename = filename.substr(1, filename.find(";") - 2 ); // убираем лишнее и символ переноса строки

                size_t content_start = email_content.find("Content-Transfer-Encoding: base64", filename_start);
                if (content_start != string::npos) {
                    content_start = email_content.find("\n", content_start) + 1;
                    content_start = email_content.find("\n", content_start) + 1; // Skip two newlines
                    size_t content_end = email_content.find("--" + boundary, content_start);
                    string encoded_content = email_content.substr(content_start, content_end - content_start);

                    string decoded_content = base64_decode(encoded_content);

                    if (!save_attachment(filename, decoded_content)) {
                        cerr << "Ошибка при сохранении вложения: " << filename << endl;
                    }
                }
            }
            attachment_start = email_content.find("Content-Disposition: attachment", attachment_start + 1);
        }
    } else {
        cout << "Письмо без вложений:" << endl;
        cout << email_content << endl;
    }

    return true;
}


int main() {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        cerr << "Не удалось создать SSL context" << endl;
        ERR_print_errors_fp(stderr);
        return 1;
    }

    // 1. Создание сокета
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        cerr << "Не удалось создать сокет" << endl;
        return 1;
    }

    // 2. Настройка адреса сервера
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(POP3_PORT);

    // Use getaddrinfo instead of gethostbyname
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // Use AF_INET for IPv4 or AF_UNSPEC for both IPv4 and IPv6
    hints.ai_socktype = SOCK_STREAM;

    int gai_result = getaddrinfo(POP3_SERVER.c_str(), to_string(POP3_PORT).c_str(), &hints, &res);
    if (gai_result != 0) {
        cerr << "Ошибка при разрешении имени хоста: " << gai_strerror(gai_result) << endl;
        close(sock);
        return 1;
    }

    // Loop through the results and connect to the first one that works
    for (p = res; p != NULL; p = p->ai_next) {
        // Copy the address information to server_address
        memcpy(&server_address, p->ai_addr, p->ai_addrlen);
        break; // Use the first address
    }

    if (p == NULL) {
        cerr << "Не удалось получить адрес сервера." << endl;
        freeaddrinfo(res);
        close(sock);
        return 1;
    }


    // 3. Подключение к серверу
    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        cerr << "Не удалось подключиться к серверу" << endl;
        close(sock);
        freeaddrinfo(res);
        return 1;
    }
    freeaddrinfo(res);


    // 4. SSL Handshake
    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        cerr << "Не удалось создать SSL структуру" << endl;
        ERR_print_errors_fp(stderr);
        close(sock);
        SSL_CTX_free(ctx);
        return 1;
    }
    SSL_set_fd(ssl, sock);

    if (SSL_connect(ssl) <= 0) {
        cerr << "Не удалось установить SSL соединение" << endl;
        ERR_print_errors_fp(stderr);
        close(sock);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        return 1;
    }


    //  Проверка сертификата (рекомендуется в реальных приложениях)
    //X509 *cert = SSL_get_peer_certificate(ssl);
    //if (cert) {
    //    X509_free(cert);
    //} else {
    //    cerr << "Сертификат не получен" << endl;
    //    // Обработка ошибки
    //}

    // 5. Аутентификация (POP3 требует сначала приветствие)
    string response = recv_response(ssl);
    cout << "Приветствие сервера: " << response << endl;

    if (!send_command(ssl, "USER " + USERNAME)) return 1;
    response = recv_response(ssl);
    cout << response << endl;
    if (response.substr(0, 3) != "+OK") return 1;

    if (!send_command(ssl, "PASS " + PASSWORD)) return 1;
    response = recv_response(ssl);
    cout << response << endl;
    if (response.substr(0, 3) != "+OK") return 1;


    // 6. Получение количества писем
    if (!send_command(ssl, "STAT")) return 1;
    response = recv_response(ssl);
    cout << response << endl;

    int num_emails = 0;
    stringstream ss(response.substr(4)); // Пропускаем "+OK "
    ss >> num_emails;

    if (num_emails > 0) {
        cout << "Всего писем: " << num_emails << endl;

        // 7. Получение первого письма (или другого, если нужно)
        int email_number = 1;
        if (!get_email(ssl, email_number)) {
            cerr << "Не удалось получить письмо " << email_number << endl;
        }

    } else {
        cout << "Нет новых писем." << endl;
    }


    // 8. Завершение сессии
    if (!send_command(ssl, "QUIT")) return 1;
    response = recv_response(ssl);
    cout << response << endl;

    // 9. Закрытие соединения
    SSL_shutdown(ssl);
    close(sock);
    SSL_free(ssl);
    SSL_CTX_free(ctx);

    return 0;
}