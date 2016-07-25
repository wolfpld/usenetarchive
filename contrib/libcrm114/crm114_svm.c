//	crm114_svm.c - Support Vector Machine

////////////////////////////////////////////////////////////////////////
//    This code is originally copyright and owned by William
//    S. Yerazunis as file crm_neural_net.c.  In return for addition of
//    significant derivative work, Jennifer Barry is hereby granted a full
//    unlimited license to use this code, includng license to relicense under
//    other licenses.
////////////////////////////////////////////////////////////////////////
//
// Copyright 2009-2010 William S. Yerazunis.
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

#include "crm114_svm.h"


// SVM file magic string and its length
static const char svm_first_bits[] = {"CRM114 SVM FILE FOLLOWS:"};
#define SVM_FIRST_BITS svm_first_bits
#define SVM_FIRST_NBIT (sizeof(svm_first_bits) - 1)

#define SVM_DEFAULT_MAP_SIZE 1000  //size the VECTOR MAP starts at

//#define SVM_USE_MMAP             //define if you want file I/O to be done
                                   //using crm_mmap and crm_munmap rather than
                                   //fread and fwrite
                                   //this makes classify modify learned data
                                   //(sets was_mapped and pointers to data)
                                   //and so makes classify not reentrant and
                                   //unable to use learned data in ROM

extern int CRM114__SVM_DEBUG_MODE;         //defined in crm114_svm_lib_fncts.h

/*
  Describe the content of the beginning of an SVM "file" (formerly
  stored in disk files, now stored in block 0 of CRM114_DATABLOCKs).
  I didn't name this SVM_BLOCK_HDR because we already have too many
  "blocks", including svm_block, which is quite different.

  This codes the beginning of the file format described by Jenny's
  long comment below.  I defined this structure March 2010, to replace
  half a dozen macros and about a dozen copies of code that implicitly
  declared the members of this structure.

  For a binary file to be portable, these things must be the same on
  all systems: byte ordering, alignment, size, and signedness, of
  every data type in the file.  (We probably don't have to worry about
  data representation; we can probably assume twos-complement signed
  integers and IEEE floating point.)  Using straight C types, as
  CRM114 does, doesn't work at all.  For C types, all those traits
  vary across systems.  (CRM114 has never attempted to make its files
  portable.)

  Looking only at two closely related processors, x86 32-bit and
  64-bit, and using the same compiler (GCC 4.3.2), the sizes and/or
  alignments of these C types differ: long, long long, double, long
  double, size_t, ptrdiff_t, and all pointers.

  SVM files contain size_t (in SVM_FILE_HDR, below), and doubles and
  pointers (in various structures in the matrix/vector library that
  are written into the file).  SVM files can't be made portable just
  by tweaking a couple integer data types, not even just across those
  two related x86 processors.

                                                       --KH
*/

typedef struct
{
  char firstbits[SVM_FIRST_NBIT]; // ID string, not NUL-terminated
  size_t size;			// size of data in file, <= file size
  size_t old_offset;		// offset to matrix of old vectors
  size_t new_offset;		// offset to appended vectors (not read)
  int n_new;			// number of examples read but not learned
  int n_old;			// # ex learned that are not support vectors
  int has_solution;		// T/F: file contains a solution
  int n0;			// # examples (vectors, documents) in class 0
  int n1;			// # examples in class 1
  int n0f;			// total features in class 0
  int n1f;			// total features in class 1
  int map_size;			// current length of vector map
  int vmap[];			// vector map, variable len, after this struct
} SVM_FILE_HDR;


//static function declarations
static Vector *convert_document(int crm114_class,
				const struct crm114_feature_row features[],
				long n_features);

//depending on whether SVM_USE_MMAP is set these use mmap or fread
static int map_svm_db(svm_block *blck, CRM114_DATABLOCK *db);
static void svm_get_meta_data(CRM114_DATABLOCK *db, svm_block *blck);
static int has_new_vectors(CRM114_DATABLOCK *db);
static Vector *get_theta_from_svm_db(CRM114_DATABLOCK *db);

//these are around for the times when we want to read in the file
//without overwriting what we have in memory (ie in a learn during
// a classify)
static int read_svm_db(svm_block *blck, CRM114_DATABLOCK *db);
static int read_svm_db_dsp(svm_block *blck, struct data_state *dsp);

//this set of write functions can expand the CRM114_DATABLOCK
static size_t theta_room(Vector *theta);
static size_t block_size(svm_block *blck);
#ifdef SVM_USE_MMAP
static CRM114_DATABLOCK *replace_db(CRM114_DATABLOCK *db, size_t new0);
#endif
static CRM114_DATABLOCK *maybe_expand_db(CRM114_DATABLOCK *db, size_t needed,
					 size_t expansion_size);
static size_t write_svm_db(svm_block *blck, CRM114_DATABLOCK **db);
static size_t write_svm_db_dsp(svm_block *blck, struct data_state *dsp);
static size_t svm_write_theta_dsp(Vector *theta, struct data_state *dsp);

//this can expand the CRM114_DATABLOCK
static size_t append_vector_to_svm_db(Vector *v, CRM114_DATABLOCK **db);

//this writes everything back to disk using fwrite or unmap as
//appropriate.  if the file was read, it always uses fwrite.  if
//the file was mapped in, it tries to alter that memory to have the
//correct new values in it and, if it can't, fwrites it.
static size_t svm_save_changes(svm_block *blck, CRM114_DATABLOCK **db);

static void svm_block_init(svm_block *blck);
static void svm_block_free_data(svm_block blck);
static void svm_learn_new_examples(svm_block *blck, int microgroom);

//set the debug value
//NOTE THAT: CRM114__SVM_DEBUG_MODE, the variable used in the other svm files
//is min(0, svm_trace-1).
//see crm114_matrix_util.h for details, but a general scheme is
//0 will print out nothing
//1 will print out enough to update you on the progress
//2 is the first setting that prints anything from functions not in this file
//3-6 will print out enough info to tell you where the solver is getting
// stuck (not that that should happen!)
//7 can be used for big runs but only if you know what you're looking for
//8-9 should only be used on small runs because they print out big
// matrices
static int svm_trace = 0;

//This is a "smart mode" where the SVM trains on examples in the way it
//thinks is best for it to learn.
//Mainly:
// It waits until it has SVM_BASE_EXAMPLES before doing a learn
// regardless of whether the user has actually put on an append.
// After that it does the incremental algorithm on up to SVM_INCR_FRAC
// appended examples.
// If more than SVM_INCR_FRAC get appended, it does a from start learn.
static int svm_smart_mode = 0;

/**********************CONVERTING FEATURES TO A VECTOR*************************/

/*******************************************************************
 *Helper function to convert features to a vector.
 *
 *INPUT:
 * CRM114 class: 0 or 1
 * features: array of feature-row pairs
 * n_features: how many there are
 *
 *OUTPUT:
 * A vector of the features as a MATR_COMPACT, SPARSE_ARRAY.  This
 * feature vector multiplies the features by their label and adds in a
 * constant term - both things specific to the SVM.  In other words,
 * if input class is 1, the vector will multiply every feature by -1
 * (since class 1 indicates a feature with a -1 label).  In addition,
 * if SVM_ADD_CONSTANT is set, EVERY vector returned from this
 * function will have a +/-1 (according to its label) in the first
 * column.  This is to introduce a "constant" value into the SVM
 * classification, as discussed in the comment to
 * svm_solve_no_init_sol in crm114_svm_lib_fncts.c.
 *
 *WARNINGS:
 *1) You need to free the returned vector (using crm114__vector_free)
 *   once you are done with it.
 *2) The returned vector is NOT just a vector of the features.  We do
 *   SVM-specific manipulations to it, specifically, multiplying the
 *   features by their label and adding a column if SVM_ADD_CONSTANT
 *   is set.
 *******************************************************************/
static Vector *convert_document(int crm114_class,
				const struct crm114_feature_row features[],
				long n_features) {
  long i;
  int svm_class;		// aka label
  Vector *v;
  VectorIterator vit;

  if (crm114_class != 0) {
    //this is a negative example
    svm_class = -1;
  } else {
    svm_class = 1;
  }

  if (!n_features) {
    //blank document
    if (SVM_ADD_CONSTANT) {
      v = crm114__vector_make_size(1, SPARSE_ARRAY, MATR_COMPACT, 1);
      vectorit_set_at_beg(&vit, v);
      crm114__vectorit_insert(&vit, 0, svm_class, v);
    } else {
      v = crm114__vector_make_size(1, SPARSE_ARRAY, MATR_COMPACT, 0);
    }
    return v;
  }

  v = crm114__vector_make_size(features[n_features-1].feature+1, SPARSE_ARRAY,
		       MATR_COMPACT, n_features);

  vectorit_set_at_beg(&vit, v);

  //the SVM solver does not incorporate a constant offset
  //putting this in all of our feature vectors gives that constant offset
  if (SVM_ADD_CONSTANT) {
    crm114__vectorit_insert(&vit, 0, svm_class, v);
  }
  //put the features into the vector
  // We require the caller to have sorted them for us, and to have
  // uniqued them if CRM114_UNIQUE is set.
  for (i = 0; i < n_features; i++) {
    if (features[i].feature != 0) {
      crm114__vectorit_find(&vit, features[i].feature, v);
      //count occurrences of this feature, positive or negative as needed
      if (vectorit_curr_col(vit, v) == features[i].feature)
	crm114__vectorit_insert(&vit, features[i].feature,
			vectorit_curr_val(vit, v) + svm_class, v);
      else
	crm114__vectorit_insert(&vit, features[i].feature, svm_class, v);
    }
  }

  //make v only take up the amount of memory it should
  if (v && v->type == SPARSE_ARRAY) {
    crm114__expanding_array_trim(v->data.sparray);
  }
  return v;
}


/*
  Below is Jennifer Barry's original comment about the SVM file and
  functions to read/write it, which applied to the SVM classifier she
  wrote in the summer of 2009 as part of the program and scripting
  language crm114.  Her SVM consolidated all its data in a single
  file.  (An earlier version by Bill Yerazunis used three files: class
  0, class 1, solution.)

  The SVM you're reading is Jennifer's modified to be part of the
  crm114 library, and to use block 0 of a CRM114_DATABLOCK instead of
  a file, with the file functions modified accordingly.  Functions
  that read/wrote the file conventionally, using the C library
  functions fread() and fwrite(), now call analogous functions written
  for this conversion -- crm114__dbread(), crm114__dbwrite() -- that
  "read" and "write" CRM114_DATABLOCK block 0.

                                             Kurt Hackenberg Feb. 2010
*/

/*
  Setting SVM_USE_MMAP makes classify modify the learned data.
  Mapping in a vector sets was_mapped and a pointer to the data in the
  structure Vector residing in the "file"; mapping in a matrix does
  the analogous thing to the structure Matrix in the "file".

  That makes classify not reentrant and unable to use learned data in
  read-only memory.

  So don't do that.

                                                            --KH
*/

/**************************SVM FILE FUNCTIONS*********************************/

/******************************************************************************
 *
 *There are two sets of functions here.  One set is used when SVM_USE_MMAP is
 *defined and, whenever possible, uses crm_mmap and crm_munmap to do file I/O.
 *The other, used when SVM_USE_MMAP is not defined, uses exclusively fread and
 *fwrite to do file I/O.  When caching is enabled, having SVM_USE_MMAP
 *will SIGNIFICANTLY speed up file I/O.  In addition, using mmap allows shared
 *file I/O.  We there recommend you use SVM_USE_MMAP if possible.
 *
 *Note that SVM_USE_MMAP may call fread and fwrite.  It uses fwrite when it is
 *necessary to grow the file.  The file approximately doubles in size each
 *time fwrite is called, so calls to fwrite should, hopefully, amortize out
 *over the run.  It uses fread when it needs to do a learn in classify since
 *classify shouldn't make changes to the file.  This can be avoided by calling
 *learn without ERASE or APPEND before doing any classifies.
 *
 *Without SVM_USE_MMAP enabled, mmap is NEVER called.
 *
 *The SVM file is a binary file formatted as follows:
 *
 *SVM_FIRST_NBIT bytes: A string or whatever you want defined in
 * SVM_FIRST_BITS.  This isn't a checksum since we don't want to have to read
 * in the whole file every time in order to verify it - it's simply a stored
 * value (or even a string) that all SVM stat files have as the first few
 * bytes to identify them.  While there is as much error checking as I can do
 * in this code, non-SVM binary files can create seg faults by mapping two
 * vector headers into the same space so that changing one changes part of
 * another.  There is almost nothing I can do about that, so, to eliminate
 * that problem as far as I can, we have a little "magic string" in front.
 *
 *N_OFFSETS_IN_SVM_FILE size_t's:
 *
 * size_t size: The offset until the end of the actual data stored in the file.
 *  We leave a large hole at the end of the file so we can append to it without
 *  having to uncache it from memory.  This is the offset to the beginning of
 *  the hole.  When reading the file in, we do not need to read past this
 *  offset since the rest is garbage.  This changes each time we append a
 *  vector.
 *
 * size_t OLD_OFFSET: The offset in bytes from the BEGINNING of the file to the
 *  matrix of old vectors if it exists.  This stays constant through maps and
 *  unmaps, but may change at fwrite.
 *
 * size_t NEW_OFFSET: The offset in bytes from the BEGINNING of the file to the
 *  begining of any vectors that have been appended since the last full map
 *  or read.  This is "size" at the time of the last read or map.
 *
 *N_CONSTANTS_NOT_IN_BLOCK ints:
 *
 * int n_new: number of examples that we have read in but HAVEN'T learned on.
 *  Clearly this number cannot include vectors that have been appended since
 *  the last read or map of the file.  If n_new > 0, we certainly have new
 *  vectors to learn on, but n_new = 0 DOES NOT indicate no new vectors to
 *  learn on.  It is also necessary to seek to NEW_OFFSET and check if there
 *  are vectors there.
 *
 *N_CONSTANTS_IN_SVM_BLOCK ints:
 *
 * int n_old: number of examples we have learned on that aren't support vectors
 *
 * int has_solution: 1 if there is a solution in the file, 0 else
 *
 * int n0: number of examples in class 0
 *
 * int n1: number of examples in class 1
 *
 * int n0f: total number of features in class 0
 *
 * int n1f: total number of features in class 1
 *
 * int map_size: the current size of the map that maps matrix rows to their
 *  actual location in the file.  This stays larger (usually) than the
 *  number of total vectors in the file because appending wouldn't allow
 *  us to grow at this point it the file.  Therefore, we leave a "hole" so
 *  that we don't always have to unmap the file if new vectors have been
 *  appended.
 *
 *VECTOR MAP:
 *
 * A map_size array of ints.  The ith entry in VECTOR_MAP is the offset from
 * the BEGINNING of the file to the ith vector where vectors are in SV, old,
 * new order.
 *
 *DECISION BOUNDARY:
 *
 * Decision vector: the decision vector written as a vector
 *
 * int fill: the amount of filler we leave to allow the decision boundary to
 *  to grow without having to grow the file.
 *
 * void fill: a "hole" allowing the decision vector to grow in size in new
 *  learns.
 *
 *RESTART CONSTANTS:
 *
 * int num_examples: the total number of examples we've learned on (since the
 *  beginning or the last FROMSTART)
 *
 * int max_train_val: sum_c alpha_c <= max_train_val (constant used in
 *  restarting - see crm114_svm_lib_fncts.c for details)
 *
 *SV MATRIX:
 *
 * The support vector matrix header.  When the file is written using fwrite,
 * the support vectors are written after the header.  However, since SVs can
 * change on subsequent learns, the vectors written after SV matrix (if any)
 * aren't guaranteed to be SVs any more.  The VECTOR MAP must be used to
 * reconstruct the SV MATRIX.
 *
 *OLDXY MATRIX (at OLD_OFFSET):
 *
 * The oldXy matrix header.  The oldXy matrix consists of examples we have
 * learned on, but that aren't SVs.  When the file is written using fwrite,
 * all rows of oldXy are written after oldXy.  However since these rows can
 * change on subsequent learns, the vectors written after oldXy (if any)
 * aren't guaranteed to actually be old, non-SV examples.  The VECTOR MAP
 * must be used to reconstruct the OLDXY MATRIX.
 *
 *NEW VECTORS YET TO BE LEARNED ON (at NEW_OFFSET or stored in VECTOR_MAP):
 *
 * Each new vector is formatted as a vector (ie we don't keep the matrix header
 * - this makes appending easy).  Some of them may not be in the VECTOR MAP if
 * they have been appended since the last full read/map in.  These are all
 * listed after NEW_OFFSET.
 *
 *The file is formatted this way to make the following actions quick both using
 * fread/fwrite and mmap/munmap:
 *
 * Finding if the file has a solution: requires a seek to has_solution and a
 *  read of that value.
 *
 * Finding the decision boundary if it exists: requires a sequential fread
 *  of N_CONSTANTS_IN_SVM_BLOCK, a seek to DECISION BOUNDARY, reading in the
 *  vector stored there.
 *
 * Querying if there are unlearned on vectors: requries a seek to the position
 *  of NEW_OFFSET in the file, a sequential read of NEW_OFFSET and of n_new.
 *  If n_new = 0, requires a seek to NEW_OFFSET.
 *
 * Appending a vector:
 *  using fread/fwrite: requires opening the file for appending and writing
 *   out the vector
 *  using mmap/munmap: requires mapping in the file, reading in size and
 *   seeking to point size in the file.  if there is room, writes the vector
 *   there.  else forcibly munmaps the file and opens it for appending.
 *****************************************************************************/

#ifdef SVM_USE_MMAP

//maps the db block 0 into blck.  used before calling learn_new_examples.
//returns T/F success/failure
static int map_svm_db(svm_block *blck, CRM114_DATABLOCK *db) {
  char *addr, *last_addr, *st_addr;
  SVM_FILE_HDR *fh;
  size_t act_size;
  Vector *v;
  int fill, curr_rows = 0, i;

  if (!blck || !db) {
    //this really shouldn't happen
    return 0;
  }

  svm_block_init(blck);

  st_addr = &db->data[db->cb.block[0].start_offset];
  fh = (SVM_FILE_HDR *)st_addr;
  act_size = db->cb.block[0].allocated_size;

  if (act_size < sizeof(SVM_FILE_HDR)
      || strncmp(SVM_FIRST_BITS, fh->firstbits, SVM_FIRST_NBIT)
      || fh->size > act_size
      || fh->size < sizeof(SVM_FILE_HDR) + fh->map_size * sizeof(fh->vmap[0])) {
    //corrupted file
    return 0;
  }

  last_addr = st_addr + fh->size; //last address that contains good data

  blck->n_old = fh->n_old;	//# of learned-on, non-SV vectors
  blck->has_solution = fh->has_solution; //do we have a solution?
  blck->n0 = fh->n0;		//# learned-on examples in class 0
  blck->n1 = fh->n1;		//# learned-on examples in class 1
  blck->n0f = fh->n0f;		//# features in class 0
  blck->n1f = fh->n1f;		//# features in class 1
  blck->map_size = fh->map_size; //space allocated for vmap

  addr = st_addr + sizeof(SVM_FILE_HDR) + fh->map_size * sizeof(fh->vmap[0]);

  if (blck->has_solution) {
    //read in the solution
    blck->sol = (SVM_Solution *)malloc(sizeof(SVM_Solution));
    blck->sol->theta = crm114__vector_map((void **)&addr, last_addr); //decision boundary
    blck->sol->SV = NULL;
    if (addr + sizeof(int) > last_addr) {
      svm_block_free_data(*blck);
      svm_block_init(blck);
      return 0;
    }
    fill = *((int *)addr); //hole to grow decision boundary
    addr += sizeof(int);
    if (!blck->sol->theta || addr + fill + 2*sizeof(int) +
	sizeof(Matrix) > last_addr) {
      svm_block_free_data(*blck);
      svm_block_init(blck);
      return 0;
    }
    addr += fill;
    //restart constants
    blck->sol->num_examples = *((int *)addr);
    addr += sizeof(int);
    blck->sol->max_train_val = *((int *)addr);
    addr += sizeof(int);
    blck->sol->SV = (Matrix *)addr; //SV matrix header
    addr += sizeof(Matrix);
    blck->sol->SV->was_mapped = 1;
    blck->sol->SV->data =
      (Vector **)malloc(sizeof(Vector *)*blck->sol->SV->rows);
    if (!blck->sol->SV->data) {
      svm_block_free_data(*blck);
      svm_block_init(blck);
      return 0;
    }
    //read in the SV vectors using vmap
    for (i = 0; i < blck->sol->SV->rows; i++) {
      addr = st_addr + fh->vmap[i + curr_rows];
      blck->sol->SV->data[i] = crm114__vector_map((void **)&addr, last_addr);
      if (!blck->sol->SV->data[i]) {
	break;
      }
    }
    if (i != blck->sol->SV->rows) {
      blck->sol->SV->rows = i;
      svm_block_free_data(*blck);
      svm_block_init(blck);
      return 0;
    }
    curr_rows += blck->sol->SV->rows;
  }


  //oldXy matrix
  if (blck->n_old) {
    addr = st_addr + fh->old_offset;
    if (addr + sizeof(Matrix) > last_addr) {
      svm_block_free_data(*blck);
      svm_block_init(blck);
      return 0;
    }
    blck->oldXy = (Matrix *)addr; //oldXy header
    addr += sizeof(Matrix);
    blck->oldXy->was_mapped = 1;
    blck->oldXy->data = (Vector **)malloc(sizeof(Vector *)*blck->oldXy->rows);
    if (!blck->oldXy->data) {
      svm_block_free_data(*blck);
      svm_block_init(blck);
      return 0;
    }
    //read in oldXy vectors using vmap
    for (i = 0; i < blck->oldXy->rows; i++) {
      addr = st_addr + fh->vmap[i + curr_rows];
      blck->oldXy->data[i] = crm114__vector_map((void **)&addr, last_addr);
      if (!blck->oldXy->data[i]) {
	break;
      }
    }
    if (i != blck->oldXy->rows) {
      blck->oldXy->rows = i;
      svm_block_free_data(*blck);
      svm_block_init(blck);
      return 0;
    }
    curr_rows += blck->oldXy->rows;
  }

  //newXy vectors
  if (fh->n_new) {
    //read in ones we've already read in and put in vmap
    addr = st_addr + fh->vmap[curr_rows];
    v = crm114__vector_map((void **)&addr, last_addr);
    i = 0;
    if (v) {
      blck->newXy = crm114__matr_make_size(fh->n_new, v->dim, v->type,
					   v->compact, v->size);
      if (!blck->newXy) {
	svm_block_free_data(*blck);
	svm_block_init(blck);
	return 0;
      }
      crm114__matr_shallow_row_copy(blck->newXy, 0, v);
      for (i = 1; i < fh->n_new; i++) {
	addr = st_addr + fh->vmap[i + curr_rows];
	v = crm114__vector_map((void **)&addr, last_addr);
	if (!v) {
	  break;
	}
	crm114__matr_shallow_row_copy(blck->newXy, i, v);
      }
    }
    if (i != fh->n_new) {
      svm_block_free_data(*blck);
      svm_block_init(blck);
      return 0;
    }
  }

  addr = st_addr + fh->new_offset;

  //read in any vectors that have been appended since the last map
  if (addr < last_addr) {
    v = crm114__vector_map((void **)&addr, last_addr);
    if (v) {
      if (!blck->newXy) {
	blck->newXy = crm114__matr_make_size(0, v->dim, v->type, v->compact, v->size);
      }
      if (!blck->newXy) {
	svm_block_free_data(*blck);
	svm_block_init(blck);
	return 0;
      }
      crm114__matr_shallow_row_copy(blck->newXy, blck->newXy->rows, v);
      while (addr < last_addr) {
	v = crm114__vector_map((void **)&addr, last_addr);
	if (v && v->dim) {
	  crm114__matr_shallow_row_copy(blck->newXy, blck->newXy->rows, v);
	} else {
	  if (v && !v->dim) {
	    crm114__vector_free(v);
	  }
	  break;
	}
      }
    }
  }

  return 1;
}

//gets the integers (like n0, n1, etc) stored in the first few bytes
//of the file without reading in the whole file.
//puts them in blck
static void svm_get_meta_data(CRM114_DATABLOCK *db, svm_block *blck) {
  SVM_FILE_HDR *fh;
  size_t act_size;


  if (!db || !blck) {
    return;
  }

  fh = (SVM_FILE_HDR *)&db->data[db->cb.block[0].start_offset];
  act_size = db->cb.block[0].allocated_size;

  if (act_size < sizeof(SVM_FILE_HDR)
      || strncmp(SVM_FIRST_BITS, fh->firstbits, SVM_FIRST_NBIT)
      || fh->size > act_size
      || fh->size < sizeof(SVM_FILE_HDR) + fh->map_size * sizeof(fh->vmap[0])) {
    svm_block_init(blck);
    return;
  }

  blck->n_old = fh->n_old;
  blck->has_solution = fh->has_solution;
  blck->n0 = fh->n0;
  blck->n1 = fh->n1;
  blck->n0f = fh->n0f;
  blck->n1f = fh->n1f;
  blck->map_size = fh->map_size;
}

//returns 1 if the file has vectors that have been appended but not yet
//learned on
//returns 0 else
static int has_new_vectors(CRM114_DATABLOCK *db) {
  Vector *v;
  char *addr, *last_addr, *st_addr;
  SVM_FILE_HDR *fh;
  size_t act_size;

  if (db == NULL) {
    //heck, we don't even have a pointer!
    return 0;
  }

  st_addr = &db->data[db->cb.block[0].start_offset];
  fh = (SVM_FILE_HDR *)st_addr;
  act_size = db->cb.block[0].allocated_size;

  if (act_size < sizeof(SVM_FILE_HDR)
      || strncmp(SVM_FIRST_BITS, fh->firstbits, SVM_FIRST_NBIT)
      || fh->size > act_size
      || fh->size < sizeof(SVM_FILE_HDR) + fh->map_size * sizeof(fh->vmap[0])) {
    return 0;
  }

  if (fh->n_new) {
    //yep, definitely have new vectors
    return 1;
  }

  //do we have vectors we haven't read in before but have appended?
  addr = st_addr + fh->new_offset;
  last_addr = st_addr + fh->size;
  if (addr >= last_addr) {
    return 0;
  }

  //do we really have a vector?  let's try reading one in
  v = crm114__vector_map((void **)&addr, last_addr);
  if (v) {
    crm114__vector_free(v);
    return 1;
  }

  return 0;
}


//returns the decision boundary from an svm file
//we map the decision boundary from the file so you must
// FREE THE DECISION BOUNDARY returned by the function
static Vector *get_theta_from_svm_db(CRM114_DATABLOCK *db) {
  Vector *v;
  char *last_addr, *addr, *st_addr;
  SVM_FILE_HDR *fh;
  size_t act_size;

  if (db == NULL) {
    return NULL;
  }

  st_addr = &db->data[db->cb.block[0].start_offset];
  fh = (SVM_FILE_HDR *)st_addr;
  act_size = db->cb.block[0].allocated_size;

  if (act_size < sizeof(SVM_FILE_HDR)
      || strncmp(SVM_FIRST_BITS, fh->firstbits, SVM_FIRST_NBIT)
      || fh->size > act_size
      || fh->size < sizeof(SVM_FILE_HDR) + fh->map_size * sizeof(fh->vmap[0])) {
    return NULL;
  }

  if ( !fh->has_solution) {
    return NULL;
  }

  addr = st_addr + sizeof(SVM_FILE_HDR) + fh->map_size * sizeof(fh->vmap[0]);
  last_addr = st_addr + fh->size;
  v = crm114__vector_map((void **)&addr, last_addr);

  return v;
}

#else	// !defined(SVM_USE_MMAP)

//reads a binary svm block from a CRM114_DATABLOCK
//returns T/F success/failure
static int map_svm_db(svm_block *blck, CRM114_DATABLOCK *db) {
  return read_svm_db(blck, db);
}

//gets the integers (like n0, n1, etc) stored in the first few bytes
//of the file without reading in the whole file.
//puts them in blck
static void svm_get_meta_data(CRM114_DATABLOCK *db, svm_block *blck) {
  struct data_state ds;
  SVM_FILE_HDR fh;
  size_t things_read;

  if (!db || !blck) {
    return;
  }

  crm114__dbopen(db, &ds);
  things_read = crm114__dbread(&fh, sizeof(fh), 1, &ds);
  crm114__dbclose(&ds);

  if (things_read != 1
      || strncmp(SVM_FIRST_BITS, fh.firstbits, SVM_FIRST_NBIT)
      || fh.size < sizeof(SVM_FILE_HDR) + fh.map_size * sizeof(fh.vmap[0])) {
    svm_block_init(blck);
    return;
  }

  blck->n_old = fh.n_old;
  blck->has_solution = fh.has_solution;
  blck->n0 = fh.n0;
  blck->n1 = fh.n1;
  blck->n0f = fh.n0f;
  blck->n1f = fh.n1f;
  blck->map_size = fh.map_size;

  return;
}

//returns 1 if the file has vectors that have been appended but not yet
//learned on
//returns 0 else
static int has_new_vectors(CRM114_DATABLOCK *db) {
  struct data_state ds;
  SVM_FILE_HDR fh;
  size_t things_read;
  Vector *v;

  if (!db) {
    //heck, we don't even have a file!
    return 0;
  }

  crm114__dbopen(db, &ds);
  things_read = crm114__dbread(&fh, sizeof(fh), 1, &ds);
  if (things_read != 1
      || strncmp(SVM_FIRST_BITS, fh.firstbits, SVM_FIRST_NBIT)
      || fh.size < sizeof(SVM_FILE_HDR) + fh.map_size * sizeof(fh.vmap[0])) {
    crm114__dbclose(&ds);
    return 0;
  }

  if (fh.n_new) {
    //we have new vectors
    crm114__dbclose(&ds);
    return 1;
  }

  crm114__dbseek(&ds, fh.new_offset, SEEK_SET);
  if (crm114__dbeof(&ds) || crm114__dbtell(&ds) >= fh.size) {
    //no new vectors
    crm114__dbclose(&ds);
    return 0;
  }

  //maybe new vectors?  sometimes the end of a file is a funny place
  v = crm114__db_vector_read_bin_dsp(&ds);
  crm114__dbclose(&ds);
  if (v) {
    crm114__vector_free(v);
    if (crm114__dbtell(&ds) <= fh.size) {
      return 1;
    }
  }
  return 0;
}

//returns the decision boundary from an svm file
//don't forget to free the boundary when you are done with it!
static Vector *get_theta_from_svm_db(CRM114_DATABLOCK *db) {
  struct data_state ds;
  SVM_FILE_HDR fh;
  Vector *v;

  if (!db) {
    return NULL;
  }

  crm114__dbopen(db, &ds);
  if (crm114__dbread(&fh, sizeof(fh), 1, &ds) != 1
      || strncmp(SVM_FIRST_BITS, fh.firstbits, SVM_FIRST_NBIT)
      || fh.size < sizeof(SVM_FILE_HDR) + fh.map_size * sizeof(fh.vmap[0])
      || !fh.has_solution) {
    crm114__dbclose(&ds);
    return NULL;
  }

  crm114__dbseek(&ds, sizeof(fh) + fh.map_size * sizeof(fh.vmap[0]), SEEK_SET);

  if (crm114__dbeof(&ds) || crm114__dbtell(&ds) >= fh.size) {
    crm114__dbclose(&ds);
    return NULL;
  }

  v = crm114__db_vector_read_bin_dsp(&ds);

  if (crm114__dbeof(&ds) || crm114__dbtell(&ds) >= fh.size) {
    crm114__vector_free(v);
    crm114__dbclose(&ds);
    return NULL;
  }

  crm114__dbclose(&ds);
  return v;
}

#endif

//functions used to read in the file.
//these are used both under map and read since we read in a file using fread
//always when we need to do a learn in classify.


//reads a binary svm block from a file
//returns 0 on failure
static int read_svm_db(svm_block *blck, CRM114_DATABLOCK *db) {
  struct data_state ds;
  int ret;

  crm114__dbopen(db, &ds);
  ret = read_svm_db_dsp(blck, &ds);
  crm114__dbclose(&ds);

  return ret;
}

static int read_svm_db_dsp(svm_block *blck, struct data_state *dsp) {
  size_t things_read;
  SVM_FILE_HDR fh;
  Vector *v;
  int *vmap, i, curr_rows = 0, fill;

  if (!blck || !dsp) {
    //this really shouldn't happen
    return 0;
  }

  svm_block_free_data(*blck);
  svm_block_init(blck);

  things_read = crm114__dbread(&fh, sizeof(fh), 1, dsp);
  if (things_read != 1
      || strncmp(SVM_FIRST_BITS, fh.firstbits, SVM_FIRST_NBIT)
      || fh.size < sizeof(SVM_FILE_HDR) + fh.map_size * sizeof(fh.vmap[0])) {
    return 0;
  }

  blck->n_old = fh.n_old;
  blck->has_solution = fh.has_solution;
  blck->n0 = fh.n0;
  blck->n1 = fh.n1;
  blck->n0f = fh.n0f;
  blck->n1f = fh.n1f;
  blck->map_size = fh.map_size;

  vmap = (int *)malloc(sizeof(int)*blck->map_size);
  things_read = crm114__dbread(vmap, sizeof(*vmap), blck->map_size, dsp);

  //read in solution
  if (blck->has_solution) {
    blck->sol = (SVM_Solution *)malloc(sizeof(SVM_Solution));
    blck->sol->theta = crm114__db_vector_read_bin_dsp(dsp);
    blck->sol->SV = NULL;
    things_read = crm114__dbread(&fill, sizeof(int), 1, dsp);
    crm114__dbseek(dsp, fill, SEEK_CUR);
    if (!blck->sol->theta || !things_read || crm114__dbeof(dsp) ||
	crm114__dbtell(dsp) > fh.size) {
      //die!
      svm_block_free_data(*blck);
      svm_block_init(blck);
      free(vmap);
      return 0;
    }
    things_read = crm114__dbread(&(blck->sol->num_examples), sizeof(int), 1,
				 dsp);
    things_read += crm114__dbread(&(blck->sol->max_train_val), sizeof(int), 1,
				  dsp);
    if (things_read < 2) {
      //die!
      svm_block_free_data(*blck);
      svm_block_init(blck);
      free(vmap);
      return 0;
    }
    blck->sol->SV = (Matrix *)malloc(sizeof(Matrix));
    things_read = crm114__dbread(blck->sol->SV, sizeof(Matrix), 1, dsp);
    blck->sol->SV->was_mapped = 0;
    if (!things_read) {
      //die!
      svm_block_free_data(*blck);
      svm_block_init(blck);
      free(vmap);
      return 0;
    }
    blck->sol->SV->data =
      (Vector **)malloc(sizeof(Vector *)*blck->sol->SV->rows);
    for (i = 0; i < blck->sol->SV->rows; i++) {
     crm114__dbseek(dsp, vmap[i + curr_rows], SEEK_SET);
      blck->sol->SV->data[i] = crm114__db_vector_read_bin_dsp(dsp);
      if (!blck->sol->SV->data[i]) {
	//oh boy, bad file
	break;
      }
    }
    if (i != blck->sol->SV->rows) {
      blck->sol->SV->rows = i;
      svm_block_free_data(*blck);
      svm_block_init(blck);
      free(vmap);
      return 0;
    }
    curr_rows += blck->sol->SV->rows;
  }

  //read in oldXy
  if (blck->n_old) {
    crm114__dbseek(dsp, fh.old_offset, SEEK_SET);
    blck->oldXy = (Matrix *)malloc(sizeof(Matrix));
    things_read = crm114__dbread(blck->oldXy, sizeof(Matrix), 1, dsp);
    blck->oldXy->was_mapped = 0;
    blck->oldXy->data =
      (Vector **)malloc(sizeof(Vector *)*blck->oldXy->rows);
    for (i = 0; i < blck->oldXy->rows; i++) {
      crm114__dbseek(dsp, vmap[i + curr_rows], SEEK_SET);
      blck->oldXy->data[i] = crm114__db_vector_read_bin_dsp(dsp);
      if (!blck->oldXy->data[i]) {
	//oh boy, bad file
	break;
      }
    }
    if (i != blck->oldXy->rows) {
      blck->oldXy->rows = i;
      svm_block_free_data(*blck);
      svm_block_init(blck);
      free(vmap);
      return 0;
    }
    curr_rows += blck->oldXy->rows;
  }

  //read in parts of newXy we've seen before
  if (fh.n_new) {
    crm114__dbseek(dsp, vmap[curr_rows], SEEK_SET);
    v = crm114__db_vector_read_bin_dsp(dsp);
    i = 0;
    if (v) {
      blck->newXy = crm114__matr_make_size(fh.n_new, v->dim, v->type, v->compact,
				   v->size);
      if (!blck->newXy) {
	svm_block_free_data(*blck);
	svm_block_init(blck);
	free(vmap);
	return 0;
      }
      crm114__matr_shallow_row_copy(blck->newXy, 0, v);
      for (i = 1; i < fh.n_new; i++) {
	crm114__dbseek(dsp, vmap[curr_rows+i], SEEK_SET);
	v = crm114__db_vector_read_bin_dsp(dsp);
	if (!v) {
	  break;
	}
	crm114__matr_shallow_row_copy(blck->newXy, i, v);
      }
    }
    if (i != fh.n_new) {
      svm_block_free_data(*blck);
      svm_block_init(blck);
      free(vmap);
      return 0;
    }
  }

  //read in new vectors
  crm114__dbseek(dsp, fh.new_offset, SEEK_SET);
  if (!crm114__dbeof(dsp) && crm114__dbtell(dsp) < fh.size) {
    v = crm114__db_vector_read_bin_dsp(dsp);
    if (v && v->dim) {
      if (!(blck->newXy)) {
	blck->newXy = crm114__matr_make_size(0, v->dim, v->type, v->compact, v->size);
      }
      if (!blck->newXy) {
	svm_block_free_data(*blck);
	svm_block_init(blck);
	free(vmap);
	return 0;
      }
      crm114__matr_shallow_row_copy(blck->newXy, blck->newXy->rows, v);
      while (!crm114__dbeof(dsp) && crm114__dbtell(dsp) < fh.size) {
	v = crm114__db_vector_read_bin_dsp(dsp);
	if (v && v->dim) {
	  crm114__matr_shallow_row_copy(blck->newXy, blck->newXy->rows, v);
	} else {
	  if (v && !v->dim) {
	    crm114__vector_free(v);
	  }
	  break;
	}
      }
    } else if (v) {
      crm114__vector_free(v);
    }
  }
  free(vmap);
  return 1;
}

//fwrite functions.  used by both read and mmap modes since under mmap it
//is sometimes necessary to grow the file

// helper for block_size()
// size of solution vector plus room to grow
static size_t theta_room(Vector *theta) {
  size_t theta_size;
  int dec_size;

  theta_size = (theta) ? crm114__vector_size(theta) : 0;

  for (dec_size = MAX(1, MATR_DEFAULT_VECTOR_SIZE * sizeof(double));
       dec_size < theta_size;
       dec_size *= 2)
    ;

  return sizeof(dec_size) + dec_size;
}

// Given a set of data in its internal form (a svm_block), figure
// out how much space would be needed to store it in a "file": a
// CRM114_DATABLOCK.
//
// This does not calculate minimum size.  It calculates the size that
// write_svm_db_dsp() would write, which includes some room to grow.
static size_t block_size(svm_block *blck) {
  int nv = 0;
  int map_size;
  size_t size;
  int i;

  if (blck->sol && blck->sol->SV) {
    nv += blck->sol->SV->rows;
  }
  if (blck->oldXy) {
    nv += blck->oldXy->rows;
  }
  if (blck->newXy) {
    nv += blck->newXy->rows;
  }
  for (map_size = blck->map_size; map_size < nv; map_size *= 2)
    ;

  size = sizeof(SVM_FILE_HDR) + map_size * sizeof(int);
  if (blck->sol)
    size += theta_room(blck->sol->theta)
      + sizeof(int)		// sol->num_examples
      + sizeof(int)		// sol->max_train_val
      + crm114__matr_size(blck->sol->SV); // support vectors
  else
    // leave room for a solution (but not much)
    size += theta_room(NULL)	// no theta
      + sizeof(int)		// sol->num_examples
      + sizeof(int)		// sol->max_train_val
      + sizeof(Matrix);		// header for support vectors
  size += (blck->oldXy) ? crm114__matr_size(blck->oldXy) : sizeof(Matrix);
  // new vectors aren't in a matrix
  if (blck->newXy && blck->newXy->data)
    for (i = 0; i < blck->newXy->rows; i++)
      if (blck->newXy->data[i])
	size += crm114__vector_size(blck->newXy->data[i]);

  return size;
}

/*
  There are logically two copies of the learned data and solution: one
  in our CRM114_DATABLOCK's block 0 (formerly a disk file), and one in
  general memory, a bunch of vectors hanging off a svm_block.

  If we're not doing SVM_USE_MMAP, actual storage corresponds: a copy
  in the CRM114_DATABLOCK, and a copy hanging off the svm_block.  Data
  is read from the "file" into the svm_block, and written in the
  opposite direction.  Reading a vector from the "file" to memory
  malloc()'s memory and copies the data into it.  See
  crm114__vector_read_bin() and crm114__db_vector_read_bin().

  If we are doing SVM_USE_MMAP, there's only one copy.  Originally in
  this case, the disk file was mapped into memory rather than read
  conventionally, and the vectors in the file were "mapped" to the
  svm_block, so that block ended up pointing into the memory-mapped
  file instead of pointing to a lot of malloc()ed chunks.  See
  crm114__vector_map().  write_svm_file(), which wrote the whole set
  of data reachable from svm_block to the file, could not write to the
  existing file, because that would overwrite the data that svm_block
  points to, the data being copied to the file.  Bad, bad.  So
  write_svm_file() wrote to a new, temporary file, then deleted the
  old one and renamed the new one to the old one.  So after that, the
  svm_block was invalid -- pointed to something that no longer existed
  -- and could never be used again; it had to be thrown away
  immediately.

  Now instead of a file we have the CRM114_DATABLOCK's block 0, but
  everything else is the same.  There's still only one copy of the
  data when SVM_USE_MMAP; we still use crm114__vector_map().  So in
  that case we do the equivalent of writing to the new temporary file:
  we allocate a new CRM114_DATABLOCK and write to it, and free the old
  one -- after freeing the svm_block stuff that points into it.

  When not SVM_USE_MMAP, we expand the existing CRM114_DATABLOCK if
  necessary, with realloc(), and then write to it.  No problem,
  svm_block doesn't point into CRM114_DATABLOCK.

  The function write_svm_db(), below, is the former write_svm_file(),
  hacked up to write to a CRM114_DATABLOCK instead of to a file;
  replace_db() and maybe_expand_db() are its helpers.
  maybe_expand_db() is also used by append_vector_to_svm_db().

                                                      --KH
*/

#ifdef SVM_USE_MMAP

static CRM114_DATABLOCK *replace_db(CRM114_DATABLOCK *db, size_t new0) {
  size_t newtot;		// size of new CRM114_DATABLOCK
  CRM114_DATABLOCK *newdb;

  newtot = sizeof(db->cb) + new0;
  if ((newdb = malloc(newtot)) != NULL) {
    // don't bother to copy block 0, it'll be overwritten
    memcpy(&newdb->cb, &db->cb, sizeof(newdb->cb));
    newdb->cb.datablock_size = newtot;
    newdb->cb.block[0].allocated_size = new0;
    free(db);
  }

  return newdb;
}

#endif

static CRM114_DATABLOCK *maybe_expand_db(CRM114_DATABLOCK *db, size_t needed,
					 size_t expansion_size) {
  size_t newtot;
  CRM114_DATABLOCK *newdb;

  if (needed > db->cb.block[0].allocated_size) {
    newtot = sizeof(db->cb) + expansion_size;
    if ((newdb = realloc(db, newtot)) != NULL) {
      newdb->cb.datablock_size = newtot;
      newdb->cb.block[0].allocated_size = expansion_size;
    }
  } else {
    newdb = db;
  }

  return newdb;
}

//writes an svm block to a CRM114_DATABLOCK in binary format
//first reallocates the block, maybe bigger, if necessary
//returns the number of bytes written and pointer to new block
static size_t write_svm_db(svm_block *blck, CRM114_DATABLOCK **db) {
  size_t size_written;
  struct data_state ds;
  size_t needed;		// space needed
  size_t cur0;			// size of current block 0
  size_t expansion_size; // size of new block 0, if we expand or replace it
  CRM114_DATABLOCK *newdb;

  size_written = 0;

  needed = block_size(blck);
  cur0 = (*db)->cb.block[0].allocated_size;
#if 0
  // always more than currently need, never less than previous
  expansion_size = MAX(cur0, needed * 3 / 2);
#elif 1
  // always more than currently need, maybe less than previous
  expansion_size = needed * 3 / 2;
#else
  // exact fit every time
  expansion_size = needed;
#endif
#ifdef SVM_USE_MMAP
  newdb = replace_db(*db, expansion_size);
#else
  newdb = maybe_expand_db(*db, needed, expansion_size);
#endif
  if (newdb != NULL) {
    crm114__dbopen(newdb, &ds);
    size_written = write_svm_db_dsp(blck, &ds);
    crm114__dbclose(&ds);
    *db = newdb;
  }

  return size_written;
}

//writes an svm block to a CRM114_DATABLOCK in binary format
//returns the number of bytes written
//frees blck
static size_t write_svm_db_dsp(svm_block *blck, struct data_state *dsp) {
  SVM_FILE_HDR fh;
  size_t size;
  int i, curr_rows = 0, nv = 0, n_new;
  int *vmap;
  Matrix *M = crm114__matr_make(0, 0, SPARSE_ARRAY, MATR_COMPACT);

  if (!blck) {
    return 0;
  }

  if (!dsp) {
    return 0;
  }

  if (blck->sol) {
    blck->has_solution = 1;
  } else {
    blck->has_solution = 0;
  }

  if (blck->sol && blck->sol->SV) {
    nv += blck->sol->SV->rows;
  }

  if (blck->oldXy) {
    nv += blck->oldXy->rows;
  }

  if (blck->newXy) {
    nv += blck->newXy->rows;
    n_new = blck->newXy->rows;
  } else {
    n_new = 0;
  }

  while (nv > blck->map_size) {
    //grow the map if we need to
    if (!(blck->map_size)) {
      blck->map_size = 1;
    }
    blck->map_size *= 2;
  }

  vmap = (int *)malloc(sizeof(int)*blck->map_size);

  memcpy(fh.firstbits, SVM_FIRST_BITS, SVM_FIRST_NBIT);
  fh.size = 0;
  fh.old_offset = 0;
  fh.new_offset = 0;
  fh.n_new = n_new;
  fh.n_old = blck->n_old;
  fh.has_solution = blck->has_solution;
  fh.n0 = blck->n0;
  fh.n1 = blck->n1;
  fh.n0f = blck->n0f;
  fh.n1f = blck->n1f;
  fh.map_size = blck->map_size;

  size = sizeof(fh) * crm114__dbwrite(&fh, sizeof(fh), 1, dsp);

  //vector map
  size += sizeof(int)*crm114__dbwrite(vmap, sizeof(int), blck->map_size, dsp);

  if (blck->sol) {
    //write theta
    size += svm_write_theta_dsp(blck->sol->theta, dsp);
    //write the constants
    size += sizeof(int)*crm114__dbwrite(&(blck->sol->num_examples), sizeof(int), 1, dsp);
    size += sizeof(int)*crm114__dbwrite(&(blck->sol->max_train_val), sizeof(int), 1, dsp);
    //write out the matrix
    size += sizeof(Matrix)*crm114__dbwrite(blck->sol->SV, sizeof(Matrix), 1, dsp);
    for (i = 0; i < blck->sol->SV->rows; i++) {
      vmap[i + curr_rows] = size;
      size += crm114__db_vector_write_bin_dsp(blck->sol->SV->data[i], dsp);
    }
    curr_rows += blck->sol->SV->rows;
  } else {
    //leave room for the solution
    size += svm_write_theta_dsp(NULL, dsp);
    size += sizeof(int)*crm114__dbwrite(&curr_rows, sizeof(int), 1, dsp);
    i = SVM_MAX_X_VAL;
    size += sizeof(int)*crm114__dbwrite(&i, sizeof(int), 1, dsp);
    size += sizeof(Matrix)*crm114__dbwrite(M, sizeof(Matrix), 1, dsp);
  }

  //this is where the oldXy matrix is stored
  fh.old_offset = size;
  crm114__dbseek(dsp, size, SEEK_SET);

  if (blck->oldXy) {
    size += sizeof(Matrix)*crm114__dbwrite(blck->oldXy, sizeof(Matrix), 1, dsp);
    for (i = 0; i < blck->oldXy->rows; i++) {
      vmap[i+curr_rows] = size;
      size += crm114__db_vector_write_bin_dsp(blck->oldXy->data[i], dsp);
    }
    curr_rows += blck->oldXy->rows;
  } else {
    size += sizeof(Matrix)*crm114__dbwrite(M, sizeof(Matrix), 1, dsp);
  }

  if (blck->newXy && blck->newXy->data) {
    for (i = 0; i < blck->newXy->rows; i++) {
      if (blck->newXy->data[i]) {
	vmap[i+curr_rows] = size;
	size += crm114__db_vector_write_bin_dsp(blck->newXy->data[i], dsp);
      }
    }
    curr_rows += blck->newXy->rows;
  }


  //this tells you where the data in the file ends
  fh.size = size;
  //this tells you the offset to appended vectors
  //so you can check if there *are* new vectors quickly
  fh.new_offset = size;

  // overwrite the file header now that we've filled in the three offsets
  crm114__dbseek(dsp, 0, SEEK_SET);
  (void)crm114__dbwrite(&fh, sizeof(fh), 1, dsp);

  //now we actually have vmap
  //so write it out
  (void)crm114__dbwrite(vmap, sizeof(int), curr_rows, dsp);
  free(vmap);

  crm114__matr_free(M);
  svm_block_free_data(*blck);
  svm_block_init(blck);

  return size;
}

//writes theta to a file, leaving it room to grow
static size_t svm_write_theta_dsp(Vector *theta, struct data_state *dsp) {
  int dec_size = MATR_DEFAULT_VECTOR_SIZE*sizeof(double);
  size_t size = 0, theta_written, theta_size;

  if (!dsp) {
    if (svm_trace) {
      fprintf(stderr, "svm_write_theta_dsp: null data_state pointer.\n");
    }
    return 0;
  }

  if (theta) {
    theta_size = crm114__vector_size(theta);
    while (theta_size >= dec_size) {
      if (!(dec_size)) {
	dec_size = 1;
      }
      dec_size *= 2;
    }

    theta_written = crm114__db_vector_write_bin_dsp(theta, dsp);
  } else {
    theta_written = 0;
  }

  size += theta_written;
  dec_size -= theta_written;
  if (dec_size < 0) {
    dec_size = 0;
  }
  size += sizeof(int)*crm114__dbwrite(&dec_size, sizeof(int), 1, dsp);
  size += crm114__dbwrite_zeroes(1, dec_size, dsp);

  return size;
}

//appends a vector to the svm DATABLOCK to be learned on later
//frees the vector
static size_t append_vector_to_svm_db(Vector *v, CRM114_DATABLOCK **db) {
  size_t vsize;
  char *st_addr;
  SVM_FILE_HDR *fh;
  size_t act_size;
  size_t cur0, expansion_size;
  CRM114_DATABLOCK *newdb;

  st_addr = &(*db)->data[(*db)->cb.block[0].start_offset];
  fh = (SVM_FILE_HDR *)st_addr;
  act_size = (*db)->cb.block[0].allocated_size;

  if (act_size < sizeof(SVM_FILE_HDR)
      || strncmp(SVM_FIRST_BITS, fh->firstbits, SVM_FIRST_NBIT)
      || fh->size > act_size
      || fh->size < sizeof(SVM_FILE_HDR) + fh->map_size * sizeof(fh->vmap[0])) {
    crm114__vector_free(v);
    return 0;
  }
  vsize = crm114__vector_size(v);
  cur0 = (*db)->cb.block[0].allocated_size;
  expansion_size = MAX((size_t)((double)cur0 * 1.1), cur0 + vsize);
  //make sure the block is big enough
  if ((newdb = maybe_expand_db(*db, fh->size + vsize, expansion_size))
       != NULL) {
    //db may have moved, so set up address and header again
    st_addr = &newdb->data[newdb->cb.block[0].start_offset];
    fh = (SVM_FILE_HDR *)st_addr;
    //we have room to write the vector
    //so add it
    (void)crm114__vector_memmove((Vector *)(st_addr + fh->size), v);
    fh->size += vsize;	//update space used
    *db = newdb;
    crm114__vector_free(v);
    return vsize;
  } else {		// realloc() failed
    crm114__vector_free(v);
    return 0;
  }
}


//this function writes the changes that have been made to blck
//to disk
//if addr is NULL, it will fwrite blck to filename
//if blck was mapped in, it will attempt to write things back into
//memory and
//if this isn't possible it will force a fwrite the file
//this frees all data associated with blck
static size_t svm_save_changes(svm_block *blck, CRM114_DATABLOCK **db) {
#ifndef SVM_USE_MMAP
  return write_svm_db(blck, db);
#else

  size_t theta_room, theta_req;
  char *st_addr, *curr, *prev, *last_addr;
  SVM_FILE_HDR *fh;
  size_t act_size;
  svm_block old_block;
  int nv = 0, i, curr_rows = 0;

  st_addr = &(*db)->data[(*db)->cb.block[0].start_offset];
  fh = (SVM_FILE_HDR *)st_addr;
  act_size = (*db)->cb.block[0].allocated_size;

  if (act_size < sizeof(SVM_FILE_HDR)
      || strncmp(SVM_FIRST_BITS, fh->firstbits, SVM_FIRST_NBIT)
      || fh->size > act_size
      || fh->size < sizeof(SVM_FILE_HDR) + fh->map_size * sizeof(fh->vmap[0])) {
    return 0;
  }

  if (fh->size + sizeof(double)*MATR_DEFAULT_VECTOR_SIZE >= act_size) {
    //we have no more room to append vectors to this file
    //so write it out now
    //otherwise size won't change
    if (svm_trace) {
      fprintf(stderr, "Writing file to leave a hole at the end.\n");
    }
    return write_svm_db(blck, db);
  }

  last_addr = st_addr + fh->size;

  //if we are going to unmap the file, old_offset won't change
  //since oldXy will be in the same place
  //new_offset, however, will go away because we now have locations
  //for all of the "new vectors".  that we need to do a learn is
  //marked with a non_zero n_new
  fh->new_offset = fh->size;

  //make all of the constants correct
  if (blck->sol) {
    blck->has_solution = 1;
  } else {
    blck->has_solution = 0;
  }

  if (blck->sol && blck->sol->SV) {
    nv += blck->sol->SV->rows;
  }

  if (blck->oldXy) {
    nv += blck->oldXy->rows;
  }

  if (blck->newXy) {
    nv += blck->newXy->rows;
  }

  while (nv > blck->map_size) {
    if (!(blck->map_size)) {
      blck->map_size = 1;
    }
    blck->map_size *= 2;
  }


  if (blck->newXy) {
    fh->n_new = blck->newXy->rows;
  } else {
    fh->n_new = 0;
  }

  old_block.n_old = fh->n_old;
  old_block.has_solution = fh->has_solution;
  old_block.n0 = fh->n0;
  old_block.n1 = fh->n1;
  old_block.n0f = fh->n0f;
  old_block.n1f = fh->n1f;
  old_block.map_size = fh->map_size;

  fh->n_old = blck->n_old;
  fh->has_solution = blck->has_solution;
  fh->n0 = blck->n0;
  fh->n1 = blck->n1;
  fh->n0f = blck->n0f;
  fh->n1f = blck->n1f;
  fh->map_size = blck->map_size;

  if (blck->map_size > old_block.map_size) {
    //we don't have enough room to do a vector map
    //we need to write out the file
    if (svm_trace) {
      fprintf(stderr, "Writing svm  file to grow map size from %d to %d.\n",
	      old_block.map_size, blck->map_size);
    }
    return write_svm_db(blck, db);
  }

  //do we have room to write theta?
  curr = st_addr + sizeof(*fh) + fh->map_size * sizeof(fh->vmap[0]);

  //keep where theta starts
  prev = curr;

  //this is how much room for theta
  if (old_block.has_solution) {
    theta_room = crm114__vector_size((Vector *)curr);
  } else {
    theta_room = 0;
  }
  curr += theta_room;
  theta_room += *((int *)curr);
  curr = prev;

  //how much room will theta actually take?
  if (blck->has_solution && blck->sol && blck->sol->theta) {
    theta_req = crm114__vector_size(blck->sol->theta);
  } else {
    theta_req = 0;
  }

  if (curr + theta_room > last_addr || theta_room < theta_req) {
    //we don't have enough room in the file to write
    //the decision boundary
    //so we need to use fwrite
    if (svm_trace) {
      fprintf
	(stderr,
	 "Writing file to grow decision boundary size from %lu to %lu.\n",
	 theta_room, theta_req);
    }
    return write_svm_db(blck, db);
  }

  //we have enough room to unmap the solution to this file
  //let's do it!

  //write the new solution boundary
  if (blck->has_solution && blck->sol) {
    if (blck->sol->theta) {
      //copy over the decision boundary
      //it is possible that curr and blck->sol->theta
      //overlap if we didn't actually do a learn
      //so use memmove NOT memcpy
      prev = crm114__vector_memmove((Vector *)curr, blck->sol->theta);
    }
    //leave a marker to let us know how much filler space we have
    *((int *)prev) = theta_room-theta_req;
    //keep the filler!
    curr += theta_room + sizeof(int);
    //write in the solution constants
    if (blck->has_solution && blck->sol) {
      *((int *)curr) = blck->sol->num_examples;
    }
    curr += sizeof(int);
    if (blck->has_solution && blck->sol) {
      *((int *)curr) = blck->sol->max_train_val;
    }
    curr += sizeof(int);

    if (blck->sol->SV) {
      //copy the matrix header
      *((Matrix *)curr) = *(blck->sol->SV);
      //now use the map (remember back where we stored it in vmap?)
      //to record which of the vectors (already somewhere in this chunk
      //of memory) belong to this matrix
      for (i = 0; i < blck->sol->SV->rows; i++) {
	//vmap stores offsets from the beginning of the file
	if (((char *)blck->sol->SV->data[i]) < st_addr ||
	    ((char *)blck->sol->SV->data[i]) > last_addr) {
	  //oh oh, something is very wrong
	  //give up and write the file
	  if (svm_trace) {
	    fprintf(stderr, "save_changes: somehow a vector is outside the mapped memory.\n");
	  }
	  return write_svm_db(blck, db);
	}
	fh->vmap[i + curr_rows] = ((char *)blck->sol->SV->data[i]) - st_addr;
      }
      curr_rows += blck->sol->SV->rows;
    }
  } else {	// copy out null solution
    Matrix *M = crm114__matr_make(0, 0, SPARSE_ARRAY, MATR_COMPACT);

    //leave a marker to let us know how much filler space we have
    *((int *)prev) = theta_room-theta_req;
    //keep the filler!
    curr += theta_room + sizeof(int);
    //write in the solution constants
    *((int *)curr) = 0;		// num_examples
    curr += sizeof(int);
    *((int *)curr) = SVM_MAX_X_VAL; // max_train_val
    curr += sizeof(int);
    *((Matrix *)curr) = *M;
    curr += sizeof(Matrix);
    crm114__matr_free(M);
  }

  if (blck->n_old && blck->oldXy && blck->oldXy->data) {
    curr = st_addr + fh->old_offset; //note that this shouldn't change!
    *((Matrix *)curr) = *(blck->oldXy);
    for (i = 0; i < blck->oldXy->rows; i++) {
      if (((char *)blck->oldXy->data[i]) < st_addr ||
	  ((char *)blck->oldXy->data[i]) > last_addr) {
	//whoops
	if (svm_trace) {
	  fprintf(stderr, "save_changes: somehow a vector is outside the mapped memory.\n");
	}
	return write_svm_db(blck, db);
      }
      fh->vmap[i + curr_rows] = ((char *)blck->oldXy->data[i]) - st_addr;
    }
    curr_rows += blck->oldXy->rows;
  }

  if (blck->newXy) {
    //newXy isn't saved as a matrix
    //since new vectors come and go all the time
    for (i = 0; i < blck->newXy->rows; i++) {
      if (((char *)blck->newXy->data[i]) < st_addr ||
	  ((char *)blck->newXy->data[i]) > last_addr) {
	if (svm_trace) {
	  fprintf(stderr, "save_changes: somehow a vector is outside the mapped memory.\n");
	}
	return write_svm_db(blck, db);
      }
      fh->vmap[i + curr_rows] = ((char *)blck->newXy->data[i]) - st_addr;
    }
  }

  //whew!  we made it
  svm_block_free_data(*blck);
  svm_block_init(blck);
  return fh->size;

#endif
}


// read/write learned data ("file", block 0) from/to text file

/*
Write SVM learned data (CRM114_DATABLOCK block 0) to the kind of
text file that's portable storage of the binary datablock.

Don't write theta filler or filler size, vmap, vmap_size, or any of
the offsets.  All that is constructed in the read routine.

Each matrix is identified, and its vectors follow its header directly,
in order, so the text file needs no analog of vmap.

Returns T/F success/failure.
*/

int crm114__svm_learned_write_text_fp(CRM114_DATABLOCK *db, FILE *fp)
{
  int ret = FALSE;
  svm_block blck;

  svm_block_init(&blck);
  if (read_svm_db(&blck, db))
    {
      fprintf(fp, C0_DOCS_FEATS_FMT "\n", blck.n0, blck.n0f);
      fprintf(fp, C1_DOCS_FEATS_FMT "\n", blck.n1, blck.n1f);
      fprintf(fp, TN_HAS_OLDXY " %d\n", blck.oldXy != NULL);
      fprintf(fp, TN_HAS_NEWXY " %d\n", blck.newXy != NULL);
      fprintf(fp, TN_HAS_SOL " %d\n", blck.has_solution);
      if (blck.oldXy != NULL)
	crm114__matr_write_text_fp(TN_OLDXY, blck.oldXy, fp);
      if (blck.newXy != NULL)
	crm114__matr_write_text_fp(TN_NEWXY, blck.newXy, fp);
      if (blck.has_solution)
	{
	  fprintf(fp, "%s %d %s %d\n",
		  TN_NUM_EXAMPLES, blck.sol->num_examples,
		  TN_MAX_TRAIN_VAL, blck.sol->max_train_val);
	  crm114__matr_write_text_fp(TN_SV, blck.sol->SV, fp);
	  crm114__vector_write_text_fp(TN_THETA, blck.sol->theta, fp);
	}

      svm_block_free_data(blck);
      svm_block_init(&blck);
      ret = TRUE;		// suuure!
    }

  return ret;
}

// read SVM learned data from text file into CRM114_DATABLOCK block 0
// can realloc the datablock
// returns T/F
int crm114__svm_learned_read_text_fp(CRM114_DATABLOCK **db, FILE *fp)
{
  svm_block blck;
  size_t expected_size;
  int has_oldXy;
  int has_newXy;

  svm_block_init(&blck);
  if (fscanf(fp, " " C0_DOCS_FEATS_FMT, &blck.n0, &blck.n0f) != 2)
    goto err;
  if (fscanf(fp, " " C1_DOCS_FEATS_FMT, &blck.n1, &blck.n1f) != 2)
    goto err;
  if (fscanf(fp, " " TN_HAS_OLDXY " %d", &has_oldXy) != 1)
    goto err;
  if (fscanf(fp, " " TN_HAS_NEWXY " %d", &has_newXy) != 1)
    goto err;
  if (fscanf(fp, " " TN_HAS_SOL " %d", &blck.has_solution) != 1)
    goto err;
  if (has_oldXy)
    if ((blck.oldXy = crm114__matr_read_text_fp(TN_OLDXY, fp)) == NULL)
      goto err;
  if (has_newXy)
    if ((blck.newXy = crm114__matr_read_text_fp(TN_NEWXY, fp)) == NULL)
      goto err;
  if (blck.has_solution)
    {
      if ((blck.sol = malloc(sizeof(SVM_Solution))) == NULL)
	goto err;
      if (fscanf(fp, " " TN_NUM_EXAMPLES " %d " TN_MAX_TRAIN_VAL " %d",
		 &blck.sol->num_examples, &blck.sol->max_train_val) != 2)
	goto err;
      if ((blck.sol->SV = crm114__matr_read_text_fp(TN_SV, fp)) == NULL)
	goto err;
      if ((blck.sol->theta = crm114__vector_read_text_fp(TN_THETA, fp)) == NULL)
	goto err;
    }
  expected_size = block_size(&blck);
  return (write_svm_db(&blck, db) == expected_size);

 err:
  svm_block_free_data(blck);
  svm_block_init(&blck);
  return FALSE;
}

/***************************SVM BLOCK FUNCTIONS*******************************/

//initializes an svm block
static void svm_block_init(svm_block *blck) {
  blck->sol = NULL;
  blck->newXy = NULL;
  blck->oldXy = NULL;
  blck->n_old = 0;
  blck->has_solution = 0;
  blck->n0 = 0;
  blck->n1 = 0;
  blck->n0f = 0;
  blck->n1f = 0;
  blck->map_size = SVM_DEFAULT_MAP_SIZE;
}

//frees all data associated with a block
static void svm_block_free_data(svm_block blck) {

  if (blck.sol) {
    crm114__svm_free_solution(blck.sol);
  }

  if (blck.oldXy) {
    crm114__matr_free(blck.oldXy);
  }

  if (blck.newXy) {
    crm114__matr_free(blck.newXy);
  }
}

/***************************CRM114_DATABLOCK INIT*****************************/

// initialize data block 0 to be valid and empty
void crm114__init_block_svm(CRM114_DATABLOCK *db, int c)
{
  svm_block blck;
  struct data_state ds;

  if (db->cb.how_many_blocks == 1 && c == 0) {
    // set up an empty data set in its internal form
    svm_block_init(&blck);
    // write it to the data block, if it fits
    if (block_size(&blck) <= db->cb.block[c].allocated_size) {
      crm114__dbopen(db, &ds);
      write_svm_db_dsp(&blck, &ds);
      crm114__dbclose(&ds);
    }
  }
}

/******************* utility ******************/

// Return the sum of a vector's values, excluding class label in
// column 0, if present.  That is, return the number of features in a
// document, including multiple occurrences.
static double nfeat(Vector *doc) {
  VectorIterator vit;
  double sum;

  for (vectorit_set_at_beg(&vit, doc), sum = 0.0;
       !vectorit_past_end(vit, doc);
       vectorit_next(&vit, doc))
#if SVM_ADD_CONSTANT
    if (vectorit_curr_col(vit, doc) != 0)
#endif
      sum += vectorit_curr_val(vit, doc);

  return sum;
}

/***************************LEARNING FUNCTIONS********************************/

//does the actual work of learning new examples
static void svm_learn_new_examples(svm_block *blck, int microgroom) {
  int i;
  int inc = 0, offset = 0, n_ex = 0, lim;
  double d;
  PreciseSparseElement *thetaval = NULL;
  Vector *row;

  if (!blck->newXy && !blck->sol) {
    //reset the block
    svm_block_free_data(*blck);
    svm_block_init(blck);
    return;
  }

  //update n0, n1, n0f, n1f
  if (blck->newXy) {
    for (i = 0; i < blck->newXy->rows; i++) {
      row = matr_get_row(blck->newXy, i);
      if (!row) {
	//this would be weird
	continue;
      }
      d = nfeat(row);
      if (d >= 0) {
	blck->n0++;
	blck->n0f += (int)d;
      } else {
	blck->n1++;
	blck->n1f += (int)fabs(d);
      }
    }
  }

  //actually learn something!
  if (svm_trace) {
    fprintf(stderr, "Calling SVM solve.\n");
  }
  crm114__svm_solve(&(blck->newXy), &(blck->sol));

  if (!blck->sol || !blck->sol->theta) {
    svm_block_free_data(*blck);
    svm_block_init(blck);
    return;
  }

  if (svm_trace) {
    fprintf(stderr,
	    "Reclassifying all old examples to find extra support vectors.\n");
  }

  if (blck->oldXy) {
    n_ex += blck->oldXy->rows;
    if (microgroom && blck->oldXy->rows >= SVM_GROOM_OLD) {
      thetaval = (PreciseSparseElement *)
	malloc(sizeof(PreciseSparseElement)*blck->oldXy->rows);
    }
    //check the classification of everything in oldXy
    //put anything not classified with high enough margin into sol->SV
    lim = blck->oldXy->rows;
    for (i = 0; i < lim; i++) {
      row = matr_get_row(blck->oldXy, i - offset);
      if (!row) {
	continue;
      }
      d = crm114__dot(blck->sol->theta, row);
      if (d <= 0) {
	inc++;
      }
      if (d <= 1+SV_TOLERANCE) {
	crm114__matr_shallow_row_copy(blck->sol->SV, blck->sol->SV->rows, row);
	crm114__matr_erase_row(blck->oldXy, i - offset);
	offset++;
      } else if (thetaval) {
	thetaval[i-offset].col = i - offset;
	thetaval[i-offset].data = d;
      }
    }

    if (thetaval && blck->oldXy->rows >= SVM_GROOM_OLD) {
      //microgroom
      if (svm_trace) {
	fprintf(stderr, "Microgrooming...\n");
      }
      qsort(thetaval, blck->oldXy->rows, sizeof(PreciseSparseElement),
	    crm114__precise_sparse_element_val_compare);
      //take the top SVM_GROOM_FRAC of this
      qsort(&(thetaval[(int)(blck->oldXy->rows*SVM_GROOM_FRAC)]),
	    blck->oldXy->rows - (int)(blck->oldXy->rows*SVM_GROOM_FRAC),
	    sizeof(PreciseSparseElement), crm114__precise_sparse_element_col_compare);
      lim = blck->oldXy->rows;
      for (i = (int)(blck->oldXy->rows*SVM_GROOM_FRAC); i < lim; i++) {
	crm114__matr_remove_row(blck->oldXy, thetaval[i].col);
      }
    }
    if (thetaval) {
      free(thetaval);
    }
    if (!blck->oldXy->rows) {
      crm114__matr_free(blck->oldXy);
      blck->oldXy = NULL;
    }
  }

  if (svm_trace) {
    fprintf(stderr, "Of %d old training examples, we got %d incorrect.  There are now %d support vectors (we added %d).\n",
	    n_ex, inc, blck->sol->SV->rows, offset);
  }

  //if we have any vectors that weren't support vectors
  //they are now stored in newXy.
  //so copy newXy into oldXy

  if (blck->newXy) {
    crm114__matr_append_matr(&(blck->oldXy), blck->newXy);
    crm114__matr_free(blck->newXy);
    blck->newXy = NULL;
  }

  //update the counts we keep of the number of rows
  //of oldXy (mostly so we know whether it exists)
  if (blck->oldXy) {
    blck->n_old = blck->oldXy->rows;
  } else {
    blck->n_old = 0;
  }

  //we've solved it!  so we have a solution
  blck->has_solution = 1;
}

/*
  Jennifer's comment below.  This function (top-level SVM learn) has
  since been modified for the library: instead of taking text and a
  CRM114 command line, calling the vector tokenizer, and storing the
  learned document in a file, it now takes feature-rows (output of
  vector tokenizer, already sorted and maybe uniqued), a class to
  learn the document into (0 or 1, optionally reversed by
  CRM114_REFUTE), and a malloc()'ed CRM114_DATABLOCK, and stores the
  learned document in the data block.  It will expand (reallocate) the
  data block if necessary.

                                                     --KH Feb. 2010
*/

/******************************************************************************
 *Use an SVM to learn a classification task.
 *This expects two classes: a class with a +1 label and a class with
 *a -1 label.  These are denoted by the presence or absense of the
 *CRM114_REFUTE label (see the FLAGS section of the comment).
 *For an overview of how the algorithm works, look at the comments in
 *crm114_svm_lib_fncts.c.
 *
 *INPUT: This function is for use with CRM 114 so it takes the
 * canonical arguments:
 * csl: The control block.  Never actually used.
 * apb: The argparse block.  This is passed to vector_tokenize_selector
 *  and I use the flags (see the FLAG section).
 * txtptr: A pointer to the text to classify.
 * txtstart: The text to classify starts at txtptr+txtstart
 * txtlen: number of characters to classify
 *
 *OUTPUT: 0 on success
 *
 *FLAGS: The SVM calls crm_vector_tokenize_selector so uses any flags
 * that that function uses.  For learning, it interprets flags as
 * follows:
 *
 * CRM114_REFUTE: If present, this indicates that this text has a -1
 *  label and should be classified as such.  If absent, indicates
 *  that this text has a +1 label.
 *
 * CRM114_UNIQUE: If present, CRM114_UNIQUE indicates that we should ignore
 *  the number of times we see a feature.  With CRM114_UNIQUE, feature
 *  vectors are binary - a 1 in a column indicates that a feature
 *  with that column number was seen once or more.  Without it, features
 *  are integer valued - a number in a column indicates the number of
 *  times that feature was seen in the document.
 *
 * CRM114_MICROGROOM: If there are more than SVM_GROOM_OLD (defined in
 *  (crm114_config.h) examples that we have learned on but are
 *  not support vectors, CRM114_MICROGROOM will remove the SVM_GROOM_FRAC
 *  (defined in crm114_config.h) of them furthest from the decision
 *  boundary.  CRM114_MICROGROOM ONLY runs AFTER an actual learn - ie
 *  we will never microgroom during an APPEND.  In fact, PASSING IN
 *  MICROGROOM WITH APPEND DOES NOTHING.  Also note that the effects
 *  of microgrooming are not obvious until the next time the file is
 *  written using fwrite.  This will actually happen the next time enough
 *  vectors are added
 *
 * CRM114_APPEND: The example will be added to the set of examples but
 *  not yet learned on.  We will learn on this example the next time
 *  a learn without APPEND or ERASE is called or if classify is called.
 *  If you call learn with CRM114_APPEND and actual learn will NEVER happen.
 *  All calls to learn with CRM114_APPEND will execute very quickly.
 *
 * CRM114_FROMSTART: Relearn on every seen (and not microgroomed away) example
 *  instead of using an incremental method.  If CRM114_FROMSTART and
 *  CRM114_APPEND are both flagged, the FROMSTART learn will be done the
 *  next time there is a learn without APPEND or ERASE or a classify.  If
 *  examples are passed in using CRM114_APPEND after CRM114_FROMSTART, we will
 *  also learn those examples whenever we do the FROMSTART learn.
 *
 * CRM114_ERASE: Erases the example from the example set.  If this
 *  example was just appended and never learned on or if it is not
 *  in the support vector set of the last solution, this simply erases
 *  the example from the set of examples.  If the example is a support
 *  vector, we relearn everything from the start including any new examples
 *  that were passed in using CRM114_APPEND and haven't been learned on.  If
 *  CRM114_ERASE and CRM114_APPEND are passed in together and a relearn is required,
 *  the relearn is done the next time learn is called without APPEND or ERASE
 *  or a classify is called.
 *
 * ALL FLAGS NOT LISTED HERE OR USED IN THE VECTOR_TOKENIZER ARE IGNORED.
 *
 *WHEN WE LEARN:
 *
 * The various flags can seem to interact bizarrely to govern whether a
 * learn actually happens, but, in fact, everything follows three basic rules:
 *
 * 1) WE NEVER LEARN ON CRM114_APPEND.
 * 2) IF WE LEARN, WE LEARN ON ALL EXAMPLES PRESENT.
 * 3) WHEN ERASING, WE DO EXACTLY AS MUCH WORK IS REQUIRED TO ERASE THE
 *    EXAMPLE AND NO MORE EXCEPT WHERE THIS CONFLICTS WITH THE FIRST 2 RULES.
 *
 * Therefore, rule 2 says that a FROMSTART, for example, will learn on both
 * old and new examples.  Likewise rule 2 states that an ERASE that requires
 * a relearn, will learn on both old and new examples.  An ERASE that DOESN'T
 * require a relearn, however, is governed by rule 3 and therefore
 * will NOT run a learn on new examples because that is NOT necessary to
 * erase the example.  Rule 1 ensures that passing in CRM114_MICROGROOM with
 * CRM114_APPEND does nothing because we only MICROGROOM after a learn and we
 * NEVER learn on CRM114_APPEND.  Etc.
 *
 *FORCING A LEARN:
 *
 * You can force a learn by passing in a NULL txtptr or a txtlen of 0.
 * This will call the svm learn functions EVEN IF there are no new
 * examples.  If the SVM is incorrectly classifying examples it has
 * already seen, forcing a relearn will fix that problem.
 *****************************************************************************/

CRM114_ERR crm114_learn_features_svm(CRM114_DATABLOCK **db,
				     int class,
				     const struct crm114_feature_row features[],
				     long featurecount) {

  unsigned long long classifier_flags; // local copy because we may modify it
  long i, j;
  svm_block blck;
  Vector *nex, *row;
  int read_file = 0, do_learn = 1, lim = 0;

  if (crm114__user_trace) {
    svm_trace = 1;
  }

  if (crm114__internal_trace) {
    //this is a "mediumly verbose" setting
    svm_trace = SVM_INTERNAL_TRACE_LEVEL + 1;
  }

  CRM114__SVM_DEBUG_MODE = svm_trace - 1;
  if (CRM114__SVM_DEBUG_MODE < 0) {
    CRM114__SVM_DEBUG_MODE = 0;
  }

  if (svm_trace) {
    fprintf(stderr, "Doing an SVM learn.\n");
  }

  if ( !((*db)->cb.how_many_blocks == 1 && (*db)->cb.how_many_classes == 2))
    return CRM114_BADARG;
  if ( !(class == 0 || class == 1))
    return CRM114_BADARG;

  classifier_flags = (*db)->cb.classifier_flags;

  // Refute means learn as not member of class specified.  In SVM,
  // everything is in one class or the other, so if it's not in this
  // one, it must be in the other.
  if (classifier_flags & CRM114_REFUTE)
    class ^= 1;			// 1 -> 0, 0 -> 1

  //set things to NULL that should be null
  svm_block_init(&blck);


  if (featurecount > 0) {
    //get the new example
    // We require our caller to have sorted and uniqued the features:
    // sort always, unique if CRM114_UNIQUE.
    nex = convert_document(class, features, featurecount);

    if (classifier_flags & CRM114_ERASE) {
      //find this example and remove all instances of it
      //then do a FROMSTART unless we haven't learned on this
      //example yet
      //requires reading in the whole file
      //load our stat file in
      if (map_svm_db(&blck, *db)) {
	read_file = 1;
      }
      do_learn = 0; //we are erasing, not learning
      if (blck.sol && blck.sol->SV) {
	j = 0;
	lim = blck.sol->SV->rows;
	for (i = 0; i < lim; i++) {
	  row = matr_get_row(blck.sol->SV, i-j);
	  if (!row) {
	    continue;
	  }
	  if (crm114__vector_equals(nex, row)) {
	    //support vector
	    //have to start over
	    do_learn = 1;
	    classifier_flags |= CRM114_FROMSTART;
	    crm114__matr_remove_row(blck.sol->SV, i-j);
	    if (crm114__vector_get(nex, 0) < 0) {
	      blck.n1--;
	      blck.n1f -= nfeat(nex);
	    } else {
	      blck.n0--;
	      blck.n0f -= nfeat(nex);
	    }
	    j++;
	  }
	}
      }
      if (blck.oldXy) {
	j = 0;
	lim = blck.oldXy->rows;
	for (i = 0; i < lim; i++) {
	  row = matr_get_row(blck.oldXy, i-j);
	  if (!row) {
	    continue;
	  }
	  if (crm114__vector_equals(nex, row)) {
	    crm114__matr_remove_row(blck.oldXy, i-j);
	    j++;
	    if (crm114__vector_get(nex, 0) < 0) {
	      blck.n1--;
	      blck.n1f -= nex->nz;
	      if (SVM_ADD_CONSTANT) {
		blck.n1f++;
	      }
	    } else {
	      blck.n0--;
	      blck.n0f -= nex->nz;
	      if (SVM_ADD_CONSTANT) {
		blck.n0f++;
	      }
	    }
	  }
	}

      }
      if (blck.newXy) {
	j = 0;
	lim = blck.newXy->rows;
	for (i = 0; i < lim; i++) {
	  row = matr_get_row(blck.newXy, i-j);
	  if (!row) {
	    continue;
	  }
	  if (crm114__vector_equals(nex, row)) {
	    crm114__matr_remove_row(blck.newXy, i-j);
	    j++;
	  }
	}
      }
      crm114__vector_free(nex);
    } else {
      //add the vector to the new matrix
      append_vector_to_svm_db(nex, db);
    }
  }

  if (classifier_flags & CRM114_FROMSTART) {
    do_learn = 1;
    if (!read_file) {
      if (map_svm_db(&blck, *db)) {
	read_file = 1;
      }
    }
    //move oldXy into newXy
    if (blck.oldXy) {
      crm114__matr_append_matr(&(blck.newXy), blck.oldXy);
      crm114__matr_free(blck.oldXy);
      blck.oldXy = NULL;
      blck.n_old = 0;
    }
    //move the support vectors into newXy
    if (blck.sol) {
      crm114__matr_append_matr(&(blck.newXy), blck.sol->SV);
      crm114__svm_free_solution(blck.sol);
      blck.sol = NULL;
    }
    blck.n0 = 0;
    blck.n1 = 0;
    blck.n0f = 0;
    blck.n1f = 0;
  }

  if (!(classifier_flags & CRM114_APPEND) && do_learn) {
    if (!read_file) {
      if (!map_svm_db(&blck, *db)) {
	do_learn = 0;
      } else {
	read_file = 1;
      }
    }
    //do we actually want to do this learn?
    //let's consult smart mode
    if (read_file && svm_smart_mode) {
      //wait until we have a good base of examples to learn
      if (!blck.has_solution && (!blck.newXy ||
				 blck.newXy->rows < SVM_BASE_EXAMPLES)) {
	if (svm_trace) {
	  fprintf(stderr, "Running under smart_mode: postponing learn until we have enough examples.\n");
	}
	do_learn = 0;
      }

      //if we have more than SVM_INCR_FRAC examples we haven't yet
      //learned on, do a fromstart
      if (blck.sol && blck.sol->SV && blck.oldXy && blck.newXy &&
	  blck.newXy->rows >=
	  SVM_INCR_FRAC*(blck.oldXy->rows + blck.sol->SV->rows)) {
	if (svm_trace) {
	  fprintf(stderr, "Running under smart_mode: Doing a fromstart to incorporate new examples.\n");
	}
	crm114__matr_append_matr(&(blck.newXy), blck.oldXy);
	crm114__matr_free(blck.oldXy);
	blck.oldXy = NULL;
	blck.n_old = 0;
      }
    }
    if (do_learn) {
      svm_learn_new_examples(&blck,
			     //CRM114_MICROGROOM might not fit in an int.
			     //Don't look it up and tell me it fits.
			     //It could change.
			     (classifier_flags & CRM114_MICROGROOM) != 0);

      (*db)->cb.class[0].documents = blck.n0;
      (*db)->cb.class[0].features  = blck.n0f;
      (*db)->cb.class[1].documents = blck.n1;
      (*db)->cb.class[1].features  = blck.n1f;
    }
  }

  if (read_file) {
    //we did something to it!
    //save it
    (void)svm_save_changes(&blck, db);
  }

  //free everything
  svm_block_free_data(blck);

  return CRM114_OK;
}

/****************************CLASSIFICATION FUNCTIONS*************************/

// translate the sign of an SVM label to a CRM class
static inline int sign_class(double x) {
  return (x < 0.0);		// always either 0 or 1
}

// Given an unknown document and a solution vector, return the
// unknown's number of hits in each of SVM's two classes.  Maybe.

static void nhit(
		 Vector *vu,	// unknown vector (document being classified)
		 Vector *vs,	// solution vector
		 int *nh0,	// number of hits in class 0
		 int *nh1	// number of hits in class 1
		 ) {
  VectorIterator vitu;
  VectorIterator vits;
  unsigned int cu;
  unsigned int cs;
  double nh[2];

  vectorit_set_at_beg(&vitu, vu);
  vectorit_set_at_beg(&vits, vs);
#if SVM_ADD_CONSTANT
  if (vectorit_curr_col(vitu, vu) == 0)
    vectorit_next(&vitu, vu);
  if (vectorit_curr_col(vits, vs) == 0)
    vectorit_next(&vits, vs);
#endif
  nh[0] = 0.0;
  nh[1] = 0.0;
  while ( !(vectorit_past_end(vitu, vu) || vectorit_past_end(vits, vs))) {
    cu = vectorit_curr_col(vitu, vu);
    cs = vectorit_curr_col(vits, vs);
    if (cu == cs)
      nh[sign_class(vectorit_curr_val(vits, vs))] += vectorit_curr_val(vitu, vu);
    // step whichever is lower, or both if equal
    if (cu <= cs)
      vectorit_next(&vitu, vu);
    if (cu >= cs)
      vectorit_next(&vits, vs);
  }

  *nh0 = (int)nh[0];
  *nh1 = (int)nh[1];
}

/*
  Jennifer's comment below.  This function has since been modified for
  the CRM library.  See comment on the learn function.  Also, this
  function now ignores CRM114_REFUTE.
*/

/******************************************************************************
 *Use an SVM for a classification task.
 *This expects two classes: a class with a +1 label and a class with
 *a -1 label.  The class with the +1 label is class 0 and the class
 *with the -1 label is class 1.  When learning, class 1 is denoted by
 *passing in the CRM114_REFUTE flag.  The classify is considered to FAIL
 *if the example classifies as class 1 (-1 label).  The SVM requires
 *at least one example to do any classification, although really you should
 *give it at least one from each class.  If classify is called without any
 *examples to learn on at all, it will classify the example as class 0, but
 *it will also print out an error.
 *
 *If classify is called and there are new examples that haven't been learned
 *on or a FROMSTART learn that hasn't been done, this function will do that
 *BEFORE classifying.  in other words:
 *
 *CLASSIFY WILL DO A LEARN BEFORE CLASSIFYING IF NECESSARY.  IT WILL NOT STORE
 *THAT LEARN BECAUSE IT HAS NO WRITE PRIVILEGES TO THE FILE.
 *
 *INPUT: This function is for use with CRM 114 so it takes the
 * canonical arguments:
 * csl: The control block.  Used to skip if classify fails.
 * apb: The argparse block.  This is passed to vector_tokenize_selector
 *  and I use the flags (see the FLAG section).
 * txtptr: A pointer to the text to classify.
 * txtstart: The text to classify starts at txtptr+txtstart
 * txtlen: number of characters to classify
 *
 *OUTPUT: return is 0 on success
 * The text output (stored in out_var) is formatted as follows:
 *
 * LINE 1: CLASSIFY succeeds/fails success probability: # pR: #
 *  (note that success probability is high for success and low for failure)
 * LINE 2: Best match to class #0/1 probability: # pR: #
 *  (probability >= 0.5 since this is the best matching class.)
 * LINE 3: Total features in input file: #
 * LINE 4: #0 (label +1): documents: #, features: #, prob: #, pR #
 * LINE 5: #1 (label -1): documents: #, features: #, prob: #, pR #
 *  (prob is high for match class, low else.  pR is positive for match class.)
 *
 * I've picked a random method for calculating probability and pR.  Thinking
 * about it, there may be literature for figuring out the probability at
 * least.  Anyone who wants to do that, be my guest.  For now, I've found
 * a function that stays between 0 and 1 and called it good.  Specifically,
 * if theta is the decision boundary and x is the example to classify:
 *
 *       prob(class = 0) = 0.5 + 0.5*tanh(theta dot x)
 *       pR = sgn(theta dot x)*(pow(11, fabs(theta dot x)) - 1)
 *
 *FLAGS: The SVM calls crm_vector_tokenize_selector so uses any flags
 * that that function uses.  For classifying, it interprets flags as
 * follows:
 *
 * CRM114_REFUTE: Returns the OPPOSITE CLASS.  In other words, if this should
 *  classify as class 1, it now classifies as class 0.  I don't know why
 *  you would want to do this, but you should be aware it happens.
 *
 * CRM114_UNIQUE: If present, CRM114_UNIQUE indicates that we should ignore
 *  the number of times we see a feature.  With CRM114_UNIQUE, feature
 *  vectors are binary - a 1 in a column indicates that a feature
 *  with that column number was seen once or more.  Without it, features
 *  are integer valued - a number in a column indicates the number of
 *  times that feature was seen in the document.  If you used CRM114_UNIQUE
 *  to learn, use CRM114_UNIQUE to classify! (duh)
 *
 * CRM114_MICROGROOM: If classify does a learn, it will MICROGROOM.  See the
 *  comment to learn for how microgroom works.
 *
 * ALL FLAGS NOT LISTED HERE OR USED IN THE VECTOR_TOKENIZER ARE IGNORED.
 * INCLUDING FLAGS USED FOR LEARN!
 *****************************************************************************/

CRM114_ERR crm114_classify_features_svm(CRM114_DATABLOCK *db,
					const struct crm114_feature_row features[],
					long nfr, CRM114_MATCHRESULT *result) {

  Vector *nex, *theta;
  double dottheta = 0;
  int class, sgn;
  svm_block blck;
  int hits[2];

  if (crm114__user_trace) {
    svm_trace = 1;
  }

  if (crm114__internal_trace) {
    //this is a "mediumly verbose" setting
    svm_trace = SVM_INTERNAL_TRACE_LEVEL + 1;
  }

  CRM114__SVM_DEBUG_MODE = svm_trace - 1;

  if (CRM114__SVM_DEBUG_MODE < 0) {
    CRM114__SVM_DEBUG_MODE = 0;
  }

  if (svm_trace) {
    fprintf(stderr, "Doing an SVM classify.\n");
  }

  if ( !(db->cb.how_many_blocks == 1 && db->cb.how_many_classes == 2))
    return CRM114_BADARG;

  svm_block_init(&blck);

  //do we have new vectors to learn on?
  if (has_new_vectors(db)) {
    //we use read so that we don't make changes to the file
    //also doing a learn when you can't benefit from it is stupid
    //so we don't do that in smart mode
    if (!svm_smart_mode && read_svm_db(&blck, db)) {
      svm_learn_new_examples(&blck, 0);
    }
    if (blck.sol) {
      theta = blck.sol->theta;
    } else {
      svm_block_free_data(blck);
      svm_block_init(&blck);
      theta = NULL;
    }
  } else {
    svm_get_meta_data(db, &blck);
    theta = get_theta_from_svm_db(db);
  }

  //get the new example
  // as Jenny said, I don't know why you'd want to refute here...
  // removed that (now ignore CRM114_REFUTE here) because flags now come
  // from CRM114_DATABLOCK, not CRM learn or classify statement, and
  // would have to be explicitly changed before doing a classify
  nex = convert_document(0, features, nfr);

  //classify it
  if (theta) {
    dottheta = crm114__dot(nex, theta);
    nhit(nex, theta, &hits[0], &hits[1]);
    if (blck.sol) {
      svm_block_free_data(blck);
    } else {
      crm114__vector_free(theta);
    }
  } else {
    dottheta = 0;
    hits[0] = 0;
    hits[1] = 0;
  }

  if (svm_trace) {
    fprintf(stderr,
	    "The dot product of the example and decision boundary is %lf\n",
	    dottheta);
  }
  if (dottheta < 0) {
    class = 1;
    sgn = -1;
  } else {
    class = 0;
    sgn = 1;
  }

  if (fabs(dottheta) > 6/log10(11)) {
    dottheta = sgn*6/log10(11);
  }

  //annnnnnd... write it all back out
  crm114__clear_copy_result(result, &db->cb);
  result->unk_features = (int)nfeat(nex);
  result->class[0].hits = hits[0];
  result->class[1].hits = hits[1];
  result->bestmatch_index = class;
  //these are very arbitrary units of measurement
  //i picked tanh because... it's a function with a middle at 0
  //and nice asymptotic properties near 1
  //yay!
  result->class[0].prob = 0.5 + 0.5*tanh(dottheta);
  result->class[0].pR   = sgn*(pow(11, fabs(dottheta))-1);
  result->class[1].prob = 1.0 - result->class[0].prob;
  result->class[1].pR   = -1.0 * result->class[0].pR;
  {
    int nsucc = 0;
    int c;
    double remainder;

    for (c = 0; c < result->how_many_classes; c++)
      if (result->class[c].success)
	nsucc++;
    switch (nsucc)
      {
      case 0:
      case 2:
	result->tsprob = 0.0;
	remainder = 0.0;
	for (c = 0; c < result->how_many_classes; c++)
	  if (result->class[c].success)
	    result->tsprob += result->class[c].prob;
	  else
	    remainder += result->class[c].prob;
	// kluge!
	result->overall_pR = crm114__pR(result->tsprob, remainder);
	break;
      case 1:
	for (c = 0; c < result->how_many_classes; c++)
	  if (result->class[c].success) {
	    result->tsprob = result->class[c].prob;
	    result->overall_pR = result->class[c].pR;
	    break;
	  }
	break;
      }
  }

  crm114__vector_free(nex);

  return CRM114_OK;
}


/****************************SAMPLE MAINS*************************************/

//#define MAKE_PREC_RECALL_GRAPHS
#ifdef MAKE_PREC_RECALL_GRAPHS

//    the command line argv
char **prog_argv;

//    the auxilliary input buffer (for WINDOW input)
char *newinputbuf;

//    the globals used when we need a big buffer  - allocated once, used
//    wherever needed.  These are sized to the same size as the data window.
char *inbuf;
char *outbuf;
char *tempbuf;

//classify spam
//this generates svm_time.txt and svm_err.txt
int main(int argc, char **argv) {

  //CSL_CELL csl;
  ARGPARSE_BLOCK apb;
  char *txtptr = NULL;
  long txtstart, txtlen;
  FILE *fp, *slfp, *hlfp;
  size_t size;
  Matrix *X;
  char *gdir, *glist, *bdir, buf[256];
  svm_block blck;
  int i = 0, start, read_good = 0, ng = 0, nb = 0, label, curr_pt = 0;
  unsigned int features[MAX_SVM_FEATURES];
  Vector *v;
  double d;
  int errpts[20], deth = 0, deta = 0, ah = 0;
  FILE *err_file = fopen("svm_err.txt", "w");

  csl = (CSL_CELL *)malloc(sizeof(CSL_CELL));
  apb.p1start = (char *)malloc(sizeof(char)*MAX_PATTERN);
  strcpy(apb.p1start, "");
  apb.p1len = strlen(apb.p1start);
  apb.a1start = buf;
  apb.a1len = 0;
  apb.p2start = NULL;
  apb.p2len = 0;
  apb.p3start = buf;
  apb.p3len = 0;
  apb.b1start = buf;
  apb.b1len = 0;
  apb.s1start = buf;
  apb.s1len = 0;
  apb.s2start = buf;
  apb.s2len = 0;

  gdir = argv[1];
  bdir = argv[2];

  unlink(apb.p1start);

  data_window_size = DEFAULT_DATA_WINDOW;
  printf("data_window_size = %d\n", data_window_size);
  outbuf = (char *)malloc(sizeof(char)*data_window_size);
  prog_argv = (char **)malloc(sizeof(char *));
  prog_argv[0] = (char *)malloc(sizeof(char)*MAX_PATTERN);
  //list of files in ham folder
  strcpy(buf, gdir);
  start = strlen(buf);
  strcpy(&(buf[start]), "/list.txt\0");
  start = strlen(buf);
  printf("start = %d\n", start);
  hlfp = fopen(buf, "r");

  //list of files in spam folder
  strcpy(buf, bdir);
  start = strlen(buf);
  strcpy(&(buf[start]), "/list.txt\0");
  start = strlen(buf);
  slfp = fopen(buf, "r");

  svm_block_init(&blck);
  i = 0;
  while (fscanf(hlfp, "%s", buf) != EOF) {
    ng++;
  }
  while (fscanf(slfp, "%s", buf) != EOF) {
    nb++;
  }
  printf("ng = %d, nb = %d\n", ng, nb);

  errpts[0] = 125;
  curr_pt = 0;
  while (errpts[curr_pt] < nb + ng) {
    errpts[curr_pt+1] = 2*errpts[curr_pt];
    curr_pt++;
  }
  errpts[curr_pt-1] = nb + ng;
  curr_pt = 0;

  rewind(hlfp);
  rewind(slfp);
  while (!feof(hlfp) || !feof(slfp)) {
    v = NULL;
    if ((read_good && !feof(hlfp)) || feof(slfp)) {
      ah++;
      strcpy(buf, gdir);
      start = strlen(buf);
      strcpy(&buf[start], "/");
      start = strlen(buf);
      if (fscanf(hlfp, "%s", &(buf[start])) == EOF) {
	continue;
      }
      read_good++;
      if (read_good >= ng/nb + 1) {
	read_good = 0;
      }
      apb.sflags = CRM114_UNIQUE;
      label = 1;
    } else if (!feof(slfp)) {
      strcpy(buf, bdir);
      start = strlen(buf);
      strcpy(&buf[start], "/");
      start = strlen(buf);
      if (fscanf(slfp, "%s", &(buf[start])) == EOF) {
	continue;
      }
      start = strlen(buf);
      apb.sflags = CRM114_REFUTE | CRM114_UNIQUE;
      read_good = 1;
      label = -1;
    }
    printf("Reading %s i = %d\n", buf, i);
    fp = fopen(buf, "r");
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);
    txtptr = (char *)realloc(txtptr, size);
    size = fread(txtptr, 1, size, fp);
    fclose(fp);

    //do a classify
    //d = crm_svm_classify(csl, &apb, txtptr, 0, size);

    v = convert_document(txtptr, size, features, &apb);
    if (blck.sol) {
      d = crm114__dot(blck.sol->theta, v);
    } else {
      d = 0;
    }
    printf("d = %f\n", d);
    if (d < 1) {
      //do a learn
      //crm_svm_learn(csl, &apb, txtptr, 0, size);
      if (!blck.newXy) {
      blck.newXy = crm114__matr_make_size(1, v->dim, v->type, v->compact, v->size);
      }
      crm114__matr_shallow_row_copy(blck.newXy, blck.newXy->rows-1, v);
      svm_learn_new_examples(&blck, 0);
    }

    if (d > 0 && label > 0) {
      //a correct ham detection!
      deth++;
      deta++;
    }
    //could be less than or equal if d is actual dot
    //right now it is return from classify
    if (d < 0 && label < 0) {
      //an incorrect ham detection
      deta++;
    }

    i++;
    if (i == errpts[curr_pt]) {
      //record this
      fprintf(err_file, "%d %d %d %d\n", i, deth, deta, ah);
      deth = 0;
      deta = 0;
      ah = 0;
      curr_pt++;
    }


  }
  fclose(hlfp);
  fclose(slfp);
  fclose(err_file);

  free(outbuf);
  free(csl);
  free(apb.p1start);
  free(txtptr);
  svm_block_free_data(blck);

  return 0;
}

#endif

//#define SVM_NON_TEXT
#ifdef SVM_NON_TEXT
//and yet another main to test taking non-text data

int main(int argc, char **argv) {
  Vector *theta;
  int currarg = 1, i;
  char *opt;
  FILE *thout = NULL;
  SVM_Solution *sol = NULL;
  Matrix *Xy;

  if (argc < 2) {
    fprintf(stderr,
	    "Usage: linear_svm [options] example_file [solution_file].\n");
    exit(1);
  }

  opt = argv[currarg];
  DEBUG_MODE = 0;
  while (currarg < argc && opt[0] == '-') {
    switch(opt[1]) {
    case 'v':
      DEBUG_MODE = atoi(&(opt[2]));
      break;
    case 't':
      currarg++;
      thout = fopen(argv[currarg], "w");
      if (!thout) {
	fprintf(stderr, "Bad theta output file name: %s.  Writing to stdout.\n",
		argv[currarg]);
	thout = stdout;
      }
      break;
    case 'p':
      thout = stdout;
      break;
    case 's':
      currarg++;
      sol = read_solution(argv[currarg]);
      break;
    default:
      fprintf(stderr, "Options are:\n");
      fprintf(stderr, "\t-v#: Verbosity level.\n");
      fprintf(stderr, "\t-t filename: Theta ascii output file.\n");
      fprintf(stderr, "\t-p filename: Print theta to screen.\n");
      fprintf(stderr, "\t-s filename: Starting solution file.\n");
      break;
    }
    currarg++;
    opt = argv[currarg];
  }

  printf("DEBUG_MODE = %d\n", DEBUG_MODE);

  if (currarg >= argc) {
    fprintf(stderr, "Error: No input file or no output file.\n");
    fprintf(stderr,
	    "Usage: linear_svm [options] example_file [solution_file].\n");
    if (thout != stdout) {
      fclose(thout);
    }
    exit(1);
  }

  Xy = crm114__matr_read_bin(argv[currarg]);
  currarg++;

  //if (sol) {
  //solve(NULL, &sol);
  //} else {
    solve(&Xy, &sol);
    //}
  crm114__matr_free(Xy);

  theta = sol->theta;

  if (thout == stdout) {
    //otherwise this just gets in the way
    fprintf(thout, "There are %d SVs\n", sol->SV->rows);
    fprintf(thout, "Solution using Cutting Planes is\n");
  }

  if (thout) {
    crm114__vector_write_sp_fp(theta,thout);
  }

  if (currarg < argc) {
    //write out the solution
    write_solution(sol, argv[currarg]);
  }

  free_solution(sol);

  return 0;
}

#endif
