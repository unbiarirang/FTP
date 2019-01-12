#include "client.h"
#include "utils.h"
#include "commands.h"
#include "qdebug.h"


int client(int argc, char **argv)
{
    char sentence[8192];
    conn conn_info;
    int port = 5000;
    char ip[128] = "123.206.96.15";
    int status = STATUS_INIT;
    long long int readByte;
    WSADATA s;

    if (WSAStartup(MAKEWORD(2, 2), &s) != 0)
    {
        qDebug("Init Windows Socket Failed! Error: %d\n", GetLastError());
        getchar();
        return -1;
    }

    connfd = new_conn_socket(ip, port);

    int p = recv(connfd, sentence, 8192, 0);
    sentence[p - 1] = '\0';
    qDebug("%s\r\n", sentence);

//    while (1) {
//        if (write_to_server(connfd, sentence) == -1) {
//            printf("Write to server failed.\n");
//            break;
//        }

//        if (strncmp(sentence, "PORT", 4) == 0)
//            PORT(sentence, &status, &conn_info);
//        else if (strncmp(sentence, "STOR", 4) == 0 &&
//                 status & STATUS_PORT)
//            STOR_PORT(sentence, &status, &conn_info);
//        else if (strncmp(sentence, "STOR", 4) == 0 &&
//                 status & STATUS_PASV)
//            STOR_PASV(sentence, &status, &conn_info);
//        else if (strncmp(sentence, "RETR", 4) == 0 &&
//                 status & STATUS_PORT)
//            RETR_PORT(sentence, &status, &conn_info);
//        else if (strncmp(sentence, "RETR", 4) == 0 &&
//                 status & STATUS_PASV)
//            RETR_PASV(sentence, &status, &conn_info);
//        else if (strncmp(sentence, "LIST", 4) == 0 &&
//                 status & STATUS_PORT)
//            LIST_PORT(sentence, &status, &conn_info);
//        else if (strncmp(sentence, "LIST", 4) == 0 &&
//                 status & STATUS_PASV)
//            LIST_PASV(sentence, &status, &conn_info);

//        if (read_from_server(connfd, sentence) == -1) {
//            printf("Read from server failed.\n");
//            break;
//        }

//        if (strncmp(sentence, "227", 3) == 0)
//            PASV(sentence, &status, &conn_info);
//        else if (strncmp(sentence,"221", 3) == 0) {
//            printf("%s", sentence);
//            break;
//        }
//    }

//    closesocket(connfd);
}
