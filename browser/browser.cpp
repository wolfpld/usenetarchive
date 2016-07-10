#include <QFileDialog>

#include "../libuat/Archive.hpp"

#include "browser.h"
#include "ui_browser.h"

Browser::Browser( QWidget *parent )
    : QMainWindow( parent )
    , ui( new Ui::Browser )
{
    ui->setupUi( this );
}

Browser::~Browser()
{
    delete ui;
}

void Browser::on_actionOpen_triggered()
{
    QFileDialog dialog;
    dialog.setFileMode( QFileDialog::Directory );
    dialog.setOption( QFileDialog::ShowDirsOnly );
    int res = dialog.exec();
    if( res )
    {
        std::string dir = dialog.selectedFiles()[0].toStdString();
        m_archive = std::make_unique<Archive>( dir );
        QString str;
        str += "Loaded archive with ";
        str += QString::number( m_archive->NumberOfMessages() );
        str += " messages.";
        ui->statusBar->showMessage( str, 10000 );
        FillTree();
    }
}

void Browser::FillTree()
{
    m_model = std::make_unique<TreeModel>( *m_archive );
    ui->treeView->setModel( m_model.get() );
}
