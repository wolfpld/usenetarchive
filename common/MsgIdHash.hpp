#ifndef __MSGIDHASH_HPP__
#define __MSGIDHASH_HPP__

enum { MsgIdHashBits = 16 };
enum { MsgIdHashSize = 1 << MsgIdHashBits };
enum { MsgIdHashMask = MsgIdHashSize - 1 };

#endif
