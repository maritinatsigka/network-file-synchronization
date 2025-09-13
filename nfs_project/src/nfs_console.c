#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define MAX_CMD_LEN 1024  //Maximum length of a user command

//Struct to store command-line arguments
typedef struct{
    char *log_path;
    char *host_ip;
    int host_port;
} Settings;

//Appends a timestamped message to the log file
void append_to_log(FILE *log, const char *text){
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%F %T", tm_info);
    fprintf(log, "[%s] %s\n", timestamp, text);
    fflush(log);
}

//Establishes a TCP connection to the specified host and port
int connect_to_host(const char *ip, int port){
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0){
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    //Convert and set IP address
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0){
        fprintf(stderr, "Invalid IP address: %s\n", ip);
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
        perror("connect");
        close(fd);
        return -1;
    }

    return fd;
}

//Parse CLI args and validate
void parse_arguments(int argc, char **argv, Settings *conf){
    if (argc != 7){
        fprintf(stderr, "Usage: %s -l <logfile> -h <host_ip> -p <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    for (int i = 1; i < argc; i += 2){
        if (strcmp(argv[i], "-l") == 0){
            conf->log_path = argv[i + 1];
        }
        else if (strcmp(argv[i], "-h") == 0){
            conf->host_ip = argv[i + 1];
        }
        else if (strcmp(argv[i], "-p") == 0){
            conf->host_port = atoi(argv[i + 1]);
        }
        else{
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            exit(EXIT_FAILURE);
        }
    }
    //Validate argument values
    if (!conf->log_path || !conf->host_ip || conf->host_port <= 0){
        fprintf(stderr, "Invalid arguments.\n");
        exit(EXIT_FAILURE);
    }
}

//Prompts the user and reads a command from stdin
int read_user_command(char *buffer, size_t size){
    printf("> ");
    if (!fgets(buffer, size, stdin)){
        return 0;
    }
    buffer[strcspn(buffer, "\n")] = '\0';  //Remove newline
    return strlen(buffer) > 0;
}

//Main loop to handle interaction with nfs_manager
void handle_session(int sockfd, FILE *log_file){
    char command[MAX_CMD_LEN];
    char response[MAX_CMD_LEN];

    while (read_user_command(command, sizeof(command))){
        //Log user input
        append_to_log(log_file, command);
        //Send command to server
        write(sockfd, command, strlen(command));
        write(sockfd, "\n", 1);

        //Read response from server
        int received = read(sockfd, response, sizeof(response) - 1);
        if (received <= 0){
            printf("Connection lost.\n");
            break;
        }
        response[received] = '\0';
        printf("%s", response);

        if (strncmp(command, "shutdown", 8) == 0){
            break;
        }
    }
}

int main(int argc, char **argv){
    Settings conf = {0};
    parse_arguments(argc, argv, &conf);

    FILE *log = fopen(conf.log_path, "a");
    if (!log){
        perror("fopen log file");
        exit(EXIT_FAILURE);
    }
    
    //Connect to manager
    int sock = connect_to_host(conf.host_ip, conf.host_port);
    if (sock < 0){
        fclose(log);
        exit(EXIT_FAILURE);
    }

    handle_session(sock, log);
    fclose(log);
    close(sock);
    return 0;
}
