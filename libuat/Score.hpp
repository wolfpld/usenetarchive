#ifndef __SCORE_HPP__
#define __SCORE_HPP__

#include <stdint.h>
#include <string>

enum ScoreField
{
    SF_RealName,
    SF_From,
    SF_Subject
};

struct ScoreEntry
{
    int32_t score : 31;
    int32_t exact : 1;
    ScoreField field;
    std::string match;
};

#endif
