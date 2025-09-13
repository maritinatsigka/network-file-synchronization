#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "command_exec.h"

#define BACKLOG 20  //Maximum number of pending connections in the queue

//Prints usage information and exits the program
static void print_usage(const char *progname){
    fprintf(stderr, "Usage: %s -p <port>\n", progname);
    exit(EXIT_FAILURE);
}

//Initialize TCP server socket
static int create_server_socket(int port){
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    //Allow immediate reuse of the port after program termination
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    //Bind the socket to the specified port
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    //Start listening for incoming connections
    if (listen(server_fd, BACKLOG) < 0){
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    return server_fd;
}

//Processes a single client connection
static void handle_client(int client_fd){
    char input_buf[BUF_SIZE];
    Command cmd = {0};

    //Read a full line from the client
    if (socket_read_line(client_fd, input_buf, sizeof(input_buf)) > 0){
        printf("Client command: %s\n", input_buf);
        if (parse_command(input_buf, &cmd)){
            run_command(client_fd, &cmd);
        } else{
            write(client_fd, "ERROR: Invalid command\n", 24);
        }
    }

    close(client_fd);
}


int main(int argc, char *argv[]){
    if (argc != 3 || strcmp(argv[1], "-p") != 0){
        print_usage(argv[0]);
    }

    int port = atoi(argv[2]);
    int server_fd = create_server_socket(port);

    printf("nfs_client running on port %d\n", port);

    //Accept and handle incoming client connections
    while (1){
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);
        if (client_fd < 0){
            perror("accept");
            continue;
        }

        handle_client(client_fd);
    }

    close(server_fd);
    return 0;
}