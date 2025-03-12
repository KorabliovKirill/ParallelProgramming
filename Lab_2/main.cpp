#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <unistd.h>

using namespace std;
using namespace std::chrono;

#define err_exit(code, str)                                                    \
  {                                                                            \
    cerr << str << ": " << strerror(code) << endl;                             \
    exit(EXIT_FAILURE);                                                        \
  }

const int TASKS_COUNT = 10;
// Количество потоков можно изменить здесь или передать как аргумент командной
// строки
const int DEFAULT_THREADS_COUNT = 12;

int task_execution_count[TASKS_COUNT] = {0}; // Счетчик выполнений каждой задачи
volatile int current_task = 0; // Указатель на текущее задание
pthread_mutex_t mutex;         // Мьютекс
pthread_spinlock_t spinlock; // Спинлок

void do_task(int task_no) {
  pthread_t thread_id = pthread_self();
  cout << "Поток " << thread_id << " выполняет задачу " << task_no << endl;
  task_execution_count[task_no]++;

  double result = 0;
  for (int i = 0; i < 10000; i++) {
    result += sin(cos((double)i / 1000.0));
  }

  cout << "Поток " << thread_id << " завершил задачу " << task_no
       << " (результат: " << result << ")" << endl;
}

void *thread_job(void *arg) {
  int task_no;
  int err;
  int use_mutex =
      *((int *)arg); // 1 - использовать мьютекс, 0 - использовать спинлок

  while (true) {
    usleep(rand() % 50); // Задержка для конкуренции
    if (use_mutex) {
      // Мьютекс
      err = pthread_mutex_lock(&mutex);
      if (err != 0)
        err_exit(err, "Cannot lock mutex");
    } else {
      // Спинлок
      err = pthread_spin_lock(&spinlock);
      if (err != 0)
        err_exit(err, "Cannot lock spinlock");
    }

    int temp = current_task; // Читаем значение
    usleep(rand() % 10); // Микрозадержка перед инкрементом
    current_task = temp + 1; // Неатомарная операция
    task_no = temp;

    if (use_mutex) {
      err = pthread_mutex_unlock(&mutex);
      if (err != 0)
        err_exit(err, "Cannot unlock mutex");
    } else {
      err = pthread_spin_unlock(&spinlock);
      if (err != 0)
        err_exit(err, "Cannot unlock spinlock");
    }

    if (task_no < TASKS_COUNT) {
      do_task(task_no);
    } else {
      return NULL;
    }
  }
}

int main(int argc, char *argv[]) {
  int threads_count = (argc > 1) ? atoi(argv[1]) : DEFAULT_THREADS_COUNT;
  int use_mutex = 1; // По умолчанию используем мьютекс (0 - спинлок)

  pthread_t *threads = new pthread_t[threads_count];
  int err;

  // Инициализация мьютекса
  err = pthread_mutex_init(&mutex, NULL);
  if (err != 0)
    err_exit(err, "Cannot initialize mutex");

  // Инициализация спинлока
  err = pthread_spin_init(&spinlock, PTHREAD_PROCESS_PRIVATE);
  if (err != 0)
    err_exit(err, "Cannot initialize spinlock");

  // Замеряем время с мьютексом
  auto start = high_resolution_clock::now();

  for (int i = 0; i < threads_count; ++i) {
    err = pthread_create(&threads[i], NULL, thread_job, &use_mutex);
    if (err != 0)
      err_exit(err, "Cannot create thread");
  }

  for (int i = 0; i < threads_count; ++i) {
    pthread_join(threads[i], NULL);
  }

  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start);
  cout << "\nВремя выполнения с мьютексом: " << duration.count() << " мс"
       << endl;

  // Сбрасываем счетчики
  for (int i = 0; i < TASKS_COUNT; i++) {
    task_execution_count[i] = 0;
  }
  current_task = 0;

  // Переключаемся на спинлок
  use_mutex = 0;
  start = high_resolution_clock::now();

  for (int i = 0; i < threads_count; ++i) {
    err = pthread_create(&threads[i], NULL, thread_job, &use_mutex);
    if (err != 0)
      err_exit(err, "Cannot create thread");
  }

  for (int i = 0; i < threads_count; ++i) {
    pthread_join(threads[i], NULL);
  }

  end = high_resolution_clock::now();
  duration = duration_cast<milliseconds>(end - start);
  cout << "Время выполнения со спинлоком: " << duration.count() << " мс"
       << endl;

  cout << "\nСтатистика выполнения задач:" << endl;
  for (int i = 0; i < TASKS_COUNT; i++) {
    cout << "Задача " << i << " выполнена " << task_execution_count[i] << " раз"
         << endl;
  }

  pthread_mutex_destroy(&mutex);
  pthread_spin_destroy(&spinlock);
  delete[] threads;
  return 0;
}