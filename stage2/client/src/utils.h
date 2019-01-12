#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <winsock2.h>
#include <WinBase.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <dirent.h>

#pragma comment(lib, "ws2_32.lib")

#define MAX_CLIENT_NUM 20

#define STATUS_INIT         (1<<0)
#define STATUS_USER         (1<<1)
#define STATUS_LOGIN        (1<<2)
#define STATUS_PORT         (1<<3)
#define STATUS_PASV         (1<<4)
#define STATUS_RNFR         (1<<5)
#define STATUS_QUIT         (1<<6)

int read_from_server(int connfd, char* sentence);
int write_to_server(int connfd, char* sentence);
int is_arg_count(char* sentence, int count);
char* get_full_path(char *curr_path, char *path);
int is_file_exist(char *full_path);
void get_arg(char* str, int num);
void get_first_arg(char* str);
int new_conn_socket(const char *ip_address, int port_num);
int new_listen_socket(int port_num);
char* strsep(char** stringp, const char* delim);
void divide_port(char* str, int* port, int n);

typedef struct conn {
    char ip_address[128];
    int port;
} conn;

#endif // UTILS_H
