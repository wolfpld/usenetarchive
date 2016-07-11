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
        auto idx = dir.find_last_of( '/' );
        ui->tabWidget->setTabText( 0, dir.substr( idx+1 ).c_str() );
    }
}

void Browser::FillTree()
{
    m_model = std::make_unique<TreeModel>( *m_archive );
    ui->treeView->setModel( m_model.get() );
    for( int i=0; i<4; i++ )
    {
        ui->treeView->resizeColumnToContents( i );
    }
    connect( ui->treeView->selectionModel(), SIGNAL( currentChanged( QModelIndex, QModelIndex ) ), this, SLOT( onTreeSelectionChanged( QModelIndex ) ) );
}

void Browser::on_treeView_clicked(const QModelIndex &index)
{
    auto idx = m_model->GetIdx( index );
    if( idx == -1 ) return;

    ui->textBrowser->setPlainText( m_archive->GetMessage( idx ) );
}

void Browser::onTreeSelectionChanged( const QModelIndex& index )
{
    on_treeView_clicked( index );
}
