#include "utils.h"
#include "commands.h"

char root[128] = "/tmp";

int main(int argc, char *argv[]) {
	int g_port = 21;
	int g_listenfd, g_connfd;
	char sentence[8192];
	user user_table[MAX_CLIENT_NUM];

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
        {"por", required_argument, 0, 'p'},
        {"roo", required_argument, 0, 'r'}
    };
    int option_index = 0;
    int opt;
    while (opt = getopt_long(argc, argv, "p:r:", long_options, &option_index)) {
        if (opt == -1) break;

        switch(opt) {
        case 'p':
            g_port = (int)strtol(optarg, (char **)NULL, 10);
            break;
        case 'r':
            strcpy(root, optarg);
            strcat(root, "/");
            break;
        default:
            break;
        }
    }

	chdir(root);

	for (int i = 0; i < MAX_CLIENT_NUM; i++) {
		strcpy(user_table[i].id,"anonymous");
		strcpy(user_table[i].pw, "anonymous@");
		user_table[i].is_online = 0;
	}

    g_listenfd = new_listen_socket(g_port);
    printf("Listening...\n");

	int pid;
	while (true) {
		if ((g_connfd = accept(g_listenfd, NULL, NULL)) == -1) {
			printf("Error accept(): %s(%d)\n", strerror(errno), errno);
			continue;
		}

		for (int i = 0; i < MAX_CLIENT_NUM; i++) {
			if (waitpid(user_table[i].pid, NULL, WNOHANG))
				user_table[i].is_online = 0;
		}

		for (int i = 0; i < MAX_CLIENT_NUM; i++) {
			if (!user_table[i].is_online) {
				user_table[i].is_online = 1;
				pid = i;
				break;
			}
			if (i == MAX_CLIENT_NUM - 1) {
				printf("Server is busy.\n");
				pid = -1;
			}
		}
		if (pid == -1) {
			strcpy(sentence, "Server is busy.\n");
			continue;
		}

		strcpy(sentence, "220 Anonymous FTP server ready\r\n");
		int len = strlen(sentence);
		write(g_connfd, sentence, len);
		printf("USER %d connected\n", pid);

		user_table[pid].pid = fork();

        int status = STATUS_INIT,
	        transfer_bytes = 0,
	        transfer_files = 0,
	        listenfd, 
            connfd;
	    conn conn_info;
	    char origin_name[128];

		if (user_table[pid].pid == 0) {
			while (true) {
                if (read_from_client(g_connfd, sentence) == -1)
                    continue;
				printf("%s\n", sentence);
			
                if (strncmp("USER", sentence, 4) == 0)
                    USER(sentence, &status);
                else if (strncmp("PASS", sentence, 4) == 0)
                    PASS(sentence, &status);
				else if (strcmp("SYST", sentence) == 0)
                    SYST(sentence, status);
				else if (strncmp("TYPE", sentence, 4) == 0)
                    TYPE(sentence, status);
				else if (strncmp("PORT", sentence, 4) == 0)
                    PORT(sentence, &status, &conn_info);
				else if (strcmp("PASV", sentence) == 0) {
                    PASV(sentence, &status, g_connfd, &listenfd, &connfd, &conn_info); 
					continue;
				}
				else if (strncmp("RETR", sentence, 4) == 0)
                    RETR(sentence, &status, g_connfd, listenfd, connfd, &conn_info, &transfer_files, &transfer_bytes);
				else if (strncmp("STOR", sentence, 4) == 0)
                    STOR(sentence, &status, g_connfd, listenfd, connfd, &conn_info, &transfer_files);
				else if (strcmp(sentence, "QUIT") == 0 || strcmp(sentence, "ABOR") == 0)
					QUIT(sentence, &status, transfer_files, transfer_bytes);
				else if (strncmp("MKD", sentence, 3) == 0)
                    MKD(sentence, status);
				else if (strncmp("PWD",sentence,3) == 0)
                    PWD(sentence, status);
				else if(strncmp("CWD", sentence, 3) == 0)
                    CWD(sentence, status);
				else if (strncmp("RMD", sentence, 3) == 0)
                    RMD(sentence, status);
				else if (strncmp("RNFR",sentence, 4) == 0)
                    RNFR(sentence, &status, origin_name);
				else if (strncmp("RNTO",sentence, 4) == 0)
                    RNTO(sentence, &status, origin_name);
                else if (strncmp("LIST", sentence, 4) == 0)
                    LIST(sentence, &status, g_connfd, listenfd, connfd, &conn_info, ".");
				else
                    COMMAND_NOT_EXIST(sentence);
                    
                if (status & STATUS_QUIT) {
                    int len = strlen(sentence);
					int n = write(g_connfd, sentence, len);
			    	close(g_connfd);
					return 0;
                }
		
                if (write_to_client(g_connfd, sentence) == -1)
                    return 1;
			}
		}
	}
	close(g_listenfd);
}

