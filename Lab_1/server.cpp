#include <iostream>    // Библиотека для работы с вводом-выводом
#include <memory>      // Умные указатели
#include <vector>      // Вектор (не используется, но может быть полезен)
#include <cstring>     // Строковые операции
#include <arpa/inet.h> // Библиотека для работы с интернет-протоколами
#include <unistd.h>    // Библиотека для POSIX системных вызовов
#include <pthread.h>   // POSIX потоки

#define PORT 8080 // Порт сервера
#define BACKLOG 5 // Очередь подключений

// Структура для передачи данных в поток
struct ClientData
{
    int client_fd;   // Дескриптор клиентского сокета
    int request_num; // Номер запроса
};

// Функция обработки клиентского запроса
void *handle_client(void *arg)
{
    // Используем умный указатель для автоматического освобождения памяти
    std::unique_ptr<ClientData> data(static_cast<ClientData *>(arg));

    FILE *pipe = popen("php -r 'echo phpversion();'", "r");
    char buffer[16];              // Буфер для версии PHP (достаточно для типичных версий вроде "8.2.15")
    fscanf(pipe, "%15s", buffer); // Ограничиваем длину для безопасности
    std::string php_version = buffer;
    pclose(pipe);

    // Формируем HTTP-ответ с номером запроса
    std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
                           "<!DOCTYPE html><html><head><title>Bye-bye baby bye-bye</title>"
                           "<body><h1>Goodbye, world!\n PHP:" +
                           php_version +
                           "</h1></body></html>\r\n" +
                           std::to_string(data->request_num) + " has been processed";

    // Отправляем ответ клиенту
    send(data->client_fd, response.c_str(), response.size(), 0);

    // Закрываем соединение с клиентом
    close(data->client_fd);
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
