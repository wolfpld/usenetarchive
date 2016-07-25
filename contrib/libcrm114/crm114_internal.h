//	crm114_internal.h
// Library cross-file stuff used only internally, not part of the API.

// Copyright 2010 William S. Yerazunis.
//
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

#ifndef	__CRM114_INTERNAL_H__
#define	__CRM114_INTERNAL_H__


#define TRUE 1
#define FALSE 0

// number of elements in an array
#define COUNTOF(array) (sizeof(array) / sizeof((array)[0]))

// a and b are any expressions; one of them is evaluated twice
#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define MIN(a, b) ((a) <= (b) ? (a) : (b))


// resize a block inside a db.
CRM114_ERR crm114__resize_a_block ( CRM114_DATABLOCK **db,
				  int whichblock,
				  size_t new_block_size);

// string copy, don't overflow, always terminate
char *crm114__strn1cpy(char dst[], const char src[], size_t n);

// calculate a pR
double crm114__pR(double p,		// probability
	  double comp);		// complement, 1 - p, sum of other probs

// clear a result, copy in everything it copies from a cb
void crm114__clear_copy_result(CRM114_MATCHRESULT *r,
			       const CRM114_CONTROLBLOCK *cb);

// Given a result that's had everything copied in from its CB (eg, by
// crm114__clear_copy_result() above), and has also had its per-class
// probabilities set, calculate and set its per-class pR, total
// success probability, overall pR, and best match.
void crm114__result_pR_outcome(CRM114_MATCHRESULT *r);

// The whole schmeer, for classifiers that have an array of class probabilities.
void crm114__result_do_common(CRM114_MATCHRESULT *r,
			      const CRM114_CONTROLBLOCK *cb,
			      const double ptc[CRM114_MAX_CLASSES]);

size_t crm114__bit_entropy_default_class_size(const CRM114_CONTROLBLOCK *p_cb);

void crm114__init_block_bit_entropy(CRM114_DATABLOCK *p_db, int whichclass);
void crm114__init_block_svm(CRM114_DATABLOCK *db, int c);
void crm114__init_block_pca(CRM114_DATABLOCK *db, int c);
CRM114_ERR crm114__init_block_fscm ( CRM114_DATABLOCK *p_db, int whichblock);


//     microgroom a Markov family hash table
int crm114__markov_microgroom (MARKOV_FEATUREBUCKET *h, int hs, unsigned int h1);
//
//     and microgrooming for winnow files
long crm114__winnow_microgroom (WINNOW_FEATUREBUCKET *h,
			    unsigned char *seen_features ,
			    unsigned long hfsize,
			    unsigned long hindex);

void crm114__pack_winnow_css (WINNOW_FEATUREBUCKET *h,
		  unsigned char* xhashes,
		  long hs, long packstart, long packlen);
void crm114__pack_winnow_seg (WINNOW_FEATUREBUCKET *h,
			 unsigned char* xhashes,
			 long hs, long packstart, long packlen);


// Read/write text files to/from CRM114_DATABLOCK

// make sure we use the same string in read and write
// changing any of these would be an INCOMPATIBLE change to the text files
// tn -> text name

// CB
#define TN_DATABLOCK_SIZE "datablock size"
#define TN_FLAGS "classifier flags"
#define TN_MARKOV_PHASE_WEIGHTS "Markov phase weights"
#define TN_BE_CROSSLINK_THRESH "bit entropy crosslink threshold"
#define TN_REGEX "token regex"
#define TN_PIPE_LEN "matrix columns"
#define TN_PIPE_PHASES "matrix rows"
#define TN_BLOCKS "blocks"
#define TN_CLASSES "classes"
#define TN_CLASS_SUCCESS "S"
#define TN_CLASS_FAILURE "F"
#define TN_ENTROPY_NODE "Node"

/*
  C99 has printf type "a", which prints a double in hex, and so
  represents it exactly.  Unfortunately, Microsoft doesn't have it.
  So we have to use some decimal representation of floating point
  numbers, which is not necessarily exact.

  At least we pick one that prints all the significant digits of the
  approximation.
*/
#define PRINTF_DOUBLE_PROB_FMT "%25.23f"
#define SCANF_DOUBLE_PROB_FMT "%lg"
#define PRINTF_DOUBLE_FMT "%.20g"
#define SCANF_DOUBLE_FMT "%lg"
#define PRINTF_FLOAT_FMT "%.8g"
#define SCANF_FLOAT_FMT "%g"

// SVM/PCA
#define C0_DOCS_FEATS_FMT "class 0 documents %d features %d"
#define C1_DOCS_FEATS_FMT "class 1 documents %d features %d"
#define TN_HAS_OLDXY "has oldXy"
#define TN_HAS_NEWXY "has newXy"
#define TN_HAS_X "has X"
#define TN_HAS_SOL "has solution"
#define TN_THETA "theta"
#define TN_NUM_EXAMPLES "num_examples"
#define TN_MAX_TRAIN_VAL "max_train_val"
#define TN_MUDOTTHETA "mudottheta"
#define TN_SV "SV"
#define TN_OLDXY "oldXy"
#define TN_NEWXY "newXy"
#define TN_X "X"

// bit entropy formats
#define TN_FIRLATLEN "firlatlen"
#define TN_NODESLEN "nodeslen"
#define TN_TOTALBITS "totalbits"

// multiblock
#define TN_BLOCK "block"
#define TN_END "end"

int crm114__tf_read_text_fp(int *val, const char tstr[], const char fstr[],
			    FILE *fp);
int crm114__svm_learned_write_text_fp(CRM114_DATABLOCK *db, FILE *fp);
int crm114__svm_learned_read_text_fp(CRM114_DATABLOCK **db, FILE *fp);
int crm114__pca_learned_write_text_fp(CRM114_DATABLOCK *db, FILE *fp);
int crm114__pca_learned_read_text_fp(CRM114_DATABLOCK **db, FILE *fp);
int crm114__hyperspace_learned_write_text_fp(const CRM114_DATABLOCK *db,
					     FILE *fp);
int crm114__hyperspace_learned_read_text_fp(CRM114_DATABLOCK **db, FILE *fp);
int crm114__markov_learned_write_text_fp(const CRM114_DATABLOCK *db, FILE *fp);
int crm114__markov_learned_read_text_fp(CRM114_DATABLOCK **db, FILE *fp);
int crm114__bit_entropy_learned_write_text_fp(const CRM114_DATABLOCK *db, 
					      FILE *fp);
int crm114__bit_entropy_learned_read_text_fp(CRM114_DATABLOCK **db, FILE *fp);
int crm114__fscm_learned_write_text_fp(const CRM114_DATABLOCK *db, 
					      FILE *fp);
int crm114__fscm_learned_read_text_fp(CRM114_DATABLOCK **db, FILE *fp);


//   The following mumbo-jumbo needed for BSD to compile cleanly, because
//    BSD's logl function is not defined in all builds!  What a crock!
#ifdef NO_LOGL
#warning Redefinining crm114__logl as log because logl is missing
#define crm114__logl(x) log(x)
#else
#define crm114__logl(x) logl(x)
#endif

#ifdef NO_SQRTF
#warning Redefining sqrtf as sqrt because sqrtf is missing
#define sqrtf(x) sqrt((x))
#endif
//     End BSD crapola.


#endif	// !__CRM114_INTERNAL_H__
