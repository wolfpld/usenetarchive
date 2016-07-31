#include "groupcharter.h"
#include "ui_groupcharter.h"

GroupCharter::GroupCharter( const char* sdesc, const size_t slen, const char* ldesc, const size_t llen, QWidget *parent ) :
    QDialog(parent),
    ui(new Ui::GroupCharter)
{
    ui->setupUi(this);

    if( sdesc && slen > 0 )
    {
        ui->label->setText( QString( std::string( sdesc, slen ).c_str() ) );
    }
    if( ldesc && llen > 0 )
    {
        ui->textBrowser->setPlainText( QString( std::string( ldesc, llen ).c_str() ) );
    }
    else
    {
        ui->textBrowser->setHtml( "<html><body><br/><div style=\"font-size:100pt\">&nbsp;:(</div><center><b>Group charter is not available.</b></center></body></html>" );
    }
}

GroupCharter::~GroupCharter()
{
    delete ui;
}

void GroupCharter::on_pushButton_clicked()
{
    close();
}
