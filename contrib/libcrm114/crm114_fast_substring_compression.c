// FSCM classifier.  Copyright 2001-2010 William S. Yerazunis.
//
//   This file is part of the CRM114 Library.
//
// The CRM114 Library is free software: you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.
//
//   The CRM114 Library is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with the CRM114 Library.  If not, see <http://www.gnu.org/licenses/>.

//  This file is derived (er, "cargo-culted") from crm114_markov.c via
//  crm114_hyperspace.c et. al, so some of the code may look really,
//  really familiar.
//
//

//  include some standard files
#include "crm114_sysincludes.h"

//  include any local crm114 configuration file
#include "crm114_config.h"

//  include the crm114 data structures file
#include "crm114_structs.h"

#include "crm114_lib.h"

#include "crm114_internal.h"



/////////////////////////////////////////////////////////////////
//
//           Compression-match Classification
//
//     This classifier is based on the use of the Lempel-Ziv LZ77
//     (published in 1977) algorithm for fast compression; more
//     compression implies better match.
//
//     The basic idea of LZ77 is to encode strings of N characters
//     as a small doublet (Nstart, Nlen), where Nstart and Nlen are
//     backward references into previously seen text.  If there's
//     no previous correct back-reference string, then don't compress
//     those characters.
//
//     Thus, LZ77 is a form of _universal_ compressor; it starts out knowing
//     nothing of what it's to compress, and develops compression
//     tables "on the fly".
//
//     It is well known that one way of doing text matching is to
//     compare relative compressibility - that is, given known
//     texts K1 and K2, the unknown text U is in the same class as K1
//     iff the LZ77 compression of K1|U is smaller (fewer bytes) than
//     the LZ77 compression of K2|U .  (remember you need to subtract
//     the compressed lengths of K1 and K2 respectively).
//
//     There are several ways to generate LZ compression fast; one
//     way is by forward pointers on N-letter prefixes.  Another
//     way is to decide on a maximum string depth and build transfer
//     tables.
//
//     One problem with LZ77 is that finding the best possible compression
//     is NP-hard.  Consider this example:
//
//         ABCDEFGHI DEFGHIJLMN BCDEFGHIJ JKLMNOPQ ABCDEFGHIJKLMNOP
//
//     Is it better to code the final part with the A-J segment
//     followed by the J-P segment, or with a literal ABC, then D-N,
//     then the literal string OP?  Without doing the actual math, you
//     can't easily decide which is the better compression.  In the
//     worst case, the problem becomes the "knapsack" problem and
//     is thus NP-hard.
//
//     To avoid this, we take the following heuristic for our first
//     coding attempt:
//
//          Look for the longest match of characters that
//          match the unknown at this point and use that.
//
//     In the worst case, this algorithm will traverse the entire
//     known text for each possible starting character, and possibly
//     do a local search if that starting character matches the
//     character in the unknown text.  Thus, the time to run is
//     bounded by |U| * |K|.
//
//     Depending on the degree of overlap between strings, this
//     heuristic will be at best no worse than half as good as the
//     best possible heuristic, and (handwave not-proven) not worse
//     than one quarter as good as the best possible heuristic.
//
//     As a speedup, we can use a prefix lookforward based on N
//     (the number of characters we reqire to match before switching
//     from literal characters to start/length ranges).   Each character
//     also carries an index, saying "for the N-character lookforward
//     prefix I am at the start of, you can find another example of this
//     at the following index."
//
//     For example, the string "ABC DEF ABC FGH ABC XYZ" would have
//     these entries inserted sequentially into the lookforward table:
//
//     ABC --> 0
//     BC  --> 1
//     C D --> 2
//      DE --> 3
//     DEF --> 4
//     EF  --> 5
//     F A --> 6
//      AB --> 7
//
//    At this point, note that "ABC" recurs again.  Since we want to
//    retain both references to "ABC" strings, we place the index of
//    the second ABC (== 8) into the "next occurrence" tag of the
//    first "ABC".  (or, more efficiently, set the second ABC to point
//    to the first ABC, and then have the lookforward table point to the
//    second ABC (thus, the chain of ABCs is actually in the reverse order
//    of their encounters).
//
//    For prefix lengths of 1, 2, and 3, the easiest method is to
//    direct-map the prefixes into a table.  The table lengths would
//    be 256, 65536, and 16 megawords (64 megabytes).  The first two are
//    eminently tractable; the third marginally so.  The situation
//    can be improved by looking only at the low order six bits of
//    the characters as addresses into the direct map table.  For normal
//    ASCII, this means that the control characters are mapped over the
//    capital letters, and the digits and punctuation are mapped over
//    lowercase letters, and uses up only 16 megabytes for the table entries.
//
//    Of course, a direct-mapped, 1:1 table is not the only possible
//    table.  It is also possible to create a hash table with overflow
//    chains.  For example, an initial two-character table (256Kbytes) yields
//    the start of a linear-search chain; this chain points to a linked list of
//    all of the third characters yet encountered.
//
//    Here's some empirical data to get an idea of the size of the
//    table actually required:
//
//    for SA's hard_ham
//       lines         words       three-byte sequences
//         114K          464K        121K
//          50K          210K         61K
//          25K          100K         47K
//          10K           47K         31K
//           5K           24K         21K
//
//    For SA's easy_ham_2:
//       lines         words       three-byte sequences
//         134K          675K          97K
//          25K          130K          28K
//
//    For SA's spam_2:
//       lines         words       three-byte sequences
//         197K          832K          211K
//         100K          435K          116K
//
//    So, it looks like in the long term (and in English) there is
//    an expectation of roughly as many 3-byte sequences as there are
//    lines of text, probably going asymptotic at around
//    a quarter of a million unique 3-byte sequences.  Note that
//    the real maximum is 2^24 or about 16 million three-byte
//    sequences; however some of them would never occur except in
//    binary encodings.
//
//
//     ----- Embedded Limited-Length Markov Table Option -----
//
//    Another method of accomplishing this (suitable for N of 3 and larger)
//    is to use transfer tables allocated only on an as-needed basis,
//    and to store the text in the tables directly (thus forming a sort of
//    Markov chain of successive characters in memory).
//
//    The root table contains pointers to the inital occurrence in the known
//    text of all 256 possible first bytes; each subsequent byte then
//    allocates another transfer table up to some predetermined limit on
//    length.
//
//    A disadvantage of this method is that it uses up more memory for
//    storage than the index chaining method; further it must (by necessity)
//    "cut off" at some depth thereby limiting the longest string that
//    we want to allocate another table for.  In the worst case, this
//    system generates a doubly diagonal tree with |K|^2 / 4 tables.
//    On the other hand, if there is a cutoff length L beyond which we
//    don't expand the tables, then only the tables that are needed get
//    allocated.  As a nice side effect, the example text becomes less
//    trivial to extract (although it's there, you have to write a
//    program to extract it rather than just using "strings", unlike the
//    bit entropy classifier where a well-populated array contains a lot
//    of uncertainty and it's very difficult to even get a single
//    byte unambiguously.
//
//    Some empirical data on this method:
//
//     N = 10
//       Text length (bytes)       Tables allocated
//           1211                       7198
//          69K                       232K
//          96K                       411K
//         204K                       791K
//
//     N=5
//       Text length (bytes)       Tables allocated
//            1210                      2368
//           42K                       47K
//           87K                       79K
//          183K                      114K
//          386K                      177K
//          841K                      245K
//         1800K                      342K
//         3566K                      488K
//         6070K                      954K
//         8806K                     1220K
//
//      N=4
//       Text length (bytes)       Tables allocated
//           87K                       40K
//          183K                       59K
//          338K                       89K
//          840K                      121K
//         1800K                      167K
//         3568K                      233K
//         6070K                      438K
//
//      N=3
//       Text length (bytes)       Tables allocated
//           87K                       14K
//          183K                       22K
//          386K                       31K
//          840K                       42K
//         1800K                       58K
//         3568K                       78K
//         6070K                      132K
//
//
//    Let's play with the numbers a little bit.  Note that
//    the 95 printable ASCII characters could have a theoretical
//    maximum of 857K sequences, and the full 128-character ASCII
//    (low bit off) is 2.09 mega-sequences.  If we casefold A-Z onto
//    a-z and all control characters to "space", then the resulting
//    69 characters is only 328K possible unique sequences.
//
//    A simpler method is to fold 0x7F-0x8F down onto 0x00-0x7F, and
//    then 0x00-0x3F onto 0x40-0x7F (yielding nothing but printable
//    characters- however, this does not preserve whitespace in any sense).
//    When folded this way, the SA hard_ham corpus (6 mbytes, 454 words, 114
//    K lines) yields 89,715 unique triplets.
//
//    Of course, for other languages, the statistical asymptote is
//    probably present, but casefolding in a non-Roman font is probably
//    going to give us weak results.
//
//    --- Other Considerations ---
//
//    Because it's unpleasant and CPU-consuming to read and write
//    non-binary-format statistics files (memory mapping is far
//    faster) it's slightly preferable to have statistics files
//    that don't have internal structures that change sizes (appending is
//    more pleasant than rewriting with new offsets).  Secondarily,
//    a lot of users are much more comfortable with the knowledge
//    that their statistics files won't grow without bound.  Therefore,
//    fixed-size structures are often preferred.
//
//
//   --- The text storage ---
//
//   In all but the direct-mapped table method, the text itself needs
//   to be stored because the indexing method only says where to look
//   for the first copy of any particular string head, not where all
//   of the copies are.  Thus, each byte of the text needs an index
//   (usually four bytes) of "next match" information.  This index
//   points to the start of the next string that starts with the
//   current N letters.
//
//   Note that it's not necessary for the string to be unique; the
//   next match chains can contain more than one prefix.  As long
//   as the final matching code knows that the first N bytes need
//   to be checked, there's no requirement that chains cannot be
//   shared among multiple N-byte prefixes.  Indeed, in the limit,
//   a simple sequential search can be emulated by a shared-chain
//   system with just ONE chain (each byte's "try next" pointer
//   points to the next byte in line).  These nonunique "try next"
//   chains may be a good way to keep the initial hash table
//   manageabley small.  However, how to efficiently do this
//   "in line" is unclear (the goal of in-line is to hide the
//   known text so that "strings" can't trivially extract it;
//   the obvious solution is to have two data structures (one by
//   bytes, one by uint32's, but the byte structure is then easily
//   perusable).
//
//   Another method would be to have the initial table point not
//   to text directly, but to a lookforward chain.  Within the chain,
//   cells are allocated only when the offset backward exceeds the
//   offset range allowed by the in-line offset size.  For one-byte
//   text and three-byte offsets, this can only happen if the text
//   grows beyond 16 megabytes of characters (64 megabyte footprint)
//
//    --- Hash Tables Revisited ---
//
//   Another method is to have multiple hash entries for every string
//   starting point.  For example, we might hash "ABC DEF ABC", "ABC DEF",
//   and "ABC" and put each of these into the hash table.
//
//   We might even consider that we can _discard_ the original texts
//   if our hash space is large enough that accidental clashes are
//   sufficiently rare.  For example, with a 64-bit hash, the risk of
//   any two particular strings colliding is 1E-19, and the risk of
//   any collision whatsoever does not approach 50% with less than
//   1 E 9 strings in the storage.
//
//     ------- Hashes and Hash Collisions -----
//
//   To see how the hashes would collide with the CRM114 function strnhash,
//   we ran the Spamassasin hard-ham corpus into three-byte groups, and
//   then hashed the groups.  Before hashing, there were 125,434 unique
//   three-byte sequences; after hashing, there were 124,616 unique hashes;
//   this is 818 hash collisions (a rate of 0.65%).  This is a bit higher
//   than predicted for truly randomly chosen inputs, but then again, the
//   SA corpus has very few bytes with the high order bit set.
//
//    ------ Opportunistic chain sharing -----
//
//   (Note- this is NOT being built just yet; it's just an idea) - the
//   big problem with 24-bit chain offsets is that it might not have
//   enough "reach" for the less common trigrams; in the ideal case any
//   matching substring is good enough and losing substrings is anathema.
//   However, if we have a number of sparse chains that are at risk for
//   not being reachable, we can merge those chains either together or
//   onto another chain that's in no danger of running out of offsets.
//
//   Note that it's not absolutely necessary for the two chains to be
//   sorted together; as long as the chains are connected end-to-end,
//   the result will still be effective.
//
//    -----  Another way to deal with broken chains -----
//
//    (also NOT being built yet; this is just another possibility)
//   Another option: for systems where there are chains that are about
//   to break because the text is exceeding 16 megabytes (the reach of
//   a 24-bit offset), at the end of each sample document we can insert
//   a "dummy" forwarding cell that merely serves to preserve continuity
//   of any chain that might be otherwise broken because the N-letter prefix
//   string has not occured even once in the preceding 16 megacells.
//   (worst case analysis: there are 16 million three-byte prefixes, so
//   if all but ONE prefix was actually ever seen in a 16-meg block, we'd
//   have a minor edge-case problem for systems that did not do chain
//   folding.  With chain-folding down to 18 bits (256K different chains)
//   we'd have no problem at all, even in the worst corner case.)
//
//   However, we still need to insert these chain-fixers preemptively
//   if we want to use "small" lookforward cells, because small (24-bit)
//   cells don't have the reach to be able to index to the first occurrence
//   of a chain that's never been seen in the first 16 megacharacters.
//   This means that at roughly every 16-megacell boundary we would
//   insert a forwarding dummy block (worst case size of 256K cells, on
//   the average a lot fewer because some will actually get used in real
//   chains.) That sounds like a reasonable tradeoff in size, but the
//   bookkeeping to keep it all straight is goint to be painful to code and
//   test rigorously.
//
//
//     ------- Hashes-only storage ------
//
//   In this method, we don't bother to store the actual text _at all_,
//   but we do store chains of places where it occurred in the original
//   text.  In this case, we LEARN by sliding our window of strnhash(N)
//   characters over the text.  Each position generates a four-byte
//   backward index (which may be NULL) to the most recent previous
//   encounter of this prefix; this chain grows basically without limit.
//
//   To CLASSIFY, we again slide our strnhash(N) window over the text;
//   and for each offset position we gather the (possibly empty) list of
//   places where that hash occurred.  Because the indices are pre-sorted
//   (always in descending order) it is O(n) in the length of the chains
//   to find out any commonality because the chains can be traversed by the
//   "two finger method" (same as in the hyperspace classifier).  The
//   result for any specific starting point is the list of N longest matches
//   for the leading character position as seen so far.  If we choose to
//   "commit on N characters matching, then longest that starts in that
//   chain" then the set of possible matches is the tree of indices and
//   we want the longest branch.
//
//   This is perhaps most easily done by an N-finger method where we keep
//   a list of "fingers" to the jth, j+1, j+2... window positions; at each
//   position j+k we merely insure that there is an unbroken path from j+0
//   to j+k.  (we could speed this up significantly by creating a lookaside
//   list or array that contains ONLY the valid positions at j+k; moving the
//   window to k+1 means only having to check through that array to find at
//   least one member equal to the k+1 th window chain.  In this case, the
//   "two-finger" method suffices, and the calculation can be done "in place".
//   When the path breaks (that is, no feasible matches remain), we take
//   N + k - 1 to be the length of the matched substring and begin again
//   at j = N + k + j.
//
//   Another advantage of this method is that there is no stored text to
//   recover directly; a brute-force attack, one byte at a time, will
//   recover texts but not with absolute certainty as hash collisions
//   might lead to unexpected forks in the chain.
//
//     ------- Design Decision -----
//
//   Unless something better comes up,  if we just take the strnhash() of the
//   N characters in the prefix, we will likely get a fairly reasonable
//   distribution of hash values which we can then modulo down to whatever
//   size table we're actually using.  Thus, the size of the prefix and the
//   size of the hah table are both freely variable in this design.
//
//   We will use the "hash chains only" method to store the statistics
//   information (thus providing at least moderate obscuration of the
//   text, as well as moderately efficient storage.
//
//   As a research extension, we will allow an arbitrary regex to determine
//   the meaning of the character window; repeated regexing with k+1 starting
//   positions yield what we will define as "legitimately adjacent window
//   positions".  We specifically define that we do not care if these are
//   genuinely adjacent positions; we can define these things as we wish (and
//   thereby become space-invariant, if we so choose.)
//
//   A question: should the regex define the next "character", or should
//   it define the entire next window?  The former allows more flexibility
//   (and true invariance over charactersets); the latter is easier to
//   implement and faster at runtime.  Decision: implement as "defines the
//   next character".  Reason: then we get proper overlap - the string
//   ABCDEF yields indices for ABC, BCD, CDE, and DEF, and we can
//   then use the index hashing to directly find that CDEG can be partially
//   compressed here without penalty.
//
//   A quick test shows that strnhash on [a-zA-Z0-9 .,-] shows no
//   duplications, nor any hash clashes when taken mod 256.  Thus,
//   using a Godel coding scheme (that is, where different offsets are
//   each multiplied by a unique prime number and then those products
//   are added together ) will *probably* give good hash results.
//   Because we're moduloing (taking only the low order bits) the
//   prime number "2" is a bit problematic and we may choose to skip it.
//   Note that a superincreasing property is not useful here.
//
//   Note that the entire SA corpus is only about 16 megabytes of
//   text, so a full index set of the SA corpus would be on the
//   order of 68 megabytes ( 4 megs of index, then another 16megs
//   x 4 bytes64 megs of index chains)
//
//   Note also that there is really no constraint that the chains start
//   at the low indices and move higher.  It is equally valid for the chains
//   to start at the most recent indices and point lower in memory; this
//   actually has some advantage in speed of indexing; each chain element
//   points to the _previous_ element and we do the two-finger merge
//   toward lower indices.  However, when you try to implement this,
//   you find that it gets very tangled, and you can classify much, much
//   faster if the chains go from low to high and you provide cacheing
//   in the recursive match (this is like a 1000x speedup in the classify
//   section of the algorithm, which happens much more often than
//   the learn section of the algorithm)
//
//   Note also that there is no place the actual text or even the actual
//   hashes of the text are stored.  All hashes that map to the same place
//   in the "seen at" table are deemed identical text (and no record is kept);
//   similarly each cell of "saved text" is really only a pointer to the
//   most recent previous location where something that mapped to the
//   same hash table bucket was seen).  Reconstruction of the prior text is
//   hence marginal in confidence.  This ambiguity can be increased by
//   making the hash table smaller (and thus forcing unreconstructable
//   collisions).
//
//
///////////////////////////////////////////////////////////


#ifdef NEED_PRIME_NUMBERS
///////////////////////////////////////////////////////////////
//
//     Some prime numbers to use as weights.
//
//     GROT GROT GROT  note that we have a 1 here instead of a 2 as the
//     first prime number!  That's strictly an artifice to use all of the
//     hash bits and is not an indication that I don't know that 2 is prime.

static unsigned long primes [ 260 ] = {
  1,      3,      5,      7,     11,     13,     17,     19,     23,     29,
  31,     37,     41,     43,     47,     53,     59,     61,     67,     71,
  73,     79,     83,     89,     97,     101,    103,    107,    109,    113,
  127,    131,    137,    139,    149,    151,    157,    163,    167,    173,
  179,    181,    191,    193,    197,    199,    211,    223,    227,    229,
  233,    239,    241,    251,    257,    263,    269,    271,    277,    281,
  283,    293,    307,    311,    313,    317,    331,    337,    347,    349,
  353,    359,    367,    373,    379,    383,    389,    397,    401,    409,
  419,    421,    431,    433,    439,    443,    449,    457,    461,    463,
  467,    479,    487,    491,    499,    503,    509,    521,    523,    541,
  547,    557,    563,    569,    571,    577,    587,    593,    599,    601,
  607,    613,    617,    619,    631,    641,    643,    647,    653,    659,
  661,    673,    677,    683,    691,    701,    709,    719,    727,    733,
  739,    743,    751,    757,    761,    769,    773,    787,    797,    809,
  811,    821,    823,    827,    829,    839,    853,    857,    859,    863,
  877,    881,    883,    887,    907,    911,    919,    929,    937,    941,
  947,    953,    967,    971,    977,    983,    991,    997,   1009,   1013,
  1019,   1021,   1031,   1033,   1039,   1049,   1051,   1061,   1063,   1069,
  1087,   1091,   1093,   1097,   1103,   1109,   1117,   1123,   1129,   1151,
  1153,   1163,   1171,   1181,   1187,   1193,   1201,   1213,   1217,   1223,
  1229,   1231,   1237,   1249,   1259,   1277,   1279,   1283,   1289,   1291,
  1297,   1301,   1303,   1307,   1319,   1321,   1327,   1361,   1367,   1373,
  1381,   1399,   1409,   1423,   1427,   1429,   1433,   1439,   1447,   1451,
  1453,   1459,   1471,   1481,   1483,   1487,   1489,   1493,   1499,   1511,
  1523,   1531,   1543,   1549,   1553,   1559,   1567,   1571,   1579,   1583,
  1597,   1601,   1607,   1609,   1613,   1619,   1621,   1627,   1637,   1657
} ;
#endif	// NEED_PRIME_NUMBERS

//
//    Each FSCM class uses two blocks, one for the prefix hash table
//    which has one item in the cell called "index', and
//    one for the forward chain of indices list (again, a struct of one
//    unsigned int "next").
//
//    The interleave is that for class N, block 2N is the prefix hash table,
//    and block 2N+1 is the forward chain indices.
//
//    Note also that in the chain block, forward index 0 is reserved
//    to be the cell (not byte) index to the next _unused_ forward chain
//    index cell.
//
CRM114_ERR crm114__init_block_fscm (CRM114_DATABLOCK *p_db, int whichblock)
{
  //  What kind of block are we?
  if (whichblock % 2 != 0)
    {
      //  We're forwarding chains here (for all classes).
      //  Init our chain offsets here; chain[0] is the index of the
      //  next open chaincell, which starts as a 1.
      FSCM_HASH_CHAIN_CELL *pcc;

      pcc = (FSCM_HASH_CHAIN_CELL *)
	&p_db->data[p_db->cb.block[whichblock].start_offset];
      pcc->next = 1;
    };

  return (CRM114_OK);
}


////////////////////////////////////////////////////////////////////////
//
//    How to learn in FSCM - two parts:
//      1) append our structures to the statistics file in
//         the FSCM_CHAR_STRUCT format;
//      2) index the new structures; if no prior exists on the chain,
//         let previous link be 0, otherwise it's the prior value in the
//         hash table.
//      3) Nota bene: Originally this code grew the structures downward;
//         this turned out to be a bad idea because some types of documents
//         of interest contained long runs (1000+) of identical characters and
//         the downward-growing structures took geometrically long
//         periods of time to traverse repeatedly.
//



CRM114_ERR crm114_learn_features_fscm (CRM114_DATABLOCK **db,
				       int whichclass,
				       const struct crm114_feature_row features[],
				       long featurecount)
{

  long i;

  FSCM_PREFIX_TABLE_CELL *prefix_table;     //  the prefix indexing table,
  unsigned long prefix_table_size;
  FSCM_HASH_CHAIN_CELL *chains;             //  the chain area
  unsigned int newchainstart;               //  offset in cells of new chains

  long sense;
  long microgroom;
  long unique;
  long use_unigram_features;

  if (crm114__user_trace)
    fprintf (stderr, "\nExecuting an FSCM LEARN\n");

  // set flags
  sense = +1;
  if ((*db)->cb.classifier_flags & CRM114_NOCASE)
    {
      if (crm114__user_trace)
	fprintf (stderr, "turning on case-insensitive match\n");
    };
  if ((*db)->cb.classifier_flags & CRM114_REFUTE)
    {
      /////////////////////////////////////
      //    Take this out when we finally support refutation
      ////////////////////////////////////
      fprintf (stderr, "FSCM Refute is NOT SUPPORTED YET\n"
		       "If you want refutation, this is a good time to"
		       "learn to code.");
      return (CRM114_BADARG);
      sense = -sense;
      if (crm114__user_trace)
	fprintf (stderr, " refuting learning\n");
    };
  microgroom = 0;
  if ((*db)->cb.classifier_flags & CRM114_MICROGROOM)
    {
      microgroom = 1;
      if (crm114__user_trace)
	fprintf (stderr, " enabling microgrooming.\n");
    };
  unique = 0;
  if ((*db)->cb.classifier_flags & CRM114_UNIQUE)
    {
      unique = 1;
      if (crm114__user_trace)
	fprintf (stderr, " enabling uniqueifying features.\n");
    };

  use_unigram_features = 0;
  if ((*db)->cb.classifier_flags & CRM114_UNIGRAM)
    {
      use_unigram_features = 1;
      if (crm114__user_trace)
	fprintf (stderr, " using only unigram features.\n");
    };

  //  set up our pointers for the prefix table and the chains
  //file_header = (STATISTICS_FILE_HEADER_STRUCT *) file_pointer;
  //#if 0
  //{
  //	FSCM_HEADER *f = (FSCM_HEADER *)(file_header + 1);
  //	prefix_table_size = f->prefix_hash_table_length;
  //}
  //#else
  //prefix_table_size = file_header->chunks[3].length /
  //	sizeof (FSCM_PREFIX_TABLE_CELL);
  //#endif
  // prefix_table = (FSCM_PREFIX_TABLE_CELL *)
  //	&file_pointer[file_header->chunks[3].start];
  //chains = (FSCM_HASH_CHAIN_CELL *)
  //&file_pointer[file_header->chunks[4].start];

  //   Address of prefix hash table
  prefix_table = (FSCM_PREFIX_TABLE_CELL *)
    & (*db)->data[(*db)->cb.block[2*whichclass].start_offset];
  //   length of prefix hash table, in cells
  prefix_table_size = (*db)->cb.block[2*whichclass].allocated_size
    / sizeof (FSCM_PREFIX_TABLE_CELL);
  //   and address of prefix chain area
  chains = (FSCM_HASH_CHAIN_CELL *)
    & (*db)->data[(*db)->cb.block[2*whichclass + 1].start_offset];
  //   and the next available chain.
  newchainstart = chains[0].next;


  //     ... since chains[0].next is the index, not the byte address,
  //     we leave a sentinel of three cells of zero to indicate end
  //     of one document.  This updates the next-open position
  //     so from now on, use "newchainstart" as the start of the
  //     to-be-learned chainset.
  chains[0].next += (featurecount + 3) ;

  //   For each hash, insert it into the prefix table
  //   at the right place (that is, at hash mod prefix_table_size).
  //   If the table had a zero, it becomes nonzero.  If the table
  //   is nonzero, we walk the chain and modify the first zero
  //   to point to our new hash.

  if (crm114__internal_trace)
    {
      fprintf (stderr,
	       "PT start: %zu, PT size: %lu, CZ start: %zu, CZ nextopen: %u, *chains: %lu\n",
	       (*db)->cb.block[2*whichclass].start_offset,
	       prefix_table_size,
	       (*db)->cb.block[2*whichclass+1].start_offset,
	       chains[0].next, //newchainstart,
	       (long unsigned) chains);
    };
  for (i = 0; i < featurecount; i++)
    {
      unsigned int pti, cind;
      pti = features[i].feature % prefix_table_size;

      if (crm114__internal_trace)
	{
	  fprintf (stderr,
		   "offset %ld icn: %lu hash %u tableslot %u"
		   " (prev offset %u)\n",
		   i, i + newchainstart, features[i].feature, pti,
		   prefix_table [pti].index );
	  cind = prefix_table[pti].index;
	  while ( cind != 0)
	    {
	      fprintf (stderr,
		       " ... now location %u forwards to %u \n",
		       cind, chains[cind].next);
	      cind = chains[cind].next;
	    };
	};
      //  Desired State:
      // chains [old] points to chains [new]
      // prefix_table [pti] = chains [old]
      if (prefix_table[pti].index == 0)
	{   //  first entry in this chain, so fill in the table.
	  prefix_table[pti].index = i + newchainstart;
	  chains [i + newchainstart].next = 0;
	}
      else
	{   //  not first entry-- chase the chain, we go at the end
	  cind = prefix_table[pti].index;
	  while (chains[cind].next != 0)
	    cind = chains [cind].next;     // cdr down to end of chain
	  chains[cind].next = i + newchainstart;  // point at our cell.
	  chains [i + newchainstart].next = 0;
	};
    };

  //   update the feature count and learn count
  (*db)->cb.class[whichclass].features += featurecount;
  (*db)->cb.class[whichclass].documents ++;

  //   all done.
  return (CRM114_OK);
}

/////////////////////////////////////////////////////////////////////////
//
//    Classifier helper functions start here.
//
/////////////////////////////////////////////////////////////////////////


//
//     A helper (but crucial) function - given an array of hashes and,
//     and a prefix_table / chain array pair, calculate the compressibility
//     of the hashes; this is munged (reports say best accuracy if
//     raised to the 1.5 power) and that is the returned value.
//
//
//   The algorithm here is actually suboptimal.
//   We take the first match presented (i.e. where unk_hashes[i] maps
//   to a nonzero cell in the prefix table) then for each additional
//   hash (i.e. unk_hashes[i+j] where there is a consecutive chain
//   in the chains[q, q+1, q+2]; we sum the J raised to the q_exponent
//   power for each such chain and report that result back.
//
//   The trick we employ here is that for each starting position q
//   all possible solutions are on q's chain, but also on q+1's
//   chain, on q+2's chain, on q+3's chain, and so on.
//
//   At this point, we can go two ways: we can use a single index (q)
//   chain and search forward through the entire chain, or we can use
//   multiple indices and an n-way merge of n chains to cut the number of
//   comparisons down significantly.
//
//   Which is optimal here?  Let's assume the texts obey something like
//   Zipf's law (Nth term is 1/Nth as likely as the 1st term).  Then the
//   probabile number of comparisons to find a string of length Q in
//   a text of length |T| by using the first method is
//          (1/N) + ( 1/ N) + ... = Q * (1/N) and we
//   can stop as soon as we find a string of Q elements (but since we
//   want the longest one, we check all |T| / N occurrences and that takes
//   |T| * Q / N^2 comparisons, and we need roughly |U| comparisons
//   overall, it's |T| * |U| * Q / N^2 .
//
//   In the second method (find all chains of length Q or longer) we
//   touch each of the Q chain members once. The number of members of
//   each chain is roughly |T| / N and we need Q such chains, so the
//   time is |T| * Q / N.  However, at the next position
//   we can simply drop the first chain's constraint; all of the other
//   calculations have already been done once; essentially this search
//   can be carried out *in parallel*; this cuts the work by a factor of
//   the length of the unkonown string.   However, dropping the constraint
//   is very tricky programming and so we aren't doing that right now.
//
//   We might form the sets where chain 1 and chain 2 are sequential in the
//   memory.  We then find where chains 2 and 3 are sequential in the
//   memory; where chains 3 and 4 are sequential, etc.  This is essentially
//   a relational database "join" operation, but with "in same record"
//   being replaced with "in sequential slots".
//
//   Assume we have a vector "V" of indices carrying each chain's current
//   putative pointer for a sequential match. (assume the V vector is
//   as long as the input string).
//
// 0) We initialize the indexing vector "V" to the first element of each
//    chan (or NULL for never-seen chains), and "start", "end", and
//    "total" to zero.
// 1) We set the chain index-index "C" to 0 (the leftmost member
//    of the index vector V).
// 2.0) We do a two-finger merge between the C'th and C + 1 chains,
//    moving one chain link further on the lesser index in each cycle.
//    (NB the current build method causes link indicess to be descending in
//    numerical value; so we step to the next link on the _greater_ of the
//    two chains.
//    2a) we stop the two-finger merge when:
//         V[C] == V[C+1] + 1
//         in which case
//              >> C++,
//              >> if C > end then end = C
//              >> go to 2.0 (repeat the 2-finger merge);
//    2b) if the two-finger merge runs out of chain on either the
//        C chain or the C++ chain (that is, a NULL):
//        >>  set the "out of chain" V element back to the innitial state;
//        >>  go back one chain pair ( "C = C--")
//            If V[C] == NULL
//              >> report out (end-start) as a maximal match (incrementing
//                 total by some amount),
//              >> move C over to "end" in the input stream
//              >> reset V[end+1]back to the chain starts.  Anything further
//                 hasn't been touched and so can be left alone.
//              >> go to 2.0
//
//    This algorithm still has the flaw that for the input string
//    ABCDE, the subchain BCDE is searched when the input string is at A.
//    and then again at B.  However, any local matches BC... are
//    gauranteed to be captured in the AB... match (we would look at
//    only the B's that follwed A's, not all of the B's even, so perhaps
//    this isn't much extra work after all.
//
//     Note: the reason for passing array of hashes rather than the original
//     string is that the calculation of the hashes is necessary and it's
//     more efficient to do it once and reuse.  Also, it means that the
//     hashes can be computed with a non-orthodox (i.e. not a string kernel)
//     method and that might take serious computes and many regexecs.

////////////////////////////////////////////////////////////////////////
//
//      Helper functions for the fast match.
//
////////////////////////////////////////////////////////////////////////

//     given a starting point, does it exist on a chain?
static unsigned int chain_search_one_chain_link
 (
  FSCM_HASH_CHAIN_CELL *chains,
  unsigned int chain_start,
  unsigned int must_match,
  int init_cache     // 0 = normal operation; > 0 malloc size, -1 free cache
 )
{
  int cachedex;
  typedef struct {
    unsigned int chstart;
    unsigned int cval0;
    unsigned int cval1;
  } FSCM_CHAIN_CACHE_CELL;
  static FSCM_CHAIN_CACHE_CELL *cache;
  static int cachesize;

  if ( init_cache > 0)
    {
      if (crm114__internal_trace)
	fprintf (stderr, "initializing the chain cache.\n");
      cache = (FSCM_CHAIN_CACHE_CELL *)
	calloc (init_cache, sizeof (FSCM_CHAIN_CACHE_CELL));
      cachesize = init_cache;
      return (0);
    };

  if ( init_cache < 0)
    {
      if (crm114__internal_trace)
	fprintf (stderr, "freeing the chain cache.\n");
      free (cache);
      cachesize = 0;
      return (0);
    };

  if (crm114__internal_trace)
    {
      unsigned int j;
      fprintf (stderr, " ... chain_search_one_chain chain %u mustmatch %u\n",
	       chain_start, must_match);
      j = chain_start;
      fprintf (stderr, "...chaintrace from %u: (next: %u)",
	       j, chains[j].next);
      while (j != 0)
	{
	  fprintf (stderr, " %u", j);
	  j = chains[j].next;
	};
      fprintf (stderr, "\n");
    };

  //    Does either or both of our cache elements have a tighter bound
  //    on the mustmatch than the initial chainstart?
  cachedex = chain_start % cachesize;
  if (chain_start == cache[cachedex].chstart)
    {
      if ( cache[cachedex].cval0 < must_match
	   && cache[cachedex].cval0 > chain_start)
	chain_start = cache[cachedex].cval0;
      if ( cache[cachedex].cval1 < must_match
	   && cache[cachedex].cval1 > chain_start)
	chain_start = cache[cachedex].cval1;
    }
  else  // forcibly update the cache to the new chain_start
    {
      cache[cachedex].chstart = chain_start;
      cache[cachedex].cval0 = chain_start;
      cache[cachedex].cval1 = chain_start;
    }

  while ( chain_start < must_match && chain_start > 0)
    {
      if (crm114__internal_trace)
	fprintf (stderr, "   .... from %u to %u\n",
		 chain_start, chains[chain_start].next);
      chain_start = chains[chain_start].next;
      cache[cachedex].cval1 = cache[cachedex].cval0;
      cache[cachedex].cval0 = chain_start;
    }

  if ( chain_start == must_match )
    {
      if (crm114__internal_trace)
      fprintf (stderr, "   ...Success at chainindex %u\n", chain_start );
      return (chain_start);
    };

  if (crm114__internal_trace)
    fprintf (stderr, "   ...Failed\n");
  return 0;
}


//    From this point in chainspace, how long does this chain run?
//
//      Do NOT implement this recursively, as a document matched against
//       itself will recurse for each character, so unless your compiler
//        can fix tail recursion, you'll blow the stack on long documents.
//
static unsigned int this_chain_run_length
 (
  FSCM_HASH_CHAIN_CELL *chains,       //  the known-text chains
  unsigned int *unk_indexes,    //  index vector to head of each chain
  unsigned int unk_len,            //  length of index vctor
  unsigned int starting_symbol,      //  symbol where we start
  unsigned int starting_chain_index  //  where it has to match (in chainspace)
 )
{
  unsigned int offset;
  unsigned int chain_start;
  unsigned int in_a_chain;

  if (crm114__internal_trace)
    fprintf (stderr,
	     "Looking for a chain member at symbol %u chainindex %u\n",
	     starting_symbol, starting_chain_index);

  offset = 0;   // The "offset" applies to both the unk_hashes _and_ the
                // offset in the known chainspace.
  in_a_chain = unk_indexes[starting_symbol + offset];
  while ( (starting_symbol + offset < unk_len) && in_a_chain )
    {
      chain_start = unk_indexes[starting_symbol + offset];
      if (crm114__internal_trace)
	fprintf (stderr,
		 "..searching at [symbol %u offset %u] chainindex %u\n",
		 starting_symbol, offset, chain_start);
      in_a_chain = chain_search_one_chain_link
	( chains, chain_start, starting_chain_index + offset, 0);
      if (in_a_chain) offset++;
    };
  if (crm114__internal_trace)
    fprintf (stderr,
	     "chain_run_length finished at chain index %u (offset %u)\n",
	     starting_chain_index + offset, offset);
  return (offset);
}

//        Note- the two-finger algorithm works- but it's actually kind of
//        hard to program in terms of it's asymmetry.  So instead, we use a
//        simpler repeated search algorithm with a cache at the bottom
//        level so we don't repeatedly search the same (failing) elements
//        of the chain).
//
//
//      NB: if this looks a little like how the genomics BLAST
//      match algorithm runs, yeah... I get that feeling too, although
//      I have not yet found a good description of how BLAST actually works
//      inside, and so can't say if this would be an improvement.  However,
//      it does beg the question of whether a BLAST-like algorithm might
//      work even _better_ for text matching.  Future note: use additional
//      flag <blast> to allow short interruptions of match stream.
//
//     longest_run_starting_here returns the length of the longest match
//      found that starts at exactly index[starting_symbol]
//
static unsigned int longest_run_starting_here
  (
   FSCM_HASH_CHAIN_CELL *chains,     // array of interlaced chain cells
   unsigned int *unk_indexes,       //  index vector to head of each chain
   unsigned int unk_len,            //  length of index vector
   unsigned int starting_symbol     //  index of starting symbol
    )
{
  unsigned int chain_index_start;      //  Where in the primary chain we are.
  unsigned int this_run, max_run;

  if (crm114__internal_trace)
    fprintf (stderr, "\n*** longest_run: starting at symbol %u\n",
	     starting_symbol);

  chain_index_start = unk_indexes[starting_symbol];
  this_run = max_run = 0;
  if (chain_index_start == 0)
    {
      if (crm114__internal_trace)
	fprintf (stderr, "longest_run: no starting chain here; returning\n");
      return 0;      // edge case - no match
    };
  //   If we got here, we had at +least+ a max run of one match found
  //    (that being chain_index_start)
  this_run = max_run = 1;
  if (crm114__internal_trace)
    fprintf (stderr, "longest_run: found a first entry (chain %u)\n",
	     chain_index_start);

  while (chain_index_start != 0)
    {
      unsigned int chain_index_old;
      if (crm114__internal_trace)
	fprintf (stderr, "Scanning chain starting at %u\n",
		 chain_index_start);
      this_run = this_chain_run_length
	(chains, unk_indexes, unk_len,
	 starting_symbol+1, chain_index_start+1);
      //
      if (crm114__internal_trace)
	fprintf (stderr,
		 "longest_run: chainindex run at %u is length %u\n",
		 chain_index_start, this_run);
      if (this_run > max_run)
	{
	  if (crm114__internal_trace)
	    fprintf (stderr, "longest_run: new maximum\n");
	  max_run = this_run;
	}
      else
	{
	  if (crm114__internal_trace)
	    fprintf (stderr, "longest_run: not an improvement\n");
	};
      //     And go around again till we hit a zero chain index
      chain_index_start = chains[chain_index_start].next;
      //    skip forward till end of currently found best (Boyer-Moore opt)
      chain_index_old = chain_index_start;
      while (chain_index_start > 0
	     && chain_index_start < chain_index_old + this_run)
	chain_index_start = chains [chain_index_start].next;
    };
  if (crm114__internal_trace)
    fprintf (stderr, "Final result at symbol %u run length is %u\n",
	     starting_symbol, max_run);
  if (max_run > 0)
    return ( max_run + FSCM_DEFAULT_CODE_PREFIX_LEN);
  else
      return (0);
}

//     compress_me is the top-level calculating routine which calls
//     all of the prior routines in the right way.

static double compress_me
  (
   unsigned int *unk_indexes,        //  prefix chain-entry table
   unsigned int unk_len,             // length of the entry table
   FSCM_HASH_CHAIN_CELL *chains,      // array of interlaced chain cells
   double q_exponent                  // exponent of match
    )
{
  unsigned int current_symbol, this_run_length;
  double total_score, incr_score;

  int blast_lookback;   // Only use if BLAST is desired.

  total_score = 0.0;
  current_symbol = 0;
  blast_lookback = 0;

  while (current_symbol < unk_len)
    {
      this_run_length = longest_run_starting_here
	(chains, unk_indexes, unk_len, current_symbol);
      incr_score = 0;
      if (this_run_length > 0)
	{
	  //this_run_length += blast_lookback;
	  incr_score = pow (this_run_length, q_exponent);
	  //blast_lookback = this_run_length;
	};
      //blast_lookback --;
      //if (blast_lookback < 0) blast_lookback = 0;
      //if (this_run_length > 2)
      //	fprintf (stderr, " %ld", this_run_length);
      //else
      //	fprintf (stderr, "_");
      total_score = total_score + incr_score;
      if (crm114__internal_trace)
	fprintf (stderr,  "Offset %u compresses %u score %lf\n",
		 current_symbol, this_run_length, incr_score);
      if (this_run_length > 0)
	current_symbol = current_symbol + this_run_length;
      else  current_symbol++;
    };
  return (total_score);
}


//      How to do an Improved FSCM CLASSIFY of some text.
//
//int crm_fast_substring_classify (CSL_CELL *csl, ARGPARSE_BLOCK *apb,
//				 char *txtptr, long txtstart, long txtlen)

CRM114_ERR crm114_classify_features_fscm (const CRM114_DATABLOCK *db,
					  const struct crm114_feature_row features[],
					  long nfr,
					  CRM114_MATCHRESULT *result)
{
  //      classify the compressed version of this text
  //      as belonging to a particular type.
  //
  //       Much of this code should look very familiar- it's cribbed from
  //       the code for LEARN
  //
  long i, k;
  long unk_features, classhits;
  double tprob;         //  total probability in the "success" domain.


  FSCM_PREFIX_TABLE_CELL *prefix_table;      //  the prefix indexing table,
  unsigned long prefix_table_size;
  FSCM_HASH_CHAIN_CELL *chains;  //  the chain area
  unsigned int *unk_indexes;
  int not_microgroom, use_unique, use_unigram_features;
  int classnum;

  if (crm114__internal_trace)
    fprintf (stderr, "executing a Fast Substring Compression CLASSIFY\n");

  //  GROT GROT GROT locally allocate the match result if it's not
  //  passed in?  I think so.
  if (db == NULL || features == NULL || result == NULL)
    return (CRM114_BADARG);

  //  Fill in the preamble part of the results block
  crm114__clear_copy_result (result, &db->cb);

  // set flags

  not_microgroom = 1;
  if (db->cb.classifier_flags & CRM114_MICROGROOM)
    {
      not_microgroom = 0;
      if (crm114__user_trace)
	fprintf (stderr, " disabling fast-skip optimization.\n");
    };

  use_unique = 0;
  if (db->cb.classifier_flags & CRM114_UNIQUE)
    {
      use_unique = 1;
      if (crm114__user_trace)
	fprintf (stderr, " unique engaged - repeated features are ignored \n");
    };

  use_unigram_features = 0;
  if (db->cb.classifier_flags & CRM114_UNIGRAM)
    {
      use_unigram_features = 1;
      if (crm114__user_trace)
	fprintf (stderr, " using only unigram features. \n");
    };

  //   Create the hashed-and-moduloed index vector:
  unk_indexes = (unsigned int *) calloc (nfr + 1, sizeof (unsigned int));

  //   Count features that never hit any index...
  unk_features = 0;

  //    initialize the index vector to the chain starts
  //    (some of which are NULL).  This is different for
  //    each of the classes.
  for (classnum = 0; classnum < db->cb.how_many_classes; classnum++)
    {
      unsigned int uhmpts;
      //   Get the prefix table, prefix table size, and chain area for
      //   this class's data.
      prefix_table = (FSCM_PREFIX_TABLE_CELL *)
	&(db->data[db->cb.block[classnum*2].start_offset]);
      prefix_table_size = db->cb.block[classnum*2].allocated_size
	/ sizeof (FSCM_PREFIX_TABLE_CELL);
      chains = (FSCM_HASH_CHAIN_CELL *)
	&db->data[db->cb.block[classnum*2+1].start_offset];
      //if (crm114__internal_trace)
      //  fprintf (stderr,
      //	     " Prefix table %ld is size %ld, chains at %ld\n",
      //	     prefix_table_size);

      //      Turn our features into initial locations in
      //      the chain area.
      classhits = 0;
      for (i = 0; i < nfr; i++)
	{
	  uhmpts = features[i].feature % prefix_table_size;
	  if (uhmpts == 0) unk_features++;
	  if (uhmpts != 0) classhits++;
	  unk_indexes[i] = (unsigned int) prefix_table[uhmpts].index;
	  if (crm114__internal_trace)
	    fprintf (stderr,
		     "unk_hashes[%ld] = %u, index = %u, "
		     " prefix_table[%u] = %u \n",
		     i, features[i].feature, uhmpts,
		     uhmpts, prefix_table[uhmpts].index);
	};
      //  Now for the nitty-gritty - run the compression
      //   of the unknown versus tis statistics file.
      //   For thk=0.1, power of 1.2 --> 36 errs,
      //   1.5--> 49 errs,  1.7-->52, and 1.0 bogged down
      //   At thk=0.0 exponent 1.0-->191 and 18 min
      //   thk 0.1 exp 1.35 --> 34 in 12min. and exp 1.1 -> 43
      //   thk 0.05 exp 1.1--> 50.


      chain_search_one_chain_link (0, 0, 0, 1);  // init the chain-cache

      result->class[classnum].u.fscm.compression
	= compress_me (
		       unk_indexes,
		       nfr,          // unk_hashcount,
		       chains,
		       (double) 1.35);
      result->class[classnum].hits = classhits;

      chain_search_one_chain_link (0, 0, 0, -1);  // free the chain-cache

    };

  //   Since we no longer need the unk_indexes, we can free() the storage.
  free (unk_indexes);

  ///////////////////////////////////////////////////////////
  //
  //   To translate score (which is exponentiated compression) we
  //   just normalize to a sum of 1.000 .  Note that we start off
  //   with a minimum per-class score of "tiny" to avoid divide-by-zero
  //   problems (zero scores on everything => divide by zero)

  tprob = 0.0;
  for (i = 0; i < db->cb.how_many_classes; i++)
    {
      if (result->class[i].u.fscm.compression < 0.000001)
	result->class[i].u.fscm.compression =   0.000001;
      tprob = tprob + result->class[i].u.fscm.compression;
    };
  //     Renormalize probabilities
  for (i = 0; i < db->cb.how_many_classes; i++)
    result->class[i].prob = result->class[i].u.fscm.compression / tprob;

  //     Assemble result
  result->unk_features = unk_features;

  // calc and set per-class pR, tsprob, overall_pR, bestmatch
  crm114__result_pR_outcome (result);

  if (crm114__user_trace)
    {
      for (k = 0; k < db->cb.how_many_classes; k++)
	fprintf (stderr, "Match for file %ld: compress: %f prob: %f\n",
		 k,
		 result->class[k].u.fscm.compression,
		 result->class[k].prob);
    };

  return (CRM114_OK);
}


// Read/write a DB's data blocks from/to a text file.

int crm114__fscm_learned_write_text_fp(const CRM114_DATABLOCK *db, FILE *fp)
{
  int b;		// block subscript (same as class subscript)
  unsigned int i;	// bucket subscript

  for (b = 0; b < db->cb.how_many_blocks; b++)
    {
      if (b % 2 == 0)
	{          // in the first block of two - chain start indices
	  const FSCM_PREFIX_TABLE_CELL *indices =
	    (FSCM_PREFIX_TABLE_CELL *)(&db->data[0]
				       + db->cb.block[b].start_offset);
	  const long htlen =
	    db->cb.block[b].allocated_size / sizeof(FSCM_PREFIX_TABLE_CELL);

	  fprintf(fp, TN_BLOCK " %d\n", b);
	  for (i = 0; i < htlen; i++)
	    if (indices[i].index != 0)
	      fprintf(fp, "%u %u\n", i, indices[i].index );
	  fprintf(fp, TN_END "\n");
	}
      else
	{          // in the second block of two - chain start indices
	  const FSCM_HASH_CHAIN_CELL *nexts =
	    (FSCM_HASH_CHAIN_CELL *)(&db->data[0]
				       + db->cb.block[b].start_offset);
	  const long htlen =
	    db->cb.block[b].allocated_size / sizeof(FSCM_HASH_CHAIN_CELL);

	  fprintf(fp, TN_BLOCK " %d\n", b);
	  for (i = 0; i < htlen; i++)
	    if (nexts[i].next != 0)
	      fprintf(fp, "%u %u\n", i, nexts[i].next );
	  fprintf(fp, TN_END "\n");
	}
    }
  return TRUE;			// writes always work, right?
}


int crm114__fscm_learned_read_text_fp(CRM114_DATABLOCK **db, FILE *fp)
{
  int b;		// block subscript (same as class subscript)
  int chkb;		// block subscript read from text file

  for (b = 0; b < (*db)->cb.how_many_blocks; b++)
    {
      if (b % 2 == 0)
	{
	  FSCM_PREFIX_TABLE_CELL *indices =
	    (FSCM_PREFIX_TABLE_CELL *)(&(*db)->data[0]
				       + (*db)->cb.block[b].start_offset);
	  const long htlen =
	    (*db)->cb.block[b].allocated_size / sizeof(FSCM_PREFIX_TABLE_CELL);
	  unsigned int hindex;	// bucket subscript read from text file
	  FSCM_PREFIX_TABLE_CELL tmpb;  // temp buffer
	  char line[200];
	  
	  // read and check block # line, except '\n'
	  if (fscanf(fp, " " TN_BLOCK " %d", &chkb) != 1 || chkb != b)
	    return FALSE;
	  // read and discard the rest of the block # line
	  if (fgets(line, sizeof(line), fp) == NULL
	      || line[0] != '\n')
	    return FALSE;
	  while (1)
	    {
	      // read a line
	      if (fgets(line, sizeof(line), fp) == NULL)
		return FALSE;
	      if (line[strlen(line) - 1] != '\n') // didn't get a whole line
		return FALSE;
	      line[strlen(line) - 1] = '\0';    // remove \n
	      // is it an end mark?
	      if (strcmp(line, TN_END) == 0)
		break;		// yes, done with this block
	      // not an end mark, must be bucket number and contents
	      if (sscanf(line, "%u %u", &hindex, &tmpb.index) != 2)
		return FALSE;
	      if (hindex >= htlen) // input bucket number is past end of table
		return FALSE;
	      indices[hindex] = tmpb; // all OK, stuff the bucket
	    }
	}
      else
	{
	  FSCM_HASH_CHAIN_CELL *nexts =
	    (FSCM_HASH_CHAIN_CELL *)(&(*db)->data[0]
				       + (*db)->cb.block[b].start_offset);
	  const long htlen =
	    (*db)->cb.block[b].allocated_size / sizeof(FSCM_HASH_CHAIN_CELL);
	  unsigned int hindex;	// bucket subscript read from text file
	  FSCM_HASH_CHAIN_CELL tmpb;  // temp buffer
	  char line[200];
	  
	  // read and check block # line, except '\n'
	  if (fscanf(fp, " " TN_BLOCK " %d", &chkb) != 1 || chkb != b)
	    return FALSE;
	  // read and discard the rest of the block # line
	  if (fgets(line, sizeof(line), fp) == NULL
	      || line[0] != '\n')
	    return FALSE;
	  while (1)
	    {
	      // read a line
	      if (fgets(line, sizeof(line), fp) == NULL)
		return FALSE;
	      if (line[strlen(line) - 1] != '\n') // didn't get a whole line
		return FALSE;
	      line[strlen(line) - 1] = '\0';    // remove \n
	      // is it an end mark?
	      if (strcmp(line, TN_END) == 0)
		break;		// yes, done with this block
	      // not an end mark, must be bucket number and contents
	      if (sscanf(line, "%u %u", &hindex, &tmpb.next) != 2)
		return FALSE;
	      if (hindex >= htlen) // input bucket number is past end of table
		return FALSE;
	      nexts[hindex] = tmpb; // all OK, stuff the bucket
	    }
	}
    }
     
  return TRUE;
}
