#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <queue>
#include <vector>

using namespace std;

#define err_exit(code, str)                                                    \
  {                                                                            \
    cerr << str << ": " << strerror(code) << endl;                             \
    exit(EXIT_FAILURE);                                                        \
  }

struct Task {
  int start;
  int end;
  vector<bool> *sieve;
  const vector<int> *primes;
};

queue<Task> task_queue;
pthread_mutex_t queue_mutex;
pthread_cond_t queue_cond;
bool finished = false;
int MAX_NUM;
int THREAD_COUNT;

void do_task(Task task) {
  for (int prime : *(task.primes)) {
    if (prime * prime > task.end)
      break;
    int start = max(prime * prime, ((task.start + prime - 1) / prime) * prime);
    for (int j = start; j <= task.end; j += prime) {
      (*task.sieve)[j] = false;
    }
  }
}

void *thread_job(void *arg) {
  while (true) {
    Task task;
    bool has_task = false;

    pthread_mutex_lock(&queue_mutex);
    if (!task_queue.empty()) {
      task = task_queue.front();
      task_queue.pop();
      has_task = true;
    } else if (finished) {
      pthread_mutex_unlock(&queue_mutex);
      return NULL;
    }
    pthread_mutex_unlock(&queue_mutex);

    if (has_task) {
      do_task(task);
    } else {
      pthread_mutex_lock(&queue_mutex);
      pthread_cond_wait(&queue_cond, &queue_mutex);
      pthread_mutex_unlock(&queue_mutex);
    }
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    cerr << "Usage: " << argv[0] << " <max_number> <thread_count>" << endl;
    return EXIT_FAILURE;
  }

  MAX_NUM = atoi(argv[1]);
  THREAD_COUNT = atoi(argv[2]);

  if (MAX_NUM <= 1 || THREAD_COUNT <= 0) {
    cerr << "Invalid arguments" << endl;
    return EXIT_FAILURE;
  }

  auto start_time = chrono::high_resolution_clock::now();

  pthread_mutex_init(&queue_mutex, NULL);
  pthread_cond_init(&queue_cond, NULL);
  vector<bool> sieve(MAX_NUM + 1, true);
  sieve[0] = sieve[1] = false;

  int sqrt_n = sqrt(MAX_NUM);
  vector<int> small_primes;
  for (int i = 2; i <= sqrt_n; i++) {
    if (sieve[i]) {
      small_primes.push_back(i);
      for (int j = i * i; j <= sqrt_n; j += i) {
        sieve[j] = false;
      }
    }
  }

  vector<pthread_t> threads(THREAD_COUNT);
  for (int i = 0; i < THREAD_COUNT; i++) {
    int err = pthread_create(&threads[i], NULL, thread_job, NULL);
    if (err != 0)
      err_exit(err, "Cannot create thread");
  }

  int segment_size = MAX_NUM / THREAD_COUNT;
  if (segment_size < sqrt_n)
    segment_size = sqrt_n + 1;
  for (int i = 2; i <= MAX_NUM; i += segment_size) {
    Task task;
    task.start = i;
    task.end = min(i + segment_size - 1, MAX_NUM);
    task.sieve = &sieve;
    task.primes = &small_primes;

    pthread_mutex_lock(&queue_mutex);
    task_queue.push(task);
    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);
  }

  pthread_mutex_lock(&queue_mutex);
  finished = true;
  pthread_cond_broadcast(&queue_cond);
  pthread_mutex_unlock(&queue_mutex);

  for (int i = 0; i < THREAD_COUNT; i++) {
    pthread_join(threads[i], NULL);
  }

  int prime_count = 0;
  for (int i = 2; i <= MAX_NUM; i++) {
    if (sieve[i])
      prime_count++;
  }

  ofstream out_file("primes.txt");
  if (!out_file) {
    cerr << "Error: Cannot open file 'primes.txt'" << endl;
    return EXIT_FAILURE;
  }

  int count = 0;
  for (int i = 2; i <= MAX_NUM; i++) {
    if (sieve[i]) {
      out_file << i;
      count++;
      if (count % 5 == 0)
        out_file << "\n";
      else
        out_file << "\t";
    }
  }
  if (count % 5 != 0)
    out_file << endl;
  out_file.close();

  auto end_time = chrono::high_resolution_clock::now();
  auto duration =
      chrono::duration_cast<chrono::milliseconds>(end_time - start_time);

  cout << "Results for range up to " << MAX_NUM << ":\n";
  cout << "Total prime numbers found: " << prime_count << "\n";
  cout << "Execution time: " << duration.count() << " ms\n";
  cout << "Threads used: " << THREAD_COUNT << "\n";
  cout << "Prime numbers written to 'primes.txt'\n";

  pthread_mutex_destroy(&queue_mutex);
  pthread_cond_destroy(&queue_cond);
  return 0;
}