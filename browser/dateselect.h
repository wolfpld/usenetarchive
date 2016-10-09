#ifndef DATESELECT_H
#define DATESELECT_H

#include <QDialog>

class Browser;

namespace Ui {
class DateSelect;
}

class DateSelect : public QDialog
{
    Q_OBJECT

public:
    explicit DateSelect( Browser* browser, QWidget *parent = 0 );
    ~DateSelect();

private slots:
    void on_cancelButton_clicked();

    void on_okButton_clicked();

private:
    Ui::DateSelect *ui;
    Browser* m_browser;
};

#endif // DATESELECT_H
