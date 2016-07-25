//	crm114_lib.h
//
// Copyright 2009-2010 William S. Yerazunis.
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
//
// Based on crm114.h

#ifndef	__CRM114_LIB_H__
#define	__CRM114_LIB_H__


// GROT GROT GROT global vars we have to do something about
int crm114__user_trace;
int crm114__internal_trace;


//    Structure of a library callable classifier:
//
//    Only 1 big block (which is in memory) per classification.  More
//    than one class can live in the same memory block / file.
//
//    Because intermediate forms are so different between different
//    classifiers, there is no requirement as to their internal
//    structure.  However, a fairly standard header is given below
//    which should be adhered to if reasonable.  Note that this is
//    NOT the header from crm114_structs.h, which did not have enough info.
//
//    Depending on the classifier, it may or may not be worthwhile to
//    interleave the various classes' information, or to give each
//    class it's own chunk.  That's entirely up the classifier itself.
//
//    Note that this struct is a void * to the callers of libcrm114; all
//    access should be through the get/set routines.  But till then, enjoy.
//
//    Command decision: we're going back to 32-bit tokens.  So there.
//

typedef union
  {
    struct
    {
      int phase_weights[UNIFIED_ITERS_MAX];
    } markov;
    struct
    {
      double crosslink_thresh;
    } bit_entropy;
  }
CLASSIFIER_SPECIFIC_PARAMS;

typedef struct
{
  char system_identifying_text[STATISTICS_FILE_IDENT_STRING_MAX];
  int sysid_text_len;	 // avoid asciz trash!
  char user_identifying_text[STATISTICS_FILE_IDENT_STRING_MAX];
  int userid_text_len;
  unsigned long long classifier_flags;	// uses crm114.h flags, hence 64 bit
  CLASSIFIER_SPECIFIC_PARAMS params;
  char tokenizer_regex[MAX_REGEX];
  int tokenizer_regexlen;
  // pipeline length == matrix columns
  // pipeline phases == pipeline iterations == matrix rows
  int tokenizer_pipeline_len;
  int tokenizer_pipeline_phases;
  // two-dimensional matrix of coefficients
  int tokenizer_pipeline_coeffs[UNIFIED_ITERS_MAX][UNIFIED_WINDOW_MAX];
  //
  //  if boosting, use these params (unless RULAX is on)
  double errfrac;  // what fraction of known errors to train with on next cycle
  double trufrac;  // what ratio of known correct examples to also train in.
  //  the following override errfrac and trufrac when boosting (except RULAX)
  int mincorrect;  // minimum number of known corrects to always train
  int maxcorrect;  // maximum number of known corrects to always train
  int minerrors;   // minimum number of known errors to always train
  int maxerrors;   // maximum number of known errors to always train
  //   termination conditions for boosting
  float desiredacc; // termination accuracy
  float testfrac;   // ... when tested over this fraction of the examples
  //  
  // how many bytes in this DB (init by user, lib updates as db grows)
  size_t datablock_size;	
  //
  // A "block" is a chunk of memory that classifiers use to store
  // learned data, solutions, etc.  A "class" is a logical set of
  // learned documents; a classifier decides which of its known
  // classes an unknown document belongs to.
  //
  // For some classifiers, blocks and classes correspond one to one:
  // the learned data for each class is stored in a block.  The
  // Markovian classifiers, and hyperspace, are like this.  For other
  // classifiers, they do not correspond.  SVM and PCA have two
  // classes and a solution, and all that is stored in a single block;
  // FSCM uses two blocks per class.
  //
  // Here we have two arrays of metadata about blocks and classes.
  // Each element of array "block", below, describes one block of
  // memory used by classifiers.  Each element of array "class",
  // below, describes one logical class, regardless of how that class
  // is physically stored.
  //
  // Each array also has a length: how many elements of the array are
  // in use, consecutive from 0.
  int how_many_blocks;		// array block, below: how many in use
  int how_many_classes;		// array class, below: how many in use
  struct
  {
    size_t start_offset;       // in bytes, so cb.data[start_offset] is right.
    size_t allocated_size;     // bytes allocated, start_offset + alloc = next
    // Two different ways to say how much of the block is in use:
    // low-level, in bytes, and high-level, as a fraction.
    //
    // size_used is the number of bytes in this block in use by the
    // classifier, consecutive from the beginning of the block.
    // clsf_frac_used is what fraction of the classifier's storage the
    // classifier is using.
    //
    // Examples:
    //
    // Markov fills the block with hash table, and sets size_used to
    // the number of buckets created * sizeof(bucket).  Normally
    // size_used == allocated_size (or a fraction of a bucket less).
    // But not all those buckets are in use; clsf_frac_used is buckets
    // in use / total buckets.
    //
    // Bit entropy is similar to Markov: fills block with firlat and
    // nodes, but not all are in use.
    //
    // Hyperspace just puts features and end-of-document marks in a
    // bunch of unsigned ints, consecutive from the beginning of the
    // block.  size_used is how many uints have been written *
    // sizeof(uint); clsf_frac_used is size_used / allocated_size.
    size_t size_used;	       // in bytes
    float clsf_frac_used;
  } block[CRM114_MAX_BLOCKS];
  struct
  {
    // GROT GROT GROT
    // This name is C-style NUL-terminated, but should be counted
    // bytes, as a hack to make large character sets work.  Well,
    // really-really, we should convert the whole library and engine to
    // Unicode...
    char name[CLASSNAME_LENGTH + 1];
    int success;	// T/F: this is a success class (default 1,0,0...)
    int documents;	// # of documents learned into this class
    int features;	// # of features learned into this class
  } class[CRM114_MAX_CLASSES];
} CRM114_CONTROLBLOCK;


//   A libcrm data block - used to set up a new classification.
//
// Note we declare structure member "data", below, as either [] or [0],
// depending on how old your compiler is.  [] is ISO C (C99).  [0] is
// older, a non-standard extension, not allowed by C89 or C99 but
// still common.  GCC accepts both.
//
// Either way, data is effectively after the structure.  That is, if p
// is a pointer to a CRM114_DATABLOCK, then &p->data[0] is the same
// address as p + 1.  This is a common hack to allow name references
// to a data block of unknown or variable size, and that's what we're
// doing.

typedef struct
{
  CRM114_CONTROLBLOCK cb;  // control block is copied to the front of the DB
  char data[];  //  this is just the start of the classifier data area
} CRM114_DATABLOCK;


//   This is how we return errors.  CRM114_OK is zero, but don't
//   write code that depends on it unless you need to assure you'll
//   be doing on-call maintenance in your retirement.

typedef enum
  {
    CRM114_OK,			// success
    CRM114_UNK,			// unknown error
    CRM114_BADARG,		// bad arguments
    CRM114_NOMEM,		// couldn't allocate memory
    CRM114_REGEX_ERR,		// regex lib complained
    CRM114_FULL,		// some buffer not big enough, etc.
    CRM114_CLASS_FULL,		// class data block is full, can't learn more
    CRM114_OPEN_FAILED,         // couldn't open the file
    CRM114_NOT_YET_IMPLEMENTED  // Sorry, didn't write the code for this yet.
  }
  CRM114_ERR;


//   The classmatch results are a struct of type CRM114_MATCHRESULT
//
//    For example, the best match class number is:
//      mymatchresult.bestmatch_index
//
//    and has name:
//      mymatchresult.class[mymatchresult.bestmatch_index].name
//
//    to get to the 3rd classes name:
//      mymatchresult.class[2].name
//
//   Note that the CRM114_MATCHRESULT is passed in, the caller should do
//   this (it's OK to do it as a local variable, just pass the addr).  If
//   you don't, then the match routines will return with CRM114_BADARG
//   and you will get nothing...
//    ???  GROT GROT GROT should we malloc if a NULL comes in?
//    Kurt says no.  Too much complication, and lack of clarity
//    about who owns the structure (caller or library).
typedef struct
{
  unsigned long long classifier_flags;	// uses crm114.h flagss, hence 64-bit
  double tsprob;		// total success probability
  double overall_pR;
  int bestmatch_index;
  int unk_features;
  int how_many_classes;
  // array of class results parallels array of class headers in CB
  struct
  {
    double pR;
    double prob;
    int documents;
    int features;
    int hits;
    int success;
    // GROT GROT GROT
    // should be counted bytes
    char name[CLASSNAME_LENGTH + 1];
    union
    {
      //   Empty union members are for classifiers with no unique return
      //   parameters.  We retain them for future illumination
      //
      struct			// Markov, OSB, OSBF, Winnow
      {
	float chi2;		// if CRM114_CHI2
      } markov;
      struct			// Hyperspace
      {
	float radiance;
      } hyperspace;
      struct			// Bit entropy
      {
	int jumps;
	float entropy;
      } bit_entropy;
      struct			// FSCM
      {
	float compression;
      } fscm;
      struct			// neural
      {
	float in_class;
      } neural;
      //      struct // Support Vector Machines
      //{
      //} svm;
      struct			// bytewise correlate (DEPRECATED)
      {
	int L1; int L2; int L3; int L4;
      } bytewise;
      //struct // Principal Component Analysis
      //{
      //} pca;
    } u;			// short name ("union")
  } class [CRM114_MAX_CLASSES];
} CRM114_MATCHRESULT;


//  hash function for variable tables
unsigned int crm114_strnhash (const char *str, long len);


//    Stuff for the vector tokenizer - used to turn text into hash vectors.
//

struct crm114_feature_row
{
  unsigned int feature;		// token, hashed and coefficiented
  int row; // subscript of row in coeff matrix that generated this feature
};

void crm114_feature_sort_unique
(
 struct crm114_feature_row fr[], // array of features with coeff row #s
 long *nfr,		  // in/out: # features given, # features returned
 int sort,		  // true if sort wanted
 int unique		  // true if unique wanted
 );

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
 );


//  Learn some text into the memory-mapped classifier.

CRM114_ERR crm114_learn_text (CRM114_DATABLOCK **db, int whichclass,
			      const char text[], long textlen);
CRM114_ERR crm114_learn_features (CRM114_DATABLOCK **db,
				  int whichclass,
				  struct crm114_feature_row fr[],
				  long *nfr);

CRM114_ERR crm114_classify_text (CRM114_DATABLOCK *db, const char text[],
				 long textlen, CRM114_MATCHRESULT *result);
CRM114_ERR crm114_classify_features (CRM114_DATABLOCK *db,
				     struct crm114_feature_row fr[],
				     long *nfr,
				     CRM114_MATCHRESULT *result);


// Individual classifiers are part of the API so application can
// generate and supply features itself, and can link in only some
// classifiers.  (We still have to split out classifier dispatch
// functions from crm114_base.c to make it possible not to link in all
// classifiers.)

//   OSB/Markov classifier
CRM114_ERR crm114_learn_features_markov (CRM114_DATABLOCK **db,
					 int icls,
					 const struct crm114_feature_row features[],
					 long featurecount);
CRM114_ERR crm114_classify_features_markov (const CRM114_DATABLOCK *db,
					    const struct crm114_feature_row features[],
					    long featurecount,
					    CRM114_MATCHRESULT *result);

// bit entropy classifier
CRM114_ERR crm114_learn_text_bit_entropy(CRM114_DATABLOCK **db,
					 int whichclass, const char text[],
					 long textlen);
CRM114_ERR crm114_classify_text_bit_entropy(const CRM114_DATABLOCK *db,
					    const char text[], long textlen,
					    CRM114_MATCHRESULT *result);

// hyperspace classifier
CRM114_ERR crm114_learn_features_hyperspace (CRM114_DATABLOCK **db,
					     int whichclass,
					     const struct crm114_feature_row features[],
					     long featurecount);
CRM114_ERR crm114_classify_features_hyperspace(const CRM114_DATABLOCK *db,
					       const struct crm114_feature_row features[],
					       long featurecount,
					       CRM114_MATCHRESULT *result);

// support vector machine
CRM114_ERR crm114_learn_features_svm(CRM114_DATABLOCK **db,
				     int class,
				     const struct crm114_feature_row features[],
				     long featurecount);
CRM114_ERR crm114_classify_features_svm(CRM114_DATABLOCK *db,
					const struct crm114_feature_row features[], long nfr,
					CRM114_MATCHRESULT *result);

// principal component analysis
CRM114_ERR crm114_learn_features_pca(CRM114_DATABLOCK **db,
				     int class,
				     const struct crm114_feature_row features[],
				     long featurecount);
CRM114_ERR crm114_classify_features_pca(CRM114_DATABLOCK *db,
					const struct crm114_feature_row features[],
					long nfr, CRM114_MATCHRESULT *result);

// fast substring compression match
CRM114_ERR crm114_learn_features_fscm (CRM114_DATABLOCK **db,
				       int whichclass,
				       const struct crm114_feature_row features[],
				       long featurecount);
CRM114_ERR crm114_classify_features_fscm(const CRM114_DATABLOCK *db,
					 const struct crm114_feature_row features[],
					 long nfr,
					 CRM114_MATCHRESULT *result);


//  Create a new memory-mapped classifier system according to the
//   parameters in the control block *cb

CRM114_DATABLOCK *crm114_new_db (CRM114_CONTROLBLOCK *p_cb);

void crm114_cb_reset(CRM114_CONTROLBLOCK *p_cb);
void crm114_cb_setblockdefaults(CRM114_CONTROLBLOCK *p_cb);
void crm114_cb_setclassdefaults(CRM114_CONTROLBLOCK *p_cb);
void crm114_cb_setdefaults(CRM114_CONTROLBLOCK *p_cb);
CRM114_CONTROLBLOCK *crm114_new_cb(void);
void crm114_cb_getdimensions(const CRM114_CONTROLBLOCK *p_cb,
			     int *pipe_len, int *pipe_iters);
CRM114_ERR crm114_cb_setflags
    (CRM114_CONTROLBLOCK *p_cb, unsigned long long flags);
CRM114_ERR crm114_cb_setregex(CRM114_CONTROLBLOCK *p_cb, const char regex[],
			      int regex_len);
CRM114_ERR crm114_cb_setpipeline
    ( CRM114_CONTROLBLOCK *p_cb,
      int pipe_len, int pipe_iters,
      const int pipe_coeffs[UNIFIED_ITERS_MAX][UNIFIED_WINDOW_MAX] );


// pretty-print a CRM114_MATCHRESULT on stdout
void crm114_show_result_class(const CRM114_MATCHRESULT *r, int icls);
void crm114_show_result(const char name[], const CRM114_MATCHRESULT *r);


// Hack for Windows applications, and others could use it for
// portability. See explanation with this function's code.

void crm114_free(void *p);

int crm114_cb_write_text_fp(const CRM114_CONTROLBLOCK *cb, FILE *fp);
int crm114_cb_write_text(const CRM114_CONTROLBLOCK *cb, const char filename[]);
int crm114_db_write_text_fp(const CRM114_DATABLOCK *db, FILE *fp);
int crm114_db_write_text(const CRM114_DATABLOCK *db, const char filename[]);
CRM114_CONTROLBLOCK *crm114_cb_read_text_fp(FILE *fp);
CRM114_CONTROLBLOCK *crm114_cb_read_text(const char filename[]);
CRM114_DATABLOCK *crm114_db_read_text_fp(FILE *fp);
CRM114_DATABLOCK *crm114_db_read_text(const char filename[]);


#endif	// !__CRM114_LIB_H__
