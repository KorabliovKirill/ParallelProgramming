#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <unistd.h>
#include <vector>

using namespace std::chrono;

// Глобальный мьютекс для защиты вывода
std::mutex cout_mutex;

// Структура параметров для MapReduce
struct MapReduceParams {
  float *data;                        // Массив данных
  unsigned int data_size;             // Размер массива
  float (*map_func)(float);           // Функция map
  float (*reduce_func)(float, float); // Функция reduce
  unsigned int threads_count;         // Количество потоков
};

// Структура для этапа map
struct MapThreadParams {
  float *data;
  unsigned int batch_size;
  float (*map_func)(float);
  unsigned int thread_index;
};

// Функция для этапа map
void *map_thread_job(void *arg) {
  MapThreadParams *params = static_cast<MapThreadParams *>(arg);
  auto start = high_resolution_clock::now();

  for (unsigned int i = 0; i < params->batch_size; i++) {
    params->data[i] = params->map_func(params->data[i]);
    usleep(1000); // Симуляция вычислительной нагрузки
  }

  auto end = high_resolution_clock::now();
  std::lock_guard<std::mutex> lock(cout_mutex);
  std::cout << "Map Thread " << params->thread_index + 1 << " execution time: "
            << duration_cast<microseconds>(end - start).count() << " us"
            << std::endl;

  return nullptr;
}

// Структура для этапа reduce
struct ReduceThreadParams {
  std::vector<float> *mapped_data;
  unsigned int start_idx;
  unsigned int batch_size;
  float (*reduce_func)(float, float);
  float *partial_result;
  unsigned int thread_index;
};

// Функция для этапа reduce
void *reduce_thread_job(void *arg) {
  ReduceThreadParams *params = static_cast<ReduceThreadParams *>(arg);
  auto start = high_resolution_clock::now();

  float result = params->mapped_data->at(params->start_idx);
  for (unsigned int i = 1; i < params->batch_size; i++) {
    result = params->reduce_func(
        result, params->mapped_data->at(params->start_idx + i));
  }
  *params->partial_result = result;

  auto end = high_resolution_clock::now();
  std::lock_guard<std::mutex> lock(cout_mutex);
  std::cout << "Reduce Thread " << params->thread_index + 1
            << " execution time: "
            << duration_cast<microseconds>(end - start).count() << " us"
            << std::endl;

  return nullptr;
}

// Функция MapReduce
float map_reduce(MapReduceParams *params) {
  auto start_total = high_resolution_clock::now();

  // Этап 1: Map
  std::vector<pthread_t> map_threads(params->threads_count);
  std::vector<MapThreadParams> map_params(params->threads_count);
  unsigned int batch_size = params->data_size / params->threads_count;
  unsigned int rest = params->data_size % params->threads_count;

  for (unsigned int i = 0; i < params->threads_count; ++i) {
    unsigned int current_batch_size =
        batch_size + (i == params->threads_count - 1 ? rest : 0);
    map_params[i] = {params->data + i * batch_size, current_batch_size,
                     params->map_func, i};
    pthread_create(&map_threads[i], nullptr, map_thread_job, &map_params[i]);
  }

  for (auto &thread : map_threads) {
    pthread_join(thread, nullptr);
  }

  // Этап 2: Reduce
  std::vector<float> mapped_data(params->data,
                                 params->data + params->data_size);
  std::vector<pthread_t> reduce_threads(params->threads_count);
  std::vector<ReduceThreadParams> reduce_params(params->threads_count);
  std::vector<float> partial_results(params->threads_count, 0.0);

  batch_size = mapped_data.size() / params->threads_count;
  rest = mapped_data.size() % params->threads_count;

  for (unsigned int i = 0; i < params->threads_count; ++i) {
    unsigned int current_batch_size =
        batch_size + (i == params->threads_count - 1 ? rest : 0);
    reduce_params[i] = {&mapped_data,        i * batch_size,
                        current_batch_size,  params->reduce_func,
                        &partial_results[i], i};
    pthread_create(&reduce_threads[i], nullptr, reduce_thread_job,
                   &reduce_params[i]);
  }

  for (auto &thread : reduce_threads) {
    pthread_join(thread, nullptr);
  }

  // Финальное объединение результатов
  float final_result = partial_results[0];
  for (unsigned int i = 1; i < params->threads_count; i++) {
    final_result = params->reduce_func(final_result, partial_results[i]);
  }

  auto end_total = high_resolution_clock::now();
  std::lock_guard<std::mutex> lock(cout_mutex);
  std::cout << "Total MapReduce execution time: "
            << duration_cast<microseconds>(end_total - start_total).count()
            << " us" << std::endl;

  return final_result;
}

void initialize_array(unsigned int length, float *arr) {
  for (unsigned int i = 0; i < length; i++) {
    arr[i] = static_cast<float>(i);
  }
}

// Пример функций map и reduce
float map_func(float x) {
  return x * x; // Возведение в квадрат
}

float reduce_func(float a, float b) {
  return a + b; // Суммирование
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <array_length> <threads_count>"
              << std::endl;
    return EXIT_FAILURE;
  }

  unsigned int array_length = atoi(argv[1]);
  unsigned int threads_count = atoi(argv[2]);
  threads_count = std::min(threads_count, array_length);

  std::unique_ptr<float[]> data(new float[array_length]);
  initialize_array(array_length, data.get());

  MapReduceParams params = {data.get(), array_length, map_func, reduce_func,
                            threads_count};
  float result = map_reduce(&params);

  std::cout << "\nMapReduce result: " << result << std::endl;

  // Вывод массива после MapReduce (для проверки)
  std::cout << "Array after MapReduce:" << std::endl;
  for (unsigned int i = 0; i < array_length; i++) {
    std::cout << "arr[" << i << "] = " << data[i] << std::endl;
  }

  return EXIT_SUCCESS;
}