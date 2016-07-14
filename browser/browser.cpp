#include <QFileDialog>
#include <sstream>
#include <memory>

#ifdef _WIN32
#  include <malloc.h>
#else
#  include <alloca.h>
#endif

#include "../libuat/Archive.hpp"
#include "../common/String.hpp"

#include "browser.h"
#include "ui_browser.h"

Browser::Browser( QWidget *parent )
    : QMainWindow( parent )
    , ui( new Ui::Browser )
    , m_index( -1 )
    , m_rawMessage( false )
    , m_rot13( false )
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
        ui->actionRaw_message->setEnabled( true );
        ui->actionROT13->setEnabled( true );
    }
}

void Browser::on_actionRaw_message_triggered(bool checked)
{
    if( m_index == -1 ) return;

    m_rawMessage = checked;
    auto msg = m_archive->GetMessage( m_index );
    ShowMessage( msg );
}

void Browser::on_actionROT13_triggered(bool checked)
{
    if( m_index == -1 ) return;

    m_rot13 = checked;
    auto msg = m_archive->GetMessage( m_index );
    ShowMessage( msg );
}

void Browser::ShowMessage( const char* msg )
{
    if( m_rot13 )
    {
        auto size = strlen( msg );
        char* tmp = (char*)alloca( size + 1 );
        for( int i=0; i<size; i++ )
        {
            if( ( msg[i] >= 'a' && msg[i] <= 'm' ) ||
                ( msg[i] >= 'A' && msg[i] <= 'M' ) )
            {
                tmp[i] = msg[i] + 13;
            }
            else if( ( msg[i] >= 'n' && msg[i] <= 'z' ) ||
                     ( msg[i] >= 'N' && msg[i] <= 'Z' ) )
            {
                tmp[i] = msg[i] - 13;
            }
            else
            {
                tmp[i] = msg[i];
            }
        }
        tmp[size] = '\0';
        msg = tmp;
    }

    if( m_rawMessage )
    {
        ui->textBrowser->setPlainText( msg );
    }
    else
    {
        SetText( msg );
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

static void EncodeSpecial( TextBuf& buf, const char*& txt, const char* end, char trigger, char tag )
{
    const char* tmp = txt+1;
    for(;;)
    {
        if( tmp == end || *tmp == ' ' )
        {
            tmp = end;
            break;
        }
        if( *tmp == trigger )
        {
            break;
        }
        tmp++;
    }
    if( tmp == end || tmp - txt == 1 )
    {
        buf.PutC( *txt );
    }
    else
    {
        buf.PutC( trigger );
        buf.PutC( '<' );
        buf.PutC( tag );
        buf.PutC( '>' );
        buf.Write( txt+1, tmp - txt - 1 );
        buf.Write( "</", 2 );
        buf.PutC( tag );
        buf.PutC( '>' );
        buf.PutC( trigger );
        txt = tmp;
    }
}

static void Encode( TextBuf& buf, const char* txt, const char* end )
{
    while( txt != end )
    {
        if( *txt == '<' )
        {
            buf.Write( "&lt;", 4 );
        }
        else if( *txt == '>' )
        {
            buf.Write( "&gt;", 4 );
        }
        else if( *txt == '&' )
        {
            buf.Write( "&amp;", 5 );
        }
        else if( *txt == '*' )
        {
            EncodeSpecial( buf, txt, end, '*', 'b' );
        }
        else if( *txt == '/' )
        {
            EncodeSpecial( buf, txt, end, '/', 'i' );
        }
        else if( *txt == '_' )
        {
            EncodeSpecial( buf, txt, end, '_', 'u' );
        }
        else
        {
            buf.PutC( *txt );
        }
        txt++;
    }
}

void Browser::SetText( const char* txt )
{
    m_buf.Reset();
    m_buf.Write( "<body><html><pre style=\"font-family: Consolas\"><p style=\"background-color: #1c1c1c\"><font color=\"#555555\">", 106 );

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
                m_buf.Write( "</font></p>", 11 );
                headers = false;
                while( *end == '\n' ) end++;
                end--;
            }
            else
            {
                bool font = true;
                if( !first )
                {
                    m_buf.Write( "<br/>", 5 );
                }
                first = false;
                if( strnicmpl( txt, "from: ", 6 ) == 0 )
                {
                    m_buf.Write( "<font color=\"#f6a200\">", 22 );
                }
                else if( strnicmpl( txt, "newsgroups: ", 12 ) == 0 )
                {
                    m_buf.Write( "<font color=\"#0068f6\">", 22 );
                }
                else if( strnicmpl( txt, "subject: ", 9 ) == 0 )
                {
                    m_buf.Write( "<font color=\"#74f600\">", 22 );
                }
                else if( strnicmpl( txt, "date: ", 6 ) == 0 )
                {
                    m_buf.Write( "<font color=\"#f6002e\">", 22 );
                }
                else
                {
                    font = false;
                }
                Encode( m_buf, txt, end );
                if( font )
                {
                    m_buf.Write( "</font>", 7 );
                }
            }
        }
        else
        {
            bool font = true;
            if( strncmp( "-- \n", txt, 4 ) == 0 )
            {
                sig = true;
            }
            if( sig )
            {
                m_buf.Write( "<font color=\"#666666\">", 22 );
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
                    font = false;
                    break;
                case 1:
                    m_buf.Write( "<font color=\"#ae4a00\">", 22 );
                    break;
                case 2:
                    m_buf.Write( "<font color=\"#980e76\">", 22 );
                    break;
                case 3:
                    m_buf.Write( "<font color=\"#4e47ab\">", 22 );
                    break;
                default:
                    m_buf.Write( "<font color=\"#225025\">", 22 );
                    break;
                }
            }
            Encode( m_buf, txt, end );
            if( font )
            {
                m_buf.Write( "</font><br/>", 12 );
            }
            else
            {
                m_buf.Write( "<br/>", 5 );
            }
        }
        if( *end == '\0' ) break;
        txt = end + 1;
    }

    m_buf.Write( "</pre></html></body>", 20 );
    ui->textBrowser->setHtml( QString( m_buf ) );
}

void Browser::on_treeView_clicked(const QModelIndex &index)
{
    m_index = m_model->GetIdx( index );
    if( m_index == -1 ) return;

    ui->actionRaw_message->setChecked( false );
    ui->actionROT13->setChecked( false );
    m_rot13 = false;
    m_rawMessage = false;

    SetText( m_archive->GetMessage( m_index ) );
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
