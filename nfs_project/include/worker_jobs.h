#ifndef WORKER_JOBS_H
#define WORKER_JOBS_H

#include <signal.h>
#include <stdio.h>
#include <pthread.h>
#include "utils.h"

//Global flag to signal termination
extern volatile sig_atomic_t is_terminating;
//Used to coordinate shutdown across threads
extern pthread_mutex_t terminate_mutex;
extern pthread_cond_t terminate_cond;
//Shared log file for logging operations
extern FILE *log_file;

//Main loop function for each worker thread
void *sync_worker_loop(void *arg);

#endif