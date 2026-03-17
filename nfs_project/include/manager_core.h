#ifndef MANAGER_CORE_H
#define MANAGER_CORE_H

#include <pthread.h>
#include "utils.h"

//Holds information for a sync pair (source -> target)
typedef struct SyncMapping{
    char src_path[256];
    char src_host[64];
    char dst_path[256];
    char dst_host[64];
    int dst_port;
    int src_port;
    struct SyncMapping *next;
} SyncMapping;

//Global list of active sync mappings and its mutex
extern pthread_mutex_t mapping_list_mutex;
extern SyncMapping *mapping_list_head;

//Background sync request initiator
void *init_sync_request(void *arg);

//Handles commands sent from the nfs_console
void *monitor_console_input(void *arg);

//Loads sync pairs from the config file at startup
void load_sync_config(const char *filename);

//Generates a timestamp string (for logging)
void timestamp_now(char *buf, size_t len);

//Prints usage information and exits the program
void show_usage(const char *program_name);

#endif
