#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include "worker_jobs.h"
#include "manager_core.h"
#include "utils.h"

//Global linked list for active sync mappings
SyncMapping *mapping_list_head = NULL;
//Mutex to protect access to the mapping list
pthread_mutex_t mapping_list_mutex = PTHREAD_MUTEX_INITIALIZER;
extern Queue job_queue;
extern volatile sig_atomic_t is_terminating;

//Enum representing supported commands
typedef enum{
    CMD_UNKNOWN,
    CMD_ADD,
    CMD_CANCEL,
    CMD_SHUTDOWN
} CommandType;

typedef struct{
    CommandType type;
    char arg1[256];
    char arg2[256];
} Command;

//Generates current timestamp string
static void current_timestamp(char *buf, size_t len){
    time_t now = time(NULL);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", localtime(&now));
}

void show_usage(const char *program_name){
    fprintf(stderr, "Usage: %s -l <logfile> -c <config> -n <workers> -p <port> -b <buffer>\n", program_name);
    exit(EXIT_FAILURE);
}

//Parses raw input line into a Command structure
static Command parse_command(const char *line){
    Command cmd = {CMD_UNKNOWN, "", ""};
    char word[16];
    sscanf(line, "%15s", word);

    if (strcmp(word, "add") == 0){
        cmd.type = CMD_ADD;
        sscanf(line + 4, "%255s %255s", cmd.arg1, cmd.arg2);
    } else if (strcmp(word, "cancel") == 0){
        cmd.type = CMD_CANCEL;
        sscanf(line + 7, "%255s", cmd.arg1);
    } else if (strcmp(word, "shutdown") == 0){
        cmd.type = CMD_SHUTDOWN;
    }
    return cmd;
}

//Handles 'add' command: creates and registers a new sync mapping
static void respond_add(int client_fd, const char *src, const char *dst){
    SyncMapping *entry = calloc(1, sizeof(SyncMapping));
    sscanf(src, "%[^@]@%[^:]:%d", entry->src_path, entry->src_host, &entry->src_port);
    sscanf(dst, "%[^@]@%[^:]:%d", entry->dst_path, entry->dst_host, &entry->dst_port);

    pthread_mutex_lock(&mapping_list_mutex);

    //Check for duplicate entry
    SyncMapping *cur = mapping_list_head;
    while (cur){
        if (strcmp(cur->src_path, entry->src_path) == 0 &&
            strcmp(cur->src_host, entry->src_host) == 0 &&
            cur->src_port == entry->src_port &&
            strcmp(cur->dst_path, entry->dst_path) == 0 &&
            strcmp(cur->dst_host, entry->dst_host) == 0 &&
            cur->dst_port == entry->dst_port){
            
            pthread_mutex_unlock(&mapping_list_mutex);
            free(entry);

            char ts[32];
            current_timestamp(ts, sizeof(ts));
            dprintf(client_fd, "[%s] Sync task already exists: %s => %s\n", ts, src, dst);
            return;
        }
        cur = cur->next;
    }

    //Add to the head of the list
    entry->next = mapping_list_head;
    mapping_list_head = entry;
    pthread_mutex_unlock(&mapping_list_mutex);
    //Start background sync thread for this mapping
    pthread_t tid;
    pthread_create(&tid, NULL, init_sync_request, entry);
    pthread_detach(tid);

    char ts[32];
    current_timestamp(ts, sizeof(ts));
    dprintf(client_fd, "[%s] Sync task registered: %s => %s\n", ts, src, dst);
}

//Handles 'cancel' command
static void respond_cancel(int client_fd, const char *src_spec){
    int removed = 0;
    pthread_mutex_lock(&mapping_list_mutex);
    SyncMapping **cur = &mapping_list_head;
    while (*cur){
        char full[512];
        snprintf(full, sizeof(full), "%s@%s:%d", (*cur)->src_path, (*cur)->src_host, (*cur)->src_port);
        if (strcmp(full, src_spec) == 0){
            SyncMapping *tmp = *cur;
            *cur = tmp->next;
            free(tmp);
            removed = 1;
            break;
        }
        cur = &(*cur)->next;
    }
    pthread_mutex_unlock(&mapping_list_mutex);

    time_t now = time(NULL);
    if (removed){
        dprintf(client_fd, "[%ld] Sync cancelled for %s\n", now, src_spec);
    } else{
        dprintf(client_fd, "[%ld] Sync was already inactive or not found: %s\n", now, src_spec);
    }
}

//Handles 'shutdown' command
static void respond_shutdown(int client_fd, int server_fd){
    char ts[32];
    current_timestamp(ts, sizeof(ts));

    dprintf(client_fd, "[%s] Received shutdown request.\n", ts);
    dprintf(client_fd, "[%s] Queued jobs will be handled before exit.\n", ts);
    dprintf(client_fd, "[%s] Workers are finishing up. No new jobs will be accepted.\n", ts);
    dprintf(client_fd, "[%s] Shutdown will complete shortly. Closing control channel.\n", ts);

    is_terminating = 1;
    pthread_cond_broadcast(&job_queue.not_empty);

    close(client_fd);
    close(server_fd);
    pthread_exit(NULL);
}

//Determines and executes the correct handler for the client's command
static void handle_client_command(int client_fd, const char *buffer, int server_fd){
    Command cmd = parse_command(buffer);
    switch (cmd.type) {
        case CMD_ADD:
            respond_add(client_fd, cmd.arg1, cmd.arg2);
            break;
        case CMD_CANCEL:
            respond_cancel(client_fd, cmd.arg1);
            break;
        case CMD_SHUTDOWN:
            respond_shutdown(client_fd, server_fd);
            break;
        default:
            dprintf(client_fd, "Unknown command: %s\n", buffer);
    }
}

//Thread that listens for incoming manager commands on a specific port
void *monitor_console_input(void *arg){
    int port = *(int *)arg;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = {0};

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    listen(sockfd, 8);

    while (1){
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client = accept(sockfd, (struct sockaddr *)&client_addr, &len);
        if (client < 0){
            continue;
        }

        char buf[1024];
        int bytes = read(client, buf, sizeof(buf) - 1);
        if (bytes > 0){
            buf[bytes] = '\0';
            buf[strcspn(buf, "\r\n")] = 0;
            handle_client_command(client, buf, sockfd);
        }
        close(client);
    }
    return NULL;
}

//Starts synchronization for a given SyncMapping by issuing LIST and queuing jobs
void *init_sync_request(void *arg) {
    SyncMapping *entry = (SyncMapping *)arg;
    if (!entry) return NULL;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in src = {0};
    src.sin_family = AF_INET;
    src.sin_port = htons(entry->src_port);
    inet_pton(AF_INET, entry->src_host, &src.sin_addr);

    if (connect(sock, (struct sockaddr *)&src, sizeof(src)) < 0){
        close(sock);
        return NULL;
    }

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "LIST %s\n", entry->src_path);
    send(sock, cmd, strlen(cmd), 0);

    FILE *fp = fdopen(sock, "r");
    char line[4096];
    while (fgets(line, sizeof(line), fp)){
        line[strcspn(line, "\r\n")] = 0;
        if (strcmp(line, ".") == 0){
            break;
        }

        Job job = {0};
        strncpy(job.filename, line, sizeof(job.filename) - 1);
        strncpy(job.src_dir, entry->src_path, sizeof(job.src_dir) - 1);
        strncpy(job.src_ip, entry->src_host, sizeof(job.src_ip) - 1);
        job.src_port = entry->src_port;
        strncpy(job.dst_dir, entry->dst_path, sizeof(job.dst_dir) - 1);
        strncpy(job.dst_ip, entry->dst_host, sizeof(job.dst_ip) - 1);
        job.dst_port = entry->dst_port;

        queue_push(&job_queue, &job);
    }

    fclose(fp);
    free(entry);
    return NULL;
}

//Loads initial sync mappings from a config file and starts them
void load_sync_config(const char *filename){
    FILE *f = fopen(filename, "r");
    if (!f){
        exit(EXIT_FAILURE);
    }

    char line[512];
    while (fgets(line, sizeof(line), f)){
        char src[256], dst[256];
        if (sscanf(line, "%255s %255s", src, dst) != 2){
            continue;
        }

        SyncMapping *entry = calloc(1, sizeof(SyncMapping));
        sscanf(src, "%[^@]@%[^:]:%d", entry->src_path, entry->src_host, &entry->src_port);
        sscanf(dst, "%[^@]@%[^:]:%d", entry->dst_path, entry->dst_host, &entry->dst_port);

        pthread_mutex_lock(&mapping_list_mutex);
        int exists = 0;
        SyncMapping *cur = mapping_list_head;
        while (cur){
            if (strcmp(cur->src_path, entry->src_path) == 0 &&
                strcmp(cur->src_host, entry->src_host) == 0 &&
                cur->src_port == entry->src_port &&
                strcmp(cur->dst_path, entry->dst_path) == 0 &&
                strcmp(cur->dst_host, entry->dst_host) == 0 &&
                cur->dst_port == entry->dst_port){
                exists = 1;
                break;
            }
            cur = cur->next;
        }

        if (exists){
            pthread_mutex_unlock(&mapping_list_mutex);
            free(entry);
            continue;
        }

        entry->next = mapping_list_head;
        mapping_list_head = entry;
        pthread_mutex_unlock(&mapping_list_mutex);

        pthread_t tid;
        pthread_create(&tid, NULL, init_sync_request, entry);
        pthread_detach(tid);
    }
    fclose(f);
}