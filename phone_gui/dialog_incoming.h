#ifndef DIALOG_INCOMING_H
#define DIALOG_INCOMING_H

#include <QDialog>

namespace Ui {
class Dialog_Incoming;
}

class Dialog_Incoming : public QDialog
{
    Q_OBJECT

public:
    explicit Dialog_Incoming(QWidget *parent = nullptr);
    ~Dialog_Incoming();

private slots:
    void on_pushButton_Reject_clicked();

private slots:
    void on_pushButton_Accept_clicked();

signals:
    void baresip_accept_signal();
    void baresip_reject_signal();

private:
    Ui::Dialog_Incoming *ui;
};

#endif // DIALOG_INCOMING_H
