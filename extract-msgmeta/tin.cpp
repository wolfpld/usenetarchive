/** Code adapted from tin 2.2.1, http://www.tin.org/ **
 *
 * Copyright (c) 1991-2014 Iain Lea <iain@bricbrac.de>, Rich Skrenta <skrenta@pbm.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>

#include "tin.hpp"

enum { HEADER_LEN = 1024 };

/* defines for GNKSA checking */
/* success/undefined failure */
#define GNKSA_OK			0
#define GNKSA_INTERNAL_ERROR		1
/* general syntax */
#define GNKSA_LANGLE_MISSING		100
#define GNKSA_LPAREN_MISSING		101
#define GNKSA_RPAREN_MISSING		102
#define GNKSA_ATSIGN_MISSING		103
/* FQDN checks */
#define GNKSA_SINGLE_DOMAIN		200
#define GNKSA_INVALID_DOMAIN		201
#define GNKSA_ILLEGAL_DOMAIN		202
#define GNKSA_UNKNOWN_DOMAIN		203
#define GNKSA_INVALID_FQDN_CHAR		204
#define GNKSA_ZERO_LENGTH_LABEL		205
#define GNKSA_ILLEGAL_LABEL_LENGTH	206
#define GNKSA_ILLEGAL_LABEL_HYPHEN	207
#define GNKSA_ILLEGAL_LABEL_BEGNUM	208
#define GNKSA_BAD_DOMAIN_LITERAL	209
#define GNKSA_LOCAL_DOMAIN_LITERAL	210
#define GNKSA_RBRACKET_MISSING		211
/* localpart checks */
#define GNKSA_LOCALPART_MISSING		300
#define GNKSA_INVALID_LOCALPART		301
#define GNKSA_ZERO_LENGTH_LOCAL_WORD	302
/* realname checks */
#define GNKSA_ILLEGAL_UNQUOTED_CHAR	400
#define GNKSA_ILLEGAL_QUOTED_CHAR	401
#define GNKSA_ILLEGAL_ENCODED_CHAR	402
#define GNKSA_BAD_ENCODE_SYNTAX		403
#define GNKSA_ILLEGAL_PAREN_CHAR		404
#define GNKSA_INVALID_REALNAME		405

/* address types */
#define GNKSA_ADDRTYPE_ROUTE	0
#define GNKSA_ADDRTYPE_OLDSTYLE	1

 /*
 * split mail address into realname and address parts
 */
static int
gnksa_split_from(
    const char *from,
    char *address,
    char *realname,
    int *addrtype)
{
    char *addr_begin;
    char *addr_end;
    char work[HEADER_LEN];

    /* init return variables */
    *address = *realname = '\0';

    /* copy raw address into work area */
    strncpy(work, from, HEADER_LEN - 2);
    work[HEADER_LEN - 2] = '\0';
    work[HEADER_LEN - 1] = '\0';

    /* skip trailing whitespace */
    addr_end = work + strlen(work) - 1;
    while (addr_end >= work && (' ' == *addr_end || '\t' == *addr_end))
        addr_end--;

    if (addr_end < work) {
        *addrtype = GNKSA_ADDRTYPE_OLDSTYLE;
        return GNKSA_LPAREN_MISSING;
    }

    *(addr_end + 1) = '\0';
    *(addr_end + 2) = '\0';

    if ('>' == *addr_end) {
        /* route-address used */
        *addrtype = GNKSA_ADDRTYPE_ROUTE;

        /* get address part */
        addr_begin = addr_end;
        while (('<' != *addr_begin) && (addr_begin > work))
            addr_begin--;

        if ('<' != *addr_begin) /* syntax error in mail address */
            return GNKSA_LANGLE_MISSING;

        /* copy route address */
        *addr_end = *addr_begin = '\0';
        strcpy(address, addr_begin + 1);

        /* get realname part */
        addr_end = addr_begin - 1;
        addr_begin = work;

        /* strip surrounding whitespace */
        while (addr_end >= work && (' ' == *addr_end || '\t' == *addr_end))
            addr_end--;

        while ((' ' == *addr_begin) || ('\t' == *addr_begin))
            addr_begin++;

        *++addr_end = '\0';
        /* copy realname */
        strcpy(realname, addr_begin);
    } else {
        /* old-style address used */
        *addrtype = GNKSA_ADDRTYPE_OLDSTYLE;

        /* get address part */
        /* skip leading whitespace */
        addr_begin = work;
        while ((' ' == *addr_begin) || ('\t' == *addr_begin))
            addr_begin++;

        /* scan forward to next whitespace or null */
        addr_end = addr_begin;
        while ((' ' != *addr_end) && ('\t' != *addr_end) && (*addr_end))
            addr_end++;

        *addr_end = '\0';
        /* copy route address */
        strcpy(address, addr_begin);

        /* get realname part */
        addr_begin = addr_end + 1;
        addr_end = addr_begin + strlen(addr_begin) -1;
        /* strip surrounding whitespace */
        while ((' ' == *addr_end) || ('\t' == *addr_end))
            addr_end--;

        while ((' ' == *addr_begin) || ('\t' == *addr_begin))
            addr_begin++;

        /* any realname at all? */
        if (*addr_begin) {
            /* check for parentheses */
            if ('(' != *addr_begin)
                return GNKSA_LPAREN_MISSING;

            if (')' != *addr_end)
                return GNKSA_RPAREN_MISSING;

            /* copy realname */
            *addr_end = '\0';
            strcpy(realname, addr_begin + 1);
        }
    }

    /*
    * if we allow <> as From: we must disallow <> as Mesage-ID,
    * see code in post.c:check_article_to_be_posted()
    */
#if 0
    if (!strchr(address, '@') && *address) /* check for From: without an @ but allow <> */
#else
    if (!strchr(address, '@')) /* check for From: without an @ */
#endif /* 0 */
        return GNKSA_ATSIGN_MISSING;

    /* split successful */
    return GNKSA_OK;
}

static void replace( std::string& str, const char* from, size_t fromsize, const char* to, size_t tosize )
{
    size_t pos = 0;
    while( ( pos = str.find( from, pos ) ) != std::string::npos )
    {
        str.replace( pos, fromsize, to );
        pos += tosize;
    }
}

std::string GetUserName( const char* from )
{
    char address[HEADER_LEN];
    char realname[HEADER_LEN];
    int addrtype;
    gnksa_split_from( from, address, realname, &addrtype );
    if( *realname )
    {
        auto size = strlen( realname );
        auto str = realname;
        if( realname[0] == '\"' && realname[size-1] == '\"' )
        {
            realname[size-1] = '\0';
            str++;
        }
        std::string ret = str;
        replace( ret, "\\\"", 2, "\"", 1 );
        replace( ret, "\\(", 2, "(", 1 );
        replace( ret, "\\)", 2, ")", 1 );
        replace( ret, "\\\\", 2, "\\", 1 );
        return ret;
    }
    else
    {
        return address;
    }
}
