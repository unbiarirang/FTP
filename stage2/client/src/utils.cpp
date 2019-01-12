#include "utils.h"

int read_from_server(int connfd, char* sentence) {
    int p = 0;
    while(true) {
        int n = recv(connfd, sentence, 8191, 0);
        if (n < 0) {
            printf("Error read(): %s(%d)\n", strerror(errno), errno);
            closesocket(connfd);
            return -1;
        } else {
            p += n;
            break;
        }
    }
    sentence[p - 2] = '\0';
    printf("Read from server:%s\n", sentence);
    return 1;
}

int write_to_server(int connfd, char* sentence){
    int len = strlen(sentence);
    sentence[len - 1] = '\r';
    sentence[len] = '\n';
    sentence[len + 1] = '\0';

    len = strlen(sentence);
    int p = 0;
    while (p < len) {

        int n = send(connfd, sentence, len, 0);
        printf("Write to server: %s\n", sentence);
        if (n < 0) {
            printf("Error write(): %s(%d)\n", strerror(errno), errno);
            return -1;
        }
        else {
            p += n;
            break;
        }
    }
    sentence[p - 2] = '\0';
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
    char *full_name = (char*)malloc(strlen(curr_path) + 1 + strlen(temp));

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

void get_arg(char* str, int num){
    char* temp;
    temp = (char*)malloc(sizeof(char) * strlen(str));
    for(int i = 0; i < strlen(str) - num; i++)
        temp[i] = str[i + num];
    strcpy(str,temp);
}

void get_first_arg(char* str) {
    char *temp = strdup(str);
    char *p = strtok(temp, " ");
    p = strtok(NULL, " ");
    strcpy(str, p);
}

int new_conn_socket(const char *ip_address, int port_num){
    int sockfd;
    struct sockaddr_in addr;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        printf("Error socket(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_num);
    addr.sin_addr.s_addr = inet_addr(ip_address);
    memset(addr.sin_zero, 0X00, 8);
    printf("%s\n", ip_address);
    WSAAsyncSelect(sockfd, 0, 0, FD_READ | FD_WRITE | FD_CLOSE);
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

char* strsep(char** stringp, const char* delim)
{
    char* start = *stringp;
    char* p;

    p = (start != NULL) ? strpbrk(start, delim) : NULL;

    if (p == NULL)
        *stringp = NULL;
    else {
        *p = '\0';
        *stringp = p + 1;
    }

    return start;
}

void divide_port(char* str, int* port, int n) {
    char temp[128];
    int cnt = 0;
    int j = 0;
    int flag = 0;
    int k = 0;
    for (int i = n; i < strlen(str); i++) {
        if (str[i] != ',') {
            temp[j] = str[i];
            j++;
        }
        else if (str[i] == ',') {
            port[cnt] = atoi(temp);
            cnt++;
            j = 0;
            memset(temp, 0, sizeof(temp));
        }
    }
    port[cnt] = atoi(temp);
}
