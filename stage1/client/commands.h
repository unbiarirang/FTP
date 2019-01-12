extern char root[];
extern int connfd;

void PORT(char *sentence, int *status, conn *info) {
    char *num = malloc(3);
    int port = 0;

    char *arg_str = malloc(strlen(sentence)-5);
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

    int n = read(connfd, sentence, 8191);
    sentence[n - 2] = '\0';
    printf("sentence: %s\n", sentence);
    if (strncmp(sentence, "150", 3) != 0) {
        printf("RETR failed.\n");
        free(full_name);
        close(datafd);
        return;
    }

    FILE *fout = fopen(full_name, "wb");
    if (fout == NULL) {
        printf("Error fopen(): %s(%d)\n", strerror(errno), errno);
        free(full_name);
        close(datafd);
        return;
    }

    // Read file from server
    while(true) {
        n = read(datafd, sentence, 8191);
        if (n == 0) break;

        fwrite(sentence, 1, n, fout);
    }

    if (fclose(fout) != 0) {
        printf("Error fclose(): %s(%d)\n", strerror(errno), errno);
        free(full_name);
        close(datafd);
        return;
    }

    *status &= ~STATUS_PORT;

    close(datafd);
    close(listenfd);
    free(full_name);
}

void RETR_PASV(char *sentence, int *status, conn *info) {
    int datafd = new_conn_socket("127.0.0.1", info->port);
    get_arg(sentence, 5);

    char *full_name = get_full_path(root, sentence);
    printf("full_name: %s, sentence: %s\n", full_name, sentence);

    // Receive start transfer message
    if (read(connfd, sentence, 8191) == -1 ||
        strncmp(sentence, "150", 3) != 0) {
        close(datafd);
        free(full_name);
        return;
    }

    FILE *fout = fopen(full_name, "wb");
    if (fout == NULL) {
        printf("Error fopen(): %s(%d)\n", strerror(errno), errno);
        close(datafd);
        free(full_name);
        return;
    }

    // Read file from server
    sleep(1);
    int n;
    while(true) {
        n = read(datafd, sentence, 8191);
        if (n == 0) break;

        fwrite(sentence, 1, n, fout);
    }

    if (fclose(fout) != 0) {
        printf("Error fclose(): %s(%d)\n", strerror(errno), errno);
        close(datafd);
        free(full_name);
        return;
    }

    *status &= ~STATUS_PASV;

    close(datafd);
    free(full_name);
}

void STOR_PORT(char *sentence, int *status, conn *info) {
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
        close(datafd);
        free(full_name);
        return;
    }

	int n = read(connfd, sentence, 8191);
	sentence[n - 2] = '\0';
	printf("sentence: %s\r\n", sentence);
    if (strncmp(sentence, "150", 3) != 0) {
        close(datafd);
        free(full_name);
        printf("STOR faild.\n");
        return;
    }

    FILE *fin;
    fin = fopen(full_name, "rb");
    if (fin == NULL) {
        printf("Open file failed.\n");
        close(datafd);
        free(full_name);
        return;
    }

    int len;
    while (!feof(fin)) {
        len = fread(sentence, 1, sizeof(sentence), fin);
        if (write(datafd, sentence, len) == -1) {
            strcpy(sentence, "550 Retrieve file failed.\n");
            free(full_name);
            return;
        }
    }
    if (fclose(fin) != 0)
        printf("Close file failed.\n");

    *status &= ~STATUS_PORT;

    close(datafd);
    close(listenfd);
    free(full_name);
}

void STOR_PASV(char *sentence, int *status, conn *info) {
    int datafd = new_conn_socket("127.0.0.1", info->port);
    get_arg(sentence, 5);

    char *full_name = get_full_path(root, sentence);
    printf("full_name: %s\n", full_name);
    if (!is_file_exist(full_name)){
        printf("File not exist.\n");
        close(datafd);
        free(full_name);
        return;
    }

    int n = read(connfd, sentence, 8191);
    sentence[n - 2] = '\0';
    printf("STOR_PASV sentence: %s\n", sentence);

    FILE *fin;
    fin = fopen(full_name, "rb");
    if (fin == NULL) {
        printf("Upload file failed.\n");
        close(datafd);
        free(full_name);
        return;
    }

    int len;
    while (!feof(fin)) {
        len = fread(sentence, 1, sizeof(sentence), fin);
        if (write(datafd, sentence, len) == -1) {
            strcpy(sentence, "550 Retrieve file failed.\n");
            close(datafd);
            free(full_name);
            return;
        }
    }

    if (fclose(fin) != 0)
        printf("Close file failed.\n");

    *status &= ~STATUS_PASV;

    close(datafd);
    free(full_name);
}

void LIST_PORT(char *sentence, int *status, conn *info) {
    int listenfd = new_listen_socket(info->port);

    int datafd;
    if ((datafd = accept(listenfd, NULL, NULL)) == -1) {
	    printf("Error accept(): %s(%d)\n", strerror(errno), errno);
	    return;
    }

    int n = read(connfd, sentence, 8191);
    sentence[n - 2] = '\0';

    // Read file from server
    while(true) {
        n = read(datafd, sentence, 8191);
        if (n == 0) break;

        printf("%s\n", sentence);
    }

    *status &= ~STATUS_PORT;

    close(datafd);
    close(listenfd);
}

void LIST_PASV(char *sentence, int *status, conn *info) {
    int datafd = new_conn_socket("127.0.0.1", info->port);

    // Receive start transfer message
    if (read(connfd, sentence, 8191) == -1 ||
        strncmp(sentence, "150", 3) != 0) {
        close(datafd);
        return;
    }

    // Read file from server
    sleep(1);
    int n;
    while(true) {
        n = read(datafd, sentence, 8191);
        if (n == 0) break;

        printf("%s\n", sentence);
    }

    *status &= ~STATUS_PASV;

    close(datafd);
}

void PASV(char *sentence, int *status, conn *info) {
    char *num = malloc(3);
    char ip[15];
    memset(ip, 0, strlen(ip));

    char *arg_str = strchr(sentence, '(') + 1;
    char *arg_end = strchr(sentence, ')');
    *arg_end = 0;

    int i = 0;
    int port = 0;
    while((num = strsep(&arg_str, ",")) != NULL) {
        int num_int = (int)strtol(num, (char **)NULL, 10);
        if (num_int > 256) {
            strcpy(sentence, "501 Wrong parameters or arguments\n");
            return;
        }

        if (i < 3) {
            strcat(ip, num);
            strcat(ip, ".");
        } else if (i == 3)
            strcat(ip, num);
        else
            port = port * 256 + num_int;

        i++;
    }
    printf("ip: %s, port: %d\n", ip, port);
    strcat(info->ip_address, ip);
    info->port = port;

    *status |= STATUS_PASV;

    printf("Connect OK!\n");
}
