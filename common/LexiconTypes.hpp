#ifndef __LEXICONTYPES_HPP__
#define __LEXICONTYPES_HPP__

#include <algorithm>
#include <stdint.h>

enum LexiconType
{
    T_Content,
    T_Quote1,
    T_Quote2,
    T_Quote3,
    T_Signature,
    T_Subject,
    T_From,
    T_Wrote,
    NUM_LEXICON_TYPES
};

enum { T_All = 0xFF };

enum { LexiconMinLen = 3 };
enum { LexiconMaxLen = 13 };

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
        switch( v & 0x18 )
        {
        case 0x00:
            return T_Signature;
        case 0x08:
            return T_Subject;
        case 0x10:
            return T_From;
        case 0x18:
        default:
            return T_Wrote;
        }
    }
}

static inline LexiconType LexiconTypeFromQuotLevel( int level )
{
    switch( level )
    {
    case 0:
        return T_Content;
    case 1:
        return T_Quote1;
    case 2:
        return T_Quote2;
    default:
        return T_Quote3;
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

static inline uint8_t LexiconHitPos( uint8_t v )
{
    auto type = LexiconDecodeType( v );
    return v & LexiconHitPosMask[type];
}

static inline bool LexiconHitIsMaxPos( uint8_t v )
{
    auto type = LexiconDecodeType( v );
    return ( v & LexiconHitPosMask[type] ) == LexiconHitPosMask[type];
}

static inline uint32_t LexiconTransformChildNum( uint32_t children )
{
    return std::min<uint32_t>( LexiconChildMax * 8, children ) / 8;
}

#endif
