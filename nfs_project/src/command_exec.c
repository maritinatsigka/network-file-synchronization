#include "command_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>

//Helper to trim input string
static void cleanup_string(char *text){
    char *end = text + strlen(text) - 1;
    while (end >= text && (*end == '\n' || *end == '\r')){
        *end-- = '\0';
    }
}

//Reads a line from socket until newline or max_len is reached
int socket_read_line(int sock, char *buf, int max_len){
    int i = 0;
    char c;
    while (i < max_len - 1){
        if (read(sock, &c, 1) <= 0){
            return -1;
        }
        buf[i++] = c;
        if (c == '\n'){
            break;
        }
    }
    buf[i] = '\0';
    return i;
}

//Executes LIST command: sends names of all regular files in given directory
static void exec_list(int fd, const char *path){
    DIR *dir = opendir(path);
    if (!dir){
        dprintf(fd, "ERR: cannot open %s\n.\n", path);
        return;
    }
    struct dirent *e;
    while ((e = readdir(dir))){
        if (e->d_type == DT_REG){
            dprintf(fd, "%s\n", e->d_name);
        }
    }
    dprintf(fd, ".\n");
    closedir(dir);
}

//Executes PULL command: sends contents of specified file to socket
static void exec_pull(int fd, const char *path){
    int file = open(path, O_RDONLY);
    if (file < 0){
        dprintf(fd, "-1 %s\n", strerror(errno));
        return;
    }

    off_t sz = lseek(file, 0, SEEK_END);
    lseek(file, 0, SEEK_SET);
    dprintf(fd, "%ld ", sz);

    char temp[BUF_SIZE];
    ssize_t r;
    while ((r = read(file, temp, sizeof(temp))) > 0){
        write(fd, temp, r);
    }
    close(file);
}

//Executes PUSH command: receives data chunks and writes to file
static void exec_push(int fd, const char *path, int len, const char *chunk){
    static FILE *f = NULL;

    if (len == -1){
        f = fopen(path, "w");
    } else if (len == 0){
        if (f){
            fclose(f);
        }
        f = NULL;
    } else{
        if (!f){
            f = fopen(path, "a"); //Append data if file wasn't open
        }
        fwrite(chunk, 1, len, f);
        fflush(f);
    }
}

//Defines available command handlers
typedef struct{
    const char *name;
    void (*handler)(int, Command *);
} DispatchEntry;

//Wrappers to match common signature
static void wrap_list(int fd, Command *c)  { exec_list(fd, c->arg1); }
static void wrap_pull(int fd, Command *c)  { exec_pull(fd, c->arg1); }
static void wrap_push(int fd, Command *c)  { exec_push(fd, c->arg1, c->chunk_size, c->data); }

//Command dispatch table
static DispatchEntry dispatch_table[] = {
    { "LIST", wrap_list },
    { "PULL", wrap_pull },
    { "PUSH", wrap_push },
    { NULL, NULL }
};

//Finds and executes the appropriate handler for a parsed command
void run_command(int fd, Command *cmd){
    for (int i = 0; dispatch_table[i].name; ++i){
        if (strcmp(cmd->type, dispatch_table[i].name) == 0){
            dispatch_table[i].handler(fd, cmd);
            if (cmd->data){
                free(cmd->data);
            }
            return;
        }
    }
    write(fd, "ERR: Unknown command\n", 22);
}

//Parses a raw text command into a Command struct
int parse_command(char *text, Command *cmd){
    cleanup_string(text);
    memset(cmd, 0, sizeof(Command));

    if (strncmp(text, "LIST ", 5) == 0){
        strcpy(cmd->type, "LIST");
        sscanf(text + 5, "%511s", cmd->arg1);
        return 1;
    }

    if (strncmp(text, "PULL ", 5) == 0){
        strcpy(cmd->type, "PULL");
        sscanf(text + 5, "%511s", cmd->arg1);
        return 1;
    }

    if (strncmp(text, "PUSH ", 5) == 0){
        strcpy(cmd->type, "PUSH");
        char *sep = strchr(text + 5, ' ');
        if (!sep){
            return 0;
        }
        sscanf(text + 5, "%511s %d", cmd->arg1, &cmd->chunk_size);
        sep = strchr(sep + 1, ' ');
        if (!sep){
            return 0;
        }
        cmd->data = strdup(sep + 1);
        return 1;
    }

    return 0;
}
