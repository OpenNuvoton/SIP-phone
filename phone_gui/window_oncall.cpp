#include <QDebug>
#include <QGraphicsOpacityEffect>

#include "window_oncall.h"
#include "ui_window_oncall.h"
#include "dialog_incoming.h"

Window_OnCall::Window_OnCall(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Window_OnCall)
{
    ui->setupUi(this);
    ui->comboBox_peeruri->addItem("All");
    ui->comboBox_peeruri->setStyleSheet("color: rgba(0, 0, 255, 255); background-color: rgba(255, 255, 255, 255)");
    ui->pushButton_HangUp->setStyleSheet("color: rgba(0, 0, 255, 255); background-color: rgba(255, 255, 255, 255)");
    ipc_baresip = nullptr;
}

Window_OnCall::~Window_OnCall()
{
    delete ui;
}

void Window_OnCall::on_pushButton_HangUp_clicked()
{
    QString select_item;
    S_CALL_INFO *psCallInfo = nullptr;
    int i;

    select_item = ui->comboBox_peeruri->currentText();

    if(select_item.compare("All") == 0)
    {
        emit baresip_hangup_all_signal();
        return;
    }

    for(i = 0; i < call_list.size(); i++)
    {
        psCallInfo = call_list.at(i);
        if(psCallInfo)
        {
            if(psCallInfo->peer_uri.compare(select_item) == 0)
            {
                emit baresip_hangup_by_id_signal(psCallInfo->id);
                return;
            }
        }
    }
}

void Window_OnCall::setup_call_info(QString peer_uri, QString id, int *num_calls)
{
    int i;
    S_CALL_INFO *psCallInfo;

    for(i = 0; i < call_list.size(); i++)
    {
        psCallInfo = call_list.at(i);
        if(psCallInfo)
        {
            if(psCallInfo->id.compare(id) == 0)
            {
                if(num_calls)
                    *num_calls = call_list.size();
                return;
            }
        }
    }

    psCallInfo = new S_CALL_INFO;

    if(psCallInfo == nullptr)
    {
        if(num_calls)
            *num_calls = call_list.size();
        return;
    }

    psCallInfo->peer_uri = peer_uri;
    psCallInfo->id = id;

    ui->comboBox_peeruri->addItem(peer_uri);
    call_list.append(psCallInfo);

    qDebug() << "insert peer to call info list";
    if(num_calls)
        *num_calls = call_list.size();
}

void Window_OnCall::remove_call_info(QString id, int *num_calls)
{
    int i;
    S_CALL_INFO *psCallInfo = nullptr;

    for(i = 0; i < call_list.size(); i++)
    {
        psCallInfo = call_list.at(i);
        if(psCallInfo)
        {
            if(psCallInfo->id.compare(id) == 0)
            {
                break;
            }
        }
        psCallInfo = nullptr;
    }

    if(psCallInfo)
    {
        int comboBox_index;
        comboBox_index = ui->comboBox_peeruri->findText(psCallInfo->peer_uri);

        if(comboBox_index >= 0)
            ui->comboBox_peeruri->removeItem(comboBox_index);

        call_list.takeAt(i);
        delete psCallInfo;
        qDebug() << "remove peer to call info list";
    }

    if(num_calls)
        *num_calls = call_list.size();
}

void Window_OnCall::setup_baresip_ipc(IPC_baresip *psBaresipIPC)
{
    ipc_baresip = psBaresipIPC;
}


void Window_OnCall::create_incoming_dialog(QString peer_uri)
{
    int ret;
    qDebug() << "Create incoming dialog from oncall window";

    Dialog_Incoming Incoming_dialog(this);

    Incoming_dialog.setWindowTitle(peer_uri);

    QColor bgColor;
    int alpha = 128;

    bgColor = Incoming_dialog.palette().window().color();
    QString rgbaValue = QString("%1, %2, %3, %4").arg( bgColor.red()).arg( bgColor.green()).arg( bgColor.blue()).arg( alpha);
    Incoming_dialog.setStyleSheet("background-color: rgba("+rgbaValue+")"); //backgroup ,alpha 64

    connect(&Incoming_dialog, SIGNAL(baresip_accept_signal()),ipc_baresip, SLOT(baresip_accept()));
    connect(&Incoming_dialog, SIGNAL(baresip_reject_signal()),ipc_baresip, SLOT(baresip_reject()));

    ret = Incoming_dialog.exec();

    disconnect(&Incoming_dialog, SIGNAL(baresip_accept_signal()),ipc_baresip, SLOT(baresip_accept()));
    disconnect(&Incoming_dialog, SIGNAL(baresip_reject_signal()),ipc_baresip, SLOT(baresip_reject()));
}

