//  crm114_structs.h  - structures for CRM114 library

// Copyright 2010 William S. Yerazunis.
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

#ifndef __CRM114_STRUCTS_H__
#define __CRM114_STRUCTS_H__


// Markov family hash table bucket, to hold a feature and its value.
typedef struct {
  unsigned int hash;
  unsigned int value;
} MARKOV_FEATUREBUCKET;


typedef struct {
  unsigned int hash;
  float value;
} WINNOW_FEATUREBUCKET;

#define ENTROPY_RESERVED_HEADER_LEN 1024
typedef struct {
  int firlatstart;
  int firlatlen;
  int nodestart;
  int nodeslen;
  int totalbits;
} ENTROPY_HEADER_STRUCT;

typedef struct mythical_entropy_alphabet_slot {
  long count;
  int nextcell;
} ENTROPY_ALPHABET_SLOT;

typedef struct mythical_entropy_cell {
  double fir_prior;
  int fir_larger;
  int fir_smaller;
  int firlat_slot;
  ENTROPY_ALPHABET_SLOT abet[ENTROPY_ALPHABET_SIZE];
} ENTROPY_FEATUREBUCKET;


//  hyperspace featurebucket
typedef struct mythical_hyperspace_cell {
  unsigned int hash;
} HYPERSPACE_FEATUREBUCKET;

//   fscm - fast substring compression match
//   Our two data "structures".  Yeah, they've been pared
//   down to the minimum needed, which is just as it should be.
//
//    This is an FSCM prefix cell:
typedef struct {
  unsigned int index;
} FSCM_PREFIX_TABLE_CELL;
typedef struct {
  unsigned int next;
} FSCM_HASH_CHAIN_CELL;


//   Statistical Boosting - meta-classifier algorithm to
//   stack a bunch of classes to get a classifier with improved
//   accuracy.  This means that the meta-classifier needs to keep
//   complete copies of all of the training data, which is what we
//   do here.
//
//   Block 0 is used for the "augmented index"; here's what an 
//   augmented index cell looks like.  Block 1 are the feature rows
//   pointed to by start/len in block 0.
typedef struct {
  unsigned int start;  // where in block 1 do the feature rows start?
  unsigned int len;    // how many elements in the feature row?
  int trueclass;       // what is the true class of this data element?
  int evalto;          // how many classifiers have been evalled for following?
  int votesum;         // what is the vote sum of this element (one-vs-others)
  double prsum;        // what is the pR sum of this element (one-vs-others)
  double prob;         // what cumulative probability of this (one-vs-others)
} BOOST_INDEX_CELL;


//      FLAGS FLAGS FLAGS
//       all of the valid CRM114 flags are listed here
//
//      GROT GROT GROT
// The program/language crm114 uses these too.  When it starts calling
// this library, these will have to be reintegrated.  Or even better, make
// these classifier flags disjoint from the language flags.  There's
// already the issue that CRM114_NOCASE "works" in tokenization, but 
// doesn't generate case-free tokens.  Also, APPEND, NOMULTILINE, 
// FROMSTART etc. are used in both the engine and the library, and those
// uses need to not clash.

// These values were defined as, eg, (1LLU << 5).  But apparently
// Microsoft's C compiler (Visual Studio C++) is C89 with some
// extensions.  Apparently those extensions include type long long and
// constant type modifier LL, but not LLU.  So these are now rewritten
// in a way that perhaps Microsoft will understand.

//     match searchstart flags
#define CRM114_FROMSTART     ((unsigned long long)1 << 0) // run solver from zero, no incremental
#define CRM114_FROMNEXT      ((unsigned long long)1 << 1)
#define CRM114_FROMEND       ((unsigned long long)1 << 2)
#define CRM_NEWEND        ((unsigned long long)1 << 3)
#define CRM114_FROMCURRENT   ((unsigned long long)1 << 4)
//         match control flags
#define CRM114_NOCASE        ((unsigned long long)1 << 5)  // also used by libcrm114
#define CRM114_ABSENT        ((unsigned long long)1 << 6)
#define CRM114_BASIC         ((unsigned long long)1 << 7)
#define CRM114_BACKWARDS     ((unsigned long long)1 << 8)
#define CRM114_LITERAL       ((unsigned long long)1 << 9)  // also used by libcrm114
#define CRM114_NOMULTILINE   ((unsigned long long)1 << 10)


//         input/output/window flags

#if 0
// not used in library
#define CRM114_BYLINE        CRM114_NOMULTILINE
#endif

// CRM114_STRING is used in crm114_cb_setflags() to pick a regex and matrix.

#define CRM114_BYCHAR        ((unsigned long long)1 << 11)
#define CRM114_STRING        CRM114_BYCHAR     // string is bychar.  I think...

#if 0
// not used in library
#define CRM114_BYCHUNK       ((unsigned long long)1 << 12)
#define CRM114_BYEOF         ((unsigned long long)1 << 13)
#define CRM114_EOFACCEPTS    ((unsigned long long)1 << 14)
#define CRM114_EOFRETRY      ((unsigned long long)1 << 15)
#endif
#define CRM114_APPEND        ((unsigned long long)1 << 16)   // used for PCA&SVM to do incremental
#if 0	// if used in library
//           process control flags
#define CRM114_KEEP          ((unsigned long long)1 << 17)
#define CRM114_ASYNC         ((unsigned long long)1 << 18)

#endif

//        learn and classify
#define CRM114_REFUTE        ((unsigned long long)1 << 19)
#define CRM114_MICROGROOM    ((unsigned long long)1 << 20)
#define CRM114_MARKOVIAN     ((unsigned long long)1 << 21)
#define CRM114_OSB_BAYES     ((unsigned long long)1 << 22)       // synonym with OSB feature gen
#define CRM114_OSB           CRM114_OSB_BAYES
#define CRM114_CORRELATE     ((unsigned long long)1 << 23)
#define CRM114_OSB_WINNOW    ((unsigned long long)1 << 24)      //  synonym to Winnow feature combiner
#define CRM114_WINNOW        CRM114_OSB_WINNOW
#define CRM114_CHI2          ((unsigned long long)1 << 25)
#define CRM114_UNIQUE        ((unsigned long long)1 << 26)
#define CRM114_ENTROPY       ((unsigned long long)1 << 27)
#define CRM114_OSBF          ((unsigned long long)1 << 28)     // synonym with OSBF local rule
#define CRM114_OSBF_BAYES    CRM114_OSBF
#define CRM114_HYPERSPACE    ((unsigned long long)1 << 29)
#define CRM114_UNIGRAM       ((unsigned long long)1 << 30)
#define CRM114_CROSSLINK     ((unsigned long long)1 << 31)
//
//        Flags that need to be sorted back in
//           input
#define CRM114_READLINE      ((unsigned long long)1 << 32)
//           isolate flags
#define CRM114_DEFAULT       ((unsigned long long)1 << 33)
//           SVM classifier
#define CRM114_SVM           ((unsigned long long)1 << 35)
//           FSCM classifier
#define CRM114_FSCM          ((unsigned long long)1 << 36)
//           Neural Net classifier
#define CRM114_NEURAL_NET    ((unsigned long long)1 << 37)
//
#define CRM114_ERASE         ((unsigned long long)1 << 38)
//PCA classifier
#define CRM114_PCA           ((unsigned long long)1 << 39)
// Statistical Boosting meta-classifier
#define CRM114_BOOST         ((unsigned long long)1 << 40)

// keep this in sync with flag definitions
#define CRM114_FLAGS_CLASSIFIERS_MASK		\
  ( CRM114_MARKOVIAN				\
    | CRM114_OSB_BAYES				\
    | CRM114_CORRELATE				\
    | CRM114_OSB_WINNOW				\
    | CRM114_ENTROPY				\
    | CRM114_OSBF				\
    | CRM114_HYPERSPACE				\
    | CRM114_SVM				\
    | CRM114_FSCM				\
    | CRM114_NEURAL_NET				\
    | CRM114_PCA                               \
    | CRM114_BOOST )


#endif	// !__CRM114_STRUCTS_H__
