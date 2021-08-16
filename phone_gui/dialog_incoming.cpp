#include "dialog_incoming.h"
#include "ui_dialog_incoming.h"
#include <QtCore/qdebug.h>

Dialog_Incoming::Dialog_Incoming(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog_Incoming)
{
    ui->setupUi(this);
    ui->label->setStyleSheet("background-color: rgba(255, 255, 255, 0)");
    QPixmap srcPixmap(QString::fromUtf8(":/resources/img/resources/img/incoming.png"));
    ui->label->setPixmap(srcPixmap);
}

Dialog_Incoming::~Dialog_Incoming()
{
    qDebug() << "destory dialog incoming";
    delete ui;
}

void Dialog_Incoming::on_pushButton_Accept_clicked()
{
    emit baresip_accept_signal();
    this->accept();
}

void Dialog_Incoming::on_pushButton_Reject_clicked()
{
    emit baresip_reject_signal();
    this->reject();
}
