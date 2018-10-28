#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

char root[128] = "./root_client/";
char *curr_path;
int status;
int len;

int connfd;

typedef enum {false, true} bool;

#define STATUS_INIT         (1<<0)
#define STATUS_PORT         (1<<3)
#define STATUS_PASV         (1<<4)

int read_from_server(int connfd, char* sentence) {
	int p = 0;
	while (1) {
		int n = read(connfd, sentence + p, 8191 - p);
		if (n < 0) {
			printf("Error read(): %s(%d)\n", strerror(errno), errno);
			return -1;
		} else if (n == 0) {
		    break;
		} else {
			p += n;
			if (sentence[p - 1] == '\n') {
				break;
			}
		}
	}
    if (!p) return 0;

	sentence[p - 1] = '\0';
	printf("%s\n", sentence);
    return 1;
}

int write_to_server(int connfd, char* sentence, int len) {
    int p = 0;
    while (p < len) {
    	int n = write(connfd, sentence + p, len + 1 - p);
    	if (n < 0) {
    		printf("Error write(): %s(%d)\n", strerror(errno), errno);
    		return -1;
    	} else {
    		p += n;
    	}			
    }
}

// Allocate new menory. Memory must be freed.
char* get_full_path(char *path) {
    char *temp = strdup(path);
    char *full_path = malloc(strlen(root) + strlen(temp));

    strcpy(full_path, root);
    strcat(full_path, temp);

    free(temp);
    return full_path;
}

int is_file_exist(char *full_path) {
    struct stat st;
    if (stat(full_path, &st) != 0)
        return false;

    return true;
}

void PORT(char *sentence, int *status, int *listenfd) {
    char *num = malloc(3);
    int port = 0;

    char *arg_str = malloc(strlen(sentence)-5);
    strcpy(arg_str, sentence + 5);
    arg_str[strlen(arg_str)-1] = '\0';

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

	if ((*listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
        free(num);
        free(arg_str);
        return;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = port;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(*listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
        free(num);
        free(arg_str);
		return;
	}

	if (listen(*listenfd, 10) == -1) {
		printf("Error listen(): %s(%d)\n", strerror(errno), errno);
        free(num);
        free(arg_str);
		return;
	}

    *status |= STATUS_PORT;

    free(num);
    free(arg_str);
}

void RETR_PORT(char *sentence, int *status, int listenfd) {
    if (write_to_server(connfd, sentence, len) == -1)
        return;

    int datafd;
    if ((datafd = accept(listenfd, NULL, NULL)) == -1) {
	    printf("Error accept(): %s(%d)\n", strerror(errno), errno);
	    return;
    }

    sentence[strlen(sentence) - 1] = '\0';
    char *file_name = sentence + 5;
    file_name[strlen(file_name) - 1] = '\0';

    // Only accept file name, not including path
    char *p = strrchr(file_name, '/');
    if (p)
        file_name = p + 1;

    char *full_name = get_full_path(file_name);
    printf("full_name: %s\n", full_name);

    FILE *fout = fopen(full_name, "wb");
    if (fout == NULL) {
        printf("Error fopen(): %s(%d)\n", strerror(errno), errno);
        free(full_name);
        return;
    }

    // Read file from server
    while(read(datafd, sentence, 8192)) {
        printf("Got it from server: %s\n", sentence);
        fputs(sentence, fout);
    }

    if (fclose(fout) != 0) {
        printf("Error fclose(): %s(%d)\n", strerror(errno), errno);
        free(full_name);
        return;
    }

    *status &= ~STATUS_PORT;

    close(datafd);
    close(listenfd);
    free(full_name);
}

void RETR_PASV(char *sentence, int *status, int datafd) {
    if (write_to_server(connfd, sentence, len) == -1)
        return;

    sentence[strlen(sentence) - 1] = '\0';
    char *file_name = sentence + 5;
    file_name[strlen(file_name) - 1] = '\0';

    // Only accept file name, not including path
    char *p = strrchr(file_name, '/');
    if (p)
        file_name = p + 1;

    char *full_name = get_full_path(file_name);
    printf("full_name: %s, sentence: %s\n", full_name, sentence);

    FILE *fout = fopen(full_name, "wb");
    if (fout == NULL) {
        printf("Error fopen(): %s(%d)\n", strerror(errno), errno);
        free(full_name);
        return;
    }

    // Read file from server
    while(read(datafd, sentence, 8192)) {
        printf("Got it from server: %s\n", sentence);
        fputs(sentence, fout);
    }

    if (fclose(fout) != 0) {
        printf("Error fclose(): %s(%d)\n", strerror(errno), errno);
        free(full_name);
        return;
    }

    *status &= ~STATUS_PASV;

    close(datafd);
    free(full_name);
}

void STOR_PORT(char *sentence, int *status, int listenfd) {
    if (write_to_server(connfd, sentence, len) == -1)
        return;

    int datafd;
    if ((datafd = accept(listenfd, NULL, NULL)) == -1) {
	    printf("Error accept(): %s(%d)\n", strerror(errno), errno);
	    return;
    }

    sentence[strlen(sentence) - 1] = 0;
    char *file_name = sentence + 5;
    file_name[strlen(file_name) - 1] = 0;

    char *full_name = get_full_path(file_name);
    if (!is_file_exist(full_name)){
        printf("File not exist.\n");
        free(full_name);
        return;
    }

    FILE *fin;
    fin = fopen(full_name, "rb");
    if (fin == NULL) {
        printf("Open file failed.\n");
        free(full_name);
        return;
    }

    while (fgets(sentence, 8192, (FILE*)fin) != NULL) {
        printf("FILE: %s", sentence);
        if (write(datafd, sentence, 8192) == -1) {
            printf("Upload file failed.\n");
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

void STOR_PASV(char *sentence, int *status, int datafd) {
    if (write_to_server(connfd, sentence, len) == -1)
        return;

    sentence[strlen(sentence) - 1] = 0;
    char *file_name = sentence + 5;
    file_name[strlen(file_name) - 1] = 0;

    char *full_name = get_full_path(file_name);
    if (!is_file_exist(full_name)){
        printf("File not exist.\n");
        free(full_name);
        return;
    }

    FILE *fin;
    fin = fopen(full_name, "rb");
    if (fin == NULL) {
        printf("Upload file failed.\n");
        free(full_name);
        return;
    }

    while (fgets(sentence, 8192, (FILE*)fin) != NULL) {
        printf("FILE: %s", sentence);
        if (write(datafd, sentence, 8192) == -1) {
            printf("Upload file failed.\n");
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

void PASV_OK(char *sentence, int *status, int *datafd) {
    char *num = malloc(3);
    char *ip = malloc(15);
    memset(ip, 0, 15);
    int port = 0;

    char *arg_str = strchr(sentence, '(') + 1;
    char *arg_end = strchr(sentence, ')');
    *arg_end = 0;

    int i = 0;
    while((num = strsep(&arg_str, ",")) != NULL) {
        int num_int = (int)strtol(num, (char **)NULL, 10);
        if (num_int > 256) {
            strcpy(sentence, "501 Wrong parameters or arguments\n");
            free(num);
            free(ip);
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

	if ((*datafd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
        free(num);
        free(ip);
		return;
	}
    printf("PASV port: %d, ip: %s\n", port, ip);

    struct sockaddr_in data_addr;
	memset(&data_addr, 0, sizeof(data_addr));
	data_addr.sin_family = AF_INET;
	data_addr.sin_port = port;

	if (inet_pton(AF_INET, ip, &data_addr.sin_addr) <= 0) {
		printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
        free(num);
        free(ip);
		return;
	}

    if (connect(*datafd, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
        printf("Error connect(): %s(%d)\n", strerror(errno), errno);
        free(num);
        free(ip);
        return;
    }

    *status |= STATUS_PASV;

    printf("Connect OK!\n");
    free(num);
    free(ip);
}

int main(int argc, char **argv) {
	char sentence[8192];
    int port = 21;
	int p;
    status = STATUS_INIT;

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

	if ((connfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

    struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = port;

	if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) <= 0) {
		printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	if (connect(connfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		printf("Error connect(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

    // Read initial message
    if (read_from_server(connfd, sentence) == -1)
        return 1;

    while (true) {
        printf("$ ");
	    fgets(sentence, 4096, stdin);

	    len = strlen(sentence);
	    sentence[len] = '\n';
	    sentence[len + 1] = '\0';

        curr_path = malloc(128);
        strcpy(curr_path, root);

        int listenfd, datafd;

        // Preprocessing
        if (strncmp(sentence, "PORT", 4) == 0)
            PORT(sentence, &status, &listenfd);
        else if ((strncmp(sentence, "RETR", 4) == 0) &&
            (status & STATUS_PORT))
            RETR_PORT(sentence, &status, listenfd);
        else if ((strncmp(sentence, "RETR", 4) == 0) &&
            (status & STATUS_PASV))
            RETR_PASV(sentence, &status, datafd);
        else if ((strncmp(sentence, "STOR", 4) == 0) &&
            (status & STATUS_PORT))
            STOR_PORT(sentence, &status, listenfd);
        else if ((strncmp(sentence, "STOR", 4) == 0) && 
            (status & STATUS_PASV))
            STOR_PASV(sentence, &status, datafd);

        if (write_to_server(connfd, sentence, len) == -1)
            break;

        if (read_from_server(connfd, sentence) == -1)
            break;

        // Postprocessing
        if (strncmp(sentence, "227", 3) == 0)
            PASV_OK(sentence, &status, &datafd);
        else if (strncmp(sentence, "221", 3) == 0)  // QUIT command
            break;
        else if (strchr(sentence, '>')) {           // CWD command
            curr_path = strchr(sentence, '>') + 1;
            printf("curr_path: %s\n", curr_path);
        }
    }

	close(connfd);

	return 0;
}
