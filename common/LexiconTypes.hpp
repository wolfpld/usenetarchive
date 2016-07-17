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

#endif
