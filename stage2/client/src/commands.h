#ifndef COMMANDS_H
#define COMMANDS_H
#include "utils.h"
#include <QThread>
#include <QProgressBar>

extern char root[];
extern int connfd;

class retr_thread:public QThread {
    Q_OBJECT
signals:
    void progress_signals(int n);
public slots:
    void progress_slots(int n);
public:
    retr_thread(char* sentence, int* status, conn *conn_info, char* filename);
    void run();
private:
    char* sentence;
    int* status;
    conn *conn_info;
    char* filename;
};

class stor_thread:public QThread {
    Q_OBJECT
signals:
    void progress_signals(int n, int m);
public slots:
    void progress_slots(int n, int m);
public:
    stor_thread(char* sentence, int* status, conn *conn_info);
    void run();
private:
    char* sentence;
    int* status;
    conn *conn_info;
};

void PORT(char *sentence, int *status, conn *info);
void RETR_PORT(char *sentence, int *status, conn *info);
void RETR_PASV(char *sentence, int *status, conn *info, char* filename, retr_thread* th);
void STOR_PORT(char *sentence, int *status, conn *info);
void STOR_PASV(char *sentence, int *status, conn *info, stor_thread *th);
void LIST_PORT(char *sentence, int *status, conn *info);
//void LIST_PASV(char *sentence, int *status, conn *info);
void PASV(char *sentence, int *status, conn *info);

#endif // COMMANDS_H
