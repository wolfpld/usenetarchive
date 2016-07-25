// Copyright 2004 Fidelis Assis
// Copyright 2001-2010 William S. Yerazunis.
//
//   This file is part of the CRM114 Library.
//
//   The CRM114 Library is free software: you can redistribute it and/or modify
//   it under the terms of the GNU Lesser General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
//   The CRM114 Library is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU Lesser General Public License for more details.
//
//   You should have received a copy of the GNU Lesser General Public License
//   along with the CRM114 Library.  If not, see <http://www.gnu.org/licenses/>.

//  include some standard files
#include "crm114_sysincludes.h"

//  include any local crm114 configuration file
#include "crm114_config.h"

//  include the crm114 data structures file
#include "crm114_structs.h"


//     strnhash - generate the hash of a string of length N
//     goals - fast, works well with short vars includng
//     letter pairs and palindromes, not crypto strong, generates
//     hashes that tend toward relative primality against common
//     hash table lengths (so taking the output of this function
//     modulo the hash table length gives a relatively uniform distribution
//
//     In timing tests, this hash function can hash over 10 megabytes
//     per second (using as text the full 2.4.9 linux kernel source)
//     hashing individual whitespace-delimited tokens, on a Transmeta
//     666 MHz.

// This is a more portable hash function, compatible with the original.
// It should return the same value both on 32 and 64 bit architectures.
// The return type was changed to unsigned long hashes, and the other
// parts of the code updated accordingly.
// -- Fidelis
//
//
// return: unsigned long -> unsigned int, following Bill's idea that int is
// likely to be 32 bits.
// hval: int32_t -> uint32_t, to get logical >> instead of arithmetic.
// tmp: unsigned int (was unsigned long?) -> uint32_t
// -- Kurt Hackenberg

unsigned int crm114_strnhash (const char *str, long len)
{
  long i;
  uint32_t hval;
  uint32_t tmp;

  // initialize hval
  hval = len;

  //  for each character in the incoming text:
  for ( i = 0; i < len; i++)
    {
      //    xor in the current byte against each byte of hval
      //    (which alone guarantees that every bit of input will have
      //    an effect on the output)

      tmp = str[i] & 0xFF;
      tmp = tmp | (tmp << 8) | (tmp << 16) | (tmp << 24);
      hval ^= tmp;

      //    add some bits out of the middle as low order bits.
      hval = hval + (( hval >> 12) & 0x0000ffff) ;

      //     swap most and min significative bytes
      tmp = (hval << 24) | (hval >> 24);
      hval &= 0x00ffff00;           // zero most and min significative bytes of hval
      hval |= tmp;                  // OR with swapped bytes

      //    rotate hval 3 bits to the left (thereby making the
      //    3rd msb of the above mess the msb of the output hash)
      hval = (hval << 3) | ((hval >> 29) & 0x7);
    }
  return (hval);
}
