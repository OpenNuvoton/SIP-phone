#include "dialog_dial.h"
#include "ui_dialog_dial.h"
#include "ipc_baresip.h"

Dialog_Dial::Dialog_Dial(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog_Dial)
{
    ui->setupUi(this);
}

Dialog_Dial::~Dialog_Dial()
{
    delete ui;
}

void Dialog_Dial::on_pushButton_Hangup_clicked()
{
    emit baresip_hangup_signal();
    this->accept();
}
