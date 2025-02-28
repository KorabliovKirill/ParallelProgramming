#include <iostream>    // Библиотека для работы с вводом-выводом
#include <memory>      // Умные указатели
#include <vector>      // Вектор (не используется, но может быть полезен)
#include <cstring>     // Строковые операции
#include <arpa/inet.h> // Библиотека для работы с интернет-протоколами
#include <unistd.h>    // Библиотека для POSIX системных вызовов
#include <stdexcept>
#include <array>
#include <pthread.h> // POSIX потоки

#define PORT 8080 // Порт сервера
#define BACKLOG 5 // Очередь подключений

// Структура для передачи данных в поток
struct ClientData
{
    int client_fd;   // Дескриптор клиентского сокета
    int request_num; // Номер запроса
};

void *handle_client(void *arg)
{
    // Умный указатель для автоматического управления памятью
    std::unique_ptr<ClientData> data(static_cast<ClientData *>(arg));

    try
    {
        // Буфер для получения данных от клиента
        std::array<char, 1024> request_buffer{};

        // Читаем запрос клиента (хотя бы первую часть для корректной работы с HTTP)
        ssize_t bytes_received = recv(data->client_fd, request_buffer.data(),
                                      request_buffer.size() - 1, 0);
        if (bytes_received < 0)
        {
            throw std::runtime_error("Failed to receive client request: " +
                                     std::string(strerror(errno)));
        }

        // Получаем версию PHP
        std::string php_version;
        {
            FILE *pipe = popen("php -r 'echo phpversion();'", "r");
            if (!pipe)
            {
                throw std::runtime_error("Failed to open pipe to PHP");
            }

            std::array<char, 16> php_buffer{};
            if (fgets(php_buffer.data(), php_buffer.size(), pipe) != nullptr)
            {
                php_version = php_buffer.data();
                php_version = php_version.substr(0, php_version.find('\n'));
            }
            pclose(pipe);
        }

        // Формируем HTTP-ответ
        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Connection: close\r\n"
            "Content-Length: ";

        std::string body =
            "<!DOCTYPE html>"
            "<html><head><title>Bye-bye baby bye-bye</title></head>"
            "<body><h1>Goodbye, world!\n PHP:" +
            php_version + "</h1>"
                          "<p>Request #" +
            std::to_string(data->request_num) +
            " has been processed</p>"
            "</body></html>\r\n";

        response += std::to_string(body.length()) + "\r\n\r\n" + body;

        // Отправляем ответ с проверкой ошибок
        ssize_t bytes_sent = send(data->client_fd, response.c_str(),
                                  response.length(), 0);
        if (bytes_sent < 0)
        {
            throw std::runtime_error("Failed to send response: " +
                                     std::string(strerror(errno)));
        }

        // Корректное завершение соединения
        shutdown(data->client_fd, SHUT_RDWR);
        close(data->client_fd);
    }
    catch (const std::exception &e)
    {
        close(data->client_fd);
    }

    return nullptr;
}

int main()
{
    int sock_fd;                           // Дескриптор серверного сокета
    struct sockaddr_in svr_addr, cli_addr; // Структуры для адресов сервера и клиента
    socklen_t sin_len = sizeof(cli_addr);
    int request_count = 0; // Счетчик запросов

    // Создание сокета
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket error");
        return 1;
    }

    // Устанавливаем опцию повторного использования адреса
    int opt = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Настройка адреса сервера
    svr_addr.sin_family = AF_INET;
    svr_addr.sin_addr.s_addr = INADDR_ANY;
    svr_addr.sin_port = htons(PORT);

    // Привязываем сокет к порту
    if (bind(sock_fd, (struct sockaddr *)&svr_addr, sizeof(svr_addr)) == -1)
    {
        perror("Bind error");
        close(sock_fd);
        return 1;
    }

    // Начинаем слушать входящие соединения
    listen(sock_fd, BACKLOG);
    std::cout << "Server listening on port " << PORT << std::endl;

    while (true)
    {
        // Принимаем соединение от клиента
        int client_fd = accept(sock_fd, (struct sockaddr *)&cli_addr, &sin_len);
        if (client_fd == -1)
        {
            perror("Accept error");
            continue;
        }

        request_count++; // Увеличиваем счетчик запросов

        // Создаем объект данных для передачи в поток
        ClientData *data = new ClientData{client_fd, request_count};
        pthread_t thread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);

        // Вывод информации о размере стека
        size_t stack_size = 2048 * 1024; // Стек
        pthread_attr_setstacksize(&attr, stack_size);
        // Создаем поток для обработки запроса
        if (pthread_create(&thread, &attr, handle_client, data) != 0)
        {
            perror("Thread creation error");
            delete data; // Удаляем объект данных в случае ошибки
        }
        else
        {
            pthread_detach(thread); // Поток будет автоматически освобожден после завершения
        }
    }

    // Закрываем серверный сокет (этот код никогда не выполнится из-за бесконечного цикла)
    close(sock_fd);
    return 0;
}
