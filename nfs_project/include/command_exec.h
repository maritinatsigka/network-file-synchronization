#ifndef COMMAND_EXEC_H
#define COMMAND_EXEC_H

#define BUF_SIZE 4096

//Struct to hold parsed command info
typedef struct{
    char type[8];    //LIST, PULL, PUSH
    char arg1[512];  //Path for LIST/PULL/PUSH
    int chunk_size;  //Used for PUSH only
    char *data;      //Payload for PUSH
} Command;


//Reads a line of input from socket
int socket_read_line(int sock, char *buf, int max_len);

//Parses raw input text into Command structure
int parse_command(char *text, Command *cmd);

//Executes a parsed command on the given socket
void run_command(int fd, Command *cmd);

#endif
