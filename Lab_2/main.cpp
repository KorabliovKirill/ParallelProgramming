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
volatile bool done = false;  // Флаг завершения
pthread_mutex_t mutex;       // Мьютекс
pthread_spinlock_t spinlock; // Спинлок

// Структура для пользовательской условной переменной
struct custom_cond_t {
  pthread_mutex_t cond_mutex; // Мьютекс для защиты состояния
  volatile bool can_proceed; // Флаг разрешения продолжения
};

// Инициализация пользовательской условной переменной
void custom_cond_init(custom_cond_t *cond) {
  int err = pthread_mutex_init(&cond->cond_mutex, NULL);
  if (err != 0)
    err_exit(err, "Cannot initialize cond mutex");
  cond->can_proceed = true; // Изначально разрешаем первому потоку начать
}

// Ожидание условия
void custom_cond_wait(custom_cond_t *cond) {
  int err = pthread_mutex_lock(&cond->cond_mutex);
  if (err != 0)
    err_exit(err, "Cannot lock cond mutex");

  while (!cond->can_proceed && !done) {
    pthread_mutex_unlock(&cond->cond_mutex);
    usleep(1); // Короткая задержка для снижения нагрузки
    pthread_mutex_lock(&cond->cond_mutex);
  }

  // Сбрасываем флаг для следующего потока
  if (!done) {
    cond->can_proceed = false;
  }

  err = pthread_mutex_unlock(&cond->cond_mutex);
  if (err != 0)
    err_exit(err, "Cannot unlock cond mutex");
}

// Сигнализация условия
void custom_cond_signal(custom_cond_t *cond) {
  int err = pthread_mutex_lock(&cond->cond_mutex);
  if (err != 0)
    err_exit(err, "Cannot lock cond mutex");

  cond->can_proceed = true; // Разрешаем следующему потоку продолжить

  err = pthread_mutex_unlock(&cond->cond_mutex);
  if (err != 0)
    err_exit(err, "Cannot unlock cond mutex");
}

// Уничтожение пользовательской условной переменной
void custom_cond_destroy(custom_cond_t *cond) {
  pthread_mutex_destroy(&cond->cond_mutex);
}

// Глобальные объекты для синхронизации
custom_cond_t cond;
pthread_mutex_t cond_access_mutex; // Мьютекс для защиты доступа к current_task

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
  int sync_type =
      *((int *)arg); // 0 - мьютекс, 1 - спинлок, 2 - условная переменная

  while (true) {
    usleep(rand() % 50); // Задержка для конкуренции
    if (sync_type == 0) {
      // Мьютекс
      err = pthread_mutex_lock(&mutex);
      if (err != 0)
        err_exit(err, "Cannot lock mutex");
    } else if (sync_type == 1) {
      // Спинлок
      err = pthread_spin_lock(&spinlock);
      if (err != 0)
        err_exit(err, "Cannot lock spinlock");
    } else {
      // Условная переменная: защищаем доступ к current_task
      err = pthread_mutex_lock(&cond_access_mutex);
      if (err != 0)
        err_exit(err, "Cannot lock cond access mutex");
      custom_cond_wait(&cond); // Ожидаем разрешения
    }

    // Критическая секция
    int temp = current_task; // Читаем значение
    usleep(rand() % 10); // Микрозадержка перед инкрементом
    current_task = temp + 1; // Неатомарная операция
    task_no = temp;

    if (sync_type == 0) {
      err = pthread_mutex_unlock(&mutex);
      if (err != 0)
        err_exit(err, "Cannot unlock mutex");
    } else if (sync_type == 1) {
      err = pthread_spin_unlock(&spinlock);
      if (err != 0)
        err_exit(err, "Cannot unlock spinlock");
    } else {
      custom_cond_signal(&cond); // Сигнализируем следующему потоку
      err = pthread_mutex_unlock(&cond_access_mutex);
      if (err != 0)
        err_exit(err, "Cannot unlock cond access mutex");
    }

    if (task_no < TASKS_COUNT) {
      do_task(task_no);
    } else {
      if (sync_type == 2) {
        // Устанавливаем флаг завершения и сигнализируем всем потокам
        done = true;
        custom_cond_signal(&cond);
      }
      return NULL;
    }
  }
}

int main(int argc, char *argv[]) {
  int threads_count = (argc > 1) ? atoi(argv[1]) : DEFAULT_THREADS_COUNT;

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

  // Инициализация пользовательской условной переменной
  custom_cond_init(&cond);
  err = pthread_mutex_init(&cond_access_mutex, NULL);
  if (err != 0)
    err_exit(err, "Cannot initialize cond access mutex");

  // 1. Тест с мьютексом
  int sync_type = 0;
  auto start = high_resolution_clock::now();

  for (int i = 0; i < threads_count; ++i) {
    err = pthread_create(&threads[i], NULL, thread_job, &sync_type);
    if (err != 0)
      err_exit(err, "Cannot create thread");
  }

  for (int i = 0; i < threads_count; ++i) {
    pthread_join(threads[i], NULL);
  }

  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start);
  cout << "Время выполнения с мьютексом: " << duration.count() << " мс" << endl
       << endl;

  // Сбрасываем счетчики и флаги
  for (int i = 0; i < TASKS_COUNT; i++) {
    task_execution_count[i] = 0;
  }
  current_task = 0;
  done = false;

  // 2. Тест со спинлоком
  sync_type = 1;
  start = high_resolution_clock::now();

  for (int i = 0; i < threads_count; ++i) {
    err = pthread_create(&threads[i], NULL, thread_job, &sync_type);
    if (err != 0)
      err_exit(err, "Cannot create thread");
  }

  for (int i = 0; i < threads_count; ++i) {
    pthread_join(threads[i], NULL);
  }

  end = high_resolution_clock::now();
  duration = duration_cast<milliseconds>(end - start);
  cout << "Время выполнения со спинлоком: " << duration.count() << " мс" << endl
       << endl;

  // Сбрасываем счетчики и флаги
  for (int i = 0; i < TASKS_COUNT; i++) {
    task_execution_count[i] = 0;
  }
  current_task = 0;
  done = false;

  // 3. Тест с пользовательской условной переменной
  sync_type = 2;
  start = high_resolution_clock::now();

  for (int i = 0; i < threads_count; ++i) {
    err = pthread_create(&threads[i], NULL, thread_job, &sync_type);
    if (err != 0)
      err_exit(err, "Cannot create thread");
  }

  for (int i = 0; i < threads_count; ++i) {
    pthread_join(threads[i], NULL);
  }

  end = high_resolution_clock::now();
  duration = duration_cast<milliseconds>(end - start);
  cout << "Время выполнения с условной переменной: " << duration.count()
       << " мс" << endl;

  cout << "\nСтатистика выполнения задач:" << endl;
  for (int i = 0; i < TASKS_COUNT; i++) {
    cout << "Задача " << i << " выполнена " << task_execution_count[i] << " раз"
         << endl;
  }

  pthread_mutex_destroy(&mutex);
  pthread_spin_destroy(&spinlock);
  custom_cond_destroy(&cond);
  pthread_mutex_destroy(&cond_access_mutex);
  delete[] threads;
  return 0;
}