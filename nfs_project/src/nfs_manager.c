#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "manager_core.h"
#include "worker_jobs.h"
#include "utils.h"

//Global job queue
Queue job_queue;

//Global log file pointer
FILE *log_file = NULL;

//Structure to map command-line flags to config fields
typedef struct{
    const char *flag;
    void *target;
    int is_int;
} ArgMap;

//Configuration container for parsed arguments
typedef struct{
    char *logfile;
    char *config_file;
    int worker_limit;
    int port;
    int buffer_size;
} Config;

//Print parsed configuration values
static void print_banner(const Config *cfg){
    printf("Parsed args:\n");
    printf("  Log file     : %s\n", cfg->logfile);
    printf("  Config file  : %s\n", cfg->config_file);
    printf("  Worker count : %d\n", cfg->worker_limit);
    printf("  Port         : %d\n", cfg->port);
    printf("  Buffer size  : %d\n", cfg->buffer_size);
}

//Parse CLI arguments and populate the config struct
static Config parse_args(int argc, char *argv[]){
    if (argc != 11) show_usage(argv[0]);

    Config cfg = {0};

    ArgMap args[] = {
        {"-l", &cfg.logfile,      0},
        {"-c", &cfg.config_file,  0},
        {"-n", &cfg.worker_limit, 1},
        {"-p", &cfg.port,         1},
        {"-b", &cfg.buffer_size,  1},
    };

    int arg_count = sizeof(args) / sizeof(args[0]);

    for (int i = 1; i < argc; i += 2){
        const char *flag = argv[i];
        const char *val = argv[i + 1];
        int matched = 0;

        //Match flag and assign value
        for (int j = 0; j < arg_count; ++j){
            if (strcmp(flag, args[j].flag) == 0){
                if (args[j].is_int){
                    *(int *)args[j].target = atoi(val);
                }
                else{
                    *(char **)args[j].target = (char *)val;
                }
                matched = 1;
                break;
            }
        }

        if (!matched){
            fprintf(stderr, "Unknown argument: %s\n", flag);
            show_usage(argv[0]);
        }
    }

    //Check for required and valid arguments
    if (!cfg.logfile || !cfg.config_file || cfg.worker_limit <= 0 || cfg.port <= 0 || cfg.buffer_size <= 0){
        fprintf(stderr, "Missing or invalid arguments.\n");
        show_usage(argv[0]);
    }

    return cfg;
}

int main(int argc, char *argv[]){
    //Parse arguments into config
    Config cfg = parse_args(argc, argv);

    log_file = fopen(cfg.logfile, "w");
    if (!log_file){
        perror("Error opening log file");
        exit(EXIT_FAILURE);
    }

    print_banner(&cfg);

    queue_init(&job_queue, cfg.buffer_size);

    load_sync_config(cfg.config_file);

    pthread_t console_thread;
    pthread_create(&console_thread, NULL, monitor_console_input, &cfg.port);

    pthread_t workers[cfg.worker_limit];
    for (long i = 0; i < cfg.worker_limit; ++i){
        pthread_create(&workers[i], NULL, sync_worker_loop, (void *)i);
    }
    pthread_join(console_thread, NULL);

    for (int i = 0; i < cfg.worker_limit; ++i){
        pthread_join(workers[i], NULL);
    }

    queue_destroy(&job_queue);
    fclose(log_file);
    return 0;
}
