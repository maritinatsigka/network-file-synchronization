#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include "worker_jobs.h"
#include "utils.h"

volatile sig_atomic_t is_terminating = 0;
//Synchronization primitives for shutdown signaling
pthread_mutex_t terminate_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t terminate_cond = PTHREAD_COND_INITIALIZER;
extern FILE *log_file;
extern Queue job_queue;

//Connect to a remote server (source or target client)
static int connect_to(const char *ip, int port){
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0){
        return -1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0){
        close(s);
        return -1;
    }

    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0){
        close(s);
        return -1;
    }
    return s;
}

//Parse the response header and extract file size
static int get_file_info(int sock, long *out_size){
    char hdr[128];
    int r = read(sock, hdr, sizeof(hdr) - 1);
    if (r <= 0){
        return -1;

    }
    hdr[r] = 0;
    char *sp = strchr(hdr, ' ');
    if (!sp){
        return -1;
    }

    *sp = 0;
    *out_size = atol(hdr);
    return *out_size >= 0 ? 0 : -1;
}

//Build a descriptive string for logging purposes
static void make_path(char *out, size_t len, const Job *job, int is_src){
    snprintf(out, len, "%s/%s@%s:%d",
             is_src ? job->src_dir : job->dst_dir,
             job->filename,
             is_src ? job->src_ip : job->dst_ip,
             is_src ? job->src_port : job->dst_port);
}

//Log the outcome of a sync operation
static void log_result(const char *src, const char *dst, long tid, const char *op, const char *status, const char *msg){
    char ts[32];
    time_t now = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(log_file, "[%s] [%s] [%s] [%ld] [%s] [%s] [%s]\n", ts, src, dst, tid, op, status, msg);
    fflush(log_file);
}

//Perform PULL operation from source client
static int pull(const Job *job, int *fd_out, long *size_out){
    int sock = connect_to(job->src_ip, job->src_port);
    if (sock < 0){
        return -1;
    }

    dprintf(sock, "PULL %s/%s\n", job->src_dir, job->filename);

    if (get_file_info(sock, size_out) != 0){
        close(sock);
        return -1;
    }
    *fd_out = sock;
    return 0;
}

//Perform PUSH operation to target client
static int push(const Job *job, int fd, long size){
    int sock = connect_to(job->dst_ip, job->dst_port);
    if (sock < 0){
        return -1;
    }

    dprintf(sock, "PUSH %s/%s -1 start\n", job->dst_dir, job->filename);

    char buf[4096];
    long sent = 0;

    while (sent < size){
        ssize_t r = read(fd, buf, sizeof(buf));
        if (r <= 0){
            break;
        }
        dprintf(sock, "PUSH %s/%s %zd ", job->dst_dir, job->filename, r);
        write(sock, buf, r);
        sent += r;
    }

    dprintf(sock, "PUSH %s/%s 0 done\n", job->dst_dir, job->filename);
    close(sock);
    return sent == size ? 0 : -1;
}

//Process a single sync job
static void process_job(const Job *job, long tid){
    int src_fd;
    long fsize;

    char src_str[1024];
    char dst_str[1024];
    make_path(src_str, sizeof(src_str), job, 1);
    make_path(dst_str, sizeof(dst_str), job, 0);

    //Pull from source
    if (pull(job, &src_fd, &fsize) != 0){
        log_result(src_str, dst_str, tid, "PULL", "FAIL", "pull error");
        return;
    }
    log_result(src_str, dst_str, tid, "PULL", "OK", "done");

    //Push to target
    if (push(job, src_fd, fsize) != 0){
        log_result(src_str, dst_str, tid, "PUSH", "FAIL", "push error");
    }
    else{
        log_result(src_str, dst_str, tid, "PUSH", "OK", "done");
    }
    close(src_fd);
}

//Main loop for each worker thread
void *sync_worker_loop(void *arg){
    long tid = (long)arg;

    while (1){
        Job job;
        //Wait for job availability
        pthread_mutex_lock(&job_queue.mutex);
        while (job_queue.size == 0 && !is_terminating){
            pthread_cond_wait(&job_queue.not_empty, &job_queue.mutex);
        }
        //If shutting down and queue is empty, exit loop
        if (is_terminating && job_queue.size == 0){
            pthread_mutex_unlock(&job_queue.mutex);
            break;
        }

        //Fetch job from queue
        memcpy(&job, &job_queue.items[job_queue.head], sizeof(Job));
        job_queue.head = (job_queue.head + 1) % job_queue.capacity;
        job_queue.size--;

        pthread_cond_signal(&job_queue.not_full);
        pthread_mutex_unlock(&job_queue.mutex);

        process_job(&job, tid);
    }

    return NULL;
}