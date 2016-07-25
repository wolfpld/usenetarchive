//	crm114_markov_microgroom.c  - migrogrooming utilities

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

//  include the library data structures file
#include "crm114_structs.h"

#include "crm114_lib.h"


#if 0

/*
  You may have noticed that this microgroom stuff gets clunky when it
  handles wraparound.  There's at least one minor bug in wraparound,
  too: when weight_distance calculates what fraction of a chain to
  delete, it looks at only a segment of the chain, not the whole
  chain, and it truncates, so it'll probably delete too few buckets.
  I've seen that happen.

  Here's something that might help.

  This adds a layer of abstraction: it defines a chain as if it were a
  simple sequence, as if wraparound didn't exist.  A chain has a start
  location, end location, and length, and can be subscripted like a C
  array.  Little inline functions calculate these things; wraparound
  is hidden down inside those functions.  So the stuff above those
  functions doesn't have to deal with wraparound at all, and its logic
  gets simpler and cleaner.

  The idea of a segment goes away.

  The functions use modulo (%, integer divide), which may or may not
  take a little longer than a conditional jump (which disrupts the
  pipeline).  We ought to time both of them.  I guess a divide would
  take about 5 ns on a current PC.

  None of this has been tested, except on paper.  Maybe I'll get time
  to try it someday.  --KH Feb. 2010
*/

/*
  These names have these meanings:

  table	hash table, array of featurebuckets
  tlen	table length (# buckets)
  h	a hash value
  s	(start) table subscript of first element of a chain
  e	(end)   table subscript of last element of a chain
	  This forbids chain length 0.  If this were one past the last
	  element of the chain, it would forbid chain length equal to
	  table length.
  n	chain length (1..tlen)
  ic	a subscript within a chain
  it	a subscript within a table

  h is unsigned; all others are signed, to get signed arithmetic.
  Subscript means C subscript, zero-based.

  For example:

  tlen	5
  h    28
  s	3
  e	1
  n	4
  ic	0..3

  0 1 2 3 4
  - - - - -
  x x 0 x x

  28 % 5 = 3, so h belongs to the chain that starts at bucket 3, wraps
  around through bucket 1, and has a length of 4.  One of the 4
  buckets in the chain -- we don't know which -- contains hash value
  28, and a count of occurrences of that feature.
*/

/*
  Function names below: thing to other thing, where to is spelled "_".
  For example, h_s() takes a hash and returns a start -- takes a hash
  value and returns the table subscript of the start of that hash's
  chain (of the bucket where that hash ideally would be).  Names are
  short to avoid verbosity and clunkiness.
*/


/* takes a hash, returns a chain start (table subscript) */
static inline int h_s(unsigned int h, int tlen)
{
  return (h % tlen);
}

/* takes chain start and chain subscript, returns table subscript */
static inline int ic_it(int s, int ic, int tlen)
{
  return ((s + ic) % tlen);
}

/* takes start and end, returns chain length */
static inline int e_n(int s, int e, int tlen)
{
  /* yes, this works for wraparound */
  return ((((e - s) + tlen) % tlen) + 1);
}

/*
  Takes start and chain length, returns end.
  Chain length must be > 0.
*/
static inline int n_e(int s, int n, int tlen)
{
  /* end is table subscript of highest chain subscript: n - 1 */
  return ic_it(s, n - 1, tlen);
}

/* increment a table subscript */
static inline int it_incr(int it, int tlen)
{
  return ((it + 1) % tlen);
}

/* decrement a table subscript */
static inline int it_decr(int it, int tlen)
{
  return (((it - 1) + tlen) % tlen);
}


/*
  So zap (clear some buckets) and pack (repack a chain after zapping)
  would look like this.  Both of these handle wraparound; each handles
  a whole chain, not just a segment.
*/

static int zap(MARKOV_FEATUREBUCKET table[], int tlen, int s, int n)
{
  int z = 0;			/* # buckets zeroed */
  int ic;			/* chain subscript */

  for (ic = 0; ic < min(n, tlen); ic++)
    if (whatever)
      {
	table[ic_it(s, ic, tlen)].value = 0;
	z++;
      }
  return z;
}

static void pack(MARKOV_FEATUREBUCKET table[], int tlen, int s, int n)
{
  static const MARKOV_FEATUREBUCKET empty = {0, 0};
  int icfrom;			/* chain subscript */
  int itto;			/* table subscript */

  /* loop through chain with chain subscript */
  for (icfrom = 0; icfrom < n; icfrom++)
    {
      MARKOV_FEATUREBUCKET from = table[ic_it(s, icfrom, tlen)];

      if (from.value != 0)
	{
	  /* go down a level: loop through table with table subscript */
	  for (itto = h_s(from.hash, tlen);
	       !(table[itto].value == 0 || table[itto].hash == from.hash);
	       itto = it_incr(itto, tlen))
	    ;
	  table[itto] = from;
	  table[ic_it(s, icfrom, tlen)] = empty;
	}
    }
}

#endif	// untested abstraction


#ifdef WEIGHT_DISTANCE_AMNESIA

////////////////////////////////////////////////
//
//      zap - the distance-heuristic microgroomer core.

static long zap ( MARKOV_FEATUREBUCKET *h,
			 unsigned int hs,
			 unsigned int start,
			 unsigned int end )

{
  //     A question- what's the right deprecation ratio between
  //     "distance from original" vs. low point value?  The original
  //     Fidelis code did a 1:1 equivalence (being 1 place off is exactly as
  //     bad as having a count of just 1).
  //
  //     In reality, because of Zipf's law, most of the buckets
  //     stay at a value of 1 forever; they provide scant evidence
  //     no matter what.  Therefore, we will allow separate weights
  //     for V (value) and D (distance).  Note that a D of zero
  //     means "don't use distance, only value", and a V of zero
  //     means "don't use value, only distance.  Mixed values will
  //     give intermediate tradeoffs between distance( ~~ age) and
  //     value.
  //
  //     Similarly, VWEIGHT2 and DWEIGHT2 go with the _square_ of
  //     the value and distance.

#define VWEIGHT 1.0
#define VWEIGHT2 0.0

#define DWEIGHT 1.0
#define DWEIGHT2 0.0

  int vcut;
  int zcountdown;
  unsigned int packlen;
  unsigned int k;
  int actually_zeroed;

  vcut = 1;
  packlen = end - start + 1;
  //  fprintf (stderr, " S: %ld, E: %ld, L: %ld ", start, end, packlen );
  zcountdown = packlen / 32.0 ;  //   get rid of about 3% of the data
  actually_zeroed = 0;
  while (zcountdown > 0)
    {
      //  fprintf (stderr, " %ld ", vcut);
      for (k = start; k <= end;  k++)
	{
	  //      fprintf (stderr, "a");
	  if (h[k].value > 0)      //  can't zero it if it's already zeroed
	    {
	      //  fprintf (stderr, "b");
	      if ((VWEIGHT * h[k].value) +
		  (VWEIGHT2 * h[k].value * h[k].value ) +
		  (DWEIGHT * (k - h[k].hash % hs)) +
		  (DWEIGHT2 * (k - h[k].hash % hs) * (k - h[k].hash % hs))
		  <= vcut)
		{
		  //  fprintf (stderr, "*");
		  h[k].value = 0;
		  zcountdown--;
		  actually_zeroed++;
		};
	    };
	};
      vcut++;
    };
  return (actually_zeroed);
}

#endif	// defined(WEIGHT_DISTANCE_AMNESIA)


static void packseg (MARKOV_FEATUREBUCKET *h, int hs,
		     int packstart, int packlen)
{
  unsigned int ifrom, ito;
  unsigned int thash, tvalue;

  if (crm114__internal_trace) fprintf (stderr, " < %d %d >", packstart, packlen);

  for (ifrom = packstart; ifrom < packstart + packlen; ifrom++)
    {
      //  Is it an empty bucket?  (remember, we're compressing out
      //  all placeholder buckets, so any bucket that's zero-valued
      //  is a valid target.)
      if ( h[ifrom].value == 0)
	{
	  //    Empty bucket - turn it from marker to empty
	  if (crm114__internal_trace) fprintf (stderr, "x");
	  h[ifrom].hash = 0;
	}
      else
	{ if (crm114__internal_trace) fprintf (stderr, "-");};
    }

  //  Our slot values are now somewhat in disorder because empty
  //  buckets may now have been inserted into a chain where there used
  //  to be placeholder buckets.  We need to re-insert slot data in a
  //  bucket where it will be found.
  //
  ito = 0;
  for (ifrom = packstart; ifrom < packstart+packlen; ifrom++)
    {
      //    Now find the next bucket to place somewhere
      //
      thash  = h[ifrom].hash;
      tvalue = h[ifrom].value;

      if (tvalue == 0)
	{
	  if (crm114__internal_trace) fprintf (stderr, "X");
	}
      else
	{
	  ito = thash % hs;
	  // fprintf (stderr, "a %ld", ito);

	  while ( ! ( (h[ito].value == 0)
		      || (  h[ito].hash == thash)))
	    {
	      ito++;
	      if (ito >= hs) ito = 0;
	      // fprintf (stderr, "a %ld", ito);
	    };

	  //
	  //    found an empty slot, put this value there, and zero the
	  //    original one.  Sometimes this is a noop.  We don't care.

	  if (crm114__internal_trace)
	    {
	      if ( ifrom == ito ) fprintf (stderr, "=");
	      if ( ito < ifrom) fprintf (stderr, "<");
	      if ( ito > ifrom ) fprintf (stderr, ">");
	    };

	  h[ifrom].hash  = 0;
	  h[ifrom].value = 0;

	  h[ito].hash  = thash;
	  h[ito].value = tvalue;
	};
    };

  if (crm114__internal_trace)
    fprintf(stderr, "\n");
}


static void pack (MARKOV_FEATUREBUCKET *h, int hs,
		  int packstart, int packlen)
{
  //    How we pack...
  //
  //    We look at each bucket, and attempt to reinsert it at the "best"
  //    place.  We know at worst it will end up where it already is, and
  //    at best it will end up lower (at a lower index) in the file, except
  //    if it's in wraparound mode, in which case we know it will not get
  //    back up past us (since the file must contain at least one empty)
  //    and so it's still below us in the file.

  //fprintf (stderr, "Packing %ld len %ld total %ld",
  //	   packstart, packlen, packstart+packlen);
  //  if (packstart+packlen >= hs)
  //  fprintf (stderr, " BLORTTTTTT ");
  if (packstart+packlen <= hs)   //  no wraparound in this case
    {
      packseg (h, hs, packstart, packlen);
    }
  else    //  wraparound mode - do it as two separate repacks
    {
      packseg (h, hs, packstart, (hs - packstart));
      packseg (h, hs, 0, (packlen - (hs - packstart)));
    };
}


//     How to microgroom a .css file that's getting full
//
//     NOTA BENE NOTA BENE NOTA BENE NOTA BENE
//
//         This whole section of code is under intense develoment; right now
//         it "works" but not any better than nothing at all.  Be warned
//         that any patches issued on it may well never see the light of
//         day, as intense testing and comparison may show that the current
//         algorithms are, well, suckful.
//
//
//     There are two steps to microgrooming - first, since we know we're
//     already too full, we execute a 'zero unity bins'.  Then, we see
//     how the file looks, and if necessary, we get rid of some data.
//     R is the "MARKOV_MICROGROOM_RESCALE_FACTOR"
//
int crm114__markov_microgroom (MARKOV_FEATUREBUCKET *h, int hs, unsigned int h1)
{
  static int microgroom_count = 0;
  int actually_zeroed = 0;
  int packstart;     // first used bucket in the chain
  int packlen;       // # of used buckets in the chain
  int i, j;
#ifdef STOCHASTIC_AMNESIA
  unsigned int randy;
  int zeroed_countdown;
  int force_rescale;
  int steps;
#endif
#ifdef WEIGHT_DISTANCE_AMNESIA
  int packend;       // last used bucket in the chain
  int k;
#endif

  microgroom_count++;

  if (crm114__user_trace)
    {
      if (microgroom_count == 1)
	fprintf (stderr, "CSS file too full: microgrooming this css chain: ");
      fprintf (stderr, " %d ",
	   microgroom_count);
    };


  //       We have two different algorithms for amnesia - stochastic
  //       (meaning random) and weight-distance based.
  //

#ifdef STOCHASTIC_AMNESIA
  //   set our stochastic amnesia matcher - note that we add
  //   our microgroom count so that we _eventually_ can knock out anything
  //   even if we get a whole string of buckets with hash keys that all alias
  //   to the same value.
  //
  //   We also keep track of how many buckets we've zeroed and we stop
  //   zeroing additional buckets after that point.   NO!  BUG!  That
  //   messes up the tail length, and if we don't repack the tail, then
  //   features in the tail can become permanently inaccessible!   Therefore,
  //   we really can't stop in the middle of the tail (well, we could
  //   stop zeroing, but we need to pass the full length of the tail in.
  //
  //   Note that we can't do this "adaptively" in packcss, because zeroes
  //   there aren't necessarily overflow chain terminators (because -we-
  //   might have inserted them here.

  //     set the random number generator up...
  //     note that this is repeatable for a
  //     particular test set, yet dynamic.  That
  //     way, we don't always autogroom away the
  //     same feature; we depend on the previous
  //     feature.
  srand (h1);

  i = j = h1 % hs;
  packstart = i;

  steps = 0;
  force_rescale = 0;
  zeroed_countdown = MARKOV_MICROGROOM_STOP_AFTER;
  while (h[i].value != 0 && zeroed_countdown > 0)
    {
      //      fprintf (stderr, "=");
      randy = rand() + microgroom_count;
      if (force_rescale ||
	  (( h[i].value + randy ) & MARKOV_MICROGROOM_STOCHASTIC_MASK )
	  == MARKOV_MICROGROOM_STOCHASTIC_KEY )
	{
	  h[i].value = h[i].value * MARKOV_MICROGROOM_RESCALE_FACTOR;
	};
      if (h[i].value == 0)
	{
	  actually_zeroed++;
	  zeroed_countdown--;
	}
      i++;
      if (i >= hs ) i = 0;
      steps++;
    }
  packlen = steps;
#endif	 // STOCHASTIC_AMNESIA

#ifdef WEIGHT_DISTANCE_AMNESIA
  //
  //    Weight-Distance Amnesia is an improvement by Fidelis Assis
  //    over Stochastic amnesia in that it doesn't delete information
  //    randomly; instead it uses the heuristic that low-count buckets
  //    at or near their original insert point are likely to be old and
  //    stale so expire those first.
  //
  //
  i = j = k = h1 % hs;
  packstart = i;

  //     Now find the _end_ of the bucket chain.
  //

  while (h[j].hash != 0)
    {
      j++;
      if (j >= hs) j = 0;
      if (j == k) break; //   don't hang on 100% full .css file
    }
  j--;
  if (j < 0) j = hs - 1;

  //  j is now the _last_ _used_ bucket.
  packend = j;

  //     Now we have the start and end of the bucket chain.
  //
  //     An advanced version of this algorithm would make just two passes;
  //     one to find the lowest-ranked buckets, and another to zero them.
  //     However, Fidelis' instrumentation indicates that an in-place,
  //     multisweep algorithm may be as fast, or even faster, in the most
  //     common situations.  So for now, we'll do a multisweep.
  //
  //
  //     Normal Case:  hs=10, packstart = 4, packend = 7
  //     buck#  0 1 2 3 4 5 6 7 8 9
  //            0 0 0 0 X X X X 0 0
  //     so packlen = 4 ( == 7 - 4 + 1)
  //
  //     fixup for wraparound
  //     example hs = 10, packstart = 8, packend = 1
  //     buck#  0 1 2 3 4 5 6 7 8 9
  //            X X 0 0 0 0 0 0 X X
  //    and so packlen = 4  (10 - 8 + 1 + 1)

  if (packstart < packend )
    {
      packlen = packend - packstart + 1;
    }
  else
    {
      packlen = ( hs - packstart ) + packend + 1;
    };

  //     And now zap some buckets - are we in wraparound?
  //
  if ( packstart < packend )
    {
      //      fprintf (stderr, "z");
      actually_zeroed = zap ( h, hs, packstart, packend);
    }
  else
    {
      //fprintf (stderr, "Z");
      actually_zeroed  = zap (h, hs, packstart, hs - 1);
      actually_zeroed += zap (h, hs, 0, packend);
    };
#endif	 // WEIGHT_DISTANCE_AMNESIA


  //   now we pack the buckets
  pack (h, hs, packstart, packlen);

  return (actually_zeroed);
}
