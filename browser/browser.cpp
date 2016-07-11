#include <QFileDialog>
#include <sstream>

#include "../libuat/Archive.hpp"
#include "../common/String.hpp"

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
        ui->statusBar->showMessage( str, 0 );
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

static void Encode( std::ostringstream& s, const char* txt, const char* end )
{
    while( txt != end )
    {
        if( *txt == '<' )
        {
            s << "&lt;";
        }
        else if( *txt == '>' )
        {
            s << "&gt;";
        }
        else
        {
            s.put( *txt );
        }
        txt++;
    }
}

void Browser::SetText( const char* txt )
{
    std::ostringstream s;
    s << "<body><html>";
    s << "<p style=\"background-color: #1c1c1c\">";

    bool headers = true;
    bool first = true;
    bool sig = false;
    for(;;)
    {
        auto end = txt;
        while( *end != '\n' && *end != '\0' ) end++;
        if( headers )
        {
            if( end-txt == 0 )
            {
                s << "</p>";
                headers = false;
                while( *end == '\n' ) end++;
                end--;
            }
            else
            {
                if( !first )
                {
                    s << "<br/>";
                }
                first = false;
                if( strnicmp( "from: ", txt, 6 ) == 0 )
                {
                    s << "<font color=\"#f6a200\">";
                }
                else if( strnicmp( "newsgroups: ", txt, 12 ) == 0 )
                {
                    s << "<font color=\"#0068f6\">";
                }
                else if( strnicmp( "subject: ", txt, 9 ) == 0 )
                {
                    s << "<font color=\"#74f600\">";
                }
                else if( strnicmp( "date: ", txt, 6 ) == 0 )
                {
                    s << "<font color=\"#f6002e\">";
                }
                else
                {
                    s << "<font color=\"#555555\">";
                }
                Encode( s, txt, end );
                s << "</font>";
            }
        }
        else
        {
            if( strncmp( "-- \n", txt, 4 ) == 0 )
            {
                sig = true;
            }
            if( sig )
            {
                s << "<font color=\"#666666\">";
            }
            else
            {
                int level = 0;
                auto test = txt;
                while( test != end )
                {
                    if( *test == '>' || *test == ':' || *test == '|' )
                    {
                        level++;
                    }
                    else if( *test != ' ' )
                    {
                        break;
                    }
                    test++;
                }
                switch( level )
                {
                case 0:
                    s << "<font>";
                    break;
                case 1:
                    s << "<font color=\"#ae4a00\">";
                    break;
                case 2:
                    s << "<font color=\"#980e76\">";
                    break;
                case 3:
                    s << "<font color=\"#4e47ab\">";
                    break;
                default:
                    s << "<font color=\"#225025\">";
                    break;
                }
            }
            Encode( s, txt, end );
            s << "</font><br/>";
        }
        if( *end == '\0' ) break;
        txt = end + 1;
    }

    s << "</html></body>";
    ui->textBrowser->setHtml( s.str().c_str() );
}

void Browser::on_treeView_clicked(const QModelIndex &index)
{
    auto idx = m_model->GetIdx( index );
    if( idx == -1 ) return;

    SetText( m_archive->GetMessage( idx ) );
}

void Browser::onTreeSelectionChanged( const QModelIndex& index )
{
    on_treeView_clicked( index );
}

void Browser::on_treeView_expanded(const QModelIndex &index)
{
    if( m_model->IsRoot( index ) )
    {
        RecursiveExpand(index);
    }
}

void Browser::RecursiveExpand(const QModelIndex& index)
{
    auto num = m_model->rowCount(index);
    for( int i=0; i<num; i++ )
    {
        auto idx = m_model->index( i, 0, index );
        ui->treeView->expand(idx);
        RecursiveExpand(idx);
    }
}
