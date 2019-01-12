#include "commands.h"

extern QProgressBar *pb;
extern int g_datafd;

void PORT(char *sentence, int *status, conn *info) {
    char *num = (char*)malloc(3);
    int port = 0;

    char *arg_str = (char*)malloc(strlen(sentence)-5);
    strcpy(arg_str, sentence + 5);

    int i = 0;
    while((num = strsep(&arg_str, ",")) != NULL) {
        int num_int = (int)strtol(num, (char **)NULL, 10);
        if (num_int > 256) {
            strcpy(sentence, "501 Wrong parameters or arguments\n");
            free(num);
            free(arg_str);
            return;
        }

        if (i >= 4)
            port = port * 256 + num_int;

        i++;
    }

    strcpy(info->ip_address, "127.0.0.1");
    info->port = port;

    *status |= STATUS_PORT;

    free(num);
    free(arg_str);
}

void RETR_PORT(char *sentence, int *status, conn *info) {
    int listenfd = new_listen_socket(info->port);

    int datafd;
    if ((datafd = accept(listenfd, NULL, NULL)) == -1) {
        printf("Error accept(): %s(%d)\n", strerror(errno), errno);
        return;
    }

    get_arg(sentence, 5);
    char *full_name = get_full_path(root, sentence);
    printf("full_name: %s\n", full_name);

    int n = recv(connfd, sentence, 8191, 0);
    sentence[n - 2] = '\0';
    printf("sentence: %s\n", sentence);
    if (strncmp(sentence, "150", 3) != 0) {
        printf("RETR failed.\n");
        free(full_name);
        closesocket(datafd);
        return;
    }

    FILE *fout = fopen(full_name, "wb");
    if (fout == NULL) {
        printf("Error fopen(): %s(%d)\n", strerror(errno), errno);
        free(full_name);
        closesocket(datafd);
        return;
    }

    // Read file from server
    while(true) {
        n = recv(datafd, sentence, 8191, 0);
        if (n == 0) break;

        fwrite(sentence, 1, n, fout);
    }

    if (fclose(fout) != 0) {
        printf("Error fclose(): %s(%d)\n", strerror(errno), errno);
        free(full_name);
        closesocket(datafd);
        return;
    }

    *status &= ~STATUS_PORT;

    closesocket(datafd);
    closesocket(listenfd);
    free(full_name);
}

void RETR_PASV(char *sentence, int *status, conn *info, char* filename, retr_thread* th) {
    th->connect(th, SIGNAL(progress_signals(int)),th, SLOT(progress_slots(int)));
    char *sentence_orig = (char*)malloc(8192);
    memset(sentence_orig,0,sizeof(sentence_orig));
    strcpy(sentence_orig, sentence);
    printf("===sentence_normal: %s\n", sentence);
    printf("===sentence_orig: %s\n", sentence_orig);
    printf("len: %d\n", strlen(sentence));

    printf("RETR_PASV info->ip_address: %s, port: %d\n", info->ip_address, info->port);
    int datafd = new_conn_socket(info->ip_address, info->port);
    g_datafd = datafd;

    printf("file_name: %s\n", filename);
    char *full_name = get_full_path(root, filename);
    printf("full_name: %s len: %d\nfilename: %s len: %d\n",
           full_name, strlen(full_name), filename, strlen(filename));

    FILE *fbackup = fopen("backup.txt", "r+b");
    if (fbackup == NULL) {
        printf("Error backup fopen(): %s(%d)\n", strerror(errno), errno);
        closesocket(datafd);
        free(full_name);
        return;
    }

    FILE *fout;
    int n;
    long long int readByte;
    char c = fgetc(fbackup);
    if (c == '0' || c == '-') {
        fout = fopen(full_name, "wb");
        if (fout == NULL) {
            printf("!!Error fopen(): %s(%d)\n", strerror(errno), errno);
            closesocket(datafd);
            free(full_name);
            return;
        }
        readByte = 0;
        printf("New download!!!\n");
    }
    else {
        fout = fopen(full_name, "r+b");
        if (fout == NULL) {
            printf("@@Error fopen(): %s(%d)\n", strerror(errno), errno);
            closesocket(datafd);
            free(full_name);
            return;
        }
        rewind(fbackup);
        fscanf(fbackup, "%lld", &readByte);
        printf("Resume download. start from here: %lld\n", readByte);

        sprintf(sentence, "REST %lld\r\n", readByte);
        n = send(connfd, sentence, strlen(sentence), 0);
        n = recv(connfd, sentence, 8191, 0);
        printf("sentence0: %s\n", sentence);

        if (strncmp(sentence, "350", 3) != 0) {
            strcpy(sentence, "Failed to restart.");
            return;
        }

        rewind(fout);
        fseek(fout, readByte, SEEK_SET);
    }

    send(connfd, sentence_orig, strlen(sentence_orig), 0);
    read_from_server(connfd, sentence_orig);
    printf("sentence1: %s\n", sentence_orig);

    emit th->progress_signals(0);
    // Read file from server
    while(true) {
        n = recv(datafd, sentence, 8191, 0);
        if (n == 0) break;

        fwrite(sentence, 1, n, fout);
        readByte += n;
        rewind(fbackup);
        fprintf(fbackup, "%lld", readByte);
    }
    emit th->progress_signals(100);

    fclose(fbackup);
    fbackup = fopen("backup.txt", "w");
    fprintf(fbackup, "0");

    if (fclose(fout) != 0) {
        printf("Error fclose(): %s(%d)\n", strerror(errno), errno);
        closesocket(datafd);
        free(full_name);
        return;
    }
    if (fclose(fbackup) != 0) {
        printf("Error fclose(): %s(%d)\n", strerror(errno), errno);
        closesocket(datafd);
        free(full_name);
        return;
    }

    *status &= ~STATUS_PASV;

    closesocket(datafd);
    free(full_name);

    if (read_from_server(connfd, sentence_orig) == -1) {
        printf("Read from server failed.\n");
    }
    printf("sentence2: %s\n", sentence_orig);
}

void STOR_PORT(char *sentence, int *status, conn *info, stor_thread* th) {
    th->connect(th, SIGNAL(progress_signals(int,int)), th, SLOT(progress_slots(int,int)));
    int listenfd = new_listen_socket(info->port);
    int datafd;
    if ((datafd = accept(listenfd, NULL, NULL)) == -1) {
        printf("Error accept(): %s(%d)\n", strerror(errno), errno);
        return;
    }

    get_arg(sentence, 5);

    char *full_name = get_full_path(root, sentence);
    printf("full_name: %s\n", full_name);
    if (!is_file_exist(full_name)){
        printf("File not exist.\n");
        closesocket(datafd);
        free(full_name);
        return;
    }

    int n = recv(connfd, sentence, 8191, 0);
    sentence[n - 2] = '\0';
    printf("sentence: %s\r\n", sentence);
    if (strncmp(sentence, "150", 3) != 0) {
        closesocket(datafd);
        free(full_name);
        printf("STOR faild.\n");
        return;
    }

    FILE *fin;
    fin = fopen(full_name, "rb");
    if (fin == NULL) {
        printf("Open file failed.\n");
        closesocket(datafd);
        free(full_name);
        return;
    }

    int len;
    while (!feof(fin)) {
        len = fread(sentence, 1, sizeof(sentence), fin);
        if (send(datafd, sentence, len, 0) == -1) {
            strcpy(sentence, "550 Retrieve file failed.\n");
            free(full_name);
            return;
        }
    }
    if (fclose(fin) != 0)
        printf("Close file failed.\n");

    *status &= ~STATUS_PORT;

    closesocket(datafd);
    closesocket(listenfd);
    free(full_name);
}

void STOR_PASV(char *sentence, int *status, conn *info, stor_thread *th) {
    char *sentence_orig = strdup(sentence);
    int len = strlen(sentence);
    sentence[len - 1] = '\0';
    sentence[len] = '\n';
    th->connect(th, SIGNAL(progress_signals(int,int)), th, SLOT(progress_slots(int,int)));
    int datafd = new_conn_socket(info->ip_address, info->port);
    get_first_arg(sentence);

    printf("sentence: %s\n", sentence);
    char *full_name = get_full_path(root, sentence);
    printf("full_name: %s\n", full_name);
    if (!is_file_exist(full_name)){
        printf("File not exist.\n");
        closesocket(datafd);
        free(full_name);
        return;
    }

    FILE *fbackup = fopen("backup.txt", "r+b");
    if (fbackup == NULL) {
        printf("Error fopen(): %s(%d)\n", strerror(errno), errno);
        close(datafd);
        free(full_name);
        return;
    }

    FILE *fin;
    fin = fopen(full_name, "rb");
    if (fin == NULL) {
        printf("Upload file failed.\n");
        close(datafd);
        free(full_name);
        return;
    }
    fseek(fin, 0, SEEK_END);
    int file_size = ftell(fin);
    rewind(fin);

    long long int writeByte;
    int n;
    char c = fgetc(fbackup);
    if (c == '0' || c == '-') {
        writeByte = 0;
        printf("New upload!!!\n");
    }
    else {
        rewind(fbackup);
        fscanf(fbackup, "%lld", &writeByte);
        printf("Resume upload. start from here: %lld\n", writeByte);
        sprintf(sentence, "REST %lld\r\n", writeByte);
        n = send(connfd, sentence, strlen(sentence), 0);
        n = recv(connfd, sentence, 8191, 0);
        if (strncmp(sentence, "350", 3) != 0) {
            strcpy(sentence, "Failed to restart.");
            return;
        }
        printf("%s\n", sentence);
    }

    printf("sentence_orig: %s\n", sentence_orig);
    n = write_to_server(connfd, sentence_orig);
    n = read_from_server(connfd, sentence_orig);
    printf("sentence1: %s\n", sentence_orig);

    rewind(fin);
    fseek(fin, writeByte, SEEK_SET);

    while (!feof(fin)) {
        n = fread(sentence, 1, sizeof(sentence), fin);
        if (send(datafd, sentence, n, 0) == -1) {
            strcpy(sentence, "550 Retrieve file failed.\n");
            closesocket(datafd);
            free(full_name);
            return;
        }
        writeByte += n;
        emit th->progress_signals(writeByte, file_size);
        rewind(fbackup);
        fprintf(fbackup, "%lld", writeByte);
    }
    emit th->progress_signals(file_size, file_size);

    printf("File upload complete\n");

    fclose(fbackup);
    fbackup = fopen("backup.txt", "w");
    fputc('0', fbackup);

    if (fclose(fin) != 0)
        printf("Close file failed.\n");
    if (fclose(fbackup) != 0)
        printf("Close file failed.\n");

    *status &= ~STATUS_PASV;

    closesocket(datafd);
    free(full_name);

    if (read_from_server(connfd, sentence_orig) == -1) {
        printf("Read from server failed.\n");
    }
    printf("sentence2: %s\n", sentence_orig);
}

void LIST_PORT(char *sentence, int *status, conn *info) {
    int listenfd = new_listen_socket(info->port);

    int datafd;
    if ((datafd = accept(listenfd, NULL, NULL)) == -1) {
        printf("Error accept(): %s(%d)\n", strerror(errno), errno);
        return;
    }

    int n = recv(connfd, sentence, 8191, 0);
    sentence[n - 2] = '\0';

    // Read file from server
    while(true) {
        n = recv(datafd, sentence, 8191, 0);
        if (n == 0) break;

        printf("%s\n", sentence);
    }

    *status &= ~STATUS_PORT;

    closesocket(datafd);
    closesocket(listenfd);
}

void LIST_PASV(char *sentence, int *status, conn *info) {
    int datafd = new_conn_socket(info->ip_address, info->port);

    // Receive start transfer message
    if (recv(connfd, sentence, 8191, 0) == -1 ||
        strncmp(sentence, "150", 3) != 0) {
        closesocket(datafd);
        return;
    }

    // Read file from server
    int n;
    char* line = (char*)malloc(256);
    while(true) {
        n = recv(datafd, sentence, 8191, 0);
        if (n == 0) break;

        printf("LIST_PASV: %s\n", sentence);

    }

    *status &= ~STATUS_PASV;

    closesocket(datafd);
}

void PASV(char *sentence, int *status, conn *info) {
    memset(info->ip_address, 0, sizeof(info->ip_address));
    char ip[128];
    int _port[10];
    get_arg(sentence, 26);

    divide_port(sentence, _port, 1);
    for (int i = 0; i < 3; i++) {
        sprintf(ip, "%d", _port[i]);
        strcat(info->ip_address, ip);
        strcat(info->ip_address, ".");
        memset(ip, 0, sizeof(ip));
    }

    sprintf(ip, "%d", _port[3]);
    strcat(info->ip_address, ip);
    info->port = _port[4] * 256 + _port[5];

    *status |= STATUS_PASV;

    printf("Connect OK!\n");
}

retr_thread::retr_thread(char* sentence, int* status, conn *conn_info, char* filename) {
    this->sentence = sentence;
    this->status = status;
    this->conn_info = conn_info;
    this->filename = filename;
}

void retr_thread::run() {
    RETR_PASV(sentence, status, conn_info, filename, this);
}

void retr_thread::progress_slots(int max) {
    pb->setMinimum(0);  // 最小值
    pb->setMaximum(max);  // 最大值
    pb->setValue(max);
}

stor_thread::stor_thread(char* sentence, int* status, conn *conn_info) {
    this->sentence = sentence;
    this->status = status;
    this->conn_info = conn_info;
}

void stor_thread::run() {
    STOR_PASV(sentence, status, conn_info, this);
}

void stor_thread::progress_slots(int n, int max) {
    pb->setMinimum(0);  // 最小值
    pb->setMaximum(max);  // 最大值
    pb->setValue(n);
}
