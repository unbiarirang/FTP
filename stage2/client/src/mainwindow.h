#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "client.h"
#include "utils.h"
#include "commands.h"
#include "qdebug.h"
#include "qinputdialog.h"
#include "qmessagebox.h"
#include "qdatetime.h"
#include "qtreewidget.h"
#include "qobject.h"
#include <QDropEvent>
#include <QModelIndex>
#include <QListWidgetItem>
#include <QDir>
#include <QTreeView>
#include <QDirModel>
#include <QtGui>
#include <QDialog>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include <QThread>

namespace Ui {
class MainWindow;
}


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(int argc, char *argv[], QWidget *parent = 0);
    void refresh_remote_list();
    void refresh_local_list();
    virtual void closeEvent(QCloseEvent *event);
    void LIST_PASV(char *sentence, int *status, conn *info);
    ~MainWindow();

private slots:
    void on_btn_login_clicked();
    void on_list_remote_itemDoubleClicked(QListWidgetItem *item);
    void on_tree_local_doubleClicked(const QModelIndex &index);
    void on_list_local_itemDoubleClicked(QListWidgetItem *item);

    void on_btn_refresh_remote_clicked();
    void on_btn_refresh_local_clicked();

private:
    Ui::MainWindow *ui;
};

class SortFilter:public QSortFilterProxyModel{
public:
    SortFilter();
    ~SortFilter();
protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;
};
#endif // MAINWINDOW_H
