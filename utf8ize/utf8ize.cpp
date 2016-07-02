#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <strings.h>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>

#include <gmime/gmime.h>

#include "../common/ExpandingBuffer.hpp"
#include "../common/Filesystem.hpp"
#include "../common/FileMap.hpp"
#include "../common/MessageView.hpp"
#include "../common/RawImportMeta.hpp"
#include "../common/String.hpp"

static std::string mime_part_to_text( GMimeObject* obj )
{
    std::string ret;

    if( !GMIME_IS_PART( obj ) ) return ret;

    GMimeContentType* content_type = g_mime_object_get_content_type( obj );
    GMimeDataWrapper* c = g_mime_part_get_content_object( GMIME_PART( obj ) );
    GMimeStream* memstream = g_mime_stream_mem_new();

    gint64 len = g_mime_data_wrapper_write_to_stream( c, memstream );
    guint8* b = g_mime_stream_mem_get_byte_array( (GMimeStreamMem*)memstream )->data;

    const char* charset;
    if( ( charset = g_mime_content_type_get_parameter( content_type, "charset" ) ) && strcasecmp( charset, "utf-8" ) != 0 )
    {
        iconv_t cv = g_mime_iconv_open( "UTF-8", charset );
        if( char* converted = g_mime_iconv_strndup( cv, (const char*)b, len ) )
        {
            ret = converted;
            g_free( converted );
        }
        else
        {
            if( b )
            {
                ret = std::string( (const char*)b, len );
            }
        }
        g_mime_iconv_close( cv );
    }
    else
    {
        if( b )
        {
            ret = std::string( (const char*)b, len );
        }
    }

    g_mime_stream_close( memstream );
    g_object_unref( memstream );
    return ret;
}

int main( int argc, char** argv )
{
    if( argc != 3 )
    {
        fprintf( stderr, "USAGE: %s source destination\n", argv[0] );
        exit( 1 );
    }
    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Source directory doesn't exist.\n" );
        exit( 1 );
    }
    if( Exists( argv[2] ) )
    {
        fprintf( stderr, "Destination directory already exists.\n" );
        exit( 1 );
    }

    g_mime_init( GMIME_ENABLE_RFC2047_WORKAROUNDS );
    const char* charsets[] = {
        "UTF-8",
        "ISO8859-2",
        "CP1250",
        0
    };
    g_mime_set_user_charsets( charsets );

    CreateDirStruct( argv[2] );

    std::string base = argv[1];
    base.append( "/" );

    MessageView mview( base + "meta", base + "data" );
    const auto size = mview.Size();

    std::string dbase = argv[2];
    dbase.append( "/" );
    std::string dmetafn = dbase + "meta";
    std::string ddatafn = dbase + "data";

    FILE* dmeta = fopen( dmetafn.c_str(), "wb" );
    FILE* ddata = fopen( ddatafn.c_str(), "wb" );

    std::ostringstream ss;
    ExpandingBuffer eb;
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x3FF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto post = mview[i];
        auto raw = mview.Raw( i );

        GMimeStream* istream = g_mime_stream_mem_new_with_buffer( post, raw.size );
        GMimeParser* parser = g_mime_parser_new_with_stream( istream );
        g_object_unref( istream );

        GMimeMessage* message = g_mime_parser_construct_message( parser );
        g_object_unref( parser );

        GMimeHeaderList* ls = GMIME_OBJECT( message )->headers;
        GMimeHeaderIter* hit = g_mime_header_iter_new();

        if( g_mime_header_list_get_iter( ls, hit ) )
        {
            while( g_mime_header_iter_is_valid( hit ) )
            {
                auto name = g_mime_header_iter_get_name( hit );
                auto value = g_mime_header_iter_get_value( hit );
                auto decode = g_mime_utils_header_decode_phrase( g_mime_utils_header_decode_text( value ) );
                ss << name << ": " << decode << "\n";
                if( !g_mime_header_iter_next( hit ) ) break;
            }
        }
        g_mime_header_iter_free( hit );

        std::string content;
        GMimeObject* last = nullptr;
        GMimePartIter* pit = g_mime_part_iter_new( (GMimeObject*)message );
        do
        {
            GMimeObject* part = g_mime_part_iter_get_current( pit );
            if( GMIME_IS_OBJECT( part ) && GMIME_IS_PART( part ) )
            {
                GMimeContentType* content_type = g_mime_object_get_content_type( part );
                if( content.empty() && ( !content_type || g_mime_content_type_is_type( content_type, "text", "plain" ) ) )
                {
                    content = mime_part_to_text( part );
                }
                if( g_mime_content_type_is_type( content_type, "text", "html" ) )
                {
                    last = part;
                }
            }
        }
        while( g_mime_part_iter_next( pit ) );
        g_mime_part_iter_free( pit );
        if( content.empty() )
        {
            if( last )
            {
                content = mime_part_to_text( last );
            }
        }
        if( content.empty() )
        {
            GMimeObject* body = g_mime_message_get_body( message );
            content = mime_part_to_text( body );
        }

        ss << "\n" << content;

        printf( "%s\n--==--\n", ss.str().c_str() );

        /*
        GMimeStream* ostream = g_mime_stream_mem_new();
        g_mime_object_write_to_stream( g_mime_message_get_body( message ), ostream );
        //g_mime_object_write_to_stream( (GMimeObject*)message, ostream );
        g_mime_stream_flush( ostream );

        GByteArray* out = g_mime_stream_mem_get_byte_array( GMIME_STREAM_MEM( ostream ) );
        printf( "%s\n", std::string( out->data, out->data + out->len ).c_str() );

        g_object_unref( message );
        g_object_unref( ostream );
        */

        ss.str( "" );
    }

    fclose( dmeta );
    fclose( ddata );

    g_mime_shutdown();

    return 0;
}
