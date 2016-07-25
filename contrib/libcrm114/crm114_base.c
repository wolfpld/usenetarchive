//	crm114_base.c - main library interface

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
#include "crm114_internal.h"


// Whether to sort/unique features before handing them to
// classifiers. Optimistic: assumes that all feature classifiers have
// been converted to library style, and that each feature classifier
// does the same sorting and uniquing for both learn and classify.

// values for when_sort and when_unique in array when_su[]
#define A 1		// always
#define U 0		// if CRM114_UNIQUE
#define N -1		// never

static const struct
{
  unsigned long long flags;
  int when_sort;
  int when_unique;
} when_su[] =
  {
    // Bill says: test whether Markov family saves time by always
    // sorting, so it goes through hash table sequentially.  (Saves
    // thrashing of cache and/or virtual memory.)  But this only works
    // if it's sorted on feature % hash_table_length.
    {CRM114_MARKOVIAN,  U, U},
    {CRM114_OSBF,       U, U},
    {CRM114_OSB_BAYES,  U, U},
    {CRM114_OSB_WINNOW, U, U},	// !!! changed by Bill 1/20/10 was A A
    {CRM114_HYPERSPACE, A, U},
#ifndef PRODUCTION_CLASSIFIERS_ONLY
    {CRM114_SVM,        A, U},
    {CRM114_FSCM,       N, N},
    {CRM114_NEURAL_NET, U, U},
    {CRM114_PCA,        A, U},
#endif
  };

// Find whether to sort and/or unique features for a features
// classifier.  Input arg flags is the usual; this function needs
// specifically a classifier bit and CRM114_UNIQUE.  Two return args:
// independent booleans (int, T/F) for sort and unique.
//
// Function returns T/F for found classifier (fails if input
// classifier bits don't specify exactly one classifier that takes
// features).
//
// Simple: just searches table above, converts when to T/F.

//    nb: name is "find feature row classifier sort unique", but shorter.
static int find_fr_clsf_su(unsigned long long flags, int *sort, int *unique)
{
  int ret;
  unsigned int i;

  for (ret = FALSE, i = 0; !ret && i < COUNTOF(when_su); i++)
    if ((flags & CRM114_FLAGS_CLASSIFIERS_MASK) == when_su[i].flags)
      {
	int when_sort = when_su[i].when_sort;
	int when_uniq = when_su[i].when_unique;

	*sort   = when_sort == A || (when_sort == U && (flags & CRM114_UNIQUE));
	*unique = when_uniq == A || (when_uniq == U && (flags & CRM114_UNIQUE));
	ret = TRUE;
      }

  return ret;
}

// End sort/unique helper stuff.


// Top-level library function: learn some text.

CRM114_ERR crm114_learn_text (CRM114_DATABLOCK **db, int whichclass,
			      const char text[], long textlen)
{
  struct crm114_feature_row *fr;
  struct crm114_feature_row *fr1;
  long nfr;		// number of struct crm114_feature_row
  long nfr1;		// modified number of struct crm114_feature_row
  long next_offset;
  CRM114_ERR err;

  switch ((*db)->cb.classifier_flags & CRM114_FLAGS_CLASSIFIERS_MASK)
    {
    case CRM114_OSB_BAYES:
#if 0	// after Markov consolidation
    case CRM114_MARKOVIAN:
    case CRM114_OSBF:
#endif
#if 0	// after classifier converted
    case CRM114_OSB_WINNOW:
#endif
    case CRM114_HYPERSPACE:
#ifndef PRODUCTION_CLASSIFIERS_ONLY
    case CRM114_SVM:
    case CRM114_PCA:
    case CRM114_FSCM:
#if 0	// after classifiers converted
    case CRM114_NEURAL_NET:
#endif	// 0
#endif	// !defined(PRODUCTION_CLASSIFIERS_ONLY)

      // allocate for max possible features -- quite a few
      nfr = textlen * (*db)->cb.tokenizer_pipeline_phases;
      if ((fr = malloc(nfr * sizeof(struct crm114_feature_row))) != NULL)
	{
	  if ((err = crm114_vector_tokenize(text, 0L, textlen, &(*db)->cb, fr,
					    nfr, &nfr1, &next_offset))
	      == CRM114_OK)
	    {
	      nfr = nfr1;
	      // give back excess memory
	      if (nfr > 0)	// realloc(0) -> free()
		if ((fr1 = realloc(fr, nfr * sizeof(struct crm114_feature_row)))
		    != NULL)
		  fr = fr1;
	      err = crm114_learn_features(db, whichclass, fr, &nfr);
	    }

	  free(fr);
	}
      else
	err = CRM114_NOMEM;
      break;

      //  *****  Classifiers after this point are "text direct"
      //  classifiers, and do not create features *****

#if 0	// after classifier converted

    case CRM114_CORRELATE:
      err = crm114_learn_text_correlate(db, whichclass, text, textlen);
      break;

#endif	// 0

    case CRM114_ENTROPY:
      err = crm114_learn_text_bit_entropy(db, whichclass, text, textlen);
      break;

    default:
      err = CRM114_BADARG;
      break;
    }

  return err;
}


// Top-level library function: learn some features.

CRM114_ERR crm114_learn_features (CRM114_DATABLOCK **db,
				  int whichclass,
				  struct crm114_feature_row fr[],
				  long *nfr)
{
  int sort;
  int unique;
  CRM114_ERR err;

  //  Get classifier sort and unique flags, if no such classifier fail
  if (find_fr_clsf_su((*db)->cb.classifier_flags, &sort, &unique))
    {
      // do sort and unique as required/forced/prohibited
      crm114_feature_sort_unique(fr, nfr, sort, unique);

      //   now dispatch to the proper per-classifier learn_features routine
      switch ((*db)->cb.classifier_flags & CRM114_FLAGS_CLASSIFIERS_MASK)
	{
	case CRM114_OSB_BAYES:
#if 0
	  // after Markov consolidation
	case CRM114_MARKOVIAN:
	case CRM114_OSBF:
#endif
	  err = crm114_learn_features_markov(db, whichclass, fr, *nfr);
	  break;
#if 0		// after classifiers redone for libcrm114
	case CRM114_OSB_WINNOW:
	  err = crm114_learn_features_osb_winnow(db, whichclass, fr, *nfr);
	  break;
#endif
	case CRM114_HYPERSPACE:
	  err = crm114_learn_features_hyperspace(db, whichclass, fr, *nfr);
	  break;
#ifndef PRODUCTION_CLASSIFIERS_ONLY
	case CRM114_SVM:
	  err = crm114_learn_features_svm(db, whichclass, fr, *nfr);
	  break;

	case CRM114_PCA:
	  err = crm114_learn_features_pca(db, whichclass, fr, *nfr);
	  break;

	case CRM114_FSCM:
	  err = crm114_learn_features_fscm(db, whichclass, fr, *nfr);
	  break;
#if 0	// after classifiers converted
	case CRM_NEURAL:
	  err = crm114_learn_features_neural(db, whichclass, fr, *nfr);
	  break;
#endif	// 0
#endif	// !defined(PRODUCTION_CLASSIFIERS_ONLY)

	default:
	  err = CRM114_BADARG;
	  break;
	}
    }
  else
    err = CRM114_BADARG;

  return err;
}


// Top-level library function: classify some text.

CRM114_ERR crm114_classify_text (CRM114_DATABLOCK *db, const char text[],
				 long textlen, CRM114_MATCHRESULT *result)
{
  struct crm114_feature_row *fr;
  struct crm114_feature_row *fr1;
  long nfr;		// number of struct crm114_feature_row
  long nfr1;		// modified number of struct crm114_feature_row
  long next_offset;
  CRM114_ERR err;

  switch (db->cb.classifier_flags & CRM114_FLAGS_CLASSIFIERS_MASK)
    {
    case CRM114_OSB_BAYES:
#if 0	// after Markov consolidation
    case CRM114_MARKOVIAN:
    case CRM114_OSBF:
#endif
#if 0	// after classifiers converted
    case CRM114_OSB_WINNOW:
#endif
    case CRM114_HYPERSPACE:
#ifndef PRODUCTION_CLASSIFIERS_ONLY
    case CRM114_SVM:
    case CRM114_PCA:
    case CRM114_FSCM:
#if 0	// after classifiers converted
    case CRM114_NEURAL_NET:
#endif	// 0
#endif	// !defined(PRODUCTION_CLASSIFIERS_ONLY)
      // allocate for max possible features -- quite a few
      nfr = textlen * db->cb.tokenizer_pipeline_phases;
      if ((fr = malloc(nfr * sizeof(struct crm114_feature_row))) != NULL)
	{
	  if ((err = crm114_vector_tokenize(text, 0L, textlen, &db->cb, fr,
					    nfr, &nfr1, &next_offset))
	      == CRM114_OK)
	    {
	      nfr = nfr1;
	      // give back excess memory
	      if (nfr > 0)	// realloc(0) -> free()
		if ((fr1 = realloc(fr, nfr * sizeof(struct crm114_feature_row)))
		    != NULL)
		  fr = fr1;
	      err = crm114_classify_features(db, fr, &nfr, result);
	    }

	  free(fr);
	}
      else
	err = CRM114_NOMEM;
      break;

      //     Classifiers that never, ever use the tokenizer go here!
      //
#if 0	// after classifiers converted

    case CRM114_CORRELATE:
      err = crm114_classify_text_correlate(db, text, textlen, result);
      break;

#endif	// 0

    case CRM114_ENTROPY:
      err = crm114_classify_text_bit_entropy(db, text, textlen, result);
      break;

    default:
      err = CRM114_BADARG;
      break;
    }

  return err;
}


// Top-level library function: classify some features.

CRM114_ERR crm114_classify_features (CRM114_DATABLOCK *db,
				     struct crm114_feature_row fr[],
				     long *nfr,
				     CRM114_MATCHRESULT *result)
{
  int sort;
  int unique;
  CRM114_ERR err;

  //  Get classifier sort and unique flags, if no such classifier fail
  if (find_fr_clsf_su(db->cb.classifier_flags, &sort, &unique))
    {
      // do sort and unique as required/forced/prohibited
      crm114_feature_sort_unique(fr, nfr, sort, unique);

      //   now dispatch to the proper per-classifier classify_features routine
      switch (db->cb.classifier_flags & CRM114_FLAGS_CLASSIFIERS_MASK)
	{
	case CRM114_OSB_BAYES:
#if 0
	  // after Markov consolidation
	case CRM114_MARKOVIAN:
	case CRM114_OSBF:
#endif
	  err = crm114_classify_features_markov(db, fr, *nfr, result);
	  break;
#if 0	// after classifiers converted
	case CRM114_OSB_WINNOW:
	  err = crm114_classify_features_osb_winnow(db, fr, *nfr, result);
	  break;
#endif
	case CRM114_HYPERSPACE:
	  err = crm114_classify_features_hyperspace(db, fr, *nfr, result);
	  break;
#ifndef PRODUCTION_CLASSIFIERS_ONLY
	case CRM114_SVM:
	  err = crm114_classify_features_svm(db, fr, *nfr, result);
	  break;
	case CRM114_PCA:
	  err = crm114_classify_features_pca(db, fr, *nfr, result);
	  break;
	case CRM114_FSCM:
	  err = crm114_classify_features_fscm(db, fr, *nfr, result);
	  break;
#if 0	// after classifiers converted
	case CRM_NEURAL:
	  err = crm114_classify_features_neural(db, fr, *nfr, result);
	  break;
#endif	// 0
#endif	// !defined(PRODUCTION_CLASSIFIERS_ONLY)

	default:
	  err = CRM114_BADARG;
	  break;
	}
    }
  else
    err = CRM114_BADARG;

  return err;
}



// Feature hash multiplier matrices for various classifiers.  See
// crm114_vector_tokenize() for a description of how they're used.
// Implementation is described here.

// These feature hash multiplier matrices exist in three places: the
// constant structures below, containing defaults for various
// classifiers; an argument to crm114_cb_setpipeline(); and a matrix
// in the CRM114_CONTROLBLOCK, which is filled in, passed around, and
// used at run time.  The constants and argument exist to be copied
// into the CRM114_CONTROLBLOCK.

// Each matrix is implemented as a two-dimensional C array.  The
// amount of data varies; we'd like to let the two dimensions vary.
// There are at least three ways to to that; we've tried them all and
// they all work, and we've decided we can't do any of them.

// Originally the matrices were declared as one-dimensional arrays,
// and run-time code grouped them into pairs, treating each array as
// if it were two-dimensional by emulating C array subscript
// calculation.  It's undesirable to emulate that internal C
// mechanism, but would be acceptable internally.  However, it's not
// reasonable to make callers of the library function
// crm114_cb_setpipeline() do that same emulation to set up the
// array they pass.

// A previous version of this code declared the constant matrices
// below as bare two-dimensional arrays without the enclosing
// structure, each array with different dimensions set at compile
// time, and used macros to extract the dimensions.  But macros have
// their well-known difficulties, so we ruled that out.

// ISO C (C99) has a way to let array dimensions vary at run time, for
// arrays declared in certain places.  Previous versions of these
// functions and crm114_vector_tokenize() used that.  However, it
// seems that Microsoft doesn't implement ISO C, and some people
// compile and use this program on Microsoft.

// So now each matrix has two fixed dimensions, the maximum, and also
// has two other smaller dimensions associated with it and used at run
// time, telling how much of the array has valid data.  C does the
// subscript calculations with the declared (maximum) dimensions.
// Unused parts of each array are ignored by code here, and generally
// zero.  Some memory is wasted, and some confusion of programmers is
// caused.


// a matrix array plus its two run-time dimensions
struct coeffs
{
  int rows;
  int columns;
  int coeffs[UNIFIED_ITERS_MAX][UNIFIED_WINDOW_MAX];
};


// Initializers contain less data than the array can hold.  Writing
// the initializers with full {} tells the compiler where the data
// should go; the other parts are zero.


static const struct coeffs markov_coeffs =
  {
    16, 5,			// run-time dimensions
    // array contents follow
    {
      {1, 0, 0,  0,  0},
      {1, 3, 0,  0,  0},
      {1, 0, 5,  0,  0},
      {1, 3, 5,  0,  0},
      {1, 0, 0, 11,  0},
      {1, 3, 0, 11,  0},
      {1, 0, 5, 11,  0},
      {1, 3, 5, 11,  0},
      {1, 0, 0,  0, 23},
      {1, 3, 0,  0, 23},
      {1, 0, 5,  0, 23},
      {1, 3, 5,  0, 23},
      {1, 0, 0, 11, 23},
      {1, 3, 0, 11, 23},
      {1, 0, 5, 11, 23},
      {1, 3, 5, 11, 23},
    }
  };

// This is used by many classifiers.
static const struct coeffs osb_coeffs =
  {
    4, OSB_BAYES_WINDOW_LEN,
    {
      {1, 3, 0,  0,  0},
      {1, 0, 5,  0,  0},
      {1, 0, 0, 11,  0},
      {1, 0, 0,  0, 23},
    }
  };

static const struct coeffs fscm_coeffs =
  {
    1, FSCM_DEFAULT_CODE_PREFIX_LEN,
    {{1, 3, 5, 7, 11, 13}}
  };

// now a couple funky modifier arrays

static const struct coeffs string_coeffs =
  {
    1, 5,
    {
      { 1,  3,  5,  7, 11}
    }
  };

static const struct coeffs unigram_coeffs =
  {
    1, 1,
    {
      { 1 }
    }
  };

// default regexes for a couple cases
static const char string_kern_regex[] = {"."};
static const char fscm_kern_regex[]   = {"."};


void crm114_cb_reset(CRM114_CONTROLBLOCK *p_cb)
{
  // various code counts on this block being initially zero
  memset(p_cb, '\0', sizeof(CRM114_CONTROLBLOCK));

  // Useful only in a disk file on a Unix system that has the program crm114,
  // and only in the future.  -E isn't implemented yet.
  strcpy(p_cb->system_identifying_text, "#!crm114 -E");
  p_cb->sysid_text_len = (int)strlen (p_cb->system_identifying_text);
}

// Given a CB with its flags and number of classes set, set its number
// of blocks and set its datablock_size to correspond to that many
// blocks of the chosen's classifier's default block size.
void crm114_cb_setblockdefaults(CRM114_CONTROLBLOCK *p_cb)
{
  size_t block_size;

  //  Set how many blocks.
  switch (p_cb->classifier_flags & CRM114_FLAGS_CLASSIFIERS_MASK)
    {
    case CRM114_SVM:
    case CRM114_PCA:
      p_cb->how_many_blocks = 1;   // SVM and PCA use one block only
      break;
    case CRM114_FSCM:
      // fscm uses two blocks per class
      p_cb->how_many_blocks = p_cb->how_many_classes * 2;
      break;
    default:
      p_cb->how_many_blocks = p_cb->how_many_classes;
      break;
    };

  // set average block size
  switch (p_cb->classifier_flags & CRM114_FLAGS_CLASSIFIERS_MASK)
    {
    case CRM114_MARKOVIAN:
      block_size =
	DEFAULT_MARKOVIAN_SPARSE_SPECTRUM_FILE_LENGTH
	* sizeof(MARKOV_FEATUREBUCKET);
      break;
    case CRM114_OSB_BAYES:
      block_size =
	DEFAULT_OSB_BAYES_SPARSE_SPECTRUM_FILE_LENGTH
	* sizeof(MARKOV_FEATUREBUCKET);
      break;
#if 0	// if classifier converted
    case CRM114_OSBF:
      block_size =
	OSBF_DEFAULT_SPARSE_SPECTRUM_FILE_LENGTH
	* sizeof(MARKOV_FEATUREBUCKET);
      break;
#endif
    case CRM114_ENTROPY:
      block_size = crm114__bit_entropy_default_class_size(p_cb);
     break;
    case CRM114_HYPERSPACE:
      block_size = DEFAULT_HYPERSPACE_BUCKETCOUNT
	* sizeof (HYPERSPACE_FEATUREBUCKET);
      break;
    case CRM114_SVM:
      // Expands automatically.  But, must be big enough to hold a
      // valid empty file -- headers and such.  With the filler --
      // room to grow -- that's added by the write routines, that's
      // currently about 14,000 bytes.  So let's say this number
      // should be at least 100,000.  See crm114__init_block_svm().
      //
      // We could make everything work correctly with zero memory
      // preallocated here.  But that would take a little work, and
      // we're in a hurry.
      block_size = 100000;
      break;
    case CRM114_PCA:
      // Expands automatically.
      block_size = 100000;
      break;
    case CRM114_FSCM:
      //  Eventually will expand automagically, but for now
      //  provide 1 megaslot of index space and 2 megaslots of chain
      //  space per class.
      block_size =
	(FSCM_DEFAULT_PREFIX_TABLE_CELLS * sizeof (FSCM_PREFIX_TABLE_CELL)
	 + FSCM_DEFAULT_FORWARD_CHAIN_CELLS * sizeof (FSCM_HASH_CHAIN_CELL))
	/ 2;
      break;
    default:
      block_size = DEFAULT_CLASS_SIZE;
      break;
    }

  // Now we lie: datablock won't be this big until classes are allocated.
  // But this is the API: this number is pulled from the CB at allocation
  // time to know how big to allocate the classes.
  p_cb->datablock_size =
    sizeof(*p_cb)
    + p_cb->how_many_blocks * block_size;
  if (crm114__internal_trace)
    fprintf (stderr, "Desired db size: %zu\n", p_cb->datablock_size);
  // can't set class sizes now; they're only set when memory is allocated
}

// Given a CB with its flags set, set its default number of classes,
// default class success flags, default number of blocks, and a
// datablock_size corresponding to the default number of blocks and
// the chosen classifier's default block size.
void crm114_cb_setclassdefaults(CRM114_CONTROLBLOCK *p_cb)
{
  switch (p_cb->classifier_flags & CRM114_FLAGS_CLASSIFIERS_MASK)
    {
    case CRM114_SVM:
    case CRM114_PCA:
      p_cb->how_many_blocks = 1;
      p_cb->how_many_classes = 2;
      p_cb->class[0].success = 1;
      p_cb->class[1].success = 0;
      break;
    case CRM114_FSCM:
      p_cb->how_many_classes = DEFAULT_HOW_MANY_CLASSES;
      p_cb->how_many_blocks = 2 * p_cb->how_many_classes;
      p_cb->class[0].success = 1;
      p_cb->class[1].success = 0;
      break;
    default:
      p_cb->how_many_blocks = p_cb->how_many_classes = DEFAULT_HOW_MANY_CLASSES;
      // ??? default: first class success, all others failure
      p_cb->class[0].success = 1;  // first class default success
      p_cb->class[1].success = 0;  // second class default fails
      break;
    }
  crm114_cb_setblockdefaults(p_cb);
}

void crm114_cb_setdefaults(CRM114_CONTROLBLOCK *p_cb)
{
      crm114_cb_reset(p_cb);
      (void)crm114_cb_setflags(p_cb, (unsigned long long)0);
      crm114_cb_setclassdefaults(p_cb);
}

CRM114_CONTROLBLOCK *crm114_new_cb(void)
{
  CRM114_CONTROLBLOCK *p_cb;

  if ((p_cb = malloc(sizeof(*p_cb))) != NULL)
    crm114_cb_setdefaults(p_cb);
  return p_cb;
}


#define FORCE_INTO_RANGE(var, min, max)		\
  {						\
    if ((var) < (min))				\
      (var) = (min);				\
    if ((var) > (max))				\
      (var) = (max);				\
  }

static void libcrm_cb_force_sane(CRM114_CONTROLBLOCK *p_cb)
{
  FORCE_INTO_RANGE(p_cb->tokenizer_pipeline_len, 0, UNIFIED_WINDOW_MAX);
  FORCE_INTO_RANGE(p_cb->tokenizer_pipeline_phases, 0, UNIFIED_ITERS_MAX);
  FORCE_INTO_RANGE(p_cb->how_many_blocks, 0, CRM114_MAX_BLOCKS);
  FORCE_INTO_RANGE(p_cb->how_many_classes, 0, CRM114_MAX_CLASSES);
  switch (p_cb->classifier_flags & CRM114_FLAGS_CLASSIFIERS_MASK)
    {
    case CRM114_FSCM:
      p_cb->how_many_blocks = 2 * p_cb->how_many_classes;
      break;
    }
}


CRM114_DATABLOCK *crm114_new_db (CRM114_CONTROLBLOCK *p_cb)
{
  CRM114_DATABLOCK *p_db;
  unsigned long long classifier;
  int block_pair_size;
  int cell_pair_size;
  int nprefixcell;
  int nchaincell;
  size_t block_size;
  size_t total_block_size;
  int i;

  libcrm_cb_force_sane(p_cb);

  // Allocate and zero the whole thing.  Various code counts on all of
  // this, including data blocks, being initially zero.
  if ((p_db = calloc(p_cb->datablock_size, 1)) != NULL)
    {
      p_db->cb = *p_cb;		// copy CRM114_CONTROLBLOCK

      classifier = p_db->cb.classifier_flags & CRM114_FLAGS_CLASSIFIERS_MASK;

      // Calculate block size.  For all classifiers but FSCM, all
      // blocks are the same size, put in variable block_size.  FSCM
      // uses two different block sizes based on block number; set
      // nprefixcell and nchaincell.  Set both sets of variables in
      // both cases, else gcc thinks maybe the one not set is used
      // uninitialized later, even though it's not used.
      switch (classifier)
	{
	case CRM114_FSCM:
	  block_size = 0;	// pacify compiler
	  block_pair_size = (int)((p_db->cb.datablock_size - sizeof(p_db->cb))
				  / (p_db->cb.how_many_blocks / 2));
	  cell_pair_size = sizeof(FSCM_PREFIX_TABLE_CELL)
	    + sizeof(FSCM_HASH_CHAIN_CELL);
	  nprefixcell = nchaincell = block_pair_size / cell_pair_size;
	  if (nprefixcell > 2000000)
	    {
	      nprefixcell = 2000000;
	      nchaincell = (block_pair_size
			    - nprefixcell * (int)sizeof(FSCM_PREFIX_TABLE_CELL))
		/ sizeof(FSCM_HASH_CHAIN_CELL);
	    }
	  break;
	default:
	  block_size = (p_db->cb.datablock_size - sizeof(*p_db))
	    / p_db->cb.how_many_blocks;
	  // pacify compiler
	  nprefixcell = 0;
	  nchaincell = 0;
	  break;
	}

      // For each block, set start_offset and allocated_size, and
      // initialize its contents if the classifier wants anything
      // other than the all zeroes we did with calloc() above.
      total_block_size = 0;
      for (i = 0; i < p_db->cb.how_many_blocks; i++)
	{
	  switch (classifier)
	    {
	    case CRM114_FSCM:
	      p_db->cb.block[i].allocated_size =
		(i % 2)
		? nchaincell * (int)sizeof(FSCM_HASH_CHAIN_CELL)
		: nprefixcell * (int)sizeof(FSCM_PREFIX_TABLE_CELL);
	      break;
	    default:
	      p_db->cb.block[i].allocated_size = block_size;
	      break;
	    }
	  p_db->cb.block[i].start_offset = total_block_size;
	  total_block_size += p_db->cb.block[i].allocated_size;
	  // initialize class data structures, etc.
	  switch (classifier)
	    {
	    case CRM114_ENTROPY:
	      crm114__init_block_bit_entropy(p_db, i);
	      break;
	    case CRM114_SVM:
	      crm114__init_block_svm(p_db, i);
	      break;
	    case CRM114_PCA:
	      crm114__init_block_pca(p_db, i);
	      break;
	    case CRM114_FSCM:
	      crm114__init_block_fscm (p_db, i);
	      break;
	    }
	}
    }

  return p_db;
}


// return the run-time dimensions of the coefficients matrix in a CRM114_CONTROLBLOCK
void crm114_cb_getdimensions(const CRM114_CONTROLBLOCK *p_cb,
			     int *pipe_len, int *pipe_iters)
{
  *pipe_len = p_cb->tokenizer_pipeline_len;
  *pipe_iters = p_cb->tokenizer_pipeline_phases;
}


static void clear_regex(CRM114_CONTROLBLOCK *p_cb)
{
  memset(p_cb->tokenizer_regex, '\0', sizeof(p_cb->tokenizer_regex));
  p_cb->tokenizer_regexlen = 0;
}

static void clear_coeffs(CRM114_CONTROLBLOCK *p_cb)
{
  memset(p_cb->tokenizer_pipeline_coeffs, '\0',
	 sizeof(p_cb->tokenizer_pipeline_coeffs));
  p_cb->tokenizer_pipeline_phases = p_cb->tokenizer_pipeline_len = 0;
}

// copy a C string (null-terminated) into a CRM114_CONTROLBLOCK regex
static void copy_regex(CRM114_CONTROLBLOCK *p_cb, const char string[])
{
  p_cb->tokenizer_regexlen = (int)strlen(string);
  memcpy(p_cb->tokenizer_regex, string, p_cb->tokenizer_regexlen);
}

// copy a struct coeffs into a CRM114_CONTROLBLOCK
static void copy_coeffs(CRM114_CONTROLBLOCK *p_cb, const struct coeffs *lump)
{
  int i, j;

  clear_coeffs(p_cb);
  p_cb->tokenizer_pipeline_phases = lump->rows;
  p_cb->tokenizer_pipeline_len = lump->columns;
  for (i = 0; i < p_cb->tokenizer_pipeline_phases; i++)
    for (j = 0; j < p_cb->tokenizer_pipeline_len; j++)
      p_cb->tokenizer_pipeline_coeffs[i][j] = lump->coeffs[i][j];
}


CRM114_ERR crm114_cb_setflags(CRM114_CONTROLBLOCK *p_cb,
			      unsigned long long flags)
{
  // if caller doesn't know what he wants, give him OSB
  //  Usually this is already set, but if not, we fix it again!
  if ((flags & CRM114_FLAGS_CLASSIFIERS_MASK) == 0)
    flags |= CRM114_OSB_BAYES;

  p_cb->classifier_flags = flags;

  //  Note that if p_cb->tokenizer_regex is "" (string of length 0) then
  //  the vector tokenizer uses isgraph() to decide what's a token string.
  //  So, not setting any regex yields (roughly) [[:graph:]]+ as a regex.

  // set defaults for each classifier
  switch (flags & CRM114_FLAGS_CLASSIFIERS_MASK)
    {
    case CRM114_MARKOVIAN:
      // no regex, tokenizer defaults
      clear_regex(p_cb);
      copy_coeffs(p_cb, &markov_coeffs);
      break;
    case CRM114_OSB_BAYES:
    case CRM114_OSBF:
    case CRM114_OSB_WINNOW:
    case CRM114_HYPERSPACE:
    case CRM114_SVM:
    case CRM114_PCA:
    case CRM114_NEURAL_NET:
      // no regex, tokenizer defaults
      clear_regex(p_cb);
      copy_coeffs(p_cb, &osb_coeffs);
      break;
    case CRM114_FSCM:
      copy_regex(p_cb, fscm_kern_regex);
      copy_coeffs(p_cb, &fscm_coeffs);
      break;
    case CRM114_CORRELATE:
    case CRM114_ENTROPY:
      // These don't tokenize. No regex, no matrix.
      clear_regex(p_cb);
      clear_coeffs(p_cb);
      return CRM114_OK;
    default:
      // error, number of classifiers is not 1
      return CRM114_BADARG;
    }

  // Implement a couple modifier flags.  Use a different matrix and
  // maybe a different regex.
  //
  // Note that you can specify both of these flags, and get the string
  // regex and the unigram matrix.
  if (flags & CRM114_STRING)
    {
      copy_regex(p_cb, string_kern_regex);
      copy_coeffs(p_cb, &string_coeffs);
    }
  if (flags & CRM114_UNIGRAM)
    copy_coeffs(p_cb, &unigram_coeffs);

  return CRM114_OK;
}


CRM114_ERR crm114_cb_setregex(CRM114_CONTROLBLOCK *p_cb, const char regex[],
			      int regex_len)
{
  CRM114_ERR ret;

  if (regex_len >= 0 && regex_len <= (int)sizeof(p_cb->tokenizer_regex))
    {
      memcpy(p_cb->tokenizer_regex, regex, regex_len);
      p_cb->tokenizer_regexlen = regex_len;
      ret = CRM114_OK;
    }
  else
    ret = CRM114_BADARG;

  return ret;
}


CRM114_ERR crm114_cb_setpipeline
(
 CRM114_CONTROLBLOCK *p_cb,
 int pipe_len, int pipe_iters,
 const int pipe_coeffs[UNIFIED_ITERS_MAX][UNIFIED_WINDOW_MAX]
 )
{
  CRM114_ERR ret;

  if (pipe_len > 0 && pipe_len <= UNIFIED_WINDOW_MAX
      && pipe_iters > 0 && pipe_iters <= UNIFIED_ITERS_MAX)
    {
      int irow, icol;

      clear_coeffs(p_cb);
      p_cb->tokenizer_pipeline_len = pipe_len;
      p_cb->tokenizer_pipeline_phases = pipe_iters;
      for (irow = 0; irow < p_cb->tokenizer_pipeline_phases; irow++)
	for (icol = 0; icol < p_cb->tokenizer_pipeline_len; icol++)
	  p_cb->tokenizer_pipeline_coeffs[irow][icol]
	    = pipe_coeffs[irow][icol];
      ret = CRM114_OK;
    }
  else
    ret = CRM114_BADARG;

  return ret;
}


/*
  Translate both directions between a CRM114_DATABLOCK and a text
  file.  These text files are not just display for a human; they're
  intended as correct and complete storage (and portable storage, or
  at least as portable as any text file).  They do contain some
  human-friendly descriptive strings, though.  (Friendly if the human
  reads English.)

  Formatting is mostly printf()/scanf(), and is relatively high-level.
  Each classifier reads and writes its own data, in some meaningful
  form.  Output doesn't blindly write every byte in the datablock;
  unused data is not written, and on read is assumed to be zero.

  Lines can get pretty long, but not monstrous.  A couple K of line
  buffer should be enough.  The character set is ASCII.

  Speed is not a design goal.

  On read, a CRM114_DATABLOCK is malloc()ed, of the size specified in
  the text file's datablock_size.  A pointer to it is returned.

                                                    Kurt Hackenberg
                                                    March 2010
*/


/*
C-style quoted strings, with a subset of C \ escapes, but counted, not
NUL-terminated, so can include any byte, including NUL and multibyte
characters.
*/

// isascii() is common but non-standard
// this def lifted from GCC's ctype.h
#define	ISASCII(c)	(((c) & ~0x7f) == 0)	/* If C is a 7 bit value.  */

// if you want to code up a check for error on every putc(), go for it
static int write_counted_text_string_fp(const char str[], int len, FILE *fp)
{
  int i;

  putc('"', fp);
  for (i = 0; i < len; i++)
    switch (str[i])
      {
      case '\\':
      case '"':
	fprintf(fp, "\\%c", (unsigned char)str[i]);
	break;
      default:
	// ISASCII() is used here because it's not clear whether
	// isprint() works on a non-ASCII byte
	if (ISASCII((unsigned char)str[i]) && isprint((unsigned char)str[i]))
	  putc((unsigned char)str[i], fp);
	else
	  fprintf(fp, "\\%.3o", (unsigned char)str[i]);
	break;
      }
  putc('"', fp);

  return TRUE;
}

// NUL-terminated version
static int write_text_string_fp(const char str[], FILE *fp)
{
  return write_counted_text_string_fp(str, strlen(str), fp);
}

// skips leading whitespace
// max str len is bufsize - 1
// returns T/F success/failure
static int read_counted_text_string_fp(char buf[], int bufsize, int *len,
				       FILE *fp)
{
  enum
  {
    NOT_STRING,
    STRING,
    BACKSLASH,
    OCTAL
  } state = NOT_STRING;
  int i = 0;			// subscript in returned string
  int ret = FALSE;
  int c;
  // next 2 init 0 unnecessary, but GCC not sure
  char ochar = 0;
  int n_odigit = 0;

  if (bufsize <= 0)
    return FALSE;

  while ((c = getc(fp)) != EOF && isspace(c))
    ;
  if (c != EOF)
    {
      if (c == '"')
	{
	  state = STRING;
	  while (state != NOT_STRING && i < bufsize && (c = getc(fp)) != EOF)
	    switch (state)
	      {
	      case NOT_STRING:	// impossible
		break;
	      case STRING:
		switch (c)
		  {
		  case '"':
		    state = NOT_STRING;
		    ret = TRUE;
		    break;
		  case '\\':
		    state = BACKSLASH;
		    break;
		  default:
		    buf[i++] = c;
		    break;
		  }
		break;
	      case BACKSLASH:
		switch (c)
		  {
		  case '0':
		  case '1':
		  case '2':
		  case '3':
		  case '4':
		  case '5':
		  case '6':
		  case '7':
		    ochar = c - '0';
		    n_odigit = 1;
		    state = OCTAL;
		    break;
#if 0
		  case 'n':
		    buf[i++] = '\n';
		    state = STRING;
		    break;
		    // etc.: \a \b \f \n \r \t \v
#endif
		  default:	// includes \\ and \"
		    buf[i++] = c;
		    state = STRING;
		    break;
		  }
		break;
	      case OCTAL:
		switch (c)
		  {
		  case '0':
		  case '1':
		  case '2':
		  case '3':
		  case '4':
		  case '5':
		  case '6':
		  case '7':
		    ochar = ochar << 3 | (c - '0');
		    if (++n_odigit == 3)
		      {
			buf[i++] = ochar;
			state = STRING;
		      }
		    break;
		  default:
		    buf[i++] = ochar;
		    ungetc(c, fp);
		    state = STRING;
		    break;
		  }
	      }
	}
      else
	ungetc(c, fp);		// never mind
    }

  *len = i;
  return ret;
}

// NUL-terminated version
static int read_text_string_fp(char buf[], int bufsize, FILE *fp)
{
  int len;
  int ret;

  if (bufsize > 0)
    {
      ret = read_counted_text_string_fp(buf, bufsize - 1, &len, fp);
      buf[len] = '\0';
    }
  else
    ret = FALSE;

  return ret;
}

// Returns T/F success/failure.
int crm114_cb_write_text_fp(const CRM114_CONTROLBLOCK *cb, FILE *fp)
{
  int row, col;
  int i;

  write_counted_text_string_fp(cb->system_identifying_text,
			       cb->sysid_text_len, fp);
  fprintf(fp, "\n");

  write_counted_text_string_fp(cb->user_identifying_text,
			       cb->userid_text_len, fp);
  fprintf(fp, "\n");

  fprintf(fp, TN_DATABLOCK_SIZE " %zu\n", cb->datablock_size);
  fprintf(fp, TN_FLAGS " %#llx\n", cb->classifier_flags);

  switch (cb->classifier_flags & CRM114_FLAGS_CLASSIFIERS_MASK)
    {
    case CRM114_MARKOVIAN:
    case CRM114_OSB_BAYES:
    case CRM114_OSBF:
      // this can get to be a long line
      fprintf(fp, TN_MARKOV_PHASE_WEIGHTS);
      for (i = 0; i < COUNTOF(cb->params.markov.phase_weights); i++)
	fprintf(fp, " %d", cb->params.markov.phase_weights[i]);
      fprintf(fp, "\n");
      break;
    case CRM114_ENTROPY:
      fprintf(fp, TN_BE_CROSSLINK_THRESH " " PRINTF_DOUBLE_FMT "\n",
	      cb->params.bit_entropy.crosslink_thresh);
      break;
    }

  fprintf(fp, TN_REGEX " ");
  write_counted_text_string_fp(cb->tokenizer_regex,
			       cb->tokenizer_regexlen, fp);
  fprintf(fp, "\n");

  fprintf(fp, TN_PIPE_PHASES " %d " TN_PIPE_LEN " %d\n",
	  cb->tokenizer_pipeline_phases, cb->tokenizer_pipeline_len);
  for (row = 0; row < cb->tokenizer_pipeline_phases; row++)
    {
      for (col = 0; col < cb->tokenizer_pipeline_len; col++)
	fprintf(fp, " %d", cb->tokenizer_pipeline_coeffs[row][col]);
      fprintf(fp, "\n");
    }

  fprintf(fp, TN_BLOCKS " %d\n", cb->how_many_blocks);
  for (i = 0; i < cb->how_many_blocks; i++)
    fprintf(fp, "%zu %zu %zu " PRINTF_FLOAT_FMT "\n",
	    cb->block[i].start_offset,
	    cb->block[i].allocated_size,
	    cb->block[i].size_used,
	    cb->block[i].clsf_frac_used);

  fprintf(fp, TN_CLASSES " %d\n", cb->how_many_classes);
  for (i = 0; i < cb->how_many_classes; i++)
    {
      write_text_string_fp(cb->class[i].name, fp);
      fprintf(fp, " %s %d %d\n",
	      cb->class[i].success ? TN_CLASS_SUCCESS : TN_CLASS_FAILURE,
	      cb->class[i].documents,
	      cb->class[i].features);
    }

  return TRUE;			// optimistic, aren't we?
}

// Returns T/F success/failure.
int crm114_cb_write_text(const CRM114_CONTROLBLOCK *cb, const char filename[])
{
  FILE *fp;
  int ret;

  if ((fp = fopen(filename, "w")) != NULL)
    {
      ret = crm114_cb_write_text_fp(cb, fp);
      fclose(fp);
    }
  else
    ret = FALSE;

  return ret;
}

// Returns T/F success/failure.
int crm114_db_write_text_fp(const CRM114_DATABLOCK *db, FILE *fp)
{
  (void)crm114_cb_write_text_fp(&db->cb, fp);
  // write learned data
  switch (db->cb.classifier_flags & CRM114_FLAGS_CLASSIFIERS_MASK)
    {
    case CRM114_SVM:
      // this doesn't modify *db, but hard to declare it so
      (void)crm114__svm_learned_write_text_fp((CRM114_DATABLOCK *)db, fp);
      break;
    case CRM114_PCA:
      // this doesn't modify *db, but hard to declare it so
      (void)crm114__pca_learned_write_text_fp((CRM114_DATABLOCK *)db, fp);
      break;
    case CRM114_HYPERSPACE:
      (void)crm114__hyperspace_learned_write_text_fp(db, fp);
      break;
    case CRM114_MARKOVIAN:
    case CRM114_OSB_BAYES:
    case CRM114_OSBF:
      (void)crm114__markov_learned_write_text_fp(db, fp);
      break;
    case CRM114_ENTROPY:
      (void)crm114__bit_entropy_learned_write_text_fp(db, fp);
      break;
    case CRM114_FSCM:
      (void)crm114__fscm_learned_write_text_fp(db, fp);
      break;
    default:
      return CRM114_NOT_YET_IMPLEMENTED;
    }
  return CRM114_OK;
}

// Returns T/F success/failure.
int crm114_db_write_text(const CRM114_DATABLOCK *db, const char filename[])
{
  FILE *fp;
  int ret;

  if ((fp = fopen(filename, "w")) != NULL)
    {
      ret = crm114_db_write_text_fp(db, fp);
      fclose(fp);
    }
  else
    ret = CRM114_OPEN_FAILED;

  return ret;
}


//  read in the control block (cb) for a text-version 
//  db.  Note that we do this as separately, as the cb
//  is constant size and always at the start of a db.
CRM114_CONTROLBLOCK *crm114_cb_read_text_fp(FILE *fp)
{
  CRM114_CONTROLBLOCK *cb;
  int row, col;
  int i;

  if ((cb = calloc(1, sizeof(*cb))) == NULL)
    goto err;

  if ( !read_counted_text_string_fp(cb->system_identifying_text,
				    sizeof(cb->system_identifying_text),
				    &cb->sysid_text_len,
				    fp))
    goto err1;

  if ( !read_counted_text_string_fp(cb->user_identifying_text,
				    sizeof(cb->user_identifying_text),
				    &cb->userid_text_len,
				    fp))
    goto err1;

  if (fscanf(fp, " " TN_DATABLOCK_SIZE " %zu", &cb->datablock_size) != 1)
    goto err1;
  if (fscanf(fp, " " TN_FLAGS " %llx", &cb->classifier_flags) != 1)
    goto err1;

  switch (cb->classifier_flags & CRM114_FLAGS_CLASSIFIERS_MASK)
    {
    case CRM114_MARKOVIAN:
    case CRM114_OSB_BAYES:
    case CRM114_OSBF:
      // this can get to be a long line
      if (fscanf(fp, " " TN_MARKOV_PHASE_WEIGHTS) != 0)
	goto err1;
      for (i = 0; i < COUNTOF(cb->params.markov.phase_weights); i++)
	if (fscanf(fp, " %d", &cb->params.markov.phase_weights[i]) != 1)
	  goto err1;
      break;
    case CRM114_ENTROPY:
      if (fscanf(fp, " " TN_BE_CROSSLINK_THRESH " " SCANF_DOUBLE_FMT,
		 &cb->params.bit_entropy.crosslink_thresh) != 1)
	goto err1;
      break;
    }

  if (fscanf(fp, " " TN_REGEX) != 0)
    goto err1;
  if ( !read_counted_text_string_fp(cb->tokenizer_regex,
				    sizeof(cb->tokenizer_regex),
				    &cb->tokenizer_regexlen,
				    fp))
    goto err1;

  if (fscanf(fp, " " TN_PIPE_PHASES " %d " TN_PIPE_LEN " %d",
	     &cb->tokenizer_pipeline_phases, &cb->tokenizer_pipeline_len) != 2)
    goto err1;
  for (row = 0; row < cb->tokenizer_pipeline_phases; row++)
    for (col = 0; col < cb->tokenizer_pipeline_len; col++)
      if (fscanf(fp, " %d", &cb->tokenizer_pipeline_coeffs[row][col]) != 1)
	goto err1;

  if (fscanf(fp, " " TN_BLOCKS " %d", &cb->how_many_blocks) != 1)
    goto err1;
  for (i = 0; i < cb->how_many_blocks; i++)
    if (fscanf(fp, " %zu %zu %zu " SCANF_FLOAT_FMT,
	       &cb->block[i].start_offset,
	       &cb->block[i].allocated_size,
	       &cb->block[i].size_used,
	       &cb->block[i].clsf_frac_used) != 4)
      goto err1;

  if (fscanf(fp, " " TN_CLASSES " %d", &cb->how_many_classes) != 1)
    goto err1;
  for (i = 0; i < cb->how_many_classes; i++)
    {
      if ( !read_text_string_fp(cb->class[i].name,
				sizeof(cb->class[i].name),
				fp))
	goto err1;
      if ( !crm114__tf_read_text_fp(&cb->class[i].success, TN_CLASS_SUCCESS,
			       TN_CLASS_FAILURE, fp))
	goto err1;
      if (fscanf(fp, " %d %d",
		 &cb->class[i].documents,
		 &cb->class[i].features) != 2)
	goto err1;
    }

  return cb;

 err1:
  free(cb);
 err:
  return NULL;
}

CRM114_CONTROLBLOCK *crm114_cb_read_text(const char filename[])
{
  FILE *fp;
  CRM114_CONTROLBLOCK *cb;

  if ((fp = fopen(filename, "r")) != NULL)
    {
      cb = crm114_cb_read_text_fp(fp);
      fclose(fp);
    }
  else
    cb = NULL;

  return cb;
}

CRM114_DATABLOCK *crm114_db_read_text_fp(FILE *fp)
{
  CRM114_CONTROLBLOCK *cb;
  CRM114_DATABLOCK *db;
  int ok = FALSE;

  if ((cb = crm114_cb_read_text_fp(fp)) != NULL)
    {
      if ((db = calloc(cb->datablock_size, 1)) != NULL)
	{
	  memcpy(&db->cb, cb, sizeof(db->cb));
	  switch (db->cb.classifier_flags & CRM114_FLAGS_CLASSIFIERS_MASK)
	    {
	    case CRM114_SVM:
	      ok = crm114__svm_learned_read_text_fp(&db, fp);
	      break;
	    case CRM114_PCA:
	      ok = crm114__pca_learned_read_text_fp(&db, fp);
	      break;
	    case CRM114_HYPERSPACE:
	      ok = crm114__hyperspace_learned_read_text_fp(&db, fp);
	      break;
	    case CRM114_MARKOVIAN:
	    case CRM114_OSB_BAYES:
	    case CRM114_OSBF:
	      ok = crm114__markov_learned_read_text_fp(&db, fp);
	      break;
	    case CRM114_ENTROPY:
	      ok = crm114__bit_entropy_learned_read_text_fp(&db, fp);
	      break;
	    case CRM114_FSCM:
	      ok = crm114__fscm_learned_read_text_fp (&db, fp);
	    default:
	      ok = TRUE;	// remove this when all classifiers done
	      break;
	    }

	  if ( !ok)
	    {
	      free(db);
	      db = NULL;
	    }
	}

      free(cb);
    }
  else
    db = NULL;

  return db;
}

CRM114_DATABLOCK *crm114_db_read_text(const char filename[])
{
  FILE *fp;
  CRM114_DATABLOCK *db;

  if ((fp = fopen(filename, "r")) != NULL)
    {
      db = crm114_db_read_text_fp(fp);
      fclose(fp);
    }
  else
    db = NULL;

  return db;
}


//  Resize a block in the db.  Note that this is a "double underscore"
//  routine and therefore is global, but not documented to the user nor
//  recommended for user code to call.

CRM114_ERR crm114__resize_a_block (CRM114_DATABLOCK **db,
			 int whichblock,
			 size_t new_block_size)
{
  size_t old_db_size, db_increase, new_db_size;
  size_t move_old_begin_index, move_new_begin_index;
  int i;
  char *move_src, *move_target ;
  size_t bytes_to_move;
  CRM114_DATABLOCK *newdb;
  
  if (crm114__user_trace)
    fprintf (stderr, 
	     "Resizing the db: dblen %d, block %d oldlen %d newlen %d\n", 
	     (int) (*db)->cb.datablock_size, whichblock, 
	     (int) (*db)->cb.block[whichblock].allocated_size, 
	     (int) new_block_size);
  
  if (crm114__internal_trace)
    for (i = 0; i < (*db)->cb.how_many_blocks; i++)
      {
	fprintf (stderr, "block %d start %zu size %zu end %zu\n",
		 i,
		 (*db)->cb.block[i].start_offset,
		 (*db)->cb.block[i].allocated_size,
		 (*db)->cb.block[i].start_offset
		 +(*db)->cb.block[i].allocated_size - 1);
      };


  //
  //    Reallocate this block big enough...
  old_db_size = (*db)->cb.datablock_size;
  db_increase = new_block_size - (*db)->cb.block[whichblock].allocated_size;
  new_db_size = old_db_size + db_increase;
  //   use realloc, which expands in place if possible and copies
  //   to new memory if it can't expand in place.
  if (crm114__internal_trace)
    fprintf (stderr, "before realloc, old db is at %ld\n", (long int) *db);
  // make sure *db is always valid when we return: either what it was
  // or new block
  newdb = (CRM114_DATABLOCK *) realloc (*db, new_db_size);
  if (crm114__internal_trace)
    fprintf (stderr, "after realloc, new db is at %ld\n", (long int) newdb);
  if (newdb == NULL)
    return (CRM114_NOMEM);
  *db = newdb;
  (*db)->cb.datablock_size = new_db_size;

  //  *db is now big enough; if we're not the last block,
  //   move the remaining blocks up to give the space to whichblock's
  //   block.
  if (whichblock < (*db)->cb.how_many_blocks)
    {
      move_old_begin_index =(*db)->cb.block[whichblock+1].start_offset;
      move_new_begin_index = move_old_begin_index + db_increase;
      //  bytes_to_move is what's past the last valid block - 
      //  note that valid is only up to blocknum < cb.how_many_blocks,
      //  and is otherwise zero
      bytes_to_move = whichblock + 1 < (*db)->cb.how_many_blocks ? 
	old_db_size 
      	- sizeof (CRM114_CONTROLBLOCK)
      	- (*db)->cb.block[whichblock+1].start_offset : 0 ;
      // bytes_to_move = 0;
      //for (i = whichblock + 1; i < (*db)->cb.how_many_blocks; i++)
      //	{
      //  if (crm114__internal_trace)
      //    fprintf (stderr, "block %d is %zu bytes\n", i,
      //	     (*db)->cb.block[i].allocated_size);
      //  bytes_to_move += (*db)->cb.block[i].allocated_size;
      //};
      move_src = & (*db)->data[move_old_begin_index];
      move_target = & (*db)->data[move_new_begin_index];
      if (crm114__internal_trace)
	fprintf(stderr, "OBI: %d, NBI:%d, BTM: %d, MS: %ld, MT: %ld\n",
		(int) move_old_begin_index,
		(int) move_new_begin_index,
		(int) bytes_to_move,
		(long) move_src, (long)move_target);
      memmove (move_target, move_src, bytes_to_move);
    };
  //  update the block table to match the new start of whichblock
  (*db)->cb.block[whichblock].allocated_size += db_increase;
  //  update the block table to match the new start of subsequent blocks
  for (i = whichblock + 1; i < (*db)->cb.how_many_blocks; i++)
    { (*db)->cb.block[i].start_offset += db_increase; };
  if (crm114__internal_trace)
    fprintf (stderr,
	     "upping block %d by %d bytes; now block table is:\n",
	     whichblock, (int) db_increase);
   if (crm114__internal_trace)
    for (i = 0; i < (*db)->cb.how_many_blocks; i++)
      {
	fprintf (stderr, "block %d start %zu size %zu end %zu\n",
		 i,
		 (*db)->cb.block[i].start_offset,
		 (*db)->cb.block[i].allocated_size,
		 (*db)->cb.block[i].start_offset
		 +(*db)->cb.block[i].allocated_size - 1);
      };
   if (crm114__internal_trace)
     fprintf (stderr, "Final db size %ld completed successfully\n",
	      (long) (*db)->cb.datablock_size);
   return (CRM114_OK);
}

