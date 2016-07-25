//	crm114_vector_tokenize.c  - vectorized tokening to create 32-bit hash output

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

#include "crm114_lib.h"

// this (vector tokenize) is the only library file that uses regexes
#include "crm114_regex.h"


// qsort() compare function
static int compare_feature_rows(const void *p0, const void *p1)
{
  const struct crm114_feature_row *f0 = (const struct crm114_feature_row *)p0;
  const struct crm114_feature_row *f1 = (const struct crm114_feature_row *)p1;
  int ret;

  if (f0->feature > f1->feature)
    ret = 1;
  else if (f0->feature < f1->feature)
    ret = -1;
  else
    ret = 0;

  return ret;
}

void crm114_feature_sort_unique
(
 struct crm114_feature_row fr[], // array of features with coeff row #s
 long *nfr,		  // in/out: # features given, # features returned
 int sort,		  // true if sort wanted
 int unique		  // true if unique wanted
 )
{
  if (sort)
    qsort(fr, *nfr, sizeof(struct crm114_feature_row), compare_feature_rows);
  if (unique)
    {
      long i;
      long j;

      i = 0;
      for (j = 1; j < *nfr; j++)
	if (fr[j].feature != fr[i].feature)
	  fr[++i] = fr[j];
      if (*nfr > 0)
	*nfr = i + 1;
    }
}


///////////////////////////////////////////////////////////////////////////
//
//    This code section (from this comment block to the one declaring
//    "end of section dual-licensed to Bill Yerazunis and Joe
//    Langeway" is copyrighted and dual licensed by and to both Bill
//    Yerazunis and Joe Langeway; both have full rights to the code in
//    any way desired, including the right to relicense the code in
//    any way desired.
//
////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
//
//    Vectorized tokenizing - get a bunch of features in a nice
//    predigested form (a counted array of chars plus control params
//    go in, and a nice array of 32-bit ints come out.  The idea is to
//    encapsulate tokenization/hashing into one function that all
//    CRM114 classifiers can use, and so improved tokenization raises
//    all boats equally, or something like that.
//
//    The feature building is controlled via the pipeline coefficient
//    arrays as described in the paper "A Unified Approach To Spam
//    Filtration".  In short, each row of an array describes one
//    rendition of an arbitrarily long pipeline of hashed token
//    values; each row of the array supplies one output value.  Thus,
//    the 1x1 array {1} yields unigrams, the 5x6 array
//
//     {{ 1 3 0 0 0 0}
//      { 1 0 5 0 0 0}
//      { 1 0 0 11 0 0}
//      { 1 0 0 0 23 0}
//      { 1 0 0 0 0 47}}
//
//    yields "Classic CRM114" OSB features.  The unit vector
//
//     {{1}}
//
//    yields unigrams (that is, single units of whatever the
//    the tokenizing regex matched).  The 1x2array
//
//     {{1 1}}
//
//    yields bigrams that are not position nor order sensitive, while
//
//     {{1 2}}
//
//    yields bigrams that are order sensitive.
//
//    Because the array elements are used as dot-product multipliers
//    on the hashed token value pipeline, there is a small advantage to
//    having the elements of the array being odd (low bit set) and
//    relatively prime, as it decreases the chance of hash collisions.
//
//    Returns a CRM114_ERR.
//
///////////////////////////////////////////////////////////////////////////

CRM114_ERR crm114_vector_tokenize
(
 const char *txtptr,		// input string (null-safe!)
 long txtstart,			//     start tokenizing at this byte.
 long txtlen,			//   how many bytes of input.
 const CRM114_CONTROLBLOCK *p_cb, // flags, regex, matrix
 struct crm114_feature_row frs[], // where the output features go
 long frslen,			//   how many output features (max)
 long *frs_out,			// how many feature_rows we fill in
 long *next_offset		// next invocation should start at this offset
 )
{
  unsigned int hashpipe[UNIFIED_WINDOW_MAX]; // the pipeline for hashes
  unsigned int ihash;
  regex_t regcb;             // the compiled regex
  int need_to_free_the_regcb;    //  do we need to free the regcb?
  regmatch_t match[5];              // we only care about the outermost match
  reg_errcode_t rerr;               // regex error code
  long text_offset;
  long max_offset;
  long ifr;			    // subscript in frs
  int irow, icol;
  int i;

  //    now do the work.

  *frs_out = 0;

  need_to_free_the_regcb = 0;
  //    Compile the regex.
  if (p_cb->tokenizer_regexlen)
    {
      int cflags;

      cflags = REG_EXTENDED;
      if (p_cb->classifier_flags & CRM114_NOCASE)
	cflags |= REG_ICASE;
      if (p_cb->classifier_flags & CRM114_NOMULTILINE)
	cflags |= REG_NEWLINE;
      if (p_cb->classifier_flags & CRM114_LITERAL)
	cflags |= REG_LITERAL;

      need_to_free_the_regcb = 1;
      rerr = crm114__regncomp (&regcb, p_cb->tokenizer_regex,
			      p_cb->tokenizer_regexlen, cflags);
      if (rerr > 0)
	{
	  return (CRM114_REGEX_ERR);
	};
    };

  // fill the hashpipe with initialization
  for (i = 0; i < UNIFIED_WINDOW_MAX; i++)
    hashpipe[i] = 0xDEADBEEF ;

  //   Run the hashpipe, either with regex, or without.
  //
  text_offset = txtstart;
  max_offset = txtstart + txtlen;
  if (crm114__internal_trace)
    fprintf (stderr, "Text offset: %ld, length: %ld\n", text_offset, txtlen);
  rerr = REG_OK;
  ifr = 0;
  while (1)
    {
      //  If the pattern is empty, assume non-graph-delimited tokens
      //  (supposedly an 8% speed gain over regexec)
      if (p_cb->tokenizer_regexlen == 0)
	{
	  rerr = REG_OK;    // means found another token.... same as regexec
          //         skip non-graphical characthers
	  match[0].rm_so = 0;
	  //fprintf (stderr, "'%c'", text[text_offset+match[0].rm_so]);
          while ( ( !isgraph (txtptr [text_offset + match[0].rm_so]))
		  && ( text_offset + match[0].rm_so < max_offset))
            {
	      //fprintf (stderr, ""%c'", txtptr[text_offset+match[0].rm_so]);
	      match[0].rm_so ++;
	    }
          match[0].rm_eo = match[0].rm_so;
          while ( (isgraph (txtptr [text_offset + match[0].rm_eo]))
		  && (text_offset + match[0].rm_eo < max_offset))
	    {
	      //fprintf (stderr, "'%c'", txtptr[text_offset+match[0].rm_eo]);
	      match[0].rm_eo ++;
	    };
          if ( match[0].rm_so == match[0].rm_eo)	// at end of text
            rerr = REG_NOMATCH;
	}
      else
	{
	  rerr = crm114__regnexec (&regcb,
				  &txtptr[text_offset],
				  max_offset - text_offset,
				  5, match,
				  0);
	};

      //   Are we done?
      if ( !(rerr == REG_OK
	     && ifr + p_cb->tokenizer_pipeline_phases <= frslen))
	break;

      //   Not done,we have another token (the text in text[match[0].rm_so,
      //    of length match[0].rm_eo - match[0].rm_so size)
      //
      if (crm114__user_trace)
	{
	  fprintf (stderr, "Token; rerr: %d T.O: %ld len %d ( %d %d on >",
		   rerr,
		   text_offset,
		   match[0].rm_eo - match[0].rm_so,
		   match[0].rm_so,
		   match[0].rm_eo);
	  for (i = match[0].rm_so+text_offset;
	       i < match[0].rm_eo+text_offset;
	       i++)
	    fprintf (stderr, "%c", txtptr[i]);
	  fprintf (stderr, "< )\n");
	};

      //   Now slide the hashpipe up one slot, and stuff this new token
      //   into the front of the pipeline
      //
      memmove (& hashpipe [1], hashpipe,
	       sizeof (hashpipe) - sizeof (hashpipe[0]) );

      hashpipe[0] = crm114_strnhash( &txtptr[match[0].rm_so+text_offset],
				     match[0].rm_eo - match[0].rm_so);

      //    Now, for each row in the coefficient array, we create a
      //   feature.
      //
      for (irow = 0; irow < p_cb->tokenizer_pipeline_phases; irow++)
	{
	  ihash = 0;
	  for (icol = 0; icol < p_cb->tokenizer_pipeline_len; icol++)
	    ihash +=
	      hashpipe[icol] * p_cb->tokenizer_pipeline_coeffs[irow][icol];

	  if (crm114__internal_trace)
	    fprintf (stderr,
		     "New Feature: %x at %ld\n", ihash, ifr);

	  //    Stuff the final ihash value into features array
	  frs[ifr].feature = ihash;
	  // and stuff corresponding matrix row subscript
	  frs[ifr].row = irow;
	  ifr++;
	};

      //   And finally move on to the next place in the input.
      //
      //  Move to end of current token.
      text_offset += match[0].rm_eo;
    };

  if (next_offset)
    *next_offset = text_offset;
  *frs_out = ifr;

  if (need_to_free_the_regcb) crm114__regfree (&regcb);

  if (crm114__internal_trace)
    fprintf (stderr, "VT: Total features generated: %ld\n", *frs_out);

  return ((rerr == REG_NOMATCH) ? CRM114_OK // end of text
	  : ((rerr == REG_OK) ? CRM114_FULL // loop ended, frs is full
	     : CRM114_REGEX_ERR));	    // regnexec() complained
}

///////////////////////////////////////////////////////////////////////////
//
//   End of code section dual-licensed to Bill Yerazunis and Joe Langeway.
//
////////////////////////////////////////////////////////////////////////////
