#include "utils.h"
#include "commands.h"

char root[128] = "tmp/";
int connfd;

int main(int argc, char **argv) {
	char sentence[8192];
	conn conn_info;
	int port = 21;
	char ip[15] = "127.0.0.1";
    int status = STATUS_INIT;

    // Parse arguments - change from "-port" to "--por"
    char tmp[10];
    if (argc >= 3) {
        strcat(tmp, "-");
        strncat(tmp, argv[1], 4);
        strcpy(argv[1], tmp);
    }
    if (argc >= 5) {
        memset(tmp, 0, 10);
        strcat(tmp, "-");
        strncat(tmp, argv[3], 4);
        strcpy(argv[3], tmp);
    }

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

	connfd = new_conn_socket(ip, port);

	int p = read(connfd, sentence, 8192);
	sentence[p - 1] = '\0';
	printf("%s\r\n", sentence);

	while (1) {
        printf("$ ");
		fgets(sentence, 4096, stdin);

        sentence[p - 2] = '\0';
        if (write_to_server(connfd, sentence) == -1) {
            printf("Write to server failed.\n");
            break;
        }

		if (strncmp(sentence, "PORT", 4) == 0)
            PORT(sentence, &status, &conn_info);
		else if (strncmp(sentence, "STOR", 4) == 0 &&
			     status & STATUS_PORT)
            STOR_PORT(sentence, &status, &conn_info);
		else if (strncmp(sentence, "STOR", 4) == 0 &&
	             status & STATUS_PASV)
            STOR_PASV(sentence, &status, &conn_info);
		else if (strncmp(sentence, "RETR", 4) == 0 &&
			     status & STATUS_PORT)
            RETR_PORT(sentence, &status, &conn_info);
		else if (strncmp(sentence, "RETR", 4) == 0 && 
	             status & STATUS_PASV)
            RETR_PASV(sentence, &status, &conn_info);
		else if (strncmp(sentence, "LIST", 4) == 0 &&
			     status & STATUS_PORT)
            LIST_PORT(sentence, &status, &conn_info);
		else if (strncmp(sentence, "LIST", 4) == 0 && 
	             status & STATUS_PASV)
            LIST_PASV(sentence, &status, &conn_info);
		
        if (read_from_server(connfd, sentence) == -1) {
            printf("Read from server failed.\n");
            break;
        }

		if (strncmp(sentence, "227", 3) == 0)
            PASV(sentence, &status, &conn_info); 
		else if (strncmp(sentence,"221", 3) == 0) {
            printf("%s", sentence);
            break;
		}
	}

    close(connfd);
}
