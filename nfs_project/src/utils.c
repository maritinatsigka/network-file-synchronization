#include "../include/utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//Check if the queue is full
static int is_full(Queue *q){
    return q->size == q->capacity;
}

//Check if the queue is empty
static int is_empty(Queue *q){
    return q->size == 0;
}

void queue_init(Queue *queue, int capacity){
    queue->items = malloc(sizeof(Job) * capacity);
    if (!queue->items){
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    queue->capacity = capacity;
    queue->size = 0;
    queue->head = 0;
    queue->tail = 0;

    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
}

//Add a job to the queue
void queue_push(Queue *queue, const Job *job){
    pthread_mutex_lock(&queue->mutex);

    while (is_full(queue)){
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }

    memcpy(&queue->items[queue->tail], job, sizeof(Job));
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->size++;

    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
}

//Remove a job from the queue
void queue_pop(Queue *queue, Job *out_job){
    pthread_mutex_lock(&queue->mutex);

    while (is_empty(queue)){
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    memcpy(out_job, &queue->items[queue->head], sizeof(Job));
    queue->head = (queue->head + 1) % queue->capacity;
    queue->size--;

    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
}

void queue_destroy(Queue *queue){
    if (queue->items){
        free(queue->items);
    }

    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
}
