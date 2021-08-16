#ifndef IPC_BARESIP_H
#define IPC_BARESIP_H

#include <QThread>
#include <QTcpSocket>

typedef enum{
    eBARESIP_STATE_IDLE,
    eBARESIP_STATE_DIAL,
    eBARESIP_STATE_INCOMING,
    eBARESIP_STATE_ONCALL,
}E_BARESIP_STATE;


class IPC_baresip : public QThread
{
    Q_OBJECT
public:

    IPC_baresip(QObject *parent = nullptr);
    ~IPC_baresip();

    QString baresip_dial(QString &dial_num);
    E_BARESIP_STATE baresip_current_state();

protected:
    void run() override;

public slots:
    void socket_disconnected();
    void socket_bytesWritten(qint64 bytes);
    void socket_readyRead();

    void baresip_reject();
    void baresip_hangup();
    void baresip_hangup_all();
    void baresip_hangup_by_id(QString id);
    void baresip_accept();

signals:
    void create_incoming_dialog_signal(QString peer_uri);
    void create_oncall_window_signal();
    void close_oncall_window_signal();
    void answered_dial_call_signal();
    void cancelled_dial_call_signal();
    void register_info_signal(QString reg_info);
    void setup_call_info_signal(QString peer_uri, QString id, int *num_calls);
    void remove_call_info_signal(QString id, int *num_calls);
    void create_oncall_incoming_dialog_signal(QString peer_uri);

private:

    typedef enum{
        eBARESIP_MESSAGE_UNKNOWN,
        eBARESIP_MESSAGE_EVENT,
        eBARESIP_MESSAGE_RESPONSE,
    }E_BARESIP_MESSAGE_TYPE;

    typedef struct{
        QString class_type;
        QString type;
        QString param;
        QString accountaor;
        QString director;
        QString peeruri;
        QString id;
    }S_BARESIP_EVENT_MESSAGE;

    typedef struct{
        bool ok;
        QString data;
    }S_BARESIP_RESPONSE_MESSAGE;

    void GenBaresipMessage(
        QByteArray &message,
        QByteArray &data
    );

    int ParseBaresipMessage(
        QByteArray &message,
        QJsonObject &object
    );

    void DecodeBaresipMessage(
        QJsonObject json_object,
        E_BARESIP_MESSAGE_TYPE &eMessageType,
        S_BARESIP_EVENT_MESSAGE &sEventMessage,
        S_BARESIP_RESPONSE_MESSAGE &sResponseMessage
    );

    void DispatchBaresipMessage(
        E_BARESIP_MESSAGE_TYPE eMessageType,
        S_BARESIP_EVENT_MESSAGE *psEventMessage,
        S_BARESIP_RESPONSE_MESSAGE *psResponseMessage
    );

    void IdleState_HandleEventMessage(
        S_BARESIP_EVENT_MESSAGE *psEventMessage
    );

    void DialState_HandleEventMessage(
        S_BARESIP_EVENT_MESSAGE *psEventMessage
    );

    void IncomingState_HandleEventMessage(
        S_BARESIP_EVENT_MESSAGE *psEventMessage
    );

    void OncallState_HandleEventMessage(
        S_BARESIP_EVENT_MESSAGE *psEventMessage
    );

    void IdleState_HandleResponseMessage(
        S_BARESIP_RESPONSE_MESSAGE *psResponseMessage
    );

    void baresip_reginfo();

    QString szSIPServerName;
    QTcpSocket *socket;
    E_BARESIP_STATE eBaresip_state;
    int NumOnCall;
};

#endif // IPC_BARESIP_H
