#ifndef WINDOW_ONCALL_H
#define WINDOW_ONCALL_H

#include <QMainWindow>
#include "ipc_baresip.h"

namespace Ui {
class Window_OnCall;
}

class Window_OnCall : public QMainWindow
{
    Q_OBJECT

public:
    explicit Window_OnCall(QWidget *parent = nullptr);
    ~Window_OnCall();

    void setup_baresip_ipc(IPC_baresip *psBaresipIPC);

public slots:
    void setup_call_info(QString peer_uri, QString id, int *num_calls);
    void remove_call_info(QString id, int *num_calls);
    void create_incoming_dialog(QString peer_uri);

signals:
    void baresip_hangup_signal();
    void baresip_hangup_all_signal();
    void baresip_hangup_by_id_signal(QString id);

private slots:
    void on_pushButton_HangUp_clicked();

private:
    Ui::Window_OnCall *ui;

    typedef struct{
        QString peer_uri;
        QString id;
    }S_CALL_INFO;

    QList <S_CALL_INFO *> call_list;

    IPC_baresip *ipc_baresip;
};

#endif // WINDOW_ONCALL_H
