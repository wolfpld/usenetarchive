#include "browser.h"
#include "dateselect.h"
#include "ui_dateselect.h"

DateSelect::DateSelect( Browser* browser, QWidget *parent )
    : QDialog( parent )
    , ui( new Ui::DateSelect )
    , m_browser( browser )
{
    ui->setupUi(this);
}

DateSelect::~DateSelect()
{
    delete ui;
}

void DateSelect::on_cancelButton_clicked()
{
    close();
}

void DateSelect::on_okButton_clicked()
{
    m_browser->GoToDate( ui->dateEdit->dateTime().toTime_t() );
    close();
}
