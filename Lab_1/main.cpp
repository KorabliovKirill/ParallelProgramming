#include <cstdlib>
#include <iostream>
#include <cstring>
#include <pthread.h>
#include <chrono>
#include <memory>
#include <vector>

using namespace std;
using namespace chrono;

/* Структура для передачи аргументов потоку */
struct ThreadArgs
{
    int thread_num;
    double param1;
    double param2;
    pthread_attr_t *attr;
};

/* Функция, которую будет исполнять созданный поток */
void *thread_job(void *arg)
{
    auto start = high_resolution_clock::now();

    ThreadArgs *args = static_cast<ThreadArgs *>(arg);
    int thread_num = args->thread_num;
    double param1 = args->param1;
    double param2 = args->param2;
    pthread_attr_t *attr = args->attr;

    cout << "Thread " << thread_num << " is running with param1: " << param1 << " and param2: " << param2 << endl;

    // Вывод информации о размере стека
    size_t stack_size;
    pthread_attr_getstacksize(attr, &stack_size);
    cout << "Thread " << thread_num << " stack size: " << stack_size << " bytes" << endl;

    // Вывод информации о политике планирования
    int sched_policy;
    pthread_attr_getschedpolicy(attr, &sched_policy);
    cout << "Thread " << thread_num << " scheduling policy: " << sched_policy << endl;

    // Вывод информации о приоритете планирования
    struct sched_param sched_param;
    pthread_attr_getschedparam(attr, &sched_param);
    cout << "Thread " << thread_num << " scheduling priority: " << sched_param.sched_priority << endl;

    // Вывод информации о наследуемости планирования
    int inheritsched;
    pthread_attr_getinheritsched(attr, &inheritsched);
    cout << "Thread " << thread_num << " inherits scheduling: " << inheritsched << endl;

    // Вывод информации о scope
    int scope;
    pthread_attr_getscope(attr, &scope);
    cout << "Thread " << thread_num << " scope: " << scope << endl;

    // Выполнение арифметической операции (например, умножение параметров)
    double result = param1 * param2;
    cout << "Thread " << thread_num << " result: " << result << endl;

    auto end = high_resolution_clock::now();
    auto inner_time = duration_cast<microseconds>(end - start).count();
    cout << "Inner Time: " << inner_time << " us" << endl;

    // Возвращаем указатель на inner_time
    long *time = new long(inner_time);
    return time;
}

int main(int argc, char *argv[])
{
    // Проверка наличия аргумента командной строки
    if (argc != 4)
    {
        cerr << "Usage: " << argv[0] << " <number_of_operations> <param1> <param2>" << endl;
        return EXIT_FAILURE;
    }

    int n = atoi(argv[1]); // Преобразование аргумента в число
    if (n <= 0)
    {
        cerr << "Invalid number of operations." << endl;
        return EXIT_FAILURE;
    }

    // Векторы для хранения потоков и их аргументов
    vector<unique_ptr<pthread_t[]>> threads;
    vector<unique_ptr<ThreadArgs[]>> thread_args;

    // Выделение памяти под n потоков и их аргументы
    threads.push_back(make_unique<pthread_t[]>(n));
    thread_args.push_back(make_unique<ThreadArgs[]>(n));

    // Создание потоков и замер времени
    for (int i = 0; i < n; ++i)
    {
        auto start = high_resolution_clock::now();

        // Инициализация атрибутов потока
        pthread_attr_t attr;
        pthread_attr_init(&attr);

        // Установка атрибутов (например, размер стека)
        size_t stack_size = (i + 1) * 1024 * 1024; // Разный стек для каждого потока
        pthread_attr_setstacksize(&attr, stack_size);

        // Инициализация параметров потока
        thread_args[0][i] = {i + 1, stod(argv[2]) + i, stod(argv[3]) + i, &attr}; // thread_num, param1, param2, attr

        if (pthread_create(&threads[0][i], &attr, thread_job, &thread_args[0][i]) != 0)
        {
            cerr << "Failed to create thread " << i + 1 << endl;
            return EXIT_FAILURE;
        }

        void *result;
        pthread_join(threads[0][i], &result);
        long inner_time = *static_cast<long *>(result);
        delete static_cast<long *>(result); // Освобождаем память

        auto end = high_resolution_clock::now();
        auto outer_time = duration_cast<microseconds>(end - start).count();
        cout << "Outer Time for thread " << i + 1 << ": " << outer_time << " us" << endl;
        cout << "Thread creation cost: " << outer_time - inner_time << " us" << endl;
        cout << "==================================================================" << endl;

        pthread_attr_destroy(&attr); // Освобождение атрибутов
    }

    return EXIT_SUCCESS;
}
