#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include <unistd.h>
#include <errno.h>

#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#include <pthread.h>
#include <getopt.h>

typedef enum {false, true} bool;

#define STATUS_INIT         (1<<0)
#define STATUS_USER         (1<<1)
#define STATUS_LOGIN        (1<<2)
#define STATUS_PORT_OR_PASV (1<<3)
#define STATUS_RNFR         (1<<4)
#define STATUS_QUIT         (1<<5)

char root[128] = "tmp/";

int read_from_client(int connfd, char* sentence) {
    int p = 0;
    while (1) {
        int n = read(connfd, sentence + p, 8191 - p);
        if (n < 0) {
            printf("Error read(): %s(%d)\n", strerror(errno), errno);
            close(connfd);
            return -1;
        } else if (n == 0) {
            break;
        } else {
            p += n;
            if (sentence[p - 1] == '\n')
                break;
        }
    }
    sentence[p - 1] = '\0';
    printf("FROM CLIENT: %s\n", sentence);
    int len = p - 1;
    return len;
}

int write_to_client(int connfd, char* sentence, int len){
    int p = 0;
    while (p < len) {
        int n = write(connfd, sentence + p, len - p);
        if (n < 0) {
            printf("Error write(): %s(%d)\n", strerror(errno), errno);
            return -1;
        }
        else
            p += n;
    }
    printf("SEND TO CLIENT %s\n", sentence);
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

char* get_full_path(char *path) {
    char *temp = strdup(path);
    char *full_path = malloc(strlen(root) + strlen(temp));

    strcpy(full_path, root);
    strcat(full_path, temp);
    free(temp);
    return full_path;
}

int is_valid_path(char *full_path) {
    struct stat st;

    if (stat(full_path, &st) != 0) {
        free(full_path);
        return 0;
    }

    return 1;
}

int is_dir(char *full_path) {
    struct stat st;

    if (stat(full_path, &st) != 0) {
        free(full_path);
        return 0;
    }

    return S_ISDIR(st.st_mode);
}

void USER(char *sentence, int* status) {
    if (!is_arg_count(sentence, 1)) return;
    if (strcmp(sentence, "USER anonymous\n") == 0) {
        strcpy(sentence, "331 Guest login ok, send your complete e-mail address as password.\n");
        *status |= STATUS_USER;
    }
}

void PASS(char *sentence, int* status) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(*status & STATUS_USER)) {
        strcpy(sentence, "503 Bad sequence of commands. PASS must be preceded by USER.\n");
        return;
    }

    char* email = malloc(sizeof(char) * strlen(sentence));
    strcpy(email, &sentence[5]);
    if (strcmp(email, "valid\n") == 0) {
        strcpy(sentence, "230-\n"
            "230-Welcome to\n"
            "230-School of Software\n"
            "230-FTP Archives at ftp.ssast.org\n"
            "230-\n"
            "230-This site is provided as a public service by School of\n"
            "230-Software. Use in violation of any applicable laws is strictly\n"
            "230-prohibited. We make no guarantees, explicit or implicit, about the\n"
            "230-contents of this site. Use at your own risk.\n"
            "230-\n"
            "230 Guest login ok, access restrictions apply.\n");
        *status |= STATUS_LOGIN;
        *status &= ~STATUS_USER;
    }
    else
        strcpy(sentence, "530 wrong password\n");

    free(email);
}

void SYST(char *sentence, int status) {
    if (!is_arg_count(sentence, 0)) return;
    if (!(status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.\n");
        return;
    }

    strcpy(sentence, "215 UNIX Type: L8\n");
}

void TYPE(char *sentence, int status) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.\n");
        return;
    }

    char type = sentence[5];
    if (type == 'I')
        strcpy(sentence, "200 Type set to I.\n");
    else
        strcpy(sentence, "504 Type %c not supported.\n");
}

void PORT(char *sentence, int *status) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(*status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.\n");
        return;
    }

    char *num = malloc(sizeof(char) * 3);
    char *ip = malloc(sizeof(char) * 15);
    int port = 0;
    int i = 0;
    char *arg_str = sentence + 5;
    arg_str[strlen(arg_str)-1] = '\0';
    while((num = strsep(&arg_str, ",")) != NULL) {
        int num_int = (int)strtol(num, (char **)NULL, 10);
        printf("num: %s, num_int: %d\n", num, num_int);
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

    printf("port num: %d ip address: %s\n", port, ip);
    *status |= STATUS_PORT_OR_PASV;
    strcpy(sentence, "200 PORT command successful.\n");
}

void PASV(char *sentence, int *status) {
    if (!is_arg_count(sentence, 0)) return;
    if (!(*status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.\n");
        return;
    }

    srand(time(NULL));
    int port = (rand() % 45536) + 20000; // 20000~65535
    char p1[3];
    char p2[3];
    sprintf(p1, "%d", port / 256);
    sprintf(p2, "%d", port % 256);

    int listenfd;
	if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return ;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = port;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
		return ;
	}

	if (listen(listenfd, 10) == -1) {
		printf("Error listen(): %s(%d)\n", strerror(errno), errno);
		return ;
	}

    printf("Data connection listening...\n");

    strcpy(sentence, "227 Entering Passive Mode (123,206,96,15,");
    strcat(sentence, p1);
    strcat(sentence, ",");
    strcat(sentence, p2);
    strcat(sentence, ").\n");
    *status |= STATUS_PORT_OR_PASV;
}

void RETR(char *sentence, int status) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.\n");
        return;
    }
    if (!(status & STATUS_PORT_OR_PASV)) {
        strcpy(sentence, "503 Bad sequence of commands. RETR must be preceded by either a PORT command or a PASV command.\n");
        return;
    }

}

void STOR(char *sentence, int status) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.\n");
        return;
    }
    if (!(status & STATUS_PORT_OR_PASV)) {
        strcpy(sentence, "503 Bad sequence of commands. STOR must be preceded by either a PORT command or a PASV command.\n");
        return;
    }
}

void MKD(char *sentence, int status) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.\n");
        return;
    }

    char *dir_name = sentence + 4;
    dir_name[strlen(dir_name) - 1] = 0;
    char *full_path = get_full_path(dir_name);

    struct stat st = {0};
    if (stat(full_path, &st) == 0) {
        strcpy(sentence, "550 Directory already exists.\n");
        return;
    }

    printf("full path: %s\n", full_path);
    mkdir(full_path, 0777);
    strcpy(sentence, "257 Directory created.\n");
    free(full_path);
}

void CWD(char *sentence, int status, char *curr_path) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.\n");
        return;
    }

    char *new_path = sentence + 4;
    new_path[strlen(new_path) - 1] = 0;
    char *full_path = get_full_path(new_path);
    if (is_valid_path(full_path) == 0) {
        strcpy(sentence, "550 Unavailable path.\n");
        return;
    }

    strcpy(curr_path, full_path);
    strcpy(sentence, "250 Working directory changed.\n");
    free(full_path);
}

void PWD(char *sentence, int status, char *curr_path) {
    if (!is_arg_count(sentence, 0)) return;
    if (!(status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.\n");
        return;
    }
    
    strcpy(sentence, "257 Current working directory is ");
    strcat(sentence, curr_path);
    strcat(sentence, "\n");
}

void RMD(char *sentence, int status) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.\n");
        return;
    }

    char *dir_name = sentence + 4;
    dir_name[strlen(dir_name) - 1] = 0;
    char *full_path = get_full_path(dir_name);
    struct stat st = {0};
    if (stat(full_path, &st) == -1) {
        strcpy(sentence, "550 Directory not exists.\n");
        free(full_path);
        return;
    }

    if (rmdir(full_path) == -1) {
        strcpy(sentence, "550 Failed to remove the directory.\n");
        free(full_path);
        return;
    }

    strcpy(sentence, "250 Directory removed.\n");
    free(full_path);
    return;
}

void LIST(char *sentence, int status) {
    char *temp1 = strdup(sentence);
    char *temp2 = strdup(sentence);
    printf("temp1, temp2: %s %s\n", temp1, temp2);
    if ((is_arg_count(temp1, 0) == 0) && 
        (is_arg_count(temp2, 1) == 0)) {
        printf("temp1, temp2: %s %s\n", temp1, temp2);
        printf("WHY!!!!\n");
        free(temp1);
        free(temp2);
        return;
    }
    if (!(status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.\n");
        return;
    }
    if (!(status & STATUS_PORT_OR_PASV)) {
        strcpy(sentence, "503 Bad sequence of commands. LIST must be preceded by either a PORT command or a PASV command.\n");
        return;
    }

    char *full_path;
    strcpy(temp1, sentence);
    if (is_arg_count(temp1, 0))
        full_path = get_full_path(".");
    else {
        char *dir_name = sentence + 5;
        dir_name[strlen(dir_name) - 1] = 0;
        full_path = get_full_path(dir_name);
    }
    full_path[strlen(full_path) - 1] = 0;
    printf("full path:%s\n", full_path);
    free(temp1);
    free(temp2);

    struct dirent **name_list;
    int n = scandir(full_path, &name_list, NULL, alphasort);
    if (n == -1) {
        strcpy(sentence, "550 \n");
        return;
    }

    strcpy(sentence, "250-| Size | Name | Last modification |\n");
    char temp[100];
    struct stat st;
    while (n--) {
        stat(name_list[n]->d_name, &st);
        sprintf(temp, "250- %ld %s %s", st.st_size, name_list[n]->d_name, ctime(&st.st_mtime));
        strcat(sentence, temp);
        free(name_list[n]);
    }
    strcat(sentence, "250 End of directory.\n");
    free(name_list);
    free(full_path);
}

void RNFR(char *sentence, int *status, char *old_name) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(*status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.\n");
        return;
    }

    char *file_name = sentence + 5;
    file_name[strlen(file_name) - 1] = 0;
    char *full_name = get_full_path(file_name);
    if (is_valid_path(full_name) == 0) {
        strcpy(sentence, "550 File not exist.\n");
        return;
    }

    strcpy(old_name, full_name);
    strcpy(sentence, "350 OK. Pending for RNTO.\n");
    *status |= STATUS_RNFR;
    free(full_name);
}

void RNTO(char *sentence, int *status, char *old_name) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(*status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.\n");
        return;
    }
    if (!(*status & STATUS_RNFR)) {
        strcpy(sentence, "503 Bad sequence of commands. RNTO must be preceded by RNFR.\n");
        return;
    }

    char *new_name = sentence + 5;
    new_name[strlen(new_name) - 1] = 0;
    char *full_name = get_full_path(new_name);
    if (rename(old_name, full_name) != 0) {
        strcpy(sentence, "550 Unable to rename the file.\n");
        return;
    }

    *status &= ~STATUS_RNFR;
    strcpy(sentence, "250 File renamed successfully.\n");
    free(full_name);
}

void QUIT(char *sentence, int *status) {
    if (!is_arg_count(sentence, 0)) return;
    strcpy(sentence, "221-Thank you for using the FTP service.\n"
                     "221 Goodbye.\n");
    *status |= STATUS_QUIT;
}

void COMMAND_NOT_EXIST(char *sentence) {
    strcpy(sentence, "500 Command unrecognized.\n");
}

void *multi_run(void *data) {
	char sentence[8192];
    char curr_path[128];
    strcpy(curr_path, root);
    char old_name[128];
    int len;
    int connfd = *(int*)data;
    int status = STATUS_LOGIN; // STATUS_INIT

    while (1) {
        if (status & STATUS_QUIT) break;

	    len = read_from_client(connfd, sentence);
        if (len == -1) return NULL;

        // Deal with client request 
        if (strncmp(sentence, "USER", 4) == 0) 
            USER(sentence, &status);
        else if (strncmp(sentence, "PASS", 4) == 0)
            PASS(sentence, &status);
        else if (strncmp(sentence, "SYST" ,4) == 0)
            SYST(sentence, status);
        else if (strncmp(sentence, "TYPE", 4) == 0)
            TYPE(sentence, status);
        else if (strncmp(sentence, "PORT", 4) == 0)
            PORT(sentence, &status);
        else if (strncmp(sentence, "PASV", 4) == 0)
            PASV(sentence, &status);
        else if (strncmp(sentence, "RETR", 4) == 0)
            RETR(sentence, status);
        else if (strncmp(sentence, "STOR", 4) == 0)
            STOR(sentence, status);
        else if (strncmp(sentence, "MKD", 3) == 0)
            MKD(sentence, status);
        else if (strncmp(sentence, "CWD", 3) == 0)
            CWD(sentence, status, curr_path);
        else if (strncmp(sentence, "PWD", 3) == 0)
            PWD(sentence, status, curr_path);
        else if (strncmp(sentence, "RMD", 3) == 0)
            RMD(sentence, status);
        else if (strncmp(sentence, "LIST", 4) == 0)
            LIST(sentence, status);
        else if (strncmp(sentence, "RNFR", 4) == 0)
            RNFR(sentence, &status, old_name);
        else if (strncmp(sentence, "RNTO", 4) == 0)
            RNTO(sentence, &status, old_name);
        else if (strncmp(sentence, "QUIT", 4) == 0 ||
                 strncmp(sentence, "ABOR", 4) == 0)
            QUIT(sentence, &status);
        else
            COMMAND_NOT_EXIST(sentence);

        if (write_to_client(connfd, sentence, strlen(sentence)) == -1)
            return NULL;
    }
	close(connfd);
}



int main(int argc, char **argv) {
	int listenfd;
	struct sockaddr_in addr;
    int port = 21;

    // Parse arguments
    static struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"root", required_argument, 0, 'r'}
    };
    int option_index = 0;
    int opt;
    while (opt = getopt_long(argc, argv, "p:r:", long_options, &option_index)) {
        if (opt == -1) break;

        switch(opt) {
        case 'p':
            port = (int)strtol(optarg, (char **)NULL, 10);
            break;
        case 'r':
            strcpy(root, optarg);
            strcat(root, "/");
            break;
        default:
            break;
        }
    }


	if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = 6789;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	if (listen(listenfd, 10) == -1) {
		printf("Error listen(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

    printf("Listening...\n");

    char *initial_msg = "220 Anonymous FTP server ready.\n";
    int client_num = 0;
    int connfd[10];
    pthread_t threads[10];
	while (1) {
        int index = client_num;
		if ((connfd[index] = accept(listenfd, NULL, NULL)) == -1) {
			printf("Error accept(): %s(%d)\n", strerror(errno), errno);
			continue;
		}

        // Initial response message
        if (write_to_client(connfd[index], initial_msg, strlen(initial_msg)) == -1)
            return 1;

        pthread_create(&threads[index], NULL, multi_run, (void *)&connfd[index]);

        client_num++;
    }

	close(listenfd);
}

