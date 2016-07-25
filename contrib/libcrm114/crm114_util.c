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

//  include some standard files
#include "crm114_sysincludes.h"

//  include any local crm114 configuration file
#include "crm114_config.h"

//  include the crm114 data structures file
#include "crm114_structs.h"

#include "crm114_lib.h"

#include "crm114_internal.h"


// string copy, don't overflow, always terminate
char *crm114__strn1cpy(char dst[], const char src[], size_t n)
{
  size_t i;

  for (i = 0; i < n; i++)
    if ((dst[i] = src[i]) == '\0')
      break;
  if (i == n)
    dst[n - 1] = '\0';

  return dst;
}


// read a string and interpret it as int true/false (non-zero/zero)
// compare it to two arg strings
// returns success/failure
// string read has a maximum length
int crm114__tf_read_text_fp(int *val, // return: T/F that was read
			    const char tstr[], // string version of true
			    const char fstr[], // string version of false
			    FILE *fp)
{
  char str[200 + 1];
  int ret;

  if (fscanf(fp, " %200s", str) == 1)
    if (strcmp(str, tstr) == 0)
      {
	*val = TRUE;
	ret = TRUE;
      }
    else if (strcmp(str, fstr) == 0)
      {
	*val = FALSE;
	ret = TRUE;
      }
    else
      ret = FALSE;
  else
    ret = FALSE;

  return ret;
}


// Given a probability and its complement, return a pR, fudged to
// avoid infinities.
//
// The second argument, comp, aka remainder, is the sum of all the
// class probabilities not included in p, the first argument.  If p is
// a class probability, then comp is the sum of all the other class
// probabilities; if p is tsprob, the sum of all the success class
// probabilities, then comp is the sum of all the failure class
// probabilities.
//
// Since the class probabilities sum to 1, comp is mathematically
// equivalent to 1 - p, but calculating it that way, when p is close
// to 0, runs into the floating point problem of subtracting two
// numbers of drastically different magnitudes: accuracy is lost.
// Calculating comp as the sum of the other probabilities preserves
// the accuracy better, and gives more consistent results.


#if __STDC__ && __STDC_VERSION__ == 199901L


static double munge(double p)
{
  // force probability valid
  if (p < 0.0)
    p = 0.0;
  if (p > 1.0)
    p = 1.0;

  return p;
}

double crm114__pR(double p,		// probability
		  double comp)		// complement, 1 - p, sum of other probs
{
  double ret;

  p = munge(p);
  comp = munge(comp);
  if (comp == p)		// seen with one class, p=0
    comp = 1.0 - p;
  ret = log10(p) - log10(comp);

  // replace infinities with arbitrary finite numbers
  // isinf() and signbit() are new in C99
  //
  // it might be fun to let pR go infinite...C99 printf() can handle it...
  if (isinf(ret))
    ret = (signbit(ret)) ? -999.0 : 999.0;

  return ret;
}

#else

static double munge(double p)
{
  // Force probability valid, and avoid infinities in pR
  // (log10(0) => -inf).

  if (p < DBL_MIN)
    p = DBL_MIN;
  if (p > 1.0)
    p = 1.0;

  return p;
}

double crm114__pR(double p,		// probability
		  double comp)		// complement, 1 - p, sum of other probs
{
  p = munge(p);
  comp = munge(comp);
  if (comp == p)		// seen with one class, p=0
    comp = 1.0 - p;
  return log10(p) - log10(comp);
}

#endif


// clear a result, copy in everything it copies from a cb

void crm114__clear_copy_result(CRM114_MATCHRESULT *r,
			       const CRM114_CONTROLBLOCK *cb)
{
  int c;			// class subscript

  memset(r, '\0', sizeof(*r));

  r->classifier_flags = cb->classifier_flags;
  r->how_many_classes = cb->how_many_classes;
  for (c = 0; c < cb->how_many_classes; c++)
    {
      crm114__strn1cpy(r->class[c].name, cb->class[c].name, sizeof(r->class[c].name));
      r->class[c].success   = cb->class[c].success;
      r->class[c].documents = cb->class[c].documents;
      r->class[c].features  = cb->class[c].features;
    }
}


// Given a result that's had everything copied in from its CB (eg, by
// crm114__clear_copy_result() above), and has also had its per-class
// probabilities set, calculate and set its per-class pR, total
// success probability, overall pR, and best match.

void crm114__result_pR_outcome(CRM114_MATCHRESULT *r)
{
  int ncls = r->how_many_classes;
  double tsprob;		// total probability in the "success" domain.
  double remainder;
  int bestmatch;
  int i, j;

  // calc per-class pR
  for (i = 0; i < ncls; i++)
    {
      remainder = 0.0;
      for (j = 0; j < ncls; j++)
	if (j != i)
	  remainder += r->class[j].prob;
      r->class[i].pR = crm114__pR(r->class[i].prob, remainder);
    }

  // sum success probabilities
  tsprob = 0.0;
  for (i = 0; i < ncls; i++)
    if (r->class[i].success)
      tsprob += r->class[i].prob;
  r->tsprob = tsprob;

  // calculate overall_pR
  remainder = 0.0;
  for (i = 0; i < ncls; i++)
    if ( !r->class[i].success)
      remainder += r->class[i].prob;
  r->overall_pR = crm114__pR(tsprob, remainder);

  // find best match
  bestmatch = 0;
  for (i = 1; i < ncls; i++)
    if (r->class[i].prob > r->class[bestmatch].prob)
      bestmatch = i;
  r->bestmatch_index = bestmatch;
}


// The whole schmeer, for classifiers that have an array of class probabilities.

void crm114__result_do_common(CRM114_MATCHRESULT *r,
			      const CRM114_CONTROLBLOCK *cb,
			      const double ptc[CRM114_MAX_CLASSES])
{
  int c;	// class subscript

  crm114__clear_copy_result(r, cb);
  for (c = 0; c < r->how_many_classes; c++)
    r->class[c].prob = ptc[c];
  crm114__result_pR_outcome(r);
}


// pretty-print a CRM114_MATCHRESULT on stdout

void crm114_show_result_class(const CRM114_MATCHRESULT *r, int icls)
{
  printf(" %3d %c (%-8s): documents: %d  features: %d  hits: %5d  prob: %.3f  pR: % .3f",
	 icls,
	 (r->class[icls].success) ? 'S' : 'F',
	 r->class[icls].name,
	 r->class[icls].documents,
	 r->class[icls].features,
	 r->class[icls].hits,
	 r->class[icls].prob,
	 r->class[icls].pR);

  switch (r->classifier_flags & CRM114_FLAGS_CLASSIFIERS_MASK)
    {
    case CRM114_MARKOVIAN:
    case CRM114_OSB_BAYES:
    case CRM114_OSBF:
      if (r->classifier_flags & CRM114_CHI2)
	printf(" chi2: %8.3f", r->class[icls].u.markov.chi2);
      break;
    case CRM114_HYPERSPACE:
      printf(" radiance: %8.3e", r->class[icls].u.hyperspace.radiance);
      break;
    case CRM114_ENTROPY:
      printf(" jumps: %3d entropy: %8.3f",
	     r->class[icls].u.bit_entropy.jumps,
	     r->class[icls].u.bit_entropy.entropy);
      break;
    case CRM114_FSCM:
      printf(" compression: %8.3f", r->class[icls].u.fscm.compression);
      break;
    case CRM114_NEURAL_NET:
      printf(" in_class: %8.3f", r->class[icls].u.neural.in_class);
      break;
    case CRM114_CORRELATE:
      printf(" L1: %d L2: %d L3: %d L4: %d",
	     r->class[icls].u.bytewise.L1,
	     r->class[icls].u.bytewise.L2,
	     r->class[icls].u.bytewise.L3,
	     r->class[icls].u.bytewise.L4);
      break;
    }

  printf("\n");
}

void crm114_show_result(const char name[], const CRM114_MATCHRESULT *r)
{
  int icls;

  printf("%s", name);
  printf("Tot succ prob: %.3f  overall_pR: %f  bestmatch_index: %d  unk_features: %d\n",
	 r->tsprob, r->overall_pR, r->bestmatch_index, r->unk_features);
  for (icls = 0; icls < r->how_many_classes; icls++)
    crm114_show_result_class(r, icls);
}


//
//     Since apparently some Windows DLLs use separate heap
//     controllers for main prog and DLL, we make sure that
//     the caller of a libcrm DLL can do a proper free(), that being
//     the same one that libcrm uses.  [[yes, this is
//     madness, but that's Windows for you.  Normal users should
//     not need to do this, but for portability may want to
//     anyway.  ]]

void crm114_free(void *p)
{
  if (p != NULL) free (p);
}
