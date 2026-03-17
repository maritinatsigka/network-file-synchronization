#ifndef UTILS_H
#define UTILS_H
#include <pthread.h>

//A job that describes one file to copy from a source to a destination
typedef struct{
    char filename[256];
    char src_dir[256];
    char src_ip[64];
    int src_port;
    char dst_dir[256];
    char dst_ip[64];
    int dst_port;
} Job;

//A queue for storing jobs
typedef struct{
    Job *items;  //Array of jobs
    int capacity; //Maximum number of jobs
    int size;  //Current number of jobs in the queue
    int head;
    int tail;

    pthread_mutex_t mutex;  //Protects access to the queue
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} Queue;

void queue_init(Queue *q, int capacity);
void queue_push(Queue *q, const Job *job);
void queue_pop(Queue *q, Job *job);
void queue_destroy(Queue *q);

#endif