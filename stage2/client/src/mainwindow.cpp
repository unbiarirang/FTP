#include "mainwindow.h"
#include "ui_mainwindow.h"

char root[128] = "tmp";
int connfd;
int g_datafd;
conn conn_info;
int status = STATUS_INIT;
long long int readByte;

QProgressBar *pb;
SortFilter *proxyModel = new SortFilter();

MainWindow::MainWindow(int argc, char *argv[], QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->btn_refresh_local->setVisible(false);
    ui->btn_refresh_remote->setVisible(false);

    QDirModel *model = new QDirModel();

    proxyModel->setSourceModel(model);
    ui->tree_local->setModel(proxyModel);
    qDebug() << QCoreApplication::applicationDirPath() + QString("/") + QString(root);
    ui->tree_local->setRootIndex(proxyModel->mapFromSource(model->index(QCoreApplication::applicationDirPath() + QString("/") + QString(root))));

    pb = ui->progressbar;
}

MainWindow::~MainWindow()
{
    delete ui;
}

SortFilter::SortFilter() {}
SortFilter::~SortFilter() {}

bool SortFilter::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    if(!sourceModel()) return false;

    QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
    QDirModel *model = static_cast<QDirModel*>(sourceModel());
    //QString str = model->fileName(index);

    if (model->fileInfo(index).isDir()) return true;
    else if (model->fileInfo(index).isFile()) return true;
    return false;
}

void MainWindow::on_btn_login_clicked()
{
    if (!ui->input_ip || !ui->input_port) {
        QMessageBox::information (0, "Login",
                                     "Please input ip and port.");
        return;
    }
    const char *ip = ui->input_ip->text().toStdString().c_str();
    int port = ui->input_port->text().toInt();

    char sentence[8192];
    WSADATA s;

    if (WSAStartup(MAKEWORD(2, 2), &s) != 0)
    {
        qDebug("Init Windows Socket Failed! Error: %d\n", GetLastError());
        getchar();
        return;
    }

    connfd = new_conn_socket(ip, port);

    int p = recv(connfd, sentence, 8192, 0);
    sentence[p - 1] = '\0';
    qDebug("%s\n", sentence);

    if (strncmp(sentence, "220", 3) != 0) {
        QMessageBox::information (0, "Login",
                                     "Failed to connect the server.");
        return;
    }

    bool ok;
    QString input_name, input_pw;
    const char* username, *password;

    // Ask for username
    input_name = QInputDialog::getText(0, "Login",
                                         "Usrename:", QLineEdit::Normal,
                                         "", &ok);
    if (!ok || input_name.isEmpty()) {
        QMessageBox::information (0, "Login",
                                     "Please input user name.");
        return;
    }

    username = input_name.toStdString().c_str();

    strcpy(sentence, "USER ");
    strcat(sentence, username);
    strcat(sentence, "\n");

    if (write_to_server(connfd, sentence) == -1) {
        printf("Write to server failed.\n");
        return;
    }

    if (read_from_server(connfd, sentence) == -1) {
        printf("Read from server failed.\n");
        return;
    }

    if (strncmp(sentence, "331", 3) != 0) {
        QMessageBox::information (0, "Login",
                                     "Wrong user name.");
        return;
    }

    // Ask for password
    input_pw = QInputDialog::getText(0, "Login",
                                       "Password:", QLineEdit::Normal,
                                       "", &ok);

    if (!ok || input_name.isEmpty()) {
        QMessageBox::information (0, "Login",
                                     "Please input password.");
        return;
    }

    qDebug(sentence);
    password = input_pw.toStdString().c_str();

    strcpy(sentence, "PASS ");
    strcat(sentence, password);
    strcat(sentence, "\n");

    if (write_to_server(connfd, sentence) == -1) {
        printf("Write to server failed.\n");
        return;
    }

    if (read_from_server(connfd, sentence) == -1) {
        printf("Read from server failed.\n");
        return;
    }

    if (strncmp(sentence, "230", 3) != 0) {
        QMessageBox::information (0, "Login",
                                     "Wrong password.");
        return;
    }

    QMessageBox::information (0, "Login",
                                 "Login Success");

    refresh_local_list();
    refresh_remote_list();
}

void MainWindow::refresh_local_list() {
    ui->list_local->clear();
    QDir dir(root);

    QFileInfoList list = dir.entryInfoList();
    for (int i = 0; i < list.size(); ++i) {
        QFileInfo fileInfo = list.at(i);
        if (fileInfo.fileName().at(0) == '.') continue;
        ui->list_local->addItem(fileInfo.fileName());
    }

    //Tree view
    QDirModel *model = new QDirModel();
    proxyModel->setSourceModel(model);
    ui->tree_local->setModel(proxyModel);
    ui->tree_local->setRootIndex(proxyModel->mapFromSource(model->index(QCoreApplication::applicationDirPath() + QString("/") + QString(root))));
}

void MainWindow::refresh_remote_list() {
    ui->list_remote->clear();
    char *sentence = (char*)malloc(8192);
    strcpy(sentence, "PASV\n");

    if (write_to_server(connfd, sentence) == -1) {
        printf("Write to server failed.\n");
        return;
    }

    if (read_from_server(connfd, sentence) == -1) {
        printf("Read from server failed.\n");
        return;
    }

    if (strncmp(sentence, "227", 3) != 0){
        QMessageBox::information (0, "Error",
                                     "Error");
        return;
    }
    PASV(sentence, &status, &conn_info);

    strcpy(sentence, "LIST\n");
    if (write_to_server(connfd, sentence) == -1) {
        printf("Write to server failed.\n");
        return;
    }
    LIST_PASV(sentence, &status, &conn_info);
    if (read_from_server(connfd, sentence) == -1) {
        printf("Read from server failed.\n");
        return;
    }
    qDebug(sentence);
}

void MainWindow::closeEvent( QCloseEvent *event )
{
    char sentence[8192];
    strcpy(sentence, "QUIT\n");

    if (write_to_server(connfd, sentence) == -1) {
        printf("Write to server failed.\n");
        return;
    }

    if (read_from_server(connfd, sentence) == -1) {
        printf("Read from server failed.\n");
        return;
    }

    qDebug(sentence);
}

void MainWindow::LIST_PASV(char *sentence, int *status, conn *info) {
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
    char* filename = (char*)malloc(256);
    while(true) {
        n = recv(datafd, sentence, 8191, 0);
        if (n == 0) break;

        printf("LIST_PASV: %s\n", sentence);

        line = strtok(sentence, "\n");
        while(line != NULL) {
            if (line[0] == '.') {
                line = strtok(NULL, "\n");
                continue;
            }
            ui->list_remote->addItem(QString(line));
            line = strtok(NULL, "\n");
        }
    }

    *status &= ~STATUS_PASV;

    closesocket(datafd);
}

void MainWindow::on_list_remote_itemDoubleClicked(QListWidgetItem *item)
{
    char *sentence = (char*)malloc(8192);

    strcpy(sentence, "TYPE I\n");
    if (write_to_server(connfd, sentence) == -1) {
        printf("Write to server failed.\n");
        return;
    }
    if (read_from_server(connfd, sentence) == -1) {
        printf("Read from server failed.\n");
        return;
    }

    strcpy(sentence, "PASV\n");
    if (write_to_server(connfd, sentence) == -1) {
        printf("Write to server failed.\n");
        return;
    }
    if (read_from_server(connfd, sentence) == -1) {
        printf("Read from server failed.\n");
        return;
    }

    if (strncmp(sentence, "227", 3) != 0){
        QMessageBox::information (0, "Error",
                                     "Error");
        return;
    }
    PASV(sentence, &status, &conn_info);

    char *filename = (char*)malloc(256);
    filename = strrchr(item->text().toStdString().c_str(), ' ') + 1;
    filename[strlen(filename) - 1] = 0;
    char *filename2 = strdup(filename);
    printf("File name: %s, len: %d\n", filename, strlen(filename));

    strcpy(sentence, "RETR ");
    strcat(sentence, filename);
    int len = strlen(sentence) + 1;
    sentence[len - 1] = '\r';
    sentence[len] = '\n';
    sentence[len + 1] = '\0';

    retr_thread *th = new retr_thread(sentence, &status, &conn_info, filename2);
    th->start();

    QObject::connect(th, SIGNAL(finished()), ui->btn_refresh_local, SLOT(click()));

    refresh_local_list();
}

void MainWindow::on_list_local_itemDoubleClicked(QListWidgetItem *item)
{
    char *sentence = (char*)malloc(8192);
    strcpy(sentence, "PASV\n");

    if (write_to_server(connfd, sentence) == -1) {
        printf("Write to server failed.\n");
        return;
    }

    if (read_from_server(connfd, sentence) == -1) {
        printf("Read from server failed.\n");
        return;
    }

    if (strncmp(sentence, "227", 3) != 0){
        QMessageBox::information (0, "Error",
                                     "Error");
        return;
    }
    PASV(sentence, &status, &conn_info);

    strcpy(sentence, "STOR ");
    strcat(sentence, item->text().toStdString().c_str());
    strcat(sentence, "\n");

    //STOR_PASV(sentence, &status, &conn_info, new stor_thread(sentence, &status, &conn_info));
    stor_thread *th = new stor_thread(sentence, &status, &conn_info);
    th->start();

    QObject::connect(th, SIGNAL(finished()), ui->btn_refresh_remote, SLOT(click()));
}

void MainWindow::on_btn_refresh_remote_clicked()
{
    refresh_remote_list();
}

void MainWindow::on_tree_local_doubleClicked(const QModelIndex &index)
{
    char *sentence = (char*)malloc(8192);
    strcpy(sentence, "PASV\n");

    if (write_to_server(connfd, sentence) == -1) {
        printf("Write to server failed.\n");
        return;
    }

    if (read_from_server(connfd, sentence) == -1) {
        printf("Read from server failed.\n");
        return;
    }

    if (strncmp(sentence, "227", 3) != 0){
        QMessageBox::information (0, "Error",
                                     "Error");
        return;
    }
    PASV(sentence, &status, &conn_info);

    strcpy(sentence, "STOR ");
    strcat(sentence, index.sibling(index.row(), 0).data().toString().toStdString().c_str());
    strcat(sentence, "\n");

    //STOR_PASV(sentence, &status, &conn_info, new stor_thread(sentence, &status, &conn_info));
    stor_thread *th = new stor_thread(sentence, &status, &conn_info);
    th->start();

    QObject::connect(th, SIGNAL(finished()), ui->btn_refresh_remote, SLOT(click()));
}

void MainWindow::on_btn_refresh_local_clicked()
{
    refresh_local_list();
}
