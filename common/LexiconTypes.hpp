#ifndef __LEXICONTYPES_HPP__
#define __LEXICONTYPES_HPP__

enum LexiconType
{
    T_Content,
    T_Signature,
    T_Quote1,
    T_Quote2,
    T_Quote3,
    T_Header
};

const char* LexiconNames[] = {
    "Content",
    "Signature",
    "Quote1",
    "Quote2",
    "Quote3",
    "Header"
};

enum { LexiconPostMask = 0x07FFFFFF };
enum { LexiconChildMask = 0xF8000000 };
enum { LexiconChildShift = 27 };
enum { LexiconChildMax = 0x1F };

const uint8_t LexiconHitTypeEncoding[] = {
    0x00,   // 0000 0000
    0xE0,   // 1110 0000
    0x80,   // 1000 0000
    0xA0,   // 1010 0000
    0xC0,   // 1100 0000
    0xF0    // 1111 0000
};

const uint8_t LexiconHitPosMask[] = {
    0x7F,   // 0111 1111
    0x1F,   // 0001 1111
    0x1F,   // 0001 1111
    0x1F,   // 0001 1111
    0x0F,   // 0000 1111
    0x0F    // 0000 1111
};

static inline LexiconType LexiconDecodeType( uint8_t v )
{
    if( ( v & 0x80 ) == 0 ) return T_Content;
    switch( v & 0x60 )
    {
    case 0x00:
        return T_Quote1;
    case 0x20:
        return T_Quote2;
    case 0x40:
        return T_Quote3;
    case 0x60:
        return (v&0x10) == 0 ? T_Signature : T_Header;
    }
}

#endif
