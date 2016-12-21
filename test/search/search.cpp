#include "../../libuat/Archive.hpp"

int main( int argc, char** argv )
{
    if( argc != 3 )
    {
        return 1;
    }

    auto a = Archive::Open( argv[1] );
    a->Search( argv[2], Archive::SF_AdjacentWords );

    return 0;
}
