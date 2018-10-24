#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <errno.h>

#include <string.h>
#include <memory.h>
#include <stdio.h>

int read_from_server(int sockfd, char* sentence) {
	int p = 0;
	while (1) {
		int n = read(sockfd, sentence + p, 8191 - p);
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
	sentence[p - 1] = '\0';
	printf("%s\n", sentence);
}

int write_to_server(int sockfd, char* sentence, int len) {
    int p = 0;
    while (p < len) {
    	int n = write(sockfd, sentence + p, len + 1 - p);
    	if (n < 0) {
    		printf("Error write(): %s(%d)\n", strerror(errno), errno);
    		return -1;
    	} else {
    		p += n;
    	}			
    }
}

int main(int argc, char **argv) {
	int sockfd;
	struct sockaddr_in addr;
	char sentence[8192];
	int len;
	int p;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = 6789;
	if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) <= 0) {
		printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		printf("Error connect(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

    // read initial message
    if (read_from_server(sockfd, sentence) == 1)
        return 1;

    while (1) {
	    fgets(sentence, 4096, stdin);
	    len = strlen(sentence);
	    sentence[len] = '\n';
	    sentence[len + 1] = '\0';
	    
        if (write_to_server(sockfd, sentence, len) == -1)
            return 1;

        if (read_from_server(sockfd, sentence) == -1)
            return 1;

        if (strncmp(sentence, "221", 3) == 0) // QUIT command
            break;
    }

	close(sockfd);

	return 0;
}
