#ifndef __LEXICONTYPES_HPP__
#define __LEXICONTYPES_HPP__

#include <stdint.h>

enum LexiconType
{
    T_Content,
    T_Signature,
    T_Quote1,
    T_Quote2,
    T_Quote3,
    T_Header,
    NUM_LEXICON_TYPES
};

enum { T_All = 0xFF };

extern const char* LexiconNames[];
extern const float LexiconWeights[];

enum { LexiconPostMask = 0x07FFFFFF };
enum { LexiconChildMask = 0xF8000000 };
enum { LexiconChildShift = 27 };
enum { LexiconChildMax = 0x1F };
enum { LexiconHitShift = 30 };
enum { LexiconHitMask = 0xC0000000 };
enum { LexiconHitOffsetMask = 0x3FFFFFFF };

extern const uint8_t LexiconHitTypeEncoding[];
extern const uint8_t LexiconHitPosMask[];

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
    default:
        return (v&0x10) == 0 ? T_Signature : T_Header;
    }
}

struct LexiconMetaPacket
{
    uint32_t str;
    uint32_t data;
    uint32_t dataSize;
};

struct LexiconDataPacket
{
    uint32_t postid;
    uint32_t hitoffset;
};

static inline float LexiconHitRank( uint8_t v )
{
    auto type = LexiconDecodeType( v );
    auto pos = 1.f - float( v & LexiconHitPosMask[type] ) / LexiconHitPosMask[type];
    return LexiconWeights[type] * ( pos * 0.9f + 0.1f );
}

#endif
