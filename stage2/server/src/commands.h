extern char root[];

void USER(char *sentence, int* status) {
    if (!is_arg_count(sentence, 1)) return;

    get_arg(sentence, 5);
    if (strcmp(sentence, "anonymous") != 0) {
        strcpy(sentence, "530 Wrong user name");
        return;
    }

    strcpy(sentence, "331 user name ok, send your complete e-mail address as password.");
    *status |= STATUS_USER;
}

void PASS(char *sentence, int* status) {
    if (!is_arg_count(sentence, 1)) return;

    if (!(*status & STATUS_USER)) {
        strcpy(sentence, "503 Bad sequence of commands. PASS must be preceded by USER.");
        return;
    }

    get_arg(sentence, 5);
    if (strcmp(sentence, "anonymous@") != 0) {
        strcpy(sentence, "530 wrong password");
        return;
    }

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
        "230 Guest login ok, access restrictions apply.");

    *status |= STATUS_LOGIN;
    *status &= ~STATUS_USER;
}

void SYST(char *sentence, int status) {
    if (!is_arg_count(sentence, 0)) return;
    if (!(status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.");
        return;
    }

    strcpy(sentence, "215 UNIX Type: L8");
}

void TYPE(char *sentence, int status) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.");
        return;
    }

    if (strncmp(sentence, "TYPE I", 6) == 0)
        strcpy(sentence, "200 Type set to I.");
    else
        sprintf(sentence, "504 Type not supported.\r\n");
}

void PORT(char *sentence, int *status, conn *info) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(*status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.");
        return;
    }

    char *num = malloc(3);
    char *ip = malloc(15);
    int port = 0;
    int i = 0;
    char *arg_str = sentence + 5;

    while((num = strsep(&arg_str, ",")) != NULL) {
        int num_int = (int)strtol(num, (char **)NULL, 10);
        if (num_int > 256) {
            strcpy(sentence, "501 Wrong parameters or arguments");
            free(num);
            free(ip);
            return;
        }

        if (i < 3) {
            strcat(ip, num);
            strcat(ip, ".");
        } else if (i == 3)
            strcat(ip, num);
        else if (i == 4)
            port = num_int * 256;
        else if (i == 5)
            port += num_int;
        i++;
    }

    strcpy(info->ip_address, ip);
    info->port = port;

    *status |= STATUS_PORT;

    printf("port num: %d ip address: %s\n", port, ip);
    strcpy(sentence, "200 Ready for data transmission.");
    free(num);
    free(ip);
}

int available_port[3] = {2000, 3000, 4000};
int port_index = 0;
void PASV(char *sentence, int *status, int connfd, int *listenfd, int *datafd, conn *info) {
    if (!is_arg_count(sentence, 0)) return;
    if (!(*status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.");
        return;
    }

    srand(time(NULL));
    int port =  rand() % 45535 + 20000;    // FIXME: temp
    //int port = available_port[port_index % 3]; // FIXME: Available port
    //port_index++;
    int port1 = port / 256;
    int port2 = (port - (port1 * 256));
    strcpy(info->ip_address, "123.206.96.15");
    info->port = port;

    sprintf(sentence, "227 Entering Passive Mode (123,206,96,15,%d,%d)\r\n", port1, port2);
    int len = strlen(sentence);
    write(connfd, sentence, len);
    
    *listenfd = new_listen_socket(port);
    if ((*datafd= accept(*listenfd, NULL, NULL)) == -1) {
    	printf("Error accept(): %s(%d)", strerror(errno), errno);
    	return;
    }

    *status |= STATUS_PASV;
}

void RETR(char *sentence, int *status, int connfd, int listenfd, int datafd, conn *info, int *transfer_files, int *transfer_bytes, int *startByte) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(*status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.");
        return;
    }
    if (!(*status & STATUS_PORT) && !(*status & STATUS_PASV)) {
        strcpy(sentence, "503 Bad sequence of commands. RETR must be preceded by either a PORT command or a PASV command.");
        return;
    }

    long long int readByte = 0;
    if (*status & STATUS_REST) readByte = *startByte;
    printf("readByte: %lld\n", readByte);

    printf("===sentence: %s, len: %d\n", sentence, strlen(sentence));
    //get_arg(sentence, 5);
    get_first_arg(sentence);

    //long long int readByte = 0;
    //char readByteStr[128];
    //if (is_arg_count2(sentence, 2)) {
    //    get_second_arg(sentence, readByteStr);
    //    get_first_arg(sentence);
    //    printf("first arg = %s, second arg = %s\n", sentence, readByteStr);
    //    readByte = (long long int)strtol(readByteStr, (char **)NULL, 10);
    //}
    //else
    //    get_arg(sentence, 5);


    char m_sentence[8192];
    char file_contents[8192];
	int len = strlen(sentence);

    printf("===sentence: %s, len: %d\n", sentence, len);
    printf("filename: %s\n", sentence);
	FILE *fp = fopen(sentence, "rb");
	if (fp == NULL) {
		strcpy(sentence, "550 file dose not exist");
        return;
	}

	fseek(fp, 0, SEEK_END);
	long long int file_size = ftell(fp);
    fseek(fp, readByte, SEEK_SET); 
	
    if (*status & STATUS_PORT) {
        memset(m_sentence, 0, strlen(m_sentence));
		sprintf(m_sentence, "150 Opening BINARY mode data connection for %s(%lld bytes).\r\n", sentence, file_size);					
		int sockfd = new_conn_socket(info->ip_address, info->port);
		len = strlen(m_sentence);
		int n = write(connfd, m_sentence, len);
		while (!feof(fp)) {
			len = fread(file_contents, 1, sizeof(file_contents), fp);
			n = write(sockfd, file_contents, len);
		}
		*transfer_files++;
		*transfer_bytes = *transfer_bytes + file_size;
		close(sockfd);
	}
	else if (*status & STATUS_PASV) {
        memset(m_sentence, 0, strlen(m_sentence));
		sprintf(m_sentence, "150 Opening BINARY mode data connection for %s(%d bytes).\r\n", sentence, file_size);
        memset(sentence, 0, strlen(sentence));
		len = strlen(m_sentence);
		int n = write(connfd, m_sentence, len);

		while (!feof(fp)) {
			len = fread(file_contents, 1, sizeof(file_contents), fp);
			n = write(datafd, file_contents, len);
		}

		*transfer_files++;
		*transfer_bytes = *transfer_bytes + file_size;

		close(listenfd);
		close(datafd);
	}
	sleep(1);

    strcpy(sentence, "226 Transfer successful.");

    *startByte = 0;
    *status &= ~STATUS_REST;
    *status &= ~STATUS_PORT;
    *status &= ~STATUS_PASV;

	fclose(fp);
}

void STOR(char *sentence, int *status, int connfd, int listenfd, int datafd, conn *info, int *transfer_files, int *startByte) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(*status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.");
        return;
    }
    if (!(*status & STATUS_PORT) && !(*status & STATUS_PASV)) {
        strcpy(sentence, "503 Bad sequence of commands. STOR must be preceded by either a PORT command or a PASV command.");
        return;
    }
	if (!strcmp(sentence, "STOR ")) {
	    strcpy(sentence, "550 You should input file name.");
        return;
    }

    long long int writeByte = 0;
    if (*status & STATUS_REST) writeByte = *startByte;

    get_first_arg(sentence);

    //long long int writeByte = 0;
    //char writeByteStr[128];
    //if (is_arg_count2(sentence, 2)) {
    //    get_second_arg(sentence, writeByteStr);
    //    get_first_arg(sentence);
    //    printf("first arg = %s, second arg = %s\n", sentence, writeByteStr);
    //    writeByte = (long long int)strtol(writeByteStr, (char **)NULL, 10);
    //}
    //else
    //    get_arg(sentence, 5);

    printf("writeByte: %lld\n", writeByte);

    char m_sentence[8192];
    char file_contents[8192];

	if (*status & STATUS_PORT) {
		int sockfd = new_conn_socket(info->ip_address, info->port);
        memset(m_sentence, 0, strlen(m_sentence));
		sprintf(m_sentence, "150 Opening BINARY mode data connection for %s.\r\n", sentence);
		int len = strlen(m_sentence);
		write(connfd, m_sentence, len);
		FILE *fp = fopen(sentence, "wb");

        int n;
		while(true) {
			n = read(sockfd, file_contents, 8192);
			if (n == 0) {
				strcpy(sentence, "226 Transfer complete.");
				break;
			}
			fwrite(file_contents, 1, n, fp);
		}
		*transfer_files++;
		fclose(fp);
		close(sockfd);
	}
	else if (*status & STATUS_PASV) {
        memset(m_sentence, 0, strlen(m_sentence));
		sprintf(m_sentence, "150 Opening BINARY mode data connection for %s.\r\n", sentence);
		int len = strlen(m_sentence);
		write(connfd, m_sentence, len);

		FILE *fp;
        if (*status & STATUS_REST) fp = fopen(sentence, "r+b");
        else fp = fopen(sentence, "wb");

        if (fp == NULL)
            printf("fopen failed\n");
        rewind(fp);
        fseek(fp, writeByte, SEEK_SET); 

        int n;
		while(true) {
			n = read(datafd, file_contents, 8192);
			if (n == 0) {
				strcpy(sentence, "226 Transfer complete.");
				break;
			}
			fwrite(file_contents, 1, n, fp);
		}

		*transfer_files++;
		fclose(fp);
		close(listenfd);
		close(datafd);
	}

    *startByte = 0;
    *status &= ~STATUS_REST;
    *status &= ~STATUS_PORT;
    *status &= ~STATUS_PASV;
}

void MKD(char *sentence, int status) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.");
        return;
    }

    printf("****sentence: %s\n", sentence);
	get_arg(sentence, 4);
    printf("****arg: %s\n", sentence);
	mkdir(sentence, S_IRWXU);

    strcpy(sentence, "257 Directory created.");
}

void CWD(char *sentence, int status) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.");
        return;
    }

	get_arg(sentence, 4);
    printf("****arg: %s\n", sentence);
	chdir(sentence);

    sprintf(sentence, "250 Working directory changed. Now at %s.\r\n", sentence);
}

void PWD(char *sentence, int status) {
    if (!is_arg_count(sentence, 0)) return;
    if (!(status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.");
        return;
    }
    
	char curr_path[128];
	memset(sentence, 0, sizeof(sentence));
	getcwd(curr_path, sizeof(curr_path));
    sprintf(sentence, "257 Current working directory is %s\r\n",
            curr_path);
}

void RMD(char *sentence, int status) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.");
        return;
    }

	char path[100];
	get_arg(sentence, 4);
    printf("****arg: %s\n", sentence);
	strcpy(path, sentence);
	rmdir(path);

    strcpy(sentence, "250 Directory removed.");
}

void LIST(char *sentence, int *status, int connfd, int listenfd, int datafd, conn *info, char *curr_path) {
    if (strcmp(sentence, "LIST") != 0 && 
        strncmp(sentence, "LIST ", 5) != 0) {
        strcpy(sentence, "501 Wrong parameters or arguments.\n");
        return;
    }
    if (!(*status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.");
        return;
    }
    if (!(*status & STATUS_PORT) && !(*status & STATUS_PASV)) {
        strcpy(sentence, "503 Bad sequence of commands. RETR must be preceded by either a PORT command or a PASV command.");
        return;
    }

    if (strncmp(sentence, "LIST ", 5) == 0) {
        char m_sentence[8192];
	    get_arg(sentence, 5);
	    int len = strlen(sentence);

	    FILE *fp = fopen(sentence, "rb");
	    if (fp == NULL) {
	    	strcpy(sentence, "550 file dose not exist");
	    }
	    else {
	    	fseek(fp, 0, SEEK_END);
	    	int file_size = ftell(fp);
	    	fseek(fp, 0, SEEK_SET);
	    	
            if (*status & STATUS_PORT) {
	    		strcpy(m_sentence, "150 Opening BINARY mode data connection for List.");					
	    		int sockfd = new_conn_socket(info->ip_address, info->port);
	    		len = strlen(m_sentence);
	    		int n = write(connfd, m_sentence, len);

                sprintf(m_sentence, "250-Name: %s, Size: %d bytes.\r\n", sentence, file_size);
                strcpy(sentence, m_sentence);
                strcat(sentence, "250 end");

	    		fclose(fp);
	    		close(sockfd);
	    	}
	    	else if (*status & STATUS_PASV) {
	    		strcpy(m_sentence, "150 Opening BINARY mode data connection for List.");					
	    		len = strlen(m_sentence);
	    		int n = write(connfd, m_sentence, len);

                sprintf(m_sentence, "250-Name: %s, Size: %d bytes.\r\n", sentence, file_size);
                strcpy(sentence, m_sentence);
                strcat(sentence, "250 end");

	    		fclose(fp);
	    		close(listenfd);
	    		close(datafd);
	    	}
	    	else {
	    		strcpy(sentence,"425 Please input PASV or PORT first.");
	    	}
	    	sleep(1);
	    }
    } else {
        char m_sentence[8192];
        char file_contents[8192];
	    int len = strlen(sentence);
	    	
        if (*status & STATUS_PORT) {
	        strcpy(m_sentence, "150 Opening BINARY mode data connection for List.");					
	    	int sockfd = new_conn_socket(info->ip_address, info->port);
	    	len = strlen(m_sentence);
	    	int n = write(connfd, m_sentence, len);

            struct dirent **name_list;
            n = scandir(curr_path, &name_list, NULL, alphasort);

            // Print directory info
            strcpy(sentence, "");
            struct stat st;
            char *temp = malloc(256);
            while (n--) {
                stat(name_list[n]->d_name, &st);
                sprintf(temp, "%s\r\n", name_list[n]->d_name);
                strcat(sentence, temp);
                free(name_list[n]);
            }
            strcat(sentence, "250 end of directory");
	    	close(sockfd);
	    }
	    else if (*status & STATUS_PASV) {
	        strcpy(m_sentence, "150 Opening BINARY mode data connection for List.");					
	    	len = strlen(m_sentence);
	    	int n = write(connfd, m_sentence, len);

            struct dirent **name_list;
            n = scandir(curr_path, &name_list, NULL, alphasort);

            // Print directory info
            strcpy(sentence, "");
            struct stat st;
            char *temp = malloc(256);
            while (n--) {
                stat(name_list[n]->d_name, &st);
                sprintf(temp, "%d %s\r\n", st.st_size, name_list[n]->d_name);
                strcat(sentence, temp);
                free(name_list[n]);
            }
            strcat(sentence, "250 end of directory");
	        write(datafd, sentence, strlen(sentence));

	    	close(listenfd);
	    	close(datafd);
	    }
	    else {
	    	strcpy(sentence,"425 Please input PASV or PORT first.");
	    }
	    sleep(1);
    }

    *status &= ~STATUS_PORT;
    *status &= ~STATUS_PASV;

    strcpy(sentence, "226 Transfer successful.");
}

void RNFR(char *sentence, int *status, char *old_name) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(*status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.");
        return;
    }

	get_arg(sentence,5);
    printf("****arg: %s\n", sentence);
	memset(old_name, 0, sizeof(old_name));
	strcpy(old_name, sentence);

    *status |= STATUS_RNFR;

    strcpy(sentence, "350 OK. Pending for RNTO.");
}

void RNTO(char *sentence, int *status, char *old_name) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(*status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.");
        return;
    }
    if (!(*status & STATUS_RNFR)) {
        strcpy(sentence, "503 Bad sequence of commands. RNTO must be preceded by RNFR.");
        return;
    }

	get_arg(sentence, 5);
    printf("****arg: %s\n", sentence);
	char new_name[128];
	strcpy(new_name, sentence);

	rename(old_name, new_name);

    *status &= ~STATUS_RNFR;

    strcpy(sentence, "250 File renamed successfully.");
}

void REST(char *sentence, int *status, int *startByte) {
    if (!is_arg_count(sentence, 1)) return;
    if (!(*status & STATUS_LOGIN)) {
        strcpy(sentence, "530 Need login.");
        return;
    }

	get_arg(sentence, 5);
    printf("RESUME from here: %s\n", sentence);
    *startByte = (int)strtol(sentence, (char **)NULL, 10);

    *status |= STATUS_REST;
    sprintf(sentence, "350 Restart position accepted(%lld).\r\n", *startByte);
}

void QUIT(char *sentence, int *status, int transfer_files, int transfer_bytes) {
    if (!is_arg_count(sentence, 0)) return;

    *status |= STATUS_QUIT;

    sprintf(sentence, "221-Thank you for using the FTP service.\n"
                     "221-You have transferred %d bytes in %d files.\n"
                     "221 Goodbye.\r\n", transfer_bytes, transfer_files);
}

void COMMAND_NOT_EXIST(char *sentence) {
    strcpy(sentence, "500 Command unrecognized.");
}
