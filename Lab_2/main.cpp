#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <unistd.h>

using namespace std;

#define err_exit(code, str)                                                    \
  {                                                                            \
    cerr << str << ": " << strerror(code) << endl;                             \
    exit(EXIT_FAILURE);                                                        \
  }

const int TASKS_COUNT = 10;
const int THREADS_COUNT = 12;

int task_execution_count[TASKS_COUNT] = {0}; // Счетчик выполнений каждой задачи
volatile int current_task = 0; // Указатель на текущее задание
pthread_mutex_t mutex;         // Мьютекс

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
  while (true) {
    usleep(rand() % 50); // Задержка для конкуренции
    // Закомментированный мьютекс

    err = pthread_mutex_lock(&mutex);
    if (err != 0)
      err_exit(err, "Cannot lock mutex");

    int temp = current_task; // Читаем значение
    usleep(rand() % 10); // Микрозадержка перед инкрементом
    current_task = temp + 1; // Неатомарная операция
    task_no = temp;
    // Закомментированный мьютекс

    err = pthread_mutex_unlock(&mutex);
    if (err != 0)
      err_exit(err, "Cannot unlock mutex");

    if (task_no < TASKS_COUNT) {
      do_task(task_no);
    } else {
      return NULL;
    }
  }
}

int main() {
  pthread_t threads[THREADS_COUNT];
  int err;

  err = pthread_mutex_init(&mutex, NULL);
  if (err != 0)
    err_exit(err, "Cannot initialize mutex");

  for (int i = 0; i < THREADS_COUNT; ++i) {
    err = pthread_create(&threads[i], NULL, thread_job, NULL);
    if (err != 0)
      err_exit(err, "Cannot create thread");
  }

  for (int i = 0; i < THREADS_COUNT; ++i) {
    pthread_join(threads[i], NULL);
  }

  cout << "\nСтатистика выполнения задач:" << endl;
  for (int i = 0; i < TASKS_COUNT; i++) {
    cout << "Задача " << i << " выполнена " << task_execution_count[i] << " раз"
         << endl;
  }

  pthread_mutex_destroy(&mutex);
  return 0;
}