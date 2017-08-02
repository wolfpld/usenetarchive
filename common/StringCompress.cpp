#include <assert.h>

#include "../contrib/xxhash/xxhash.h"

#include "StringCompress.hpp"

static const char* CodeBook[256] = {
    nullptr,   // 0: end of string
    nullptr,   // 1: encoded host string
    /* 0x02   31433265 hits */ "$1",
    /* 0x03    7900071 hits */ "000",
    /* 0x04   13493930 hits */ "00",
    /* 0x05    4340078 hits */ ".1",
    /* 0x06    4294196 hits */ "0$",
    /* 0x07    3992454 hits */ "$2",
    /* 0x08    3922444 hits */ "01",
    /* 0x09    3872121 hits */ "11",
    /* 0x0a    1903497 hits */ ".00",
    /* 0x0b    3780420 hits */ "10",
    /* 0x0c    1732759 hits */ "lrn",
    /* 0x0d    1732673 hits */ "slr",
    /* 0x0e    3437775 hits */ "20",
    /* 0x0f    3239897 hits */ "12",
    /* 0x10    1615262 hits */ "dlg",
    /* 0x11    1608476 hits */ ".dl",
    /* 0x12    2942040 hits */ ".0",
    /* 0x13    1388717 hits */ "0$1",
    /* 0x14    2717401 hits */ ".2",
    /* 0x15    2670844 hits */ ".3",
    /* 0x16    2626695 hits */ ".4",
    /* 0x17    2609600 hits */ "3$",
    /* 0x18    2581756 hits */ "4$",
    /* 0x19    2570024 hits */ "1$",
    /* 0x1a    2565753 hits */ "7$",
    /* 0x1b    2564505 hits */ "2$",
    /* 0x1c    2560709 hits */ "6$",
    /* 0x1d    2560587 hits */ "5$",
    /* 0x1e    2558665 hits */ "9$",
    /* 0x1f    2551658 hits */ "8$",
    /* 0x20                 */ " ",
    /* 0x21                 */ "!",
    /* 0x22                 */ "\"",
    /* 0x23                 */ "#",
    /* 0x24                 */ "$",
    /* 0x25                 */ "%",
    /* 0x26                 */ "&",
    /* 0x27                 */ "'",
    /* 0x28                 */ "(",
    /* 0x29                 */ ")",
    /* 0x2a                 */ "*",
    /* 0x2b                 */ "+",
    /* 0x2c                 */ ",",
    /* 0x2d                 */ "-",
    /* 0x2e                 */ ".",
    /* 0x2f                 */ "/",
    /* 0x30                 */ "0",
    /* 0x31                 */ "1",
    /* 0x32                 */ "2",
    /* 0x33                 */ "3",
    /* 0x34                 */ "4",
    /* 0x35                 */ "5",
    /* 0x36                 */ "6",
    /* 0x37                 */ "7",
    /* 0x38                 */ "8",
    /* 0x39                 */ "9",
    /* 0x3a                 */ ":",
    /* 0x3b                 */ ";",
    /* 0x3c                 */ "<",
    /* 0x3d                 */ "=",
    /* 0x3e                 */ ">",
    /* 0x3f                 */ "?",
    /* 0x40                 */ "@",
    /* 0x41                 */ "A",
    /* 0x42                 */ "B",
    /* 0x43                 */ "C",
    /* 0x44                 */ "D",
    /* 0x45                 */ "E",
    /* 0x46                 */ "F",
    /* 0x47                 */ "G",
    /* 0x48                 */ "H",
    /* 0x49                 */ "I",
    /* 0x4a                 */ "J",
    /* 0x4b                 */ "K",
    /* 0x4c                 */ "L",
    /* 0x4d                 */ "M",
    /* 0x4e                 */ "N",
    /* 0x4f                 */ "O",
    /* 0x50                 */ "P",
    /* 0x51                 */ "Q",
    /* 0x52                 */ "R",
    /* 0x53                 */ "S",
    /* 0x54                 */ "T",
    /* 0x55                 */ "U",
    /* 0x56                 */ "V",
    /* 0x57                 */ "W",
    /* 0x58                 */ "X",
    /* 0x59                 */ "Y",
    /* 0x5a                 */ "Z",
    /* 0x5b                 */ "[",
    /* 0x5c                 */ "\\",
    /* 0x5d                 */ "]",
    /* 0x5e                 */ "^",
    /* 0x5f                 */ "_",
    /* 0x60                 */ "`",
    /* 0x61                 */ "a",
    /* 0x62                 */ "b",
    /* 0x63                 */ "c",
    /* 0x64                 */ "d",
    /* 0x65                 */ "e",
    /* 0x66                 */ "f",
    /* 0x67                 */ "g",
    /* 0x68                 */ "h",
    /* 0x69                 */ "i",
    /* 0x6a                 */ "j",
    /* 0x6b                 */ "k",
    /* 0x6c                 */ "l",
    /* 0x6d                 */ "m",
    /* 0x6e                 */ "n",
    /* 0x6f                 */ "o",
    /* 0x70                 */ "p",
    /* 0x71                 */ "q",
    /* 0x72                 */ "r",
    /* 0x73                 */ "s",
    /* 0x74                 */ "t",
    /* 0x75                 */ "u",
    /* 0x76                 */ "v",
    /* 0x77                 */ "w",
    /* 0x78                 */ "x",
    /* 0x79                 */ "y",
    /* 0x7a                 */ "z",
    /* 0x7b                 */ "{",
    /* 0x7c                 */ "|",
    /* 0x7d                 */ "}",
    /* 0x7e                 */ "~",
    /* 0x7f    2509140 hits */ "40",
    /* 0x80    2497171 hits */ "4.",
    /* 0x81    2426650 hits */ "02",
    /* 0x82    2415225 hits */ "51",
    /* 0x83    2303932 hits */ "05",
    /* 0x84    2256012 hits */ "d$",
    /* 0x85    2239693 hits */ "e$",
    /* 0x86    2232438 hits */ "b$",
    /* 0x87    2231411 hits */ "a$",
    /* 0x88    2230470 hits */ "f$",
    /* 0x89    2228138 hits */ "03",
    /* 0x8a    2222446 hits */ "c$",
    /* 0x8b    2205881 hits */ "21",
    /* 0x8c    2205116 hits */ "04",
    /* 0x8d    2190925 hits */ "90",
    /* 0x8e    2167432 hits */ "3.",
    /* 0x8f    2161465 hits */ "19",
    /* 0x90    2158145 hits */ "13",
    /* 0x91    2139208 hits */ "15",
    /* 0x92    2136908 hits */ "14",
    /* 0x93    2135990 hits */ "57",
    /* 0x94    2135882 hits */ "30",
    /* 0x95    1059596 hits */ "200",
    /* 0x96    2106657 hits */ "k$",
    /* 0x97    2071552 hits */ "85",
    /* 0x98    2071334 hits */ "65",
    /* 0x99    2070410 hits */ "$6",
    /* 0x9a    2060188 hits */ "1.",
    /* 0x9b    2057047 hits */ "l$",
    /* 0x9c    2051911 hits */ "41",
    /* 0x9d    2046009 hits */ "v$",
    /* 0x9e    2046000 hits */ "78",
    /* 0x9f    2044337 hits */ "h$",
    /* 0xa0    2043550 hits */ "t$",
    /* 0xa1    2043183 hits */ "i$",
    /* 0xa2    2042554 hits */ "j$",
    /* 0xa3    2042277 hits */ "n$",
    /* 0xa4    2041055 hits */ "p$",
    /* 0xa5    2040096 hits */ "s$",
    /* 0xa6    2039319 hits */ "g$",
    /* 0xa7    2038759 hits */ "r$",
    /* 0xa8    2038716 hits */ "m$",
    /* 0xa9    2038629 hits */ "rn",
    /* 0xaa    2037282 hits */ "o$",
    /* 0xab    1018463 hits */ "3$1",
    /* 0xac    2036741 hits */ "q$",
    /* 0xad    2035758 hits */ "39",
    /* 0xae    2035690 hits */ "16",
    /* 0xaf    2035557 hits */ "sl",
    /* 0xb0    2033007 hits */ "u$",
    /* 0xb1    2031097 hits */ "38",
    /* 0xb2    2030313 hits */ "37",
    /* 0xb3    2027926 hits */ "lr",
    /* 0xb4    1012087 hits */ "4$1",
    /* 0xb5    2021963 hits */ "22",
    /* 0xb6    1010733 hits */ "2$1",
    /* 0xb7    1010014 hits */ "1$1",
    /* 0xb8    2018141 hits */ "$3",
    /* 0xb9    1007159 hits */ "7$1",
    /* 0xba    2012730 hits */ "dl",
    /* 0xbb    1006318 hits */ "5$1",
    /* 0xbc    1005040 hits */ "d$1",
    /* 0xbd    1004084 hits */ "6$1",
    /* 0xbe    2006101 hits */ "50",
    /* 0xbf    1000924 hits */ "8$1",
    /* 0xc0    1000828 hits */ "9$1",
    /* 0xc1     996564 hits */ "a$1",
    /* 0xc2     995745 hits */ "e$1",
    /* 0xc3    1991037 hits */ "17",
    /* 0xc4     995045 hits */ "f$1",
    /* 0xc5     994810 hits */ "b$1",
    /* 0xc6     991771 hits */ "c$1",
    /* 0xc7    1966749 hits */ "80",
    /* 0xc8    1946107 hits */ "18",
    /* 0xc9    1943309 hits */ "42",
    /* 0xca    1919152 hits */ "09",
    /* 0xcb    1904555 hits */ "60",
    /* 0xcc    1902396 hits */ "lg",
    /* 0xcd    1900874 hits */ "06",
    /* 0xce     947731 hits */ "k$1",
    /* 0xcf     947663 hits */ "112",
    /* 0xd0     943819 hits */ "l$1",
    /* 0xd1    1886422 hits */ "36",
    /* 0xd2     937646 hits */ "s$1",
    /* 0xd3     937113 hits */ "h$1",
    /* 0xd4     936493 hits */ "o$1",
    /* 0xd5     936475 hits */ "g$1",
    /* 0xd6     936439 hits */ "v$1",
    /* 0xd7     936234 hits */ "p$1",
    /* 0xd8    1872067 hits */ "43",
    /* 0xd9     935847 hits */ "m$1",
    /* 0xda     935837 hits */ "i$1",
    /* 0xdb     935532 hits */ "j$1",
    /* 0xdc     935455 hits */ "r$1",
    /* 0xdd     935405 hits */ "t$1",
    /* 0xde     935053 hits */ "n$1",
    /* 0xdf     934789 hits */ "q$1",
    /* 0xe0     933857 hits */ "u$1",
    /* 0xe1    1860398 hits */ "07",
    /* 0xe2    1859860 hits */ "44",
    /* 0xe3    1854508 hits */ "31",
    /* 0xe4    1852884 hits */ "70",
    /* 0xe5    1839121 hits */ "08",
    /* 0xe6    1835250 hits */ "23",
    /* 0xe7     905588 hits */ "$0$",
    /* 0xe8    1809607 hits */ "99",
    /* 0xe9    1805184 hits */ "35",
    /* 0xea    1797672 hits */ "91",
    /* 0xeb    1772576 hits */ ".d",
    /* 0xec    1763691 hits */ "24",
    /* 0xed    1762762 hits */ "34",
    /* 0xee    1761310 hits */ "25",
    /* 0xef    1760450 hits */ "45",
    /* 0xf0     879101 hits */ "100",
    /* 0xf1    1755306 hits */ "2.",
    /* 0xf2    1737279 hits */ "$4",
    /* 0xf3    1706949 hits */ "98",
    /* 0xf4    1697406 hits */ "0.",
    /* 0xf5    1697191 hits */ "32",
    /* 0xf6    1692796 hits */ "26",
    /* 0xf7    1688697 hits */ "92",
    /* 0xf8    1678751 hits */ "33",
    /* 0xf9     829273 hits */ "001",
    /* 0xfa    1649380 hits */ "46",
    /* 0xfb    1638358 hits */ "27",
    /* 0xfc    1636304 hits */ "81",
    /* 0xfd    1636004 hits */ "93",
    /* 0xfe    1633194 hits */ "96",
    /* 0xff    1621619 hits */ "47",
};

enum { BigramSize = 116 };
enum { TrigramSize = 43 };

static const char* BigramTable =
    "$1"
    "$2"
    "$3"
    "$4"
    "$6"
    ".0"
    ".1"
    ".2"
    ".3"
    ".4"
    ".d"
    "0$"
    "0."
    "00"
    "01"
    "02"
    "03"
    "04"
    "05"
    "06"
    "07"
    "08"
    "09"
    "1$"
    "1."
    "10"
    "11"
    "12"
    "13"
    "14"
    "15"
    "16"
    "17"
    "18"
    "19"
    "2$"
    "2."
    "20"
    "21"
    "22"
    "23"
    "24"
    "25"
    "26"
    "27"
    "3$"
    "3."
    "30"
    "31"
    "32"
    "33"
    "34"
    "35"
    "36"
    "37"
    "38"
    "39"
    "4$"
    "4."
    "40"
    "41"
    "42"
    "43"
    "44"
    "45"
    "46"
    "47"
    "5$"
    "50"
    "51"
    "57"
    "6$"
    "60"
    "65"
    "7$"
    "70"
    "78"
    "8$"
    "80"
    "81"
    "85"
    "9$"
    "90"
    "91"
    "92"
    "93"
    "96"
    "98"
    "99"
    "a$"
    "b$"
    "c$"
    "d$"
    "dl"
    "e$"
    "f$"
    "g$"
    "h$"
    "i$"
    "j$"
    "k$"
    "l$"
    "lg"
    "lr"
    "m$"
    "n$"
    "o$"
    "p$"
    "q$"
    "r$"
    "rn"
    "s$"
    "sl"
    "t$"
    "u$"
    "v$";

static uint8_t BigramIndex[BigramSize] = {
    2,
    7,
    184,
    242,
    153,
    18,
    5,
    20,
    21,
    22,
    235,
    6,
    244,
    4,
    8,
    129,
    137,
    140,
    131,
    205,
    225,
    229,
    202,
    25,
    154,
    11,
    9,
    15,
    144,
    146,
    145,
    174,
    195,
    200,
    143,
    27,
    241,
    14,
    139,
    181,
    230,
    236,
    238,
    246,
    251,
    23,
    142,
    148,
    227,
    245,
    248,
    237,
    233,
    209,
    178,
    177,
    173,
    24,
    128,
    127,
    156,
    201,
    216,
    226,
    239,
    250,
    255,
    29,
    190,
    130,
    147,
    28,
    203,
    152,
    26,
    228,
    158,
    31,
    199,
    252,
    151,
    30,
    141,
    234,
    247,
    253,
    254,
    243,
    232,
    135,
    134,
    138,
    132,
    186,
    133,
    136,
    166,
    159,
    161,
    162,
    150,
    155,
    204,
    179,
    168,
    163,
    170,
    164,
    172,
    167,
    169,
    165,
    175,
    160,
    176,
    157,
};

static const char* TrigramTable =
    "$0$"
    ".00"
    ".dl"
    "0$1"
    "000"
    "001"
    "1$1"
    "100"
    "112"
    "2$1"
    "200"
    "3$1"
    "4$1"
    "5$1"
    "6$1"
    "7$1"
    "8$1"
    "9$1"
    "a$1"
    "b$1"
    "c$1"
    "d$1"
    "dlg"
    "e$1"
    "f$1"
    "g$1"
    "h$1"
    "i$1"
    "j$1"
    "k$1"
    "l$1"
    "lrn"
    "m$1"
    "n$1"
    "o$1"
    "p$1"
    "q$1"
    "r$1"
    "s$1"
    "slr"
    "t$1"
    "u$1"
    "v$1";

static uint8_t TrigramIndex[TrigramSize] = {
    231,
    10,
    17,
    19,
    3,
    249,
    183,
    240,
    207,
    182,
    149,
    171,
    180,
    187,
    189,
    185,
    191,
    192,
    193,
    197,
    198,
    188,
    16,
    194,
    196,
    213,
    211,
    218,
    219,
    206,
    208,
    12,
    217,
    222,
    212,
    215,
    223,
    220,
    210,
    13,
    221,
    224,
    214,
};


StringCompress::StringCompress( const std::string& fn )
{
    FILE* f = fopen( fn.c_str(), "rb" );
    assert( f );

    fread( &m_dataLen, 1, sizeof( m_dataLen ), f );
    auto data = new char[m_dataLen];
    fread( data, 1, m_dataLen, f );
    m_data = data;
    fread( &m_maxHost, 1, sizeof( m_maxHost ), f );
    fread( m_hostLookup, 1, m_maxHost * sizeof( uint8_t ), f );
    fread( m_hostOffset, 1, m_maxHost * sizeof( uint32_t ), f );
    fread( m_hostHash, 1, HashSize * sizeof( uint8_t ), f );

    fclose( f );
}

StringCompress::StringCompress( const FileMapPtrs& ptrs )
{
    FileMap<char> f( ptrs );
    memcpy( &m_dataLen, f, sizeof( m_dataLen ) );
    auto data = new char[m_dataLen];
    uint32_t offset = sizeof( m_dataLen );
    memcpy( data, f+offset, m_dataLen );
    m_data = data;
    offset += m_dataLen;
    memcpy( &m_maxHost, f+offset, sizeof( m_maxHost ) );
    offset += sizeof( m_maxHost );
    memcpy( m_hostLookup, f+offset, m_maxHost * sizeof( uint8_t ) );
    offset += m_maxHost * sizeof( uint8_t );
    memcpy( m_hostOffset, f+offset, m_maxHost * sizeof( uint32_t ) );
    offset += m_maxHost * sizeof( uint32_t );
    memcpy( m_hostHash, f+offset, HashSize * sizeof( uint8_t ) );
}

StringCompress::~StringCompress()
{
    delete[] m_data;
}

static inline bool memcmp3( const char* l, const char* r )
{
    if( l[0] < r[0] ) return true;
    if( l[0] > r[0] ) return false;
    if( l[1] < r[1] ) return true;
    if( l[1] > r[1] ) return false;
    return l[2] < r[2];
}

static inline bool memcmp3_0( const char* l, const char* r )
{
    return l[0] == r[0] && l[1] == r[1] && l[2] == r[2];
}

static inline bool memcmp2( const char* l, const char* r )
{
    if( l[0] < r[0] ) return true;
    if( l[0] > r[0] ) return false;
    return l[1] < r[1];
}

static inline bool memcmp2_0( const char* l, const char* r )
{
    return l[0] == r[0] && l[1] == r[1];
}

static inline int lower_bound3( const char* value )
{
    int first = 0;
    int it, step;
    int count = TrigramSize;
    while( count > 0 )
    {
        it = first;
        step = count / 2;
        it += step;
        if( memcmp3( TrigramTable + (it*3), value ) )
        {
            first = ++it;
            count -= step + 1;
        }
        else
        {
            count = step;
        }
    }
    return first;
}

static inline int lower_bound2( const char* value )
{
    int first = 0;
    int it, step;
    int count = BigramSize;
    while( count > 0 )
    {
        it = first;
        step = count / 2;
        it += step;
        if( memcmp2( BigramTable + (it*2), value ) )
        {
            first = ++it;
            count -= step + 1;
        }
        else
        {
            count = step;
        }
    }
    return first;
}

size_t StringCompress::Pack( const char* in, uint8_t* out ) const
{
    const uint8_t* refout = out;

    while( *in != 0 )
    {
        if( *in != '@' )
        {
            if( *in == '$' ||
                *in == '.' ||
                ( *in >= '0' && *in <= '9' ) ||
                ( *in >= 'a' && *in <= 'v' ) )
            {
                auto it3 = lower_bound3( in );
                if( it3 != TrigramSize && memcmp3_0( TrigramTable + (it3*3), in ) )
                {
                    *out++ = TrigramIndex[it3];
                    in += 3;
                    continue;
                }
                auto it2 = lower_bound2( in );
                if( it2 != BigramSize && memcmp2_0( BigramTable + (it2*2), in ) )
                {
                    *out++ = BigramIndex[it2];
                    in += 2;
                    continue;
                }
            }
            assert( *in >= 32 && *in <= 126 );
            *out++ = *in++;
        }
        else
        {
            auto test = in+1;
            const auto hash = XXH32( test, strlen( test ), 0 ) % HashSize;
            if( m_hostHash[hash] >= HostReserve )
            {
                if( strcmp( test, m_data + m_hostOffset[m_hostHash[hash] - HostReserve] ) == 0 )
                {
                    *out++ = 1;
                    *out++ = m_hostHash[hash];
                }
                else
                {
                    *out++ = '@';
                    in++;
                    while( *in != '\0' ) *out++ = *in++;
                }
            }
            else if( m_hostHash[hash] == BadHashMark )
            {
                auto it = std::lower_bound( m_hostLookup, m_hostLookup + m_maxHost, test, [this] ( const auto& l, const auto& r ) { return strcmp( m_data + m_hostOffset[l], r ) < 0; } );
                if( it != m_hostLookup + m_maxHost && strcmp( m_data + m_hostOffset[*it], test ) == 0 )
                {
                    *out++ = 1;
                    *out++ = (*it) + HostReserve;
                }
                else
                {
                    *out++ = '@';
                    in++;
                    while( *in != '\0' ) *out++ = *in++;
                }
            }
            else
            {
                *out++ = '@';
                in++;
                while( *in != '\0' ) *out++ = *in++;
            }
            break;
        }
    }

    *out++ = 0;

    return out - refout;
}

size_t StringCompress::Unpack( const uint8_t* in, char* out ) const
{
    const char* refout = out;

    while( *in != 0 )
    {
        if( *in >= 32 && *in <= 126 )
        {
            assert( CodeBook[*in][0] == *in );
            assert( CodeBook[*in][1] == '\0' );
            if( *in != '@' )
            {
                *out++ = *in++;
            }
            else
            {
                *out++ = *in++;
                while( *in != '\0' ) *out++ = *in++;
                break;
            }
        }
        else if( *in != 1 )
        {
            const char* dec = CodeBook[*in++];
            *out++ = *dec++;
            *out++ = *dec++;
            if( *dec != '\0' )
            {
                *out++ = *dec;
            }
        }
        else
        {
            in++;
            *out++ = '@';
            const char* dec = m_data + m_hostOffset[(*in) - HostReserve];
            while( *dec != '\0' ) *out++ = *dec++;
            assert( *++in == 0 );
            break;
        }
    }

    *out++ = '\0';

    return out - refout;
}

size_t StringCompress::Repack( const uint8_t* in, uint8_t* out, const StringCompress& other ) const
{
    char tmp[2048];
    other.Unpack( in, tmp );
    return Pack( tmp, out );
}

void StringCompress::WriteData( const std::string& fn ) const
{
    FILE* f = fopen( fn.c_str(), "wb" );
    assert( f );

    fwrite( &m_dataLen, 1, sizeof( m_dataLen ), f );
    fwrite( m_data, 1, m_dataLen, f );
    fwrite( &m_maxHost, 1, sizeof( m_maxHost ), f );
    fwrite( m_hostLookup, 1, m_maxHost * sizeof( uint8_t ), f );
    fwrite( m_hostOffset, 1, m_maxHost * sizeof( uint32_t ), f );
    fwrite( m_hostHash, 1, HashSize * sizeof( uint8_t ), f );

    fclose( f );
}

void StringCompress::BuildHostHash()
{
    memset( m_hostHash, 0, HashSize );
    for( int i=0; i<m_maxHost; i++ )
    {
        const auto host = m_data + m_hostOffset[i];
        const auto hash = XXH32( host, strlen( host ), 0 ) % HashSize;
        if( m_hostHash[hash] == 0 )
        {
            m_hostHash[hash] = i + HostReserve;
        }
        else
        {
            m_hostHash[hash] = BadHashMark;
        }
    }
}
