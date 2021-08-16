#ifndef DIALOG_DIAL_H
#define DIALOG_DIAL_H

#include <QDialog>

namespace Ui {
class Dialog_Dial;
}

class Dialog_Dial : public QDialog
{
    Q_OBJECT

public:
    explicit Dialog_Dial(QWidget *parent = nullptr);
    ~Dialog_Dial();

private slots:
    void on_pushButton_Hangup_clicked();

signals:
    void baresip_hangup_signal();

private:
    Ui::Dialog_Dial *ui;
};

#endif // DIALOG_DIAL_H
