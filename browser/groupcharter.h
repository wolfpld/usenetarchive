#ifndef GROUPCHARTER_H
#define GROUPCHARTER_H

#include <QDialog>

namespace Ui {
class GroupCharter;
}

class GroupCharter : public QDialog
{
    Q_OBJECT

public:
    explicit GroupCharter( const char* sdesc, const size_t slen, const char* ldesc, const size_t llen, const char* ndesc, const size_t nlen, QWidget *parent = 0);
    ~GroupCharter();

private slots:
    void on_pushButton_clicked();

private:
    Ui::GroupCharter *ui;
};

#endif // GROUPCHARTER_H
