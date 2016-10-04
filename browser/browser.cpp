#include <chrono>
#include <QFileDialog>
#include <QLabel>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <sstream>
#include <memory>
#include <time.h>

#ifdef _WIN32
#  include <malloc.h>
#else
#  include <alloca.h>
#endif

#include "../libuat/Archive.hpp"
#include "../common/MessageLogic.hpp"
#include "../common/String.hpp"

#include "about.h"
#include "browser.h"
#include "ui_browser.h"
#include "groupcharter.h"

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
    QStringList filters;
    filters << "Usenet archives (*.usenet)";
    QFileDialog dialog;
    dialog.setFileMode( QFileDialog::ExistingFile );
    dialog.setNameFilters( filters );
    int res = dialog.exec();
    if( res )
    {
        std::string dir = dialog.selectedFiles()[0].toStdString();
        m_archive.reset( Archive::Open( dir ) );
        if( !m_archive )
        {
            QMessageBox::warning( this, "Error", "Cannot open archive.", QMessageBox::NoButton, QMessageBox::Ok );
            return;
        }
        QString str;
        str += "Loaded archive with ";
        str += QString::number( m_archive->NumberOfMessages() );
        str += " messages. ";
        str += QString::number( m_archive->NumberOfTopLevel() );
        str += " threads.";
        ui->statusBar->showMessage( str, 0 );
        FillTree();
        auto idx = dir.find_last_of( '/' );
        std::string tabText;
        auto desc = m_archive->GetShortDescription();
        if( desc.first )
        {
            bool shortAvailable = desc.second > 0;
            if( shortAvailable )
            {
                for( int i=0; i<desc.second; i++ )
                {
                    if( desc.first[i] != '\n' ) tabText += desc.first[i];
                }

                tabText += " (";
            }
            auto name = m_archive->GetArchiveName();
            if( name.first && name.second != 0 )
            {
                tabText += std::string( name.first, name.first + name.second );
            }
            else
            {
                tabText += dir.substr( idx+1 );
            }
            if( shortAvailable )
            {
                tabText += ")";
            }
        }
        else
        {
            tabText = dir.substr( idx+1 );
        }
        ui->tabWidget->setTabText( 0, tabText.c_str() );
        ui->actionGroup_Charter->setEnabled( true );
        ui->actionRaw_message->setEnabled( true );
        ui->actionROT13->setEnabled( true );
        ui->actionGo_to_message->setEnabled( true );
        ui->SearchTab->setEnabled( true );
        ui->SearchContentsScroll->setUpdatesEnabled( false );
        ClearSearch();
        ui->SearchContentsScroll->setUpdatesEnabled( true );
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
    auto scroll = ui->textBrowser->verticalScrollBar();
    auto pos = scroll->value();
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
    scroll->setValue( pos );
}

void Browser::FillTree()
{
    m_model = std::make_unique<TreeModel>( *m_archive );
    ui->treeView->setUpdatesEnabled( false );
    ui->treeView->setModel( m_model.get() );
    auto rows = m_model->rowCount();
    for( int i=0; i<rows; i++ )
    {
        RecursiveSetIndex( m_model->index( i, 0 ) );
    }
    for( int i=0; i<4; i++ )
    {
        ui->treeView->resizeColumnToContents( i );
    }
    connect( ui->treeView->selectionModel(), SIGNAL( currentChanged( QModelIndex, QModelIndex ) ), this, SLOT( onTreeSelectionChanged( QModelIndex ) ) );
    ui->treeView->setCurrentIndex( m_model->index( 0, 0 ) );
    ui->treeView->setUpdatesEnabled( true );
    ui->tabWidget->setCurrentIndex( 0 );
}

static int Encode( TextBuf& buf, const char* txt, const char* end, bool special = true );

static int EncodeSpecial( TextBuf& buf, const char*& txt, const char* end, char trigger, char tag )
{
    int cnt = 0;
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
        cnt++;
    }
    else
    {
        buf.PutC( trigger );
        cnt++;
        buf.PutC( '<' );
        buf.PutC( tag );
        buf.PutC( '>' );
        cnt += Encode( buf, txt+1, tmp, false );
        buf.Write( "</", 2 );
        buf.PutC( tag );
        buf.PutC( '>' );
        buf.PutC( trigger );
        cnt++;
        txt = tmp;
    }
    return cnt;
}

static int Encode( TextBuf& buf, const char* txt, const char* end, bool special )
{
    int cnt = 0;
    while( txt != end )
    {
        if( *txt == '<' )
        {
            buf.Write( "&lt;", 4 );
            cnt++;
        }
        else if( *txt == '>' )
        {
            buf.Write( "&gt;", 4 );
            cnt++;
        }
        else if( *txt == '&' )
        {
            buf.Write( "&amp;", 5 );
            cnt++;
        }
        else if( special && *txt == '*' )
        {
            cnt += EncodeSpecial( buf, txt, end, '*', 'b' );
        }
        else if( special && *txt == '/' )
        {
            cnt += EncodeSpecial( buf, txt, end, '/', 'i' );
        }
        else if( special && *txt == '_' )
        {
            cnt += EncodeSpecial( buf, txt, end, '_', 'u' );
        }
        else if( *txt == '\t' )
        {
            int tab = 8 - ( cnt % 8 );
            for( int i=0; i<tab; i++ )
            {
                buf.PutC( ' ' );
            }
            cnt += tab;
        }
        else
        {
            buf.PutC( *txt );
            cnt++;
        }
        txt++;
    }
    return cnt;
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
                if( strnicmpl( txt, m_rot13 ? "sebz: " : "from: ", 6 ) == 0 )
                {
                    m_buf.Write( "<font color=\"#f6a200\">", 22 );
                }
                else if( strnicmpl( txt, m_rot13 ? "arjftebhcf: " : "newsgroups: ", 12 ) == 0 || strnicmpl( txt, m_rot13 ? "gb: " : "to: ", 4 ) == 0 )
                {
                    m_buf.Write( "<font color=\"#0068f6\">", 22 );
                }
                else if( strnicmpl( txt, m_rot13 ? "fhowrpg: " : "subject: ", 9 ) == 0 )
                {
                    m_buf.Write( "<font color=\"#74f600\">", 22 );
                }
                else if( strnicmpl( txt, m_rot13 ? "qngr: " : "date: ", 6 ) == 0 )
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
                auto test = txt;
                int level = QuotationLevel( test, end );
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

void Browser::RecursiveSetIndex(const QModelIndex& index)
{
    m_model->SetIndex( m_model->GetIdx( index ), index );
    auto num = m_model->rowCount(index);
    for( int i=0; i<num; i++ )
    {
        RecursiveSetIndex( m_model->index( i, 0, index ) );
    }
}

void Browser::on_lineEdit_returnPressed()
{
    const auto query = ui->lineEdit->text();
    if( query.size() == 0 ) return;

    auto t0 = std::chrono::high_resolution_clock::now();
    auto tmp = query.toStdString();
    const auto res = m_archive->Search( tmp.c_str() );
    auto t1 = std::chrono::high_resolution_clock::now();

    enum { DisplayLimit = 250 };

    QString str;
    str += "Search query found ";
    str += QString::number( res.size() );
    str += " messages in ";
    str += QString::number( std::chrono::duration_cast<std::chrono::microseconds>( t1 - t0 ).count() / 1000.f );
    str += "ms.";
    if( res.size() > DisplayLimit )
    {
        str += " (Limiting display to ";
        str += QString::number( DisplayLimit );
        str += " messages.)";
    }
    ui->statusBar->showMessage( str, 0 );

    ui->SearchContentsScroll->setUpdatesEnabled( false );
    ClearSearch();
    const auto size = std::min<size_t>( DisplayLimit, res.size() );
    for( size_t i=0; i<size; i++ )
    {
        auto& v = res[i];
        auto panel = new QFrame();
        panel->setFrameShape( QFrame::StyledPanel );
        auto vbox = new QVBoxLayout( panel );
        vbox->setMargin( 3 );

        auto gridwidget = new QWidget();
        auto grid = new QGridLayout( gridwidget );
        grid->setMargin( 0 );

        auto from = new QLabel( ( std::string( "From: <b>" ) + m_archive->GetRealName( v.postid ) + "</b>" ).c_str() );
        from->setTextFormat( Qt::RichText );
        grid->addWidget( from, 0, 0 );
        auto subject = new QLabel( ( std::string( "Subject: <b>" ) + m_archive->GetSubject( v.postid ) + "</b>" ).c_str() );
        subject->setTextFormat( Qt::RichText );
        grid->addWidget( subject, 1, 0 );

        const auto epoch = time_t( m_archive->GetDate( v.postid ) );
        char buf[64];
        strftime( buf, 64, "Date: <b>%a %F %T</b>", localtime( &epoch ) );
        auto date = new QLabel( buf );
        grid->addWidget( date, 0, 1 );

        sprintf( buf, "<font color=\"#666666\">Rank: %.2f</font>", v.rank );
        auto rank = new QLabel( buf );
        grid->addWidget( rank, 1, 1 );

        auto btn = new QPushButton();
        btn->setText( "View message" );
        grid->addWidget( btn, 0, 2, 2, 1 );

        auto postid = v.postid;
        connect( btn, &QPushButton::clicked, [this, postid] { this->SwitchToMessage( postid ); } );

        auto msg = std::string( m_archive->GetMessage( v.postid ) );
        std::string lower = msg;
        std::transform( lower.begin(), lower.end(), lower.begin(), ::tolower );

        std::vector<size_t> tpos;
        for( auto& match : v.matched )
        {
            size_t pos = 0;
            while( ( pos = lower.find( match, pos+1 ) ) != std::string::npos ) tpos.emplace_back( pos );
        }
        std::sort( tpos.begin(), tpos.end() );

        std::vector<std::pair<size_t, size_t>> ranges;
        int stop = std::min<int>( 6, tpos.size() );
        for( int i=0; i<stop; i++ )
        {
            size_t start = tpos[i];
            for( int j=0; j<10; j++ )
            {
                if( start == 0 ) break;
                start--;
                while( start > 0 && msg[start] != ' ' && msg[start] != '\n' ) start--;
            }
            size_t end = tpos[i];
            for( int j=0; j<10; j++ )
            {
                if( end == msg.size() ) break;
                end++;
                while( end < msg.size() && msg[end] != ' ' && msg[end] != '\n' ) end++;
            }
            ranges.emplace_back( start, end );
        }

        for( int i=ranges.size()-1; i>0; i-- )
        {
            if( ranges[i].first <= ranges[i-1].second )
            {
                ranges[i-1].second = ranges[i].second;
                ranges.erase( ranges.begin() + i );
            }
        }

        std::ostringstream s;
        for( auto& v : ranges )
        {
            s << "<font color=\"#666666\"> ... </font>";
            for( size_t i = v.first; i<v.second; i++ )
            {
                switch( msg[i] )
                {
                case '\n':
                    s << "<br/>";
                    break;
                case '<':
                    s << "&lt;";
                    break;
                case '>':
                    s << "&gt;";
                    break;
                case '&':
                    s << "&amp;";
                    break;
                default:
                    s.put( msg[i] );
                    break;
                }
            }
        }
        s << "<font color=\"#666666\"> ... </font>";

        auto content = new QTextBrowser();
        content->setHtml( s.str().c_str() );
        content->setLineWrapMode( QTextEdit::WidgetWidth );
        content->document()->adjustSize();
        content->setMinimumHeight( content->document()->size().height() + 2 );

        vbox->addWidget( gridwidget );
        vbox->addWidget( content );
        ui->SearchContents->addWidget( panel );
        m_searchItems.emplace_back( panel );
    }

    ui->SearchContentsScroll->setUpdatesEnabled( true );
}

void Browser::SwitchToMessage( uint32_t idx )
{
    ui->treeView->setCurrentIndex( m_model->GetIndexFor( idx ) );
    ui->tabWidget->setCurrentIndex( 0 );
}

void Browser::on_actionGo_to_message_triggered()
{
    bool ok;
    QString msgid = QInputDialog::getText( this, "Go to message", "Enter Message-ID:", QLineEdit::Normal, QString(), &ok );
    if( ok && !msgid.isEmpty() )
    {
        auto idx = m_archive->GetMessageIndex( msgid.toStdString().c_str() );
        if( idx >= 0 )
        {
            SwitchToMessage( idx );
        }
        else
        {
            QMessageBox::warning( this, "Error", "Message-ID doesn't exist.", QMessageBox::NoButton, QMessageBox::Ok );
        }
    }
}

void Browser::on_actionAbout_triggered()
{
    auto about = new About();
    about->exec();
}

void Browser::ClearSearch()
{
    for( auto& v : m_searchItems )
    {
        delete v;
    }
    m_searchItems.clear();
}

void Browser::on_actionGroup_Charter_triggered()
{
    auto s = m_archive->GetShortDescription();
    auto l = m_archive->GetLongDescription();
    auto n = m_archive->GetArchiveName();

    auto dialog = new GroupCharter( s.first, s.second, l.first, l.second, n.first, n.second );
    dialog->exec();
}
