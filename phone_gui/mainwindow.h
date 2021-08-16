#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDialog>

#include "ipc_baresip.h"
#include "window_oncall.h"
#include "dialog_dial.h"
#include "dialog_incoming.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButton_Dail_clicked();

    void on_pushButton_1_clicked();

    void on_pushButton_2_clicked();

    void on_pushButton_3_clicked();

    void on_pushButton_4_clicked();

    void on_pushButton_5_clicked();

    void on_pushButton_6_clicked();

    void on_pushButton_7_clicked();

    void on_pushButton_8_clicked();

    void on_pushButton_9_clicked();

    void on_pushButton_star_clicked();

    void on_pushButton_0_clicked();

    void on_pushButton_pound_clicked();

    void on_pushButton_Del_clicked();

    void create_incoming_dialog(QString peer_uri);

    void create_oncall_window();

    void close_oncall_window();

    void answered_dial_call();

    void cancelled_dial_call();

    void register_info(QString reg_info);

private:
    Ui::MainWindow *ui;
    QString szDialNum;
    IPC_baresip *ipc_baresip;
    Window_OnCall *oncall_win;
    Dialog_Dial *dial_dialog;
    Dialog_Incoming *incoming_dialog;
};
#endif // MAINWINDOW_H
