#include "mainwindow.h"
#include "dialog_incoming.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    szDialNum = "";
    ipc_baresip = new IPC_baresip(this);
    oncall_win = nullptr;
    dial_dialog = nullptr;
    incoming_dialog = nullptr;
    connect(ipc_baresip, SIGNAL(create_incoming_dialog_signal(QString)),this, SLOT(create_incoming_dialog(QString)));
    connect(ipc_baresip, SIGNAL(register_info_signal(QString)),this, SLOT(register_info(QString)));
    ipc_baresip->start();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_1_clicked()
{
    szDialNum.append("1");
    ui->textBrowser->setText(szDialNum);
}

void MainWindow::on_pushButton_2_clicked()
{
    szDialNum.append("2");
    ui->textBrowser->setText(szDialNum);
}

void MainWindow::on_pushButton_3_clicked()
{
    szDialNum.append("3");
    ui->textBrowser->setText(szDialNum);
}

void MainWindow::on_pushButton_4_clicked()
{
    szDialNum.append("4");
    ui->textBrowser->setText(szDialNum);
}

void MainWindow::on_pushButton_5_clicked()
{
    szDialNum.append("5");
    ui->textBrowser->setText(szDialNum);
}

void MainWindow::on_pushButton_6_clicked()
{
    szDialNum.append("6");
    ui->textBrowser->setText(szDialNum);
}

void MainWindow::on_pushButton_7_clicked()
{
    szDialNum.append("7");
    ui->textBrowser->setText(szDialNum);
}

void MainWindow::on_pushButton_8_clicked()
{
    szDialNum.append("8");
    ui->textBrowser->setText(szDialNum);
}

void MainWindow::on_pushButton_9_clicked()
{
    szDialNum.append("9");
    ui->textBrowser->setText(szDialNum);
}

void MainWindow::on_pushButton_star_clicked()
{
    szDialNum.append(".");
    ui->textBrowser->setText(szDialNum);
}

void MainWindow::on_pushButton_0_clicked()
{
    szDialNum.append("0");
    ui->textBrowser->setText(szDialNum);
}

void MainWindow::on_pushButton_pound_clicked()
{
    szDialNum.append("#");
    ui->textBrowser->setText(szDialNum);
}

void MainWindow::on_pushButton_Del_clicked()
{
    szDialNum.chop(1);
    ui->textBrowser->setText(szDialNum);
}

void MainWindow::on_pushButton_Dail_clicked()
{
    //baresip dial a call
    QString dial_uri = ipc_baresip->baresip_dial(szDialNum);

    dial_dialog = new Dialog_Dial(this);
    dial_dialog->setWindowTitle(dial_uri);

    connect(dial_dialog, SIGNAL(baresip_hangup_signal()), ipc_baresip, SLOT(baresip_hangup()));
    connect(ipc_baresip, SIGNAL(answered_dial_call_signal()), this, SLOT(answered_dial_call()));
    connect(ipc_baresip, SIGNAL(cancelled_dial_call_signal()), this, SLOT(cancelled_dial_call()));
    connect(ipc_baresip, SIGNAL(create_oncall_window_signal()), this, SLOT(create_oncall_window()));

    dial_dialog->exec();
}

void MainWindow::register_info(QString reg_info)
{
    ui->label_reginfo->setText(reg_info);
}

void MainWindow::create_incoming_dialog(QString peer_uri)
{
    int ret;
    E_BARESIP_STATE eBaresip_state;
    qDebug() << "Create incoming dialog";

    Dialog_Incoming Incoming_dialog(this);

    Incoming_dialog.setWindowTitle(peer_uri);

//    Incoming_dialog.setStyleSheet("background-color: rgba(255, 255, 255, 255)"); //white alpha 255

    connect(&Incoming_dialog, SIGNAL(baresip_accept_signal()),ipc_baresip, SLOT(baresip_accept()));
    connect(&Incoming_dialog, SIGNAL(baresip_reject_signal()),ipc_baresip, SLOT(baresip_reject()));

    connect(ipc_baresip, SIGNAL(create_oncall_window_signal()), this, SLOT(create_oncall_window()));

    ret = Incoming_dialog.exec();

    disconnect(&Incoming_dialog, SIGNAL(baresip_accept_signal()),ipc_baresip, SLOT(baresip_accept()));
    disconnect(&Incoming_dialog, SIGNAL(baresip_reject_signal()),ipc_baresip, SLOT(baresip_reject()));
}


void MainWindow::create_oncall_window()
{
    qDebug()<<"create on call window";
    // Change to On call window
    if(oncall_win == nullptr)
    {
        qDebug()<<"create on call window : new window ";
        oncall_win = new Window_OnCall(this);

        //Set frameless window
        oncall_win->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

        //Set window background color with alpha
        QColor bgColor;
        int alpha = 0;

        bgColor = oncall_win->palette().window().color();
        QString rgbaValue = QString("%1, %2, %3, %4").arg( bgColor.red()).arg( bgColor.green()).arg( bgColor.blue()).arg( alpha);

        oncall_win->setStyleSheet("background-color: rgba("+rgbaValue+")"); //background alpha 0
        oncall_win->setup_baresip_ipc(ipc_baresip);

        connect(oncall_win, SIGNAL(baresip_hangup_signal()), ipc_baresip, SLOT(baresip_hangup()));
        connect(oncall_win, SIGNAL(baresip_hangup_all_signal()), ipc_baresip, SLOT(baresip_hangup_all()));
        connect(oncall_win, SIGNAL(baresip_hangup_by_id_signal(QString)), ipc_baresip, SLOT(baresip_hangup_by_id(QString)));

        connect(ipc_baresip, SIGNAL(close_oncall_window_signal()), this, SLOT(close_oncall_window()));
        connect(ipc_baresip, SIGNAL(setup_call_info_signal(QString, QString, int *)), oncall_win, SLOT(setup_call_info(QString, QString, int *)));
        connect(ipc_baresip, SIGNAL(remove_call_info_signal(QString, int *)), oncall_win, SLOT(remove_call_info(QString, int *)));
        connect(ipc_baresip, SIGNAL(create_oncall_incoming_dialog_signal(QString)),oncall_win, SLOT(create_incoming_dialog(QString)));

        disconnect(ipc_baresip, SIGNAL(create_oncall_window_signal()), this, SLOT(create_oncall_window()));

        oncall_win->show();
        this->hide();
    }
}

void MainWindow::close_oncall_window()
{
    if(oncall_win)
    {
        disconnect(oncall_win, SIGNAL(baresip_hangup_signal()), ipc_baresip, SLOT(baresip_hangup()));
        disconnect(oncall_win, SIGNAL(baresip_hangup_all_signal()), ipc_baresip, SLOT(baresip_hangup_all()));
        disconnect(oncall_win, SIGNAL(baresip_hangup_by_id_signal(QString)), ipc_baresip, SLOT(baresip_hangup_by_id(QString)));

        disconnect(ipc_baresip, SIGNAL(close_oncall_window_signal()), this, SLOT(close_oncall_window()));
        disconnect(ipc_baresip, SIGNAL(setup_call_info_signal(QString, QString, int *)), oncall_win, SLOT(setup_call_info(QString, QString, int *)));
        disconnect(ipc_baresip, SIGNAL(remove_call_info_signal(QString, int *)), oncall_win, SLOT(remove_call_info(QString, int *)));

        delete oncall_win;
        oncall_win = nullptr;
        this->show();
    }
}

void MainWindow::answered_dial_call()
{
    qDebug()<<"answered the call";

    if(dial_dialog)
    {
        disconnect(dial_dialog, SIGNAL(baresip_hangup_signal()), ipc_baresip, SLOT(baresip_hangup()));
        disconnect(ipc_baresip, SIGNAL(answered_dial_call_signal()), this, SLOT(answered_dial_call()));
        disconnect(ipc_baresip, SIGNAL(cancelled_dial_call_signal()), this, SLOT(cancelled_dial_call()));

        dial_dialog->close();
        delete dial_dialog;
        dial_dialog = nullptr;
    }
}

void MainWindow::cancelled_dial_call()
{
    qDebug()<<"cancelled the dial call";

    if(dial_dialog)
    {
        disconnect(dial_dialog, SIGNAL(baresip_hangup_signal()), ipc_baresip, SLOT(baresip_hangup()));
        disconnect(ipc_baresip, SIGNAL(answered_dial_call_signal()), this, SLOT(answered_dial_call()));
        disconnect(ipc_baresip, SIGNAL(cancelled_dial_call_signal()), this, SLOT(cancelled_dial_call()));

        dial_dialog->close();
        delete dial_dialog;
        dial_dialog = nullptr;
    }
}
