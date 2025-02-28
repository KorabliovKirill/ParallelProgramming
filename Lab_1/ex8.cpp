#include <iostream>
#include <pthread.h>
#include <chrono>
#include <vector>
#include <memory>
#include <unistd.h> // Для usleep
#include <mutex>

using namespace std;
using namespace chrono;

mutex cout_mutex;

struct ThreadParams
{
    float *first_number_ptr;
    unsigned int batch_size;
    float (*func_ptr)(float);
    unsigned int thread_index;
};

void *thread_job(void *arg)
{
    ThreadParams *params = static_cast<ThreadParams *>(arg);
    auto start = high_resolution_clock::now();

    for (unsigned int i = 0; i < params->batch_size; i++)
    {
        params->first_number_ptr[i] = params->func_ptr(params->first_number_ptr[i]);
        usleep(1000); // Симуляция вычислительной нагрузки
    }

    auto end = high_resolution_clock::now();

    lock_guard<mutex> lock(cout_mutex); // Защита cout от одновременного вывода
    cout << "Thread " << params->thread_index + 1 << " execution time: "
         << duration_cast<microseconds>(end - start).count() << " us" << endl;

    return nullptr;
}

float executable_function(float input_number)
{
    return input_number * input_number;
}

void initialize_array(unsigned int length, float *arr)
{
    for (unsigned int i = 0; i < length; i++)
    {
        arr[i] = static_cast<float>(i);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        cerr << "Usage: " << argv[0] << " <array_length> <threads_count>" << endl;
        return EXIT_FAILURE;
    }

    unsigned int array_length = atoi(argv[1]);
    unsigned int threads_count = atoi(argv[2]);
    threads_count = min(threads_count, array_length);
    unsigned int batch_size = array_length / threads_count;
    unsigned int rest = array_length % threads_count;

    vector<pthread_t> threads(threads_count);
    vector<ThreadParams> thread_params(threads_count);
    unique_ptr<float[]> number_arr(new float[array_length]);
    initialize_array(array_length, number_arr.get());

    auto start_total = high_resolution_clock::now();

    for (unsigned int i = 0; i < threads_count; ++i)
    {
        thread_params[i] = {number_arr.get() + i * batch_size, batch_size + (i == threads_count - 1 ? rest : 0), executable_function, i};
        pthread_create(&threads[i], nullptr, thread_job, &thread_params[i]);
    }

    for (auto &thread : threads)
    {
        pthread_join(thread, nullptr);
    }

    auto end_total = high_resolution_clock::now();
    cout << "Total execution time: " << duration_cast<microseconds>(end_total - start_total).count() << " us" << endl;

    for (unsigned int i = 0; i < array_length; i++)
    {
        cout << "arr[" << i << "] = " << number_arr[i] << endl;
    }

    return EXIT_SUCCESS;
}
