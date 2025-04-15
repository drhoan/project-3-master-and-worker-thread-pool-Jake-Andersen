#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include <pthread.h>

int item_to_produce, curr_buf_size, item_to_consume;
int total_items, max_buf_size, num_workers, num_masters;

int *buffer;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;

void print_produced(int num, int master) {
    printf("Produced %d by master %d\n", num, master);
}

void print_consumed(int num, int worker) {
    printf("Consumed %d by worker %d\n", num, worker);
}

// Master producer thread function
void *generate_requests_loop(void *data) {
    int thread_id = *((int *)data);

    while (1) {
        pthread_mutex_lock(&mutex);

        // Stop if all items have been produced
        if (item_to_produce >= total_items) {
          pthread_mutex_unlock(&mutex);
          break;
        }

        // Wait if buffer is full
        while (curr_buf_size >= max_buf_size) {
          pthread_cond_wait(&empty, &mutex);
        }

        // Produce item
        buffer[curr_buf_size++] = item_to_produce;
        print_produced(item_to_produce, thread_id);
        item_to_produce++;

        pthread_cond_signal(&full); // Signal consumers
        pthread_mutex_unlock(&mutex);
    }
    return 0;
}

// Worker consumer thread function
void *consume_requests_loop(void *data) {
    int thread_id = *((int *)data);

    while (1) {
        pthread_mutex_lock(&mutex);

        // Stop if all items have been consumed
        if (item_to_consume >= total_items) {
          pthread_mutex_unlock(&mutex);
          break;
        }

        // Wait if buffer is empty
        while (curr_buf_size <= 0) {
          // If all items are produced and consumed, exit
          if (item_to_consume >= total_items) {
              pthread_mutex_unlock(&mutex);
              return 0;
          }
          pthread_cond_wait(&full, &mutex);
        }

        // Consume item
        int item = buffer[--curr_buf_size];
        print_consumed(item_to_consume, thread_id);
        item_to_consume++;

        pthread_cond_signal(&empty); // Signal producers
        pthread_mutex_unlock(&mutex);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    int *master_thread_id;
    pthread_t *master_thread;
    int *worker_thread_id;
    pthread_t *worker_thread;

    item_to_produce = 0;
    item_to_consume = 0;
    curr_buf_size = 0;

    if (argc < 5) {
        printf("./master-worker #total_items #max_buf_size #num_workers #masters e.g. ./exe 10000 1000 4 3\n");
        exit(1);
    } else {
        num_masters = atoi(argv[4]);
        num_workers = atoi(argv[3]);
        total_items = atoi(argv[1]);
        max_buf_size = atoi(argv[2]);
    }

    buffer = (int *)malloc(sizeof(int) * max_buf_size);

    // Create master producer threads
    master_thread_id = (int *)malloc(sizeof(int) * num_masters);
    master_thread = (pthread_t *)malloc(sizeof(pthread_t) * num_masters);
    for (int i = 0; i < num_masters; i++) {
        master_thread_id[i] = i;
        pthread_create(&master_thread[i], NULL, generate_requests_loop, (void *)&master_thread_id[i]);
    }

    // Create worker consumer threads
    worker_thread_id = (int *)malloc(sizeof(int) * num_workers);
    worker_thread = (pthread_t *)malloc(sizeof(pthread_t) * num_workers);
    for (int i = 0; i < num_workers; i++) {
        worker_thread_id[i] = i;
        pthread_create(&worker_thread[i], NULL, consume_requests_loop, (void *)&worker_thread_id[i]);
    }

    // Wait for all master threads to complete
    for (int i = 0; i < num_masters; i++) {
        pthread_join(master_thread[i], NULL);
        printf("master %d joined\n", i);
    }

    // After producers finish, signal all consumers in case they’re waiting
    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&full);
    pthread_mutex_unlock(&mutex);

    // Wait for all worker threads to complete
    for (int i = 0; i < num_workers; i++) {
        pthread_join(worker_thread[i], NULL);
        printf("worker %d joined\n", i);
    }

    // Clean up
    free(buffer);
    free(master_thread_id);
    free(master_thread);
    free(worker_thread_id);
    free(worker_thread);

    return 0;
}
