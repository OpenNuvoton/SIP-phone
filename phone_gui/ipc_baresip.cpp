#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QHostAddress>

#include <QByteArray>
#include <QString>

#include "ipc_baresip.h"
#include "dialog_incoming.h"


#define BARESIP_TCP_PORT 4444

IPC_baresip::IPC_baresip(QObject *parent)
    :QThread(parent)
{
    eBaresip_state = eBARESIP_STATE_IDLE;
    szSIPServerName = "baresip.org";
    socket = new QTcpSocket(this);
    NumOnCall = 0;

    qDebug() << "connecting...";

    // this is not blocking function
    socket->connectToHost("127.0.0.1", BARESIP_TCP_PORT);
//    socket->connectToHost("192.168.31.115", BARESIP_TCP_PORT);

    // we need to wait...
    if(!socket->waitForConnected(5000))
    {
        qDebug() << "Error: " << socket->errorString();
        delete socket;
        socket = nullptr;
    }
    else{
        qDebug() << "connected to baresip done";
        connect(socket, SIGNAL(disconnected()),this, SLOT(socket_disconnected()));
        connect(socket, SIGNAL(bytesWritten(qint64)),this, SLOT(socket_bytesWritten(qint64)));
        connect(socket, SIGNAL(readyRead()),this, SLOT(socket_readyRead()));
    }
}

IPC_baresip::~IPC_baresip()
{
    qDebug() << "IPC baresip destory";
    if(socket)
    {
        delete socket;
        socket = nullptr;
    }
    this->quit();
    this->wait();
    qDebug() << "IPC baresip thread done";
}

void IPC_baresip::run()
{
    int i32Cnt = 0;
    int i32MessageCnt = 10;

    baresip_reginfo();
    exec(); //if using signal and slot, must call exec()
}


void IPC_baresip::GenBaresipMessage(
    QByteArray &message,
    QByteArray &data
)
{
    QString data_len_str;

    data_len_str = QString("%1:").arg(data.length());
    message.append(data_len_str);
    message.append(data);
    message.append(',');
}

int IPC_baresip::ParseBaresipMessage(
    QByteArray &message,
    QJsonObject &object
)
{
    char *buffer;
    int buffer_length;
    int object_len = 0;
    int i;

    buffer=message.data();
    buffer_length=message.size();

    /* Make sure buffer is big enough. Minimum size is 3. */
    if (buffer_length < 3)
        return -1;

    /* No leading zeros allowed! */
    if (buffer[0] == '0' && isdigit(buffer[1]))
        return -2;

    /* The netstring must start with a number */
    if (!isdigit(buffer[0]))
        return -3;

    /* Read the number of bytes */
    for (i = 0; i < buffer_length && isdigit(buffer[i]); i++) {

        /* Error if more than 9 digits */
        if (i >= 9)
            return -4;

        /* Accumulate each digit, assuming ASCII. */
        object_len = object_len*10 + (buffer[i] - '0');
    }

    /**
     * Check buffer length. The buffer must be longer than the sum of:
     *   - the number we've read.
     *   - the length of the string itself.
     *   - the colon.
     *   - the comma.
     */
    if (i + object_len + 1 >= buffer_length)
        return -5;

    /* Read the colon */
    if (buffer[i++] != ':')
        return -6;

    /* Test for the trailing comma, and set the return values */
    if (buffer[i + object_len] != ',')
        return -7;

    /*start parser data json format*/
    QByteArray json_data = message.mid(i, object_len);

    qDebug() <<"json parser" <<json_data;

    QJsonDocument parse_doucment;
    QJsonParseError json_error;

    parse_doucment = QJsonDocument::fromJson(json_data, &json_error);

    if(json_error.error != QJsonParseError::NoError)
        return -8;

    if(parse_doucment.isObject() == false)
        return -9;

    object = parse_doucment.object();

    return i + object_len + 1;
}

void IPC_baresip::DecodeBaresipMessage(
    QJsonObject json_object,
    E_BARESIP_MESSAGE_TYPE &eMessageType,
    S_BARESIP_EVENT_MESSAGE &sEventMessage,
    S_BARESIP_RESPONSE_MESSAGE &sResponseMessage
)
{
    if(json_object.contains("event"))
    {
        eMessageType = eBARESIP_MESSAGE_EVENT;

    }
    else if(json_object.contains("response"))
    {
        eMessageType = eBARESIP_MESSAGE_RESPONSE;
    }
    else
    {
        eMessageType = eBARESIP_MESSAGE_UNKNOWN;
        qDebug() << "unknown message type:" <<json_object;
        return;
    }

    // Decode event message
    if(eMessageType == eBARESIP_MESSAGE_EVENT)
    {
        if(json_object.contains("class"))
        {
            QJsonValue class_value = json_object.take("class");

            if(class_value.isString())
            {
                sEventMessage.class_type = class_value.toVariant().toString();
            }
        }

        if(json_object.contains("type"))
        {
            QJsonValue type_value = json_object.take("type");

            if(type_value.isString())
            {
                sEventMessage.type = type_value.toVariant().toString();
            }
        }

        if(json_object.contains("param"))
        {
            QJsonValue param_value = json_object.take("param");

            if(param_value.isString())
            {
                sEventMessage.param = param_value.toVariant().toString();
            }
        }

        if(json_object.contains("accountaor"))
        {
            QJsonValue accountaor_value = json_object.take("accountaor");

            if(accountaor_value.isString())
            {
                sEventMessage.accountaor = accountaor_value.toVariant().toString();
            }
        }

        if(json_object.contains("director"))
        {
            QJsonValue director_value = json_object.take("director");

            if(director_value.isString())
            {
                sEventMessage.director = director_value.toVariant().toString();
            }
        }

        if(json_object.contains("peeruri"))
        {
            QJsonValue peeruri_value = json_object.take("peeruri");

            if(peeruri_value.isString())
            {
                sEventMessage.peeruri = peeruri_value.toVariant().toString();
            }
        }

        if(json_object.contains("id"))
        {
            QJsonValue id_value = json_object.take("id");

            if(id_value.isString())
            {
                sEventMessage.id = id_value.toVariant().toString();
            }
        }
    }

    // Decode response message
    if(eMessageType == eBARESIP_MESSAGE_RESPONSE)
    {
        if(json_object.contains("ok"))
        {
            QJsonValue ok_value = json_object.take("ok");

            if(ok_value.isBool())
            {
                sResponseMessage.ok = ok_value.toVariant().toBool();
            }
        }

        if(json_object.contains("data"))
        {
            QJsonValue data_value = json_object.take("data");

            if(data_value.isString())
            {
                sResponseMessage.data = data_value.toVariant().toString();
            }
        }
    }
}

void IPC_baresip::IdleState_HandleResponseMessage(
    S_BARESIP_RESPONSE_MESSAGE *psResponseMessage
)
{
    if(psResponseMessage->ok == true)
    {
        if(psResponseMessage->data.indexOf("User Agents") >= 0)
        {
            int uri_start_index;
            int uri_stop_index;
            uri_start_index = psResponseMessage->data.indexOf("sip");

            if(uri_start_index >= 0)
            {
                QString reg_uri;
                int server_start_index;

                uri_stop_index = psResponseMessage->data.indexOf(" ", uri_start_index);
                reg_uri = psResponseMessage->data.mid(uri_start_index, uri_stop_index - uri_start_index);

                emit register_info_signal(reg_uri);

                server_start_index = reg_uri.indexOf("@");

                if(server_start_index >= 0)
                    szSIPServerName = reg_uri.mid(server_start_index + 1);
                qDebug() << szSIPServerName;

            }
        }
    }
}

void IPC_baresip::IdleState_HandleEventMessage(
    S_BARESIP_EVENT_MESSAGE *psEventMessage
)
{
    if(psEventMessage->class_type.compare("call") == 0)
    {
        if(psEventMessage->type.compare("CALL_INCOMING") == 0)
        {
            eBaresip_state = eBARESIP_STATE_INCOMING;
            emit create_incoming_dialog_signal(psEventMessage->peeruri);
        }
    }
}

void IPC_baresip::DialState_HandleEventMessage(
    S_BARESIP_EVENT_MESSAGE *psEventMessage
)
{
    if(psEventMessage->class_type.compare("call") == 0)
    {
       if(psEventMessage->type.compare("CALL_CLOSED") == 0)
       {
           emit cancelled_dial_call_signal();
           eBaresip_state = eBARESIP_STATE_IDLE;
       }

       if(psEventMessage->type.compare("CALL_ESTABLISHED") == 0)
       {
           emit answered_dial_call_signal();
           emit create_oncall_window_signal();
           eBaresip_state = eBARESIP_STATE_ONCALL;
           emit setup_call_info_signal(psEventMessage->peeruri, psEventMessage->id, &NumOnCall);
       }
    }
}

void IPC_baresip::IncomingState_HandleEventMessage(
    S_BARESIP_EVENT_MESSAGE *psEventMessage
)
{
    if(psEventMessage->type.compare("CALL_CLOSED") == 0)
    {
        qDebug() << "Receive call closed on Incoming state";
        eBaresip_state = eBARESIP_STATE_IDLE;
    }

    if(psEventMessage->type.compare("CALL_ESTABLISHED") == 0)
    {
        eBaresip_state = eBARESIP_STATE_ONCALL;
        emit create_oncall_window_signal();
        emit setup_call_info_signal(psEventMessage->peeruri, psEventMessage->id, &NumOnCall);
    }
}

void IPC_baresip::OncallState_HandleEventMessage(
    S_BARESIP_EVENT_MESSAGE *psEventMessage
)
{
    if(psEventMessage->type.compare("CALL_CLOSED") == 0)
    {
        qDebug() << "Receive call closed on call state";

        emit remove_call_info_signal(psEventMessage->id, &NumOnCall);

        if(NumOnCall == 0){
            emit close_oncall_window_signal();
            eBaresip_state = eBARESIP_STATE_IDLE;
        }
    }

    if(psEventMessage->type.compare("CALL_ESTABLISHED") == 0)
    {
        eBaresip_state = eBARESIP_STATE_ONCALL;
        emit setup_call_info_signal(psEventMessage->peeruri, psEventMessage->id, &NumOnCall);
    }

    //Multi call support
    if(psEventMessage->type.compare("CALL_INCOMING") == 0)
    {
//        emit create_incoming_dialog_signal(psEventMessage->peeruri);
        emit create_oncall_incoming_dialog_signal(psEventMessage->peeruri);
    }
}

void IPC_baresip::DispatchBaresipMessage(
    E_BARESIP_MESSAGE_TYPE eMessageType,
    S_BARESIP_EVENT_MESSAGE *psEventMessage,
    S_BARESIP_RESPONSE_MESSAGE *psResponseMessage
)
{
    switch (eBaresip_state)
    {
        case eBARESIP_STATE_IDLE:
        {
            if(eMessageType == eBARESIP_MESSAGE_EVENT)
            {
                IdleState_HandleEventMessage(psEventMessage);
            }

            if(eMessageType == eBARESIP_MESSAGE_RESPONSE)
            {
                IdleState_HandleResponseMessage(psResponseMessage);
            }
        }
        break;
        case eBARESIP_STATE_DIAL:
        {
            if(eMessageType == eBARESIP_MESSAGE_EVENT)
            {
                DialState_HandleEventMessage(psEventMessage);
            }
        }
        break;
        case eBARESIP_STATE_INCOMING:
        {
            if(eMessageType == eBARESIP_MESSAGE_EVENT)
            {
                IncomingState_HandleEventMessage(psEventMessage);
            }
        }
        break;
        case eBARESIP_STATE_ONCALL:
        {
            if(eMessageType == eBARESIP_MESSAGE_EVENT)
            {
                OncallState_HandleEventMessage(psEventMessage);
            }
        }
        break;
    }


}

void IPC_baresip::baresip_accept()
{
    qDebug() << "baresip accept";

    QJsonObject accept_json;
    QByteArray message;

    accept_json.insert("command", "accept");

    QJsonDocument doc;
    doc.setObject(accept_json);
    QByteArray simpbyte_array = doc.toJson(QJsonDocument::Compact);

    GenBaresipMessage(message, simpbyte_array);

    QString message_str(message);
    qDebug() << "accept string" << message_str;

    if(socket)
        socket->write(message);
}

void IPC_baresip::baresip_reject()
{
    qDebug() << "baresip reject";

    QJsonObject hangup_json;
    QByteArray message;

    hangup_json.insert("command", "hangup");

    QJsonDocument doc;
    doc.setObject(hangup_json);
    QByteArray simpbyte_array = doc.toJson(QJsonDocument::Compact);

    GenBaresipMessage(message, simpbyte_array);

    QString message_str(message);
    qDebug() << "reject string" << message_str;

    if(socket)
        socket->write(message);
}


QString IPC_baresip::baresip_dial(QString &dial_num)
{
    qDebug() << "baresip dial" << dial_num;
    QString dial_uri;
    QHostAddress host_addr;
    host_addr.setAddress(dial_num);
    QByteArray message;

    if(host_addr.isGlobal() == true)
    {
        dial_uri = QString("sip:root@%1").arg(dial_num);
    }
    else
    {
        dial_uri = QString("sip:%1@%2").arg(dial_num, szSIPServerName);
    }

    QJsonObject dial_json;
    dial_json.insert("command", "dial");
    dial_json.insert("params", dial_uri);
    QJsonDocument doc;
    doc.setObject(dial_json);
    QByteArray simpbyte_array = doc.toJson(QJsonDocument::Compact);

    GenBaresipMessage(message, simpbyte_array);

    QString message_str(message);
    qDebug() << "dial string" << message_str;

    if(socket)
        socket->write(message);

    eBaresip_state = eBARESIP_STATE_DIAL;

    return dial_uri;
}

void IPC_baresip::baresip_hangup()
{
    qDebug() << "baresip hangup";

    QJsonObject hangup_json;
    QByteArray message;

    hangup_json.insert("command", "hangup");

    QJsonDocument doc;
    doc.setObject(hangup_json);
    QByteArray simpbyte_array = doc.toJson(QJsonDocument::Compact);

    GenBaresipMessage(message, simpbyte_array);

    QString message_str(message);
    qDebug() << "hangup string" << message_str;

    if(socket)
        socket->write(message);

}

void IPC_baresip::baresip_hangup_all()
{
    qDebug() << "baresip hangupall";

    QJsonObject hangupall_json;
    QByteArray message;

    hangupall_json.insert("command", "hangupall");
    hangupall_json.insert("params", "all");

    QJsonDocument doc;
    doc.setObject(hangupall_json);
    QByteArray simpbyte_array = doc.toJson(QJsonDocument::Compact);

    GenBaresipMessage(message, simpbyte_array);

    QString message_str(message);
    qDebug() << "hangupall string" << message_str;

    if(socket)
        socket->write(message);
}

void IPC_baresip::baresip_hangup_by_id(QString id)
{
    qDebug() << "baresip hangupall";

    QJsonObject hangup_id_json;
    QByteArray message;

    hangup_id_json.insert("command", "hangup");
    hangup_id_json.insert("params", id);

    QJsonDocument doc;
    doc.setObject(hangup_id_json);
    QByteArray simpbyte_array = doc.toJson(QJsonDocument::Compact);

    GenBaresipMessage(message, simpbyte_array);

    QString message_str(message);
    qDebug() << "hangup by id string" << message_str;

    if(socket)
        socket->write(message);
}

void IPC_baresip::baresip_reginfo()
{
    qDebug() << "baresip reginfo";

    QJsonObject reginfo_json;
    QByteArray message;

    reginfo_json.insert("command", "reginfo");

    QJsonDocument doc;
    doc.setObject(reginfo_json);
    QByteArray simpbyte_array = doc.toJson(QJsonDocument::Compact);

    GenBaresipMessage(message, simpbyte_array);

    QString message_str(message);
    qDebug() << "reginfo string" << message_str;

    if(socket)
        socket->write(message);

}

void IPC_baresip::socket_disconnected()
{
    qDebug() << "disconnected...";
}

void IPC_baresip::socket_bytesWritten(qint64 bytes)
{
    qDebug() << bytes << " bytes written...";
}

void IPC_baresip::socket_readyRead()
{
    QByteArray byte_data;
    int parser_len;

    // read the data from the socket
    byte_data= socket->readAll();

    while(!byte_data.isEmpty())
    {
        QJsonObject json_data;
        E_BARESIP_MESSAGE_TYPE eMessageType;
        S_BARESIP_EVENT_MESSAGE sEventMessage;
        S_BARESIP_RESPONSE_MESSAGE sResponseMessage;

        parser_len = ParseBaresipMessage(byte_data, json_data);

        if(parser_len <= 0)
            break;

        DecodeBaresipMessage(json_data, eMessageType, sEventMessage, sResponseMessage);
        DispatchBaresipMessage(eMessageType, &sEventMessage, &sResponseMessage);

        byte_data = byte_data.right(byte_data.size() - parser_len);
    }
}

E_BARESIP_STATE IPC_baresip::baresip_current_state()
{
    return eBaresip_state;
}
