//	crm114_matrix.h - Support Vector Machine

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

#ifndef __CRM114_MATRIX_H__
#define __CRM114_MATRIX_H__

#include "crm114_matrix_util.h"

/*******************************************************************
 *A matrix/vector library that deals with different data structures
 *for representing vectors transparently.
 *
 *The functions in matrix.c are commented.  See them for
 *details on this library.
 ******************************************************************/

#define MATR_DEFAULT_VECTOR_SIZE 1200 //sparse arrays start at this size
                                      //unless otherwise specified
#define MATR_COMPACT 1
#define MATR_PRECISE 0
#define QSORT_COUNTING_CUTOFF 7e7 //with this many non-zero elts or above
                                  //we use counting sort, not q-sort

extern int CRM114__MATR_DEBUG_MODE; //debug setting.  see crm114_matrix_util.h
                                    //for possible modes

//Possible vector types.
typedef enum {
  NON_SPARSE,
  SPARSE_ARRAY,
  SPARSE_LIST
} VectorType;

typedef enum {
  COUNTING,
  MERGE,
  QSORT
} SortingAlgorithms;


typedef union {
  PreciseSparseNode *pcurr;
  CompactSparseNode *ccurr;
  long nscurr;
} VectorIterator;


//data for non-sparse
//can be either doubles or ints
typedef union {
  int *compact;
  double *precise;
} NSData;

//vectors can be either expanding arrays, lists,
//or arrays of NSData
typedef union {
  ExpandingArray *sparray;   //SPARSSE_ARRAY vector
  SparseElementList *splist; //SPARSE_LIST vector
  NSData nsarray;            //NON_SPARSE vector
} VectorData;

//vector struct
typedef struct {
  VectorData data;  //data stored in the vector
  unsigned int dim; //# columns (dimension) in the vector
  int nz,           //# non-zero elements (nz = dim if v is NON_SPARSE)
    compact,        //flag for compactness
    size,           //starting size for expanding arrays
    was_mapped;     //1 if the vector was mapped into memory
  VectorType type;  //flag for type
} Vector;

//matrix struct
typedef struct {
  Vector **data;     //list of pointers to rows
  unsigned int rows, //# rows in the matrix
    cols;            //# columns in the matrix
  int nz,            //# non-zero elements (nz = rows*cols if M is NON_SPARSE)
    compact,         //flag for compactness
    size,            //starting size for expanding arrays
    was_mapped;      //1 if the matrix was mapped into memory
  VectorType type;   //flag for type
} Matrix;

//Matrix functions
Matrix *crm114__matr_make(unsigned int rows, unsigned int cols, VectorType type,
		  int compact);
Matrix *crm114__matr_make_size(unsigned int rows, unsigned int cols, VectorType type,
		       int compact, int init_size);
void crm114__matr_set_row(Matrix *A, unsigned int r, Vector *v);
void crm114__matr_shallow_row_copy(Matrix *M, unsigned int r, Vector *v);
void crm114__matr_set_col(Matrix *A, unsigned int c, Vector *v);
void crm114__matr_add_row(Matrix *M);
void crm114__matr_add_nrows(Matrix *M, unsigned int n);
void crm114__matr_add_col(Matrix *M);
void crm114__matr_add_ncols(Matrix *M, unsigned int n);
void crm114__matr_remove_row(Matrix *M, unsigned int r);
void crm114__matr_erase_row(Matrix *M, unsigned int r);
void crm114__matr_remove_col(Matrix *M, unsigned int c);
ExpandingArray *crm114__matr_remove_zero_rows(Matrix *M);
ExpandingArray *crm114__matr_remove_zero_cols(Matrix *M);
void crm114__matr_append_matr(Matrix **to_ptr, Matrix *from);
void crm114__matr_vector(Matrix *M, Vector *v, Vector *ret);
void crm114__matr_vector_seq(Matrix **A, int nmatrices, unsigned int maxrows,
		     Vector *w, Vector *z);
void crm114__matr_transpose(Matrix *A, Matrix *T);
void crm114__matr_multiply(Matrix *M1, Matrix *M2, Matrix *ret);
int crm114__matr_iszero(Matrix *M);
void crm114__matr_convert_nonsparse_to_sparray(Matrix *M, ExpandingArray *colMap);
void crm114__matr_print(Matrix *M);
void crm114__matr_write(Matrix *M, char *filename);
void crm114__matr_write_fp(Matrix *M, FILE *out);
void crm114__matr_write_text_fp(char name[], Matrix *M, FILE *fp);
size_t crm114__matr_write_bin(Matrix *M, char *filename);
size_t crm114__matr_write_bin_fp(Matrix *M, FILE *fp);
size_t crm114__matr_size(Matrix *M);
Matrix *crm114__matr_read_text_fp(char expected_name[], FILE *fp);
Matrix *crm114__matr_read_bin(char *filename);
Matrix *crm114__matr_read_bin_fp(FILE *fp);
Matrix *crm114__matr_map(void **addr, void *last_addr);
void crm114__matr_free(Matrix *M);

//inlined matrix functions defined in this file
//(inlining is compiler discretion)
static inline void matr_set(Matrix *M, unsigned int r, unsigned int c,
			    double d);
static inline double matr_get(Matrix *M, unsigned int r, unsigned int c);
static inline Vector *matr_get_row(Matrix *M, unsigned int r);


//Vector functions
Vector *crm114__vector_make(unsigned int dim, VectorType type, int compact);
Vector *crm114__vector_make_size(unsigned int dim, VectorType type, int compact,
			 int init_size);
void crm114__vector_copy(Vector *from, Vector *to);
void crm114__vector_set(Vector *v, unsigned int i, double d);
double crm114__vector_get(Vector *v, unsigned int i);
void crm114__vector_add_col(Vector *v);
void crm114__vector_add_ncols(Vector *v, unsigned int n);
void crm114__vector_remove_col(Vector *v, unsigned int c);
int crm114__vector_iszero(Vector *V);
int crm114__vector_equals(Vector *v1, Vector *v2);
void crm114__vector_zero(Vector *v);
void crm114__vector_add(Vector *v1, Vector *v2, Vector *ret);
void crm114__vector_multiply(Vector *v, double s, Vector *ret);
double crm114__dot(Vector *v1, Vector *v2);
void crm114__vector_add_multiple(Vector *base, Vector *toadd,
			 double factor, Vector *ret);
double crm114__vector_dist2(Vector *v1, Vector *v2);
void crm114__vector_convert_nonsparse_to_sparray(Vector *v, ExpandingArray *colMap);
void crm114__vector_print(Vector *v);
void crm114__vector_write_ns(Vector *v, char *filename);
void crm114__vector_write_ns_fp(Vector *v, FILE *out);
void crm114__vector_write_sp(Vector *v, char *filename);
void crm114__vector_write_sp_fp(Vector *v, FILE *out);
void crm114__vector_write_text_fp(char name[], Vector *v, FILE *fp);
size_t crm114__vector_write_bin(Vector *v, char *filename);
size_t crm114__vector_write_bin_fp(Vector *v, FILE *fp);
Vector *crm114__vector_read_text_fp(char expected_name[], FILE *fp);
Vector *crm114__vector_read_bin(char *filename);
Vector *crm114__vector_read_bin_fp(FILE *fp);
Vector *crm114__vector_map(void **addr, void *last_addr);
void *crm114__vector_memmove(Vector *to, Vector *from);
size_t crm114__vector_size(Vector *v);
void crm114__vector_free(Vector *v);

//inline'd functions defined in this file
//(inlining is compiler discretion)
static inline unsigned int vector_dim(Vector *v);
static inline int vector_num_elts(Vector *v);
static inline double norm2(Vector *v);
static inline double norm(Vector *v);
static inline double vector_dist(Vector *v1, Vector *v2) ;

//Vector iterator functions
void crm114__vectorit_zero_elt(VectorIterator *vit, Vector *v);
void crm114__vectorit_insert(VectorIterator *vit, unsigned int c, double d, Vector *v);
void crm114__vectorit_find(VectorIterator *vit, unsigned int c, Vector *v);
void crm114__vectorit_set_col(VectorIterator vit, unsigned int c, Vector *v);

//defined in this file - forced to be inline'd at high optimization
MY_INLINE void vectorit_set_at_beg(VectorIterator *vit, Vector *v);
MY_INLINE void vectorit_set_at_end(VectorIterator *vit, Vector *v);
MY_INLINE double vectorit_curr_val(VectorIterator vit, Vector *v);
MY_INLINE unsigned int vectorit_curr_col(VectorIterator vit, Vector *v);
MY_INLINE void vectorit_prev(VectorIterator *vit, Vector *v);
MY_INLINE void vectorit_next(VectorIterator *vit, Vector *v);
MY_INLINE int vectorit_past_end(VectorIterator vit, Vector *v);
MY_INLINE int vectorit_past_beg(VectorIterator vit, Vector *v);
MY_INLINE void vectorit_copy(VectorIterator from, VectorIterator *to);


/*************INLINE FUNCTION DEFINITIONS***********************************/


//Matrix
/*************************************************************************
 *Gets a pointer to a row of a matrix.
 *
 *INPUT: A: matrix from which to get a row.
 * r: row to get.
 *
 *OUTPUT: A pointer to row r of matrix M.
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(1)
 *************************************************************************/

static inline Vector *matr_get_row(Matrix *A, unsigned int r) {
  //yay, easy :)
  if (A && A->data && r < A->rows) {
    return A->data[r];
  }
  if (CRM114__MATR_DEBUG_MODE) {
    fprintf(stderr, "matr_get_row: bad arguments.\n");
  }
  return NULL;
}

/*************************************************************************
 *Sets an entry of a matrix.
 *
 *INPUT: M: matrix in which to set an entry.
 * r: row of the entry.
 * c: column of the entry.
 * d: value to set the entry to.
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY: d is non-zero = ammortized O(lg(S/R)), d = 0 = O(S/R)
 * SPARSE_LIST: O(S/R)
 *************************************************************************/

static inline void matr_set(Matrix *M, unsigned int r, unsigned int c,
			    double d) {
  int nz;
  if (!M || !M->data || r >= M->rows || !M->data[r]) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "matr_set: bad arguments.\n");
    }
    return;
  }
  nz = M->data[r]->nz;
  crm114__vector_set(M->data[r], c, d);
  M->nz += M->data[r]->nz - nz;
}

/*************************************************************************
 *Gets an entry of a matrix.
 *
 *INPUT: M: matrix from which to get an entry.
 * r: row of the entry.
 * c: column of the entry.
 *
 *OUTPUT: the value at r,c of matrix M
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY: O(lg(S/R))
 * SPARSE_LIST: O(S/R)
 *************************************************************************/

static inline double matr_get(Matrix *M, unsigned int r, unsigned int c) {
  if (!M || !M->data || r >= M->rows || !M->data[r]) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "matr_set: bad arguments.\n");
    }
    return 0;
  }
  return crm114__vector_get(M->data[r], c);
}

//Vector




/*************************************************************************
 *Dimension of a vector.
 *
 *INPUT: v: vector
 *
 *OUTPUT: The dimension (ie number of rows/columns) of v
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(1)
 *************************************************************************/

static inline unsigned int vector_dim(Vector *v) {
  if (!v) {
    return 0;
  }
  return v->dim;
}

/*************************************************************************
 *Number of elements of a vector.
 *
 *INPUT: v: vector
 *
 *OUTPUT: The dimension (ie number of rows/columns) of v if v is NON_SPARSE
 * or the number of non-zero elements of v if v is SPARSE
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(1)
 *************************************************************************/

static inline int vector_num_elts(Vector *v) {
  if (!v) {
    return 0;
  }
  return v->nz;
}


/*************************************************************************
 *Squared norm.  Note that finding square roots can be time consuming so
 *use this function to avoid that when possible.
 *
 *INPUT: v: vector to find the norm of
 *
 *OUTPUT: ||v||^2
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(s)
 * SPARSE_LIST: O(s)
 *************************************************************************/

static inline double norm2(Vector *v) {
  return crm114__dot(v, v);
}

/*************************************************************************
 *Norm of a vector.
 *
 *INPUT: v: vector to find the norm of
 *
 *OUTPUT: ||v||
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(s)
 * SPARSE_LIST: O(s)
 *************************************************************************/

static inline double norm(Vector *v) {
  return (sqrt(crm114__dot(v,v)));
}


/*************************************************************************
 *Distance between two vectors.
 *
 *INPUT: v1: first vector
 * v2: second vector
 *
 *OUTPUT: ||v1 - v2||
 *
 *TIME:
 * Both NON_SPARSE: O(c)
 * One NON_SPARSE, one SPARSE: O(s) + O(c)
 * Both SPARSE: O(s_1) + O(s_2)
 *************************************************************************/

static inline double vector_dist(Vector *v1, Vector *v2) {
  double d = crm114__vector_dist2(v1, v2);

  if (d > 0) {
    return sqrt(d);
  }

  return -1;
}


//Vector Iterator functions

/*************************************************************************
 *Set the iterator to the beginning of a vector.
 *
 *INPUT: v: vector to traverse from the beginning
 *
 *OUTPUT: vit is set to the beginning of vector v
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(1)
 *************************************************************************/

MY_INLINE void vectorit_set_at_beg(VectorIterator *vit, Vector *v) {

  if (!v || !vit) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "vectorit_set_at_beg: null arguments.\n");
    }
    if (vit) {
      vit->nscurr = -1;
    }
    return;
  }

  switch (v->type) {
  case NON_SPARSE:
    {
      vit->nscurr = 0;
      return;
    }
  case SPARSE_ARRAY:
    {
      //nscurr needs to be the actual index
      //in order that zero-ing an element in crm114__vector_add etc
      //doesn't mess things up
      if (!v->data.sparray) {
	vit->nscurr = 0;
      } else {
	vit->nscurr = v->data.sparray->first_elt;
      }
      return;
    }
  case SPARSE_LIST:
    {
      if (v->compact) {
	if (v->data.splist) {
	  vit->ccurr = (v->data.splist->head.compact);
	} else {
	  vit->ccurr = NULL;
	}
      } else {
	if (v->data.splist) {
	  vit->pcurr = (v->data.splist->head.precise);
	} else {
	  vit->pcurr = NULL;
	}
      }
      return;
    }
  default:
    {
      vit->nscurr = -1;
      return;
    }
  }
}


/*************************************************************************
 *Set the iterator to the end of a vector.
 *
 *INPUT: v: vector to traverse from the end
 *
 *OUTPUT: vit is set to the end of vector v
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(1)
 *************************************************************************/

MY_INLINE void vectorit_set_at_end(VectorIterator *vit, Vector *v) {

  if (!v || !vit) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "vectorit_set_at_end: null arguments.\n");
    }
    if (vit) {
      vit->nscurr = -1;
    }
    return;
  }

  switch (v->type) {
  case NON_SPARSE:
    {
      vit->nscurr = v->dim-1;
      break;
    }
  case SPARSE_ARRAY:
    {
      if (!v->data.sparray) {
	vit->nscurr = 0;
      } else {
	vit->nscurr = v->data.sparray->last_elt;
      }
      break;
    }
  case SPARSE_LIST:
    {
      if (v->compact) {
	if (!v->data.splist) {
	  vit->ccurr = NULL;
	} else {
	  vit->ccurr = v->data.splist->tail.compact;
	}
      } else {
	if (!v->data.splist) {
	  vit->pcurr = NULL;
	} else {
	  vit->pcurr = v->data.splist->tail.precise;
	}
      }
      break;
    }
  default:
    {
      vit->nscurr = -1;
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "vectorit_set_at_end: unrecognized type.\n");
      }
      return;
    }
  }
}

/*************************************************************************
 *Get the value (data) of the element that the iterator is pointing to.
 *
 *INPUT: vit: the vector iterator, pointing to some element in v
 * v: the vector vit is traversing
 *
 *OUTPUT: The data associated with the element vit is pointing to or
 * -RAND_MAX if vit is not traversing v.
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(1)
 *************************************************************************/

MY_INLINE double vectorit_curr_val(VectorIterator vit, Vector *v) {

  if (!v) {
    //if (CRM114__MATR_DEBUG_MODE) {
    //fprintf(stderr, "vectorit_curr_col: null vector.\n");
    //}
    return -RAND_MAX;
  }

  switch (v->type) {
  case SPARSE_ARRAY:
    if (v->data.sparray &&
	vit.nscurr >= v->data.sparray->first_elt &&
	vit.nscurr <= v->data.sparray->last_elt) {
      if (v->compact && v->data.sparray->data.compact) {
	return (double)v->data.sparray->data.compact[vit.nscurr].s.data;
      }
      if (!(v->compact) && (v->data.sparray->data.precise)) {
	return v->data.sparray->data.precise[vit.nscurr].s.data;
      }
    }
    return -RAND_MAX;
  case SPARSE_LIST:
    {
      if (v->compact && vit.ccurr) {
	return (double)vit.ccurr->data.data;
      }
      if (!(v->compact) && vit.pcurr) {
	return vit.pcurr->data.data;
      }
      return -RAND_MAX;
    }
  case NON_SPARSE:
    {
      if (vit.nscurr >= 0 && vit.nscurr < v->dim) {
	if (v->compact && v->data.nsarray.compact) {
	  return (double)v->data.nsarray.compact[vit.nscurr];
	}
	if (!(v->compact) && v->data.nsarray.precise) {
	  return v->data.nsarray.precise[vit.nscurr];
	}
      }
      return -RAND_MAX;
    }
  default:
    {
      return -RAND_MAX;
    }
  }

  return -RAND_MAX;

}

/*************************************************************************
 *Get the column of the element that the iterator is pointing to.
 *
 *INPUT: vit: the vector iterator, pointing to some element in v
 * v: the vector vit is traversing
 *
 *OUTPUT: The column associated with the element vit is pointing to or v->dim
 * if vit is past the beginning or end of v.
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(1)
 *
 *WARNINGS:
 *1) This returns v->dim even if vit is past the BEGINNING of v.  This is
 *   because column numbers need to be unsigned.  Therefore, you need
 *   to explicitly check if an iterator is past the beginning of a vector
 *   - you can't count on this returning a negative value if the iterator
 *   is past the beginning.
 *************************************************************************/

MY_INLINE unsigned int vectorit_curr_col(VectorIterator vit, Vector *v) {

  if (!v) {
    if (CRM114__MATR_DEBUG_MODE) {
    fprintf(stderr, "vectorit_curr_col: null vector.\n");
    }
    return MAX_UINT_VAL;
  }

  switch (v->type) {
  case SPARSE_ARRAY:
    if (v->data.sparray && v->data.sparray->data.compact &&
	vit.nscurr >= v->data.sparray->first_elt &&
	vit.nscurr <= v->data.sparray->last_elt) {
      if (v->compact && v->data.sparray->data.compact) {
	return v->data.sparray->data.compact[vit.nscurr].s.col;
      }
      if (!(v->compact) && (v->data.sparray->data.precise)) {
	return v->data.sparray->data.precise[vit.nscurr].s.col;
      }
    }
    return v->dim;
  case SPARSE_LIST:
    {
      if (v->compact && vit.ccurr) {
	return vit.ccurr->data.col;
      }
      if (!(v->compact) && vit.pcurr) {
	return vit.pcurr->data.col;
      }
      return v->dim;
    }
  case NON_SPARSE:
    {
      return vit.nscurr;
    }
  default:
    {
      return v->dim;
    }
  }

  return v->dim;

}


/*************************************************************************
 *Move to the previous element in the vector.  Sets past_beg and unsets
 *past_end if appropriate.
 *
 *INPUT: v: the vector vit is traversing
 *
 *OUTPUT: vit points to the previous element in the vector or is past_beg.
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(1)
 *************************************************************************/

MY_INLINE void vectorit_prev(VectorIterator *vit, Vector *v) {

  if (!v || !vit) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "vectorit_prev: null arguments.\n");
    }
    if (vit) {
      vit->nscurr = -1;
    }
    return;
  }

  switch (v->type) {
  case NON_SPARSE:
  case SPARSE_ARRAY:
    vit->nscurr--;
    return;
  case SPARSE_LIST:
    if ((v->compact)) {
      if (vit->ccurr) {
	vit->ccurr = vit->ccurr->prev;
      } else {
	vit->ccurr = v->data.splist->tail.compact;
      }
    } else {
      if (vit->pcurr) {
	vit->pcurr = vit->pcurr->prev;
      } else {
	vit->pcurr = v->data.splist->tail.precise;
      }

    }
  default:
    return;
  }
}

/*************************************************************************
 *Move to the next element in the vector.
 *
 *INPUT: v: the vector vit is traversing
 *
 *OUTPUT: vit points to the next element in the vector or is past_end.
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(1)
 *************************************************************************/

MY_INLINE void vectorit_next(VectorIterator *vit, Vector *v) {
  if (!v || !vit) {
    //if (CRM114__MATR_DEBUG_MODE) {
    //fprintf(stderr, "vectorit_next: null arguments.\n");
    //}
    return;
  }

  switch (v->type) {
  case NON_SPARSE:
  case SPARSE_ARRAY:
    vit->nscurr++;
    return;
  case SPARSE_LIST:
    if ((v->compact)) {
      if (vit->ccurr) {
	vit->ccurr = vit->ccurr->next;
      } else {
	vit->ccurr = v->data.splist->head.compact;
      }
    } else {
      if (vit->pcurr) {
	vit->pcurr = vit->pcurr->next;
      } else {
	vit->pcurr = v->data.splist->head.precise;
      }

    }
  default:
    return;
  }
}


/*************************************************************************
 *Checks if an iterator is past the end of the vector.
 *
 *INPUT: vit: the iterator
 * v: the vector vit is traversing
 *
 *OUTPUT: 1 if vit is past the end of v, 0 else
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(1)
 *************************************************************************/

MY_INLINE int vectorit_past_end(VectorIterator vit, Vector *v) {
  if (!v) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "vectorit_past_end: null arguments.\n");
    }
    return 1;
  }

  switch (v->type) {
  case SPARSE_ARRAY:
    if (!v->data.sparray || vit.nscurr > v->data.sparray->last_elt ||
	v->data.sparray->first_elt > v->data.sparray->last_elt) {
      return 1;
    }
    return 0;
  case SPARSE_LIST:
    if (v->compact && !vit.ccurr) {
      return 1;
    }
    if (!(v->compact) && !vit.pcurr) {
      return 1;
    }
    return 0;
  case NON_SPARSE:
    if (vit.nscurr >= v->dim) {
      return 1;
    }
    return 0;
  default:
    return 1;
  }
}

/*************************************************************************
 *Checks if an iterator is past the beginning of the vector.
 *
 *INPUT: vit: the iterator
 * v: the vector vit is traversing
 *
 *OUTPUT: 1 if vit is past the beginning of v, 0 else
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(1)
 *************************************************************************/

MY_INLINE int vectorit_past_beg(VectorIterator vit, Vector *v) {
  if (!v) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "vectorit_past_end: null arguments.\n");
    }
    return 1;
  }

  switch (v->type) {
  case SPARSE_ARRAY:
    if (!(v->data.sparray) || vit.nscurr < v->data.sparray->first_elt
	|| v->data.sparray->first_elt > v->data.sparray->last_elt) {
      return 1;
    }
    return 0;
  case SPARSE_LIST:
    if (v->compact && !vit.ccurr) {
      return 1;
    }
    if (!(v->compact) && !vit.pcurr) {
      return 1;
    }
    return 0;
  case NON_SPARSE:
    if (vit.nscurr < 0) {
      return 1;
    }
    return 0;
  default:
    return 1;
  }
}

/*************************************************************************
 *Copy one vector iterator to another.
 *
 *INPUT: from: vector iterator to copy from
 *
 *OUTPUT: to = from.
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(1)
 *************************************************************************/

MY_INLINE void vectorit_copy(VectorIterator from, VectorIterator *to) {
  if (!to) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "vectorit_copy: null to vector.\n");
    }
    return;
  }
  to->nscurr = from.nscurr;
}

#endif // !defined(__CRM114_MATRIX_H__)
