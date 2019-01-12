#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <getopt.h>

typedef enum {false, true} bool;

#define MAX_CLIENT_NUM 20 

#define STATUS_INIT         (1<<0)
#define STATUS_USER         (1<<1)
#define STATUS_LOGIN        (1<<2)
#define STATUS_PORT         (1<<3)
#define STATUS_PASV         (1<<4)
#define STATUS_RNFR         (1<<5)
#define STATUS_QUIT         (1<<6)

int read_from_client(int connfd, char* sentence) {
    int p = 0;
    while(true) {
        int n = read(connfd, sentence + p, 8191 - p);
        if (n < 0) {
            printf("Error read(): %s(%d)\n", strerror(errno), errno);
            close(connfd);
            return -1;
        } else {
            p += n;
            break;
        }
    }
    sentence[p - 2] = '\0';
    return 1;
}

int write_to_client(int connfd, char* sentence){
	int len = strlen(sentence);
	sentence[len] = '\r';
	sentence[len + 1] = '\n';
	sentence[len + 2] = '\0';

	len = strlen(sentence);
	printf("%s\n", sentence);
    int p = 0;
    while (p < len) {
        int n = write(connfd, sentence, len);
        if (n < 0) {
            printf("Error write(): %s(%d)\n", strerror(errno), errno);
            return -1;
        }
        else
            p += n;
    }
}

int is_arg_count(char* sentence, int count) {
    int arg_count = 0;
    for (int i = 0; i < strlen(sentence); i++) {
        if (sentence[i] == ' ')
            arg_count++;
    }

    if (arg_count == count) return 1;

    strcpy(sentence, "501 Wrong parameters or arguments.\n");
    return 0;
}

// Allocate new menory. Memory must be freed.
char* get_full_path(char *curr_path, char *path) {
    char *temp = strdup(path);
    char *full_name = malloc(strlen(curr_path) + 1 + strlen(temp));

    strcpy(full_name, curr_path);
    if (curr_path[strlen(curr_path) - 1] != '/')
        strcat(full_name, "/");
    strcat(full_name, temp);

    free(temp);
    return full_name;
}

int is_file_exist(char *full_path) {
    struct stat st;
    if (stat(full_path, &st) != 0)
        return false;

    return true;
}

int is_dir(char *full_path) {
    struct stat st;

    if (stat(full_path, &st) != 0) {
        free(full_path);
        return false;
    }

    return S_ISDIR(st.st_mode);
}

void get_arg(char* str,int num){
    char* temp;
    temp = (char*)malloc(sizeof(char) * strlen(str));
    for(int i = 0; i < strlen(str) - num; i++){
        temp[i] = str[i + num];
    }
    strcpy(str,temp);
}

int new_conn_socket(char *ip_address,int port_num){
	int sockfd;
	struct sockaddr_in addr;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port_num);
	if (inet_pton(AF_INET, ip_address, &addr.sin_addr) <= 0) {
		printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}
	sleep(1);
	if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		printf("Error connect(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	return sockfd;
}

int new_listen_socket(int port_num){
	int listenfd;
	struct sockaddr_in addr;
	if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}
	if (listenfd == -1) {
		return 1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port_num);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	if (listen(listenfd, 10) == -1) {
		printf("Error listen(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}
	return listenfd;
}

typedef struct user {
	char id[128];
	char pw[128];
	int is_online;
	pid_t pid;
} user;

typedef struct conn {
	char ip_address[15];
	int port;
} conn;

