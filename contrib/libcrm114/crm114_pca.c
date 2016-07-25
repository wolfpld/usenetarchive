//	crm114_pca.c - Principal Component Analysis

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


#include "crm114_pca.h"


// PCA file magic string and its length
static const char pca_first_bits[] = {"CRM114 PCA FILE FOLLOWS:"};
#define PCA_FIRST_BITS pca_first_bits
#define PCA_FIRST_NBIT (sizeof(pca_first_bits) - 1)

extern int CRM114__PCA_DEBUG_MODE;	//Debug mode -
					//see crm114_pca_lib_fncts.h
					//and crm114_matrix_util.h
					//for details.

//#define PCA_USE_MMAP			//make classify maybe fast and
					//definitely not reentrant

/*
  Describe the content of the beginning of a PCA "file" (formerly
  stored in disk files, now stored in block 0 of CRM114_DATABLOCKs).

  See comment on definition of analogous SVM_FILE_HDR.
*/

typedef struct
{
  char firstbits[PCA_FIRST_NBIT];
  size_t size;
  int has_new;
  int has_solution;
  int n0;
  int n1;
  int n0f;
  int n1f;
} PCA_FILE_HDR;


//static function declarations
static Vector *convert_document(int crm114_class,
				const struct crm114_feature_row features[],
				long n_features);

//depending on whether PCA_USE_MMAP is set these use mmap or fread
static int pca_map_db(pca_block *blck, CRM114_DATABLOCK *db);
static void pca_get_meta_data(CRM114_DATABLOCK *db, pca_block *blck);
static int has_new_vectors(CRM114_DATABLOCK *db);
static PCA_Solution *get_solution_from_pca_db(CRM114_DATABLOCK *db);

//these are around for the times when we want to read in the file
//without overwriting what we have in memory (ie in a learn during
// a classify)
static int pca_read_db(pca_block *blck, CRM114_DATABLOCK *db);
static int pca_read_db_dsp(pca_block *blck, struct data_state *dsp);

//this set of write functions can expand the CRM114_DATABLOCK
static size_t theta_room(pca_block *blck);
static size_t block_size(pca_block *blck);
#ifdef PCA_USE_MMAP
static CRM114_DATABLOCK *replace_db(CRM114_DATABLOCK *db, size_t new0);
#endif
static CRM114_DATABLOCK *maybe_expand_db(CRM114_DATABLOCK *db, size_t needed,
					 size_t expansion_size);
static size_t pca_write_db(pca_block *blck, CRM114_DATABLOCK **db);
static size_t pca_write_db_dsp(pca_block *blck, struct data_state *dsp);
static size_t pca_write_theta_dsp(Vector *theta, struct data_state *dsp);

static size_t append_vector_to_pca_db(Vector *v, CRM114_DATABLOCK **db);

static size_t pca_save_changes(pca_block *blck, CRM114_DATABLOCK **db);

static void pca_block_init(pca_block *blck);
static void pca_block_free_data(pca_block blck);
static void pca_learn_new_examples(pca_block *blck, int microgroom);


static int pca_trace = 0;


/*******************************************************************
 *Helper function to convert text to features.
 *
 *INPUT:
 * CRM114 class: 0 or 1
 * features: array of feature-row pairs
 * n_features: how many there are
 *
 *OUTPUT:
 * A vector of the features as a MATR_COMPACT, SPARSE_ARRAY.  This
 * feature vector adds in the class magnitude if it is non-zero using the
 * CRM114 class (0 or 1) to set the sign.
 *
 *WARNINGS:
 *1) You need to free the returned vector (using crm114__vector_free)
 *   once you are done with it.
 *2) The returned vector is NOT just a vector of the features.  We do
 *   PCA-specific manipulations to it, specifically, adding a column if
 *   PCA_CLASS_MAG is non-zero.
 *******************************************************************/
static Vector *convert_document(int crm114_class,
				const struct crm114_feature_row features[],
				long n_features) {
  long i;
  int pca_class, entry = 1;
  Vector *v;
  VectorIterator vit;

  if (crm114_class != 0) {
    //this is a negative example
    pca_class = -1*PCA_CLASS_MAG; // ???
  } else {
    pca_class = PCA_CLASS_MAG;
  }

  if (!n_features) {
    //blank document
    if (pca_class) {
      v = crm114__vector_make_size(1, SPARSE_ARRAY, MATR_COMPACT, 1);
      vectorit_set_at_beg(&vit, v);
      crm114__vectorit_insert(&vit, 0, pca_class, v);
    } else {
      v = crm114__vector_make_size(1, SPARSE_ARRAY, MATR_COMPACT, 0);
    }
    return v;
  }

  v = crm114__vector_make_size(features[n_features-1].feature+1, SPARSE_ARRAY,
			       MATR_COMPACT, n_features);

  vectorit_set_at_beg(&vit, v);

  if (pca_class) {
    //insert the class mag
    crm114__vectorit_insert(&vit, 0, pca_class, v);
  }

  //put the features into the vector
  // We require the caller to have sorted them for us, and to have
  // uniqued them if CRM114_UNIQUE is set.
  for (i = 0; i < n_features; i++) {
    if (features[i].feature != 0) {
      crm114__vectorit_find(&vit, features[i].feature, v);
      // count occurrences of this feature
      if (vectorit_curr_col(vit, v) == features[i].feature)
	crm114__vectorit_insert(&vit, features[i].feature,
				vectorit_curr_val(vit, v) + entry, v);
      else
	crm114__vectorit_insert(&vit, features[i].feature, entry, v);
    }
  }

  //make v only take up the amount of memory it should
  if (v && v->type == SPARSE_ARRAY) {
    crm114__expanding_array_trim(v->data.sparray);
  }
  return v;
}


/*
  Below is Jennifer Barry's original comment about the PCA file and
  functions to read/write it, which applied to the PCA classifier she
  wrote in the summer of 2009 as part of the program and scripting
  language crm114.  She wrote it by copying and modifying the SVM
  classifier she had just written.  Like her SVM, her PCA kept all its
  data in a single file.

  The PCA you're reading is Jennifer's modified to be part of the
  crm114 library, and to use block 0 of a CRM114_DATABLOCK instead of
  a file, with the file functions modified accordingly.  Functions
  that read/wrote the file conventionally, using the C library
  functions fread() and fwrite(), now call analogous functions written
  for this conversion -- crm114__dbread(), crm114__dbwrite() -- that
  "read" and "write" CRM114_DATABLOCK block 0.

                                             Kurt Hackenberg March 2010
*/

/*
  This PCA was coded to always do what's now conditional on
  PCA_USE_MMAP, which makes classify modify the learned data.  I made
  it conditionally compiled, like SVM.
                                                           --KH
*/

/**************************PCA FILE FUNCTIONS*********************************/

/******************************************************************************
 *
 *The PCA file is a binary file formatted as follows:
 *
 *PCA_FIRST_NBIT bytes: A string or whatever you want defined in
 * PCA_FIRST_BITS.  This isn't a checksum since we don't want to have to read
 * in the whole file every time in order to verify it - it's simply a stored
 * value (or even a string) that all PCA stat files have as the first few
 * bytes to identify them.  While there is as much error checking as I can do
 * in this code, non-PCA binary files can create seg faults by mapping two
 * vector headers into the same space so that changing one changes part of
 * another.  There is almost nothing I can do about that, so, to eliminate
 * that problem as far as I can, we have a little "magic string" in front.
 *
 *N_OFFSETS_IN_PCA_FILE size_t's:
 *
 * size_t size: The offset until the end of the actual data stored in the file.
 *  We leave a large hole at the end of the file so we can append to it without
 *  having to uncache it from memory.  This is the offset to the beginning of
 *  the hole.  When reading the file in, we do not need to read past this
 *  offset since the rest is garbage.  This changes each time we append a
 *  vector.
 *
 *N_CONSTANTS_NOT_IN_BLOCK ints: don't actually have any :)
 *
 *N_CONSTANTS_IN_PCA_BLOCK ints:
 *
 * int has_new: 1 if there are new vectors, 0 else
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
 *
 *PRINCIPLE COMPONENT:
 *
 * theta: the principle component written as a vector
 *
 * int fill: the amount of filler we leave to allow the principle component to
 *  to grow without having to grow the file.
 *
 * void fill: a "hole" allowing the decision vector to grow in size in new
 *  learns.
 *
 * double mudottheta: the decision point
 *
 *EXAMPLE VECTORS:
 *
 * Each new vector is formatted as a vector (ie we don't keep the matrix header
 * - this makes appending easy).
 *
 *The file is formatted this way to make the following actions quick both using
 * fread/fwrite and mmap/munmap:
 *
 * Finding if the file has a solution: requires a seek to has_solution and a
 *  read of that value.
 *
 * Finding the principle if it exists: requires a sequential fread
 *  of N_CONSTANTS_IN_PCA_BLOCK, a seek to DECISION BOUNDARY, reading in the
 *  vector stored there.
 *
 * Querying if there are unlearned on vectors: requries a seek has_new and a
 *  read of that value.
 *
 * Appending a vector: requires mapping in the file, reading in size and
 *   has_new, updating has_new,  and seeking to point size in the file.
 *   if there is room, writes the vector there.  else forcibly munmaps the
 *   file and opens it for appending.  creates a file if there isn't one.
 *****************************************************************************/


#ifdef PCA_USE_MMAP

//maps db block 0 into blck
//returns T/F success failure
static int pca_map_db(pca_block *blck, CRM114_DATABLOCK *db) {
  char *addr, *last_addr, *st_addr;
  PCA_FILE_HDR *fh;
  size_t act_size;
  Vector *v;
  int fill;

  if (!blck || !db) {
    //this really shouldn't happen
    return 0;
  }

  pca_block_init(blck);

  st_addr = &db->data[db->cb.block[0].start_offset];
  fh = (PCA_FILE_HDR *)st_addr;
  act_size = db->cb.block[0].allocated_size;

  if (act_size < sizeof(PCA_FILE_HDR)
      || strncmp(PCA_FIRST_BITS, fh->firstbits, PCA_FIRST_NBIT)
      || fh->size > act_size
      || fh->size < sizeof(PCA_FILE_HDR)) {
    return 0;
  }

  last_addr = st_addr + fh->size; //last address that contains good data

  blck->has_new = fh->has_new;	//do we have unlearned-on examples?
  blck->has_solution = fh->has_solution; //do we have a solution?
  blck->n0 = fh->n0;		//# learned-on examples in class 0
  blck->n1 = fh->n1;		//# learned-on examples in class 1
  blck->n0f = fh->n0f;		//# features in class 0
  blck->n1f = fh->n1f;		//# features in class 1

  addr = st_addr + sizeof(PCA_FILE_HDR);

  if (blck->has_solution) {
    blck->sol = (PCA_Solution *)malloc(sizeof(PCA_Solution));
    if (!blck->sol) {
      pca_block_free_data(*blck);
      pca_block_init(blck);
    }
    //read in the solution
    blck->sol->theta = crm114__vector_map((void **)&addr, last_addr); //decision boundary
    if (addr + sizeof(int) > last_addr) {
      pca_block_free_data(*blck);
      pca_block_init(blck);
      return 0;
    }
    fill = *((int *)addr); //hole to grow pca
    addr += sizeof(int);
    if (!blck->sol->theta || addr + fill + sizeof(double) > last_addr) {
      pca_block_free_data(*blck);
      pca_block_init(blck);
      return 0;
    }
    addr += fill;
    blck->sol->mudottheta = *((double *)addr);
    addr += sizeof(double);
  } else {
    fill = *((int *)addr);
    addr += sizeof(int);
    addr += fill + sizeof(double);
  }

  //example vectors!

  if (addr < last_addr) {
    v = crm114__vector_map((void **)&addr, last_addr);
    if (v) {
      if (!blck->X) {
	blck->X = crm114__matr_make_size(0, v->dim, v->type, v->compact, v->size);
      }
      if (!blck->X) {
	pca_block_free_data(*blck);
	pca_block_init(blck);
	return 0;
      }
      crm114__matr_shallow_row_copy(blck->X, blck->X->rows, v);
      while (addr < last_addr) {
	v = crm114__vector_map((void **)&addr, last_addr);
	if (v && v->dim) {
	  crm114__matr_shallow_row_copy(blck->X, blck->X->rows, v);
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
static void pca_get_meta_data(CRM114_DATABLOCK *db, pca_block *blck) {
  PCA_FILE_HDR *fh;
  size_t act_size;


  if (!db || !blck) {
    return;
  }

  fh = (PCA_FILE_HDR *)&db->data[db->cb.block[0].start_offset];
  act_size = db->cb.block[0].allocated_size;

  if (act_size < sizeof(PCA_FILE_HDR)
      || strncmp(PCA_FIRST_BITS, fh->firstbits, PCA_FIRST_NBIT)
      || fh->size > act_size
      || fh->size < sizeof(PCA_FILE_HDR)) {
    pca_block_init(blck);
    return;
  }

  blck->has_new = fh->has_new;	//Are there un-learned on examples?
  blck->has_solution = fh->has_solution; //1 if there is a solution
  blck->n0 = fh->n0;		//# examples in class 0
  blck->n1 = fh->n1;		//# examples in class 1
  blck->n0f = fh->n0f;          //# features in class 0
  blck->n1f = fh->n1f;          //# features in class 1
}

//returns 1 if the file has vectors that have been appended but not yet
//learned on
//returns 0 else
static int has_new_vectors(CRM114_DATABLOCK *db) {
  PCA_FILE_HDR *fh;
  size_t act_size;

  if (db == NULL) {
    //heck, we don't even have a pointer!
    return 0;
  }

  fh = (PCA_FILE_HDR *)&db->data[db->cb.block[0].start_offset];
  act_size = db->cb.block[0].allocated_size;

  if (act_size < sizeof(PCA_FILE_HDR)
      || strncmp(PCA_FIRST_BITS, fh->firstbits, PCA_FIRST_NBIT)
      || fh->size > act_size
      || fh->size < sizeof(PCA_FILE_HDR)) {
    return 0;
  }

  return fh->has_new;
}


//returns the decision boundary from an pca file
//we map the decision boundary from the file so you must
// FREE THE DECISION BOUNDARY returned by the function
static PCA_Solution *get_solution_from_pca_db(CRM114_DATABLOCK *db) {
  PCA_Solution *sol;
  char *last_addr, *addr, *st_addr;
  PCA_FILE_HDR *fh;
  size_t act_size;
  int fill;

  if (db == NULL) {
    return NULL;
  }

  st_addr = &db->data[db->cb.block[0].start_offset];
  fh = (PCA_FILE_HDR *)st_addr;
  act_size = db->cb.block[0].allocated_size;

  if (act_size < sizeof(PCA_FILE_HDR)
      || strncmp(PCA_FIRST_BITS, fh->firstbits, PCA_FIRST_NBIT)
      || fh->size > act_size
      || fh->size < sizeof(PCA_FILE_HDR)) {
    return NULL;
  }

  if ( !fh->has_solution) {
    return NULL;
  }

  addr = st_addr + sizeof(PCA_FILE_HDR);
  last_addr = st_addr + fh->size;
  sol = (PCA_Solution *)malloc(sizeof(PCA_Solution));
  sol->theta = crm114__vector_map((void **)&addr, last_addr);
  if (addr + sizeof(int) > last_addr) {
    crm114__pca_free_solution(sol);
  }
  fill = *((int *)addr);
  addr += sizeof(int);
  if (addr + fill + sizeof(double) > last_addr) {
    crm114__pca_free_solution(sol);
  }

  addr += fill;
  sol->mudottheta = *((double *)addr);

  return sol;
}


#else


//maps db block 0 into blck
//returns T/F success failure
static int pca_map_db(pca_block *blck, CRM114_DATABLOCK *db) {
  return pca_read_db(blck, db);
}

//gets the integers (like n0, n1, etc) stored in the first few bytes
//of the file without reading in the whole file.
//puts them in blck
static void pca_get_meta_data(CRM114_DATABLOCK *db, pca_block *blck) {
  struct data_state ds;
  PCA_FILE_HDR fh;
  size_t things_read;

  if (!db || !blck) {
    return;
  }

  crm114__dbopen(db, &ds);
  things_read = crm114__dbread(&fh, sizeof(fh), 1, &ds);
  crm114__dbclose(&ds);

  if (things_read != 1
      || strncmp(PCA_FIRST_BITS, fh.firstbits, PCA_FIRST_NBIT)
      || fh.size < sizeof(PCA_FILE_HDR)) {
    pca_block_init(blck);
    return;
  }

  blck->has_new = fh.has_new;
  blck->has_solution = fh.has_solution;
  blck->n0 = fh.n0;
  blck->n1 = fh.n1;
  blck->n0f = fh.n0f;
  blck->n1f = fh.n1f;

  return;
}

//returns 1 if the file has vectors that have been appended but not yet
//learned on
//returns 0 else
static int has_new_vectors(CRM114_DATABLOCK *db) {
  struct data_state ds;
  PCA_FILE_HDR fh;
  size_t things_read;

  if (db == NULL) {
    //heck, we don't even have a pointer!
    return 0;
  }

  crm114__dbopen(db, &ds);
  things_read = crm114__dbread(&fh, sizeof(fh), 1, &ds);
  crm114__dbclose(&ds);

  if (things_read != 1
      || strncmp(PCA_FIRST_BITS, fh.firstbits, PCA_FIRST_NBIT)
      || fh.size < sizeof(PCA_FILE_HDR)) {
    return 0;
  }

  return fh.has_new;
}


//returns the decision boundary from an pca file
//we map the decision boundary from the file so you must
// FREE THE DECISION BOUNDARY returned by the function
static PCA_Solution *get_solution_from_pca_db(CRM114_DATABLOCK *db) {
  struct data_state ds;
  PCA_FILE_HDR fh;
  PCA_Solution *sol;
  int fill;

  if (db == NULL) {
    return NULL;
  }

  crm114__dbopen(db, &ds);
  if (crm114__dbread(&fh, sizeof(fh), 1, &ds) != 1
      || strncmp(PCA_FIRST_BITS, fh.firstbits, PCA_FIRST_NBIT)
      || fh.size < sizeof(PCA_FILE_HDR)
      || !fh.has_solution) {
    crm114__dbclose(&ds);
    return NULL;
  }

  sol = (PCA_Solution *)malloc(sizeof(PCA_Solution));
  crm114__dbseek(&ds, sizeof(PCA_FILE_HDR), SEEK_SET);
  sol->theta = crm114__db_vector_read_bin_dsp(&ds);

  if (crm114__dbread(&fill, sizeof(int), 1, &ds) != 1) {
    crm114__pca_free_solution(sol);
    crm114__dbclose(&ds);
    return NULL;
  }
  crm114__dbseek(&ds, fill, SEEK_CUR);
  if (crm114__dbread(&sol->mudottheta, sizeof(double), 1, &ds) != 1) {
    crm114__pca_free_solution(sol);
    crm114__dbclose(&ds);
    return NULL;
  }

  crm114__dbclose(&ds);
  return sol;
}


#endif		// defined(PCA_USE_MMAP)


//functions used to read in the file
//when we need to do a learn in classify.
static int pca_read_db(pca_block *blck, CRM114_DATABLOCK *db) {
  struct data_state ds;
  int ret;

  crm114__dbopen(db, &ds);
  ret = pca_read_db_dsp(blck, &ds);
  crm114__dbclose(&ds);

  return ret;
}

//reads a binary pca block from a file
//returns 0 on failure
static int pca_read_db_dsp(pca_block *blck, struct data_state *dsp) {
  PCA_FILE_HDR fh;
  size_t things_read;
  Vector *v;
  int fill;

  if (!blck || !dsp) {
    //this really shouldn't happen
    return 0;
  }

  pca_block_free_data(*blck);
  pca_block_init(blck);

  things_read = crm114__dbread(&fh, sizeof(fh), 1, dsp);
  if (things_read != 1
      || strncmp(PCA_FIRST_BITS, fh.firstbits, strlen(PCA_FIRST_BITS))
      || fh.size < sizeof(PCA_FILE_HDR)) {
    return 0;
  }

  blck->has_new = fh.has_new;
  blck->has_solution = fh.has_solution;
  blck->n0 = fh.n0;
  blck->n1 = fh.n1;
  blck->n0f = fh.n0f;
  blck->n1f = fh.n1f;

  //read in solution
  if (blck->has_solution) {
    blck->sol = (PCA_Solution *)malloc(sizeof(PCA_Solution));
    blck->sol->theta = crm114__db_vector_read_bin_dsp(dsp);
    things_read = crm114__dbread(&fill, sizeof(int), 1, dsp);
    crm114__dbseek(dsp, fill, SEEK_CUR);
    if (!blck->sol->theta || !things_read || crm114__dbeof(dsp) ||
	crm114__dbtell(dsp) > fh.size) {
      //die!
      pca_block_free_data(*blck);
      pca_block_init(blck);
      return 0;
    }
    things_read = crm114__dbread(&(blck->sol->mudottheta), sizeof(double), 1, dsp);
    if (!things_read) {
      //die!
      pca_block_free_data(*blck);
      pca_block_init(blck);
      return 0;
    }
  } else {
    things_read = crm114__dbread(&fill, sizeof(int), 1, dsp);
    crm114__dbseek(dsp, fill + sizeof(double), SEEK_CUR);
    if (!things_read || crm114__dbeof(dsp) || crm114__dbtell(dsp) >= fh.size) {
      pca_block_free_data(*blck);
      pca_block_init(blck);
      return 0;
    }
  }

  //read in new vectors
  if (!crm114__dbeof(dsp) && crm114__dbtell(dsp) < fh.size) {
    v = crm114__db_vector_read_bin_dsp(dsp);
    if (v && v->dim) {
      if (!(blck->X)) {
	blck->X = crm114__matr_make_size(0, v->dim, v->type, v->compact, v->size);
      }
      if (!blck->X) {
	pca_block_free_data(*blck);
	pca_block_init(blck);
	return 0;
      }
      crm114__matr_shallow_row_copy(blck->X, blck->X->rows, v);
      while (!crm114__dbeof(dsp) && crm114__dbtell(dsp) < fh.size) {
	v = crm114__db_vector_read_bin_dsp(dsp);
	if (v && v->dim) {
	  crm114__matr_shallow_row_copy(blck->X, blck->X->rows, v);
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

  return 1;
}


/*
  Write a set of data hanging off a pca_block to a "file" -- a
  CRM114_DATABLOCK's block 0.  If the CRM114_DATABLOCK isn't big
  enough, replace it with one that is.

  See comments in SVM.
*/

// write helper helper
// return size of theta + room to grow
static size_t theta_room(pca_block *blck) {
  int dec_size = MATR_DEFAULT_VECTOR_SIZE*sizeof(double);
  size_t theta_size;

  if (blck->sol && blck->sol->theta) {
    theta_size = crm114__vector_size(blck->sol->theta);
    while (theta_size >= dec_size) {
      if (!(dec_size)) {
	dec_size = 1;
      }
      dec_size *= 2;
    }
  }

  return sizeof(dec_size) + dec_size;
}

// write helper
// return size blck would take in "file" including room to grow
static size_t block_size(pca_block *blck) {
  size_t size;
  int i;

  size = sizeof(PCA_FILE_HDR)
    + theta_room(blck)		// theta, filler size, filler
    + sizeof(double);		// mudottheta

  if (blck->X) {
    for (i = 0; i < blck->X->rows; i++) {
      if (blck->X->data[i]) {
	size += crm114__vector_size(blck->X->data[i]);
      }
    }
  }

  size += PCA_HOLE_FRAC * size;

  return size;
}

#ifdef PCA_USE_MMAP

// write helper
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

// write helper
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

//writes a pca block to a CRM114_DATABLOCK in binary format
//first reallocates the block, maybe bigger, if necessary
//returns the number of bytes written and pointer to new block
static size_t pca_write_db(pca_block *blck, CRM114_DATABLOCK **db) {
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
#ifdef PCA_USE_MMAP
  newdb = replace_db(*db, expansion_size);
#else
  newdb = maybe_expand_db(*db, needed, expansion_size);
#endif
  if (newdb != NULL) {
    crm114__dbopen(newdb, &ds);
    size_written = pca_write_db_dsp(blck, &ds);
    crm114__dbclose(&ds);
    *db = newdb;
  }

  return size_written;
}


//writes an pca block to a file in binary format
//returns the number of bytes written
//frees blck
static size_t pca_write_db_dsp(pca_block *blck, struct data_state *dsp) {
  size_t size;
  PCA_FILE_HDR fh;
  int i;
  Matrix *M = crm114__matr_make(0, 0, SPARSE_ARRAY, MATR_COMPACT);
  double d;

  if (!blck || !dsp) {
    return 0;
  }

  if (blck->sol && blck->sol->theta) {
    blck->has_solution = 1;
  } else {
    blck->has_solution = 0;
  }

  memcpy(fh.firstbits, PCA_FIRST_BITS, PCA_FIRST_NBIT);
  fh.size = 0;
  fh.has_new = blck->has_new;
  fh.has_solution = blck->has_solution;
  fh.n0 = blck->n0;
  fh.n1 = blck->n1;
  fh.n0f = blck->n0f;
  fh.n1f = blck->n1f;

  size = sizeof(fh) * crm114__dbwrite(&fh, sizeof(fh), 1, dsp);

  //write the principle component and the fill
  //write the principle component dot the mean vector
  if (blck->sol) {
    size += pca_write_theta_dsp(blck->sol->theta, dsp);
    size += sizeof(double)*crm114__dbwrite(&(blck->sol->mudottheta),
					   sizeof(double), 1, dsp);
  } else {
    //leave room
    size += pca_write_theta_dsp(NULL, dsp);
    d = 0.0;
    size += sizeof(double)*crm114__dbwrite(&d, sizeof(double), 1, dsp);
  }

  //now write out the example vectors
  if (blck->X) {
    for (i = 0; i < blck->X->rows; i++) {
      if (blck->X->data[i]) {
	size += crm114__db_vector_write_bin_dsp(blck->X->data[i], dsp);
      }
    }
  }

  //this tells you where the data in the file ends
  fh.size = size;

  // overwrite the file header now that we've filled in the size
  crm114__dbseek(dsp, 0, SEEK_SET);
  (void)crm114__dbwrite(&fh, sizeof(fh), 1, dsp);

  //now leave a nice big hole
  //so we can add lots of nice vectors
  //without changing the file size
  size += PCA_HOLE_FRAC * size;

  crm114__matr_free(M);
  pca_block_free_data(*blck);
  pca_block_init(blck);

  return size;
}


//writes theta to a file, leaving it room to grow
static size_t pca_write_theta_dsp(Vector *theta, struct data_state *dsp) {
  int dec_size = MATR_DEFAULT_VECTOR_SIZE*sizeof(double);
  size_t size = 0, theta_written, theta_size;

  if (!dsp) {
    if (pca_trace) {
      fprintf(stderr, "pca_write_theta_dsp: null data_state pointer.\n");
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

//appends a vector to the pca file to be learned on later
//frees the vector
static size_t append_vector_to_pca_db(Vector *v, CRM114_DATABLOCK **db) {
  size_t vsize;
  char *st_addr;
  PCA_FILE_HDR *fh;
  size_t act_size;
  size_t cur0, expansion_size;
  CRM114_DATABLOCK *newdb;

  st_addr = &(*db)->data[(*db)->cb.block[0].start_offset];
  fh = (PCA_FILE_HDR *)st_addr;
  act_size = (*db)->cb.block[0].allocated_size;

  if (act_size < sizeof(PCA_FILE_HDR)
      || strncmp(PCA_FIRST_BITS, fh->firstbits, PCA_FIRST_NBIT)
      || fh->size > act_size
      || fh->size < sizeof(PCA_FILE_HDR)) {
    crm114__vector_free(v);
    return 0;
  }
  vsize = crm114__vector_size(v);
  cur0 = (*db)->cb.block[0].allocated_size;
  expansion_size = MAX((size_t)((double)cur0 * 1.1), cur0 + vsize);
  //make sure the block is big enough
  if ((newdb = maybe_expand_db(*db, fh->size + vsize, expansion_size))
      != NULL) {
    //db may have moved, so set up addresses and sizes again
    st_addr = &newdb->data[newdb->cb.block[0].start_offset];
    fh = (PCA_FILE_HDR *)st_addr;
    //we have room to write the vector
    //so add it
    (void)crm114__vector_memmove((Vector *)(st_addr + fh->size), v);
    fh->size += vsize;	//update space used
    //now note that we have new vectors that haven't been learned on
    fh->has_new = 1;
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
static size_t pca_save_changes(pca_block *blck, CRM114_DATABLOCK **db) {
#ifndef PCA_USE_MMAP
  return pca_write_db(blck, db);
#else

  size_t theta_room, theta_req;
  char *curr, *st_addr, *prev, *last_addr;
  PCA_FILE_HDR *fh;
  size_t act_size;
  pca_block old_block;

  st_addr = &(*db)->data[(*db)->cb.block[0].start_offset];
  fh = (PCA_FILE_HDR *)st_addr;
  act_size = (*db)->cb.block[0].allocated_size;

  if (act_size < sizeof(PCA_FILE_HDR)
      || strncmp(PCA_FIRST_BITS, fh->firstbits, PCA_FIRST_NBIT)
      || fh->size > act_size
      || fh->size < sizeof(PCA_FILE_HDR)) {
    return 0;
  }

  if (fh->size + sizeof(double)*MATR_DEFAULT_VECTOR_SIZE >= act_size) {
    //we have no more room to append vectors to this file
    //so write it out now
    //otherwise size won't change
    if (pca_trace) {
      fprintf(stderr, "Writing file to leave a hole at the end.\n");
    }
    return pca_write_db(blck, db);
  }

  last_addr = st_addr + fh->size;

  //make all of the constants correct
  if (blck->sol && blck->sol->theta) {
    blck->has_solution = 1;
  } else {
    blck->has_solution = 0;
  }

  old_block.has_new = fh->has_new;
  old_block.has_solution = fh->has_solution;
  old_block.n0 = fh->n0;
  old_block.n1 = fh->n1;
  old_block.n0f = fh->n0f;
  old_block.n1f = fh->n1f;

  fh->has_new = blck->has_new;
  fh->has_solution = blck->has_solution;
  fh->n0 = blck->n0;
  fh->n1 = blck->n1;
  fh->n0f = blck->n0f;
  fh->n1f = blck->n1f;

  //do we have room to write theta?
  curr = st_addr + sizeof(*fh);

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
    if (pca_trace) {
      fprintf
	(stderr,
	 "Writing file to grow PC size from %lu to %lu.\n",
	 theta_room, theta_req);
    }
    return pca_write_db(blck, db);
  }

  //we have enough room to unmap the solution to this file
  //let's do it!

  //write the new solution boundary
  if (blck->has_solution && blck->sol) {
    //copy over the decision boundary
    //it is possible that curr and blck->sol->theta
    //overlap if we didn't actually do a learn
    //so use memmove NOT memcpy
    prev = crm114__vector_memmove((void *)curr, blck->sol->theta);
  }
  //leave a marker to let us know how much filler space we have
  *((int *)prev) = theta_room-theta_req;
  curr += theta_room + sizeof(int);
  if (blck->has_solution && blck->sol) {
    *((double *)curr) = blck->sol->mudottheta;
  }
  curr += sizeof(double);

  //and that's all folks!
  pca_block_free_data(*blck);
  pca_block_init(blck);
  return fh->size;

#endif
}


// read/write learned data ("file", block 0) from/to text file

/*
Write PCA learned data (CRM114_DATABLOCK block 0) to the kind of
text file that's portable storage of the binary datablock.

Don't write theta filler or filler size, vmap, vmap_size, or any of
the offsets.  All that is constructed in the read routine.

Each matrix is identified, and its vectors follow its header directly,
in order, so the text file needs no analog of vmap.

Returns T/F success/failure.
*/

int crm114__pca_learned_write_text_fp(CRM114_DATABLOCK *db, FILE *fp)
{
  int ret = FALSE;
  pca_block blck;

  pca_block_init(&blck);
  if (pca_read_db(&blck, db))
    {
      fprintf(fp, C0_DOCS_FEATS_FMT "\n", blck.n0, blck.n0f);
      fprintf(fp, C1_DOCS_FEATS_FMT "\n", blck.n1, blck.n1f);
      fprintf(fp, TN_HAS_X " %d\n", blck.X != NULL);
      fprintf(fp, TN_HAS_SOL " %d\n", blck.has_solution);
      if (blck.X != NULL)
	crm114__matr_write_text_fp(TN_X, blck.X, fp);
      if (blck.has_solution)
	{
	  fprintf(fp, "%s " PRINTF_DOUBLE_FMT "\n",
		  TN_MUDOTTHETA, blck.sol->mudottheta);
	  crm114__vector_write_text_fp(TN_THETA, blck.sol->theta, fp);
	}

      pca_block_free_data(blck);
      pca_block_init(&blck);
      ret = TRUE;		// suuure!
    }

  return ret;
}

// read PCA learned data from text file into CRM114_DATABLOCK block 0
// can realloc the datablock
// returns T/F
int crm114__pca_learned_read_text_fp(CRM114_DATABLOCK **db, FILE *fp)
{
  pca_block blck;
  size_t expected_size;
  int has_X;

  pca_block_init(&blck);
  if (fscanf(fp, " " C0_DOCS_FEATS_FMT, &blck.n0, &blck.n0f) != 2)
    goto err;
  if (fscanf(fp, " " C1_DOCS_FEATS_FMT, &blck.n1, &blck.n1f) != 2)
    goto err;
  if (fscanf(fp, " " TN_HAS_X " %d", &has_X) != 1)
    goto err;
  if (fscanf(fp, " " TN_HAS_SOL " %d", &blck.has_solution) != 1)
    goto err;
  if (has_X)
    if ((blck.X = crm114__matr_read_text_fp(TN_X, fp)) == NULL)
      goto err;
  if (blck.has_solution)
    {
      if ((blck.sol = malloc(sizeof(PCA_Solution))) == NULL)
	goto err;
      if (fscanf(fp, " " TN_MUDOTTHETA " " SCANF_DOUBLE_FMT,
		 &blck.sol->mudottheta) != 1)
	goto err;
      if ((blck.sol->theta = crm114__vector_read_text_fp(TN_THETA, fp)) == NULL)
	goto err;
    }
  expected_size = block_size(&blck);
  return (pca_write_db(&blck, db) == expected_size);

 err:
  pca_block_free_data(blck);
  pca_block_init(&blck);
  return FALSE;
}


/***************************PCA BLOCK FUNCTIONS*******************************/

//initializes an pca block
static void pca_block_init(pca_block *blck) {
  blck->sol = NULL;
  blck->X = NULL;
  blck->has_new = 0;
  blck->has_solution = 0;
  blck->n0 = 0;
  blck->n1 = 0;
  blck->n0f = 0;
  blck->n1f = 0;
}

//frees all data associated with a block
static void pca_block_free_data(pca_block blck) {

  if (blck.sol) {
    crm114__pca_free_solution(blck.sol);
  }
  if (blck.X) {
    crm114__matr_free(blck.X);
  }
}

/***************************CRM114_DATABLOCK INIT*****************************/

// initialize data block 0 to be valid and empty
void crm114__init_block_pca(CRM114_DATABLOCK *db, int c)
{
  pca_block blck;
  struct data_state ds;

  if (db->cb.how_many_blocks == 1 && c == 0) {
    // set up an empty data set in its internal form
    pca_block_init(&blck);
    // write it to the data block, if it fits
    if (block_size(&blck) <= db->cb.block[c].allocated_size) {
      crm114__dbopen(db, &ds);
      pca_write_db_dsp(&blck, &ds);
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
    if ( !(PCA_CLASS_MAG && vectorit_curr_col(vit, doc) == 0))
      sum += vectorit_curr_val(vit, doc);

  return sum;
}

/***************************LEARNING FUNCTIONS********************************/

//does the actual work of learning new examples
static void pca_learn_new_examples(pca_block *blck, int microgroom) {
  int i, inc, loop_it, pinc, ninc, sgn, lim;
  VectorIterator vit;
  Vector *row;
  double frac_inc, d, val, offset, back, front, ratio, last_offset, *dt;
  PreciseSparseElement *thetaval = NULL;

  if (!blck->X) {
    //reset the block
    pca_block_free_data(*blck);
    pca_block_init(blck);
    return;
  }

  //update n0, n1, n0f, n1f
  blck->n0 = 0;
  blck->n1 = 0;
  blck->n0f = 0;
  blck->n1f = 0;
  for (i = 0; i < blck->X->rows; i++) {
    row = matr_get_row(blck->X, i);
    if (!row) {
      //this would be weird
      continue;
    }
    vectorit_set_at_beg(&vit, row);
    if (!vectorit_past_end(vit, row)) {
      if (vectorit_curr_val(vit, row) < 0) {
	//a new example for class 1
	blck->n1++;
	blck->n1f += nfeat(row);
      } else {
	blck->n0++;
	blck->n0f += nfeat(row);
      }
    }
  }
  dt = (double *)malloc(sizeof(double)*blck->X->rows);


  //actually learn something!
  if (pca_trace) {
    fprintf(stderr, "Calling PCA solve.\n");
  }

  frac_inc = PCA_REDO_FRAC + 1;

  loop_it = 0;
  inc = 0;
  pinc = 0;
  ninc = 0;
  while (frac_inc >= PCA_REDO_FRAC && loop_it < PCA_MAX_REDOS) {
    crm114__pca_solve((blck->X), &(blck->sol));

    if (!blck->sol) {
      pca_block_free_data(*blck);
      pca_block_init(blck);
      free(dt);
      return;
    }

    inc = 0;
    pinc = 0;
    ninc = 0;
    if (PCA_CLASS_MAG && blck->n0 > 0 && blck->n1 > 0) {
      //check to see if we have class mag set high enough
      //it's set high enough if we correctly classify everything
      //(around a 0 decision point) USING the first element
      d = crm114__vector_get(blck->sol->theta, 0);
      if (d < 0) {
	crm114__vector_multiply(blck->sol->theta, -1, blck->sol->theta);
	blck->sol->mudottheta *= -1;
      }
      for (i = 0; i < blck->X->rows; i++) {
	dt[i] = crm114__dot(matr_get_row(blck->X, i), blck->sol->theta);
	val = matr_get(blck->X, i, 0);
	if (dt[i]*val <= 0) {
	  inc++;
	}

	//get rid of the influence of the first element
	//now we can use this dt[i] to find the correct
	//decision point later
	dt[i] -= crm114__vector_get(blck->sol->theta, 0)*val;
	if (dt[i] <= 0 && val > 0) {
	  pinc++;
	} else if (dt[i] >= 0 && val < 0) {
	  ninc++;
	}
      }
    }
    frac_inc = inc/(double)blck->X->rows;
    if (frac_inc >= PCA_REDO_FRAC) {
      for (i = 0; i < blck->X->rows; i++) {
	matr_set(blck->X, i, 0, 2*matr_get(blck->X, i, 0));
      }
      crm114__pca_free_solution(blck->sol);
      blck->sol = NULL;
      if (pca_trace) {
	fprintf(stderr, "The fraction of wrong classifications was %lf.  Repeating with class mag = %lf\n", frac_inc, matr_get(blck->X, 0, 0));
      }
      loop_it++;
    }
  }

  if (!blck->sol) {
    pca_block_free_data(*blck);
    pca_block_init(blck);
    free(dt);
    return;
  }

  offset = 0;

  if (PCA_CLASS_MAG) {
    if (loop_it) {
      //we increased the class mags - set them back to the initial value
      for (i = 0; i < blck->X->rows; i++) {
	d = matr_get(blck->X, i, 0);
	if (d > 0) {
	  matr_set(blck->X, i, 0, PCA_CLASS_MAG);
	} else {
	  matr_set(blck->X, i, 0, -PCA_CLASS_MAG);
	}
      }
    }

    //calculate decision point
    //if number of negative examples = number of positive examples,
    //this point is xbardotp.  however, it turns out that if there
    //is a skewed distribution, the point moves.  i feel like there
    //should be a theoretical way of knowing where this point is since
    //we know it for a non-skewed distribution, but i can't seem to find
    //it.  so... we do this as a binary search - we are trying to make the
    //number of positive and negative mistakes the same

    //figure out initial direction (n <? p)
    if (blck->n1 > 0) {
      ratio = blck->n0/(double)blck->n1;//ratio of positive examples to negative
    } else {
      ratio = 0;
    }
    inc = ninc+pinc;

    if ((int)(ratio*ninc + 0.5) < pinc) {
      //we are getting more positive examples wrong than negative
      //ones - we should decrease the offset
      sgn = -1;
    } else {
      sgn = 1;
    }

    offset = 0;
    //one point of the binary search is zero - we need the other
    //far point.  just go out in big jumps until we find it
    while ((sgn < 0 && (int)(ratio*ninc + 0.5) < pinc) ||
	   (sgn > 0 && (int)(ratio*ninc + 0.5) > pinc)) {
      offset += sgn;
      ninc = 0;
      pinc = 0;
      for (i = 0; i < blck->X->rows; i++) {
	val = matr_get(blck->X, i, 0);
	if ((dt[i] - offset)*val <= 0) {
	  if (val < 0) {
	    ninc++;
	  } else {
	    pinc++;
	  }
	}
      }
    }

    //now do the search
    //our boundaries on the binary search are 0 and offset
    if (offset > 0) {
      front = 0;
      back = offset;
    } else {
      front = offset;
      back = 0;
    }
    last_offset = offset + 1;
    while ((int)(ratio*ninc + 0.5) != pinc && front < back &&
	   last_offset != offset) {
      last_offset = offset;
      offset = (front + back)/2.0;
      ninc = 0;
      pinc = 0;
      for (i = 0; i < blck->X->rows; i++) {
	val = matr_get(blck->X, i, 0);
	if ((dt[i] - offset)*val <= 0) {
	  if (val < 0) {
	    ninc++;
	  } else {
	    pinc++;
	  }
	}
      }
      if ((int)(ratio*ninc + 0.5) < pinc) {
	//offset should get smaller
	//ie back should move closer to front
	back = offset;
      } else if ((int)(ratio*ninc + 0.5) > pinc) {
	front = offset;
      }
      if (pca_trace) {
	fprintf(stderr, "searching for decision point: current point = %lf pinc = %d ninc = %d ratio = %lf\n", offset, pinc, ninc, ratio);
      }
    }
    inc = pinc+ninc;
    //offset is now the decision point
    blck->sol->mudottheta = offset;

    if (pca_trace) {
      fprintf(stderr, "found decision point: %lf pinc = %d ninc = %d ratio = %lf\n", offset, pinc, ninc, ratio);
    }
  }

  if (pca_trace) {
    fprintf(stderr, "Of %d examples, we classified %d incorrectly.\n",
	    blck->X->rows, inc);
  }

  //microgroom
  if (microgroom && blck->X->rows >= PCA_GROOM_OLD) {
    if (pca_trace) {
      fprintf(stderr, "Microgrooming...\n");
    }
    thetaval =
      (PreciseSparseElement *)
      malloc(sizeof(PreciseSparseElement)*blck->X->rows);
    for (i = 0; i < blck->X->rows; i++) {
      thetaval[i].data = dt[i] - offset;
      if (matr_get(blck->X, i, 0) < 0) {
	thetaval[i].data = -thetaval[i].data;
      }
      thetaval[i].col = i;
    }

    //sort based on the value
    qsort(thetaval, blck->X->rows, sizeof(PreciseSparseElement),
	  crm114__precise_sparse_element_val_compare);
    //get rid of the top PCA_GROOM_FRAC
    qsort(&(thetaval[(int)(blck->X->rows*PCA_GROOM_FRAC)]),
	  blck->X->rows - (int)(blck->X->rows*PCA_GROOM_FRAC),
	  sizeof(PreciseSparseElement), crm114__precise_sparse_element_col_compare);
    lim = blck->X->rows;
    for (i = (int)(blck->X->rows*PCA_GROOM_FRAC); i < lim; i++) {
      crm114__matr_remove_row(blck->X, thetaval[i].col);
    }
    free(thetaval);
  }

  free(dt);

  //we've learned all new examples
  blck->has_new = 0;

  //we've solved it!  so we have a solution
  blck->has_solution = 1;
}

/*
  Jennifer's comment below.  This function (top-level PCA learn) has
  since been modified for the library: instead of taking text and a
  CRM114 command line, calling the vector tokenizer, and storing the
  learned document in a file, it now takes feature-rows (output of
  vector tokenizer, already sorted and maybe uniqued), a class to
  learn the document into (0 or 1, optionally reversed by
  CRM114_REFUTE), and a malloc()'ed CRM114_DATABLOCK, and stores the
  learned document in the data block.  It will expand (reallocate) the
  data block if necessary.

                                                     --KH March 2010
*/

/******************************************************************************
 *Use a PCA to learn a classification task.
 *This expects two classes: a class with a +1 label and a class with
 *a -1 label.  These are denoted by the presence or absense of the
 *CRM114_REFUTE label (see the FLAGS section of the comment).
 *For an overview of how the algorithm works, look at the comments in
 *crm114_pca_lib_fncts.c.
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
 *FLAGS: The PCA calls crm_vector_tokenize_selector so uses any flags
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
 * CRM114_MICROGROOM: If there are more than PCA_GROOM_OLD (defined in
 *  (crm114_config.h) examples, CRM114_MICROGROOM will remove the PCA_GROOM_FRAC
 *  (defined in crm11_config.h) of them furthest from the decision
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
 *****************************************************************************/

CRM114_ERR crm114_learn_features_pca(CRM114_DATABLOCK **db,
				     int class,
				     const struct crm114_feature_row features[],
				     long featurecount) {

  unsigned long long classifier_flags; // local copy because we may modify it
  long i, j;
  pca_block blck;
  Vector *nex, *row;
  int read_file = 0, do_learn = 1, lim = 0;

  if (crm114__user_trace) {
    pca_trace = 1;
  }

  if (crm114__internal_trace) {
    //this is a "mediumly verbose" setting
    pca_trace = PCA_INTERNAL_TRACE_LEVEL + 1;
  }

  CRM114__PCA_DEBUG_MODE = pca_trace - 1;
  if (CRM114__PCA_DEBUG_MODE < 0) {
    CRM114__PCA_DEBUG_MODE = 0;
  }

  if (pca_trace) {
    fprintf(stderr, "Doing a PCA learn.\n");
  }

  if ( !((*db)->cb.how_many_blocks == 1 && (*db)->cb.how_many_classes == 2))
    return CRM114_BADARG;
  if ( !(class == 0 || class == 1))
    return CRM114_BADARG;

  classifier_flags = (*db)->cb.classifier_flags;

  // Refute means learn as not member of class specified.  In PCA,
  // everything is in one class or the other, so if it's not in this
  // one, it must be in the other.
  if (classifier_flags & CRM114_REFUTE)
    class ^= 1;			// 1 -> 0, 0 -> 1

  //set things to NULL that should be null
  pca_block_init(&blck);


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
      if (pca_map_db(&blck, *db)) {
	read_file = 1;
      }
      do_learn = 0; //we are erasing, not learning
      j = 0;
      lim = blck.X->rows;
      for (i = 0; i < lim; i++) {
	row = matr_get_row(blck.X, i-j);
	if (!row) {
	  continue;
	}
	if (crm114__vector_equals(nex, row)) {
	  //have to start over
	  do_learn = 1;
	  classifier_flags |= CRM114_FROMSTART;
	  crm114__matr_remove_row(blck.X, i-j);
	  j++;
	  if (crm114__vector_get(nex, 0) < 0) {
	    blck.n1--;
	    blck.n1f -= nfeat(nex);
	  } else {
	    blck.n0--;
	    blck.n0f -= nfeat(nex);
	  }
	}
      }
      crm114__vector_free(nex);
    } else {
      //add the vector to the new matrix
      append_vector_to_pca_db(nex, db);
    }
  }

  if (classifier_flags & CRM114_FROMSTART) {
    do_learn = 1;
    if (!read_file) {
      if (pca_map_db(&blck, *db)) {
	read_file = 1;
      }
    }
    //get rid of the old solution
    crm114__pca_free_solution(blck.sol);
    blck.sol = NULL;

    //reset the constants
    blck.n0 = 0;
    blck.n1 = 0;
    blck.n0f = 0;
    blck.n1f = 0;
  }

  if (!(classifier_flags & CRM114_APPEND) && do_learn) {
    if (!read_file) {
      if (!pca_map_db(&blck, *db)) {
	do_learn = 0;
      } else {
	read_file = 1;
      }
    }

    if (do_learn) {
      pca_learn_new_examples(&blck,
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
    (void)pca_save_changes(&blck, db);
  }

  //free everything
  pca_block_free_data(blck);

  return CRM114_OK;
}

/****************************CLASSIFICATION FUNCTIONS*************************/

// translate the sign of a PCA label to a CRM class
static inline int sign_class(double x) {
  return (x < 0.0);		// always either 0 or 1
}

// Given an unknown document and a solution vector, return the
// unknown's number of hits in each of PCA's two classes.  Maybe.
//
// I have no idea whether this is legitimate.  It gives plausible
// numbers, about the same as the same hack gives in SVM.
//                                                     --KH
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

/******************************************************************************
 *Use a PCA for a classification task.
 *This expects two classes: a class with a +1 label and a class with
 *a -1 label.  The class with the +1 label is class 0 and the class
 *with the -1 label is class 1.  When learning, class 1 is denoted by
 *passing in the CRM114_REFUTE flag.  The classify is considered to FAIL
 *if the example classifies as class 1 (-1 label).  The PCA requires
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
 *       prob(class = 0) = 0.5 + 0.5*tanh(theta dot x - mudottheta)
 *       pR = (theta dot x  - mudottheta)*10
 *
 *FLAGS: The PCA calls crm_vector_tokenize_selector so uses any flags
 * that that function uses.  For classifying, it interprets flags as
 * follows:
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

CRM114_ERR crm114_classify_features_pca(CRM114_DATABLOCK *db,
					const struct crm114_feature_row features[],
					long nfr, CRM114_MATCHRESULT *result) {

  Vector *nex;
  double dottheta = 0;
  int class, sgn;
  pca_block blck;
  int hits[2];

  if (crm114__user_trace) {
    pca_trace = 1;
  }

  if (crm114__internal_trace) {
    //this is a "mediumly verbose" setting
    pca_trace = PCA_INTERNAL_TRACE_LEVEL + 1;
  }

  CRM114__PCA_DEBUG_MODE = pca_trace - 1;

  if (CRM114__PCA_DEBUG_MODE < 0) {
    CRM114__PCA_DEBUG_MODE = 0;
  }

  if (pca_trace) {
    fprintf(stderr, "Doing a PCA classify.\n");
  }

  if ( !(db->cb.how_many_blocks == 1 && db->cb.how_many_classes == 2))
    return CRM114_BADARG;

  pca_block_init(&blck);

  if (has_new_vectors(db)) {
    //we use read so that we don't make changes to the file
    if (pca_read_db(&blck, db)) {
      pca_learn_new_examples(&blck, 0);
    }
  } else {
    pca_get_meta_data(db, &blck);
    blck.sol = get_solution_from_pca_db(db);
  }

  //get the new example
  nex = convert_document(0, features, nfr);

  if (PCA_CLASS_MAG) {
    //we're classifying.  we don't want a class mag running around.
    crm114__vector_set(nex, 0, 0);
  }

  //classify it
  if (blck.sol && blck.sol->theta) {
    //this is biased towards the negative not sure why
    dottheta = crm114__dot(nex, blck.sol->theta) - blck.sol->mudottheta;
    nhit(nex, blck.sol->theta, &hits[0], &hits[1]); // ???
    pca_block_free_data(blck);
  } else {
    dottheta = 0;
    hits[0] = 0;
    hits[1] = 0;
  }

  if (dottheta < 0) {
    class = 1;
    sgn = -1;
  } else {
    class = 0;
    sgn = 1;
  }

  if (fabs(dottheta) > 100000) {
    dottheta = sgn*9999.9;
  }

  if (pca_trace) {
    fprintf
      (stderr,
       "The dot product of the decision boundary and the example is %lf\n",
       dottheta);
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
  result->class[0].pR   = dottheta * 1000.0;
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
