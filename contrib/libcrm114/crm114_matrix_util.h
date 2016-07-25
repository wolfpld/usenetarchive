//	crm114_matrix_util.h - Support Vector Machine

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

#ifndef __CRM114_MATRIX_UTIL_H__
#define __CRM114_MATRIX_UTIL_H__

//the libraries we'll want everywhere

#include "crm114_sysincludes.h"

/**********************************************************************
 *A utility library that defines some data structures, specifically an
 *expanding array and a linked list.  This library can be used with
 *the matrix library or on its own.  The expanding array is general, but
 *the linked list would need the union expanded to be general.
 *
 *The functions in util.c are commented.  See them for details.
 *********************************************************************/

#define MATRIX_EPSILON 1e-10           //approximation to 0
                                   //using doubles, 1e-10 works well enough
#define MATRIX_INF 1.0/MATRIX_EPSILON    //likewise, an approximation to infinity
#define MAX_UINT_VAL UINT_MAX
#define MATRIX_SORT_MAX_ALLOC_SIZE 4e8    //Maximum amount of memory to allocate in
                                  //one chunk (used for counting sort)

//these are debug settings
//the higher the setting, the more output you get
//these are cummulative so that level QP_DEBUG prints everything
//from level SOLVER_DEBUG and QP_DEBUG

#define MATR_DEBUG 1


#define MATR_OPS 6              //information about the matrix operations

#define MATR_OPS_MORE 7         //even more information about the matrix
                                //operations (ie printing out intermediate
                                //results for crm114__vector_add, etc)

#ifdef DO_INLINES
#define MY_INLINE __attribute__((always_inline)) static inline
#else
#define MY_INLINE static inline
#endif

int CRM114__MATR_DEBUG_MODE;            //the debug value
                               //for SVM, if internal trace is on
                               //CRM114__MATR_DEBUG_MODE = SVM_INTERNAL_TRACE_LEVEL
                               //for PCA, if internal trace is on
                               //CRM114__MATR_DEBUG_MODE = PCA_INTERNAL_TRACE_LEVEL

//Sparse elements
typedef struct {
  unsigned int col;
  double data; //it's worth it to use doubles for higher precision.  really.
} PreciseSparseElement;

typedef struct {
  unsigned int col;
  int data; //small data!
} CompactSparseElement;

typedef union {
  PreciseSparseElement *precise;
  CompactSparseElement *compact;
} SparseElement;


//Types that can go in an expanding array
typedef union {
  int i;
  long l;
  float f;
  double d;
  PreciseSparseElement s;
} PreciseExpandingType;

typedef union {
  unsigned int i;
  CompactSparseElement s;
} CompactExpandingType;

typedef union {
  PreciseExpandingType *precise;
  CompactExpandingType *compact;
} ExpandingType;

//Expanding array struct
typedef struct {
  ExpandingType data; //Actual data
  int length,         //Current size of the array
    last_elt,         //Location of the last data
    first_elt,        //Location of the first data
    n_elts,           //Number of elements = last_elt - first_elt
    compact,          //1 for compactness
    was_mapped;       //1 if the data was mapped into memory
} ExpandingArray;

//Elements of a linked list
typedef struct precise_mythical_node {
  PreciseSparseElement data;
  struct precise_mythical_node *next, *prev;
} PreciseSparseNode;

typedef struct compact_mythical_node {
  CompactSparseElement data;
  struct compact_mythical_node *next, *prev;
} CompactSparseNode;

typedef struct {
  PreciseSparseNode *precise;
  CompactSparseNode *compact;
  int is_compact;
} SparseNode;

//Linked list struct
typedef struct {
  SparseNode head, tail;
  int compact;
  void *last_addr;
} SparseElementList;

//Expanding array functions
ExpandingArray *crm114__make_expanding_array(int init_size, int compact);
void crm114__expanding_array_insert(ExpandingType d, ExpandingArray *A);
void crm114__expanding_array_set(ExpandingType d, int c, ExpandingArray *A);
ExpandingType crm114__expanding_array_get(int c, ExpandingArray *A);
void crm114__expanding_array_trim(ExpandingArray *A);
int crm114__expanding_array_search(unsigned int c, int init, ExpandingArray *A);
int crm114__expanding_array_insert_before(ExpandingType ne, int before,
				   ExpandingArray *A);
int crm114__expanding_array_insert_after(ExpandingType ne, int after,
				  ExpandingArray *A);
void crm114__expanding_array_clear(ExpandingArray *A);
void crm114__expanding_array_remove_elt(int elt, ExpandingArray *A);
size_t crm114__expanding_array_write(ExpandingArray *A, FILE *fp);
void crm114__expanding_array_read(ExpandingArray *A, FILE *fp);
ExpandingArray *crm114__expanding_array_map(void **addr, void *last_addr);
void crm114__expanding_array_free_data(ExpandingArray *A);
void crm114__expanding_array_free(ExpandingArray *A);

//SparseElementList functions
SparseElementList *crm114__make_list(int compact);
SparseNode crm114__list_search(unsigned int c, SparseNode init, SparseElementList *l);
SparseNode crm114__list_insert_before(SparseElement newelt, SparseNode before,
			SparseElementList *l);
SparseNode crm114__list_insert_after(SparseElement ne, SparseNode after,
		       SparseElementList *l);
void crm114__list_clear(SparseElementList *l);
void crm114__list_remove_elt(SparseElementList *l, SparseNode toremove);
int crm114__list_is_empty(SparseElementList *l);
size_t crm114__list_write(SparseElementList *l, FILE *fp);
int crm114__list_read(SparseElementList *l, FILE *fp, int n_elts);
SparseElementList *crm114__list_map(void **addr, void *last_addr, int *n_elts_ptr);
void *crm114__list_memmove(SparseElementList *to, SparseElementList *from);

//Sparse Nodes
MY_INLINE SparseNode make_null_node(int compact);
MY_INLINE int null_node(SparseNode n);
MY_INLINE double node_data(SparseNode n);
MY_INLINE unsigned int node_col(SparseNode n);
MY_INLINE SparseNode next_node(SparseNode n);
MY_INLINE SparseNode prev_node(SparseNode n);
MY_INLINE void node_set_data(SparseNode n, double d);
MY_INLINE void node_set_col(SparseNode n, unsigned int c);
MY_INLINE void node_free(SparseNode n);

//Comparator functions for QSort
int crm114__compact_expanding_type_int_compare(const void *a, const void *b);
int crm114__precise_sparse_element_val_compare(const void *a, const void *b);
int crm114__precise_sparse_element_col_compare(const void *a, const void *b);


/***********************Sparse Node Functions***************************/

//return a node with the correct compactness and
//the appropriate pointer null
MY_INLINE SparseNode make_null_node(int compact)
{
  SparseNode n;

  n.is_compact = compact;
  n.compact = NULL;
  n.precise = NULL;

  if (compact) {
    n.compact = NULL;
  } else {
    n.precise = NULL;
  }

  return n;
}

//returns 1 if the pointer with the correct compactness
//is null
MY_INLINE int null_node(SparseNode n)
{
  if (n.is_compact) {
    return (n.compact == NULL);
  }
  return (n.precise == NULL);
}

//returns the data associated with n
MY_INLINE double node_data(SparseNode n)
{
  if (null_node(n)) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "node_data: null node.\n");
    }
    return -RAND_MAX;
  }

  if (n.is_compact) {
    return (double)n.compact->data.data;
  }
  return n.precise->data.data;
}

//returns the column number associated with n
MY_INLINE unsigned int node_col(SparseNode n)
{
  if ((n.is_compact && !(n.compact)) || (!(n.is_compact) && !(n.precise))) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "node_col: null node.\n");
    }
    return MAX_UINT_VAL;
  }

  if (n.is_compact && n.compact) {
    return n.compact->data.col;
  }

  return n.precise->data.col;
}

//returns a pointer to the node after
//the one n points to
MY_INLINE SparseNode next_node(SparseNode n)
{
  SparseNode ret;

  ret.is_compact = n.is_compact;
  ret.compact = NULL;
  ret.precise = NULL;

  if (null_node(n)) {
    return make_null_node(n.is_compact);
  }
  if (n.is_compact) {
    ret.compact = n.compact->next;
  } else {
    ret.precise = n.precise->next;
  }
  return ret;
}

//returns a pointer to the node before
//the one n points to
MY_INLINE SparseNode prev_node(SparseNode n)
{
  SparseNode ret;

  ret.is_compact = n.is_compact;
  ret.compact = NULL;
  ret.precise = NULL;

  if (null_node(n)) {
    return make_null_node(n.is_compact);
  }

  if (n.is_compact) {
    ret.compact = n.compact->prev;
  } else {
    ret.precise = n.precise->prev;
  }
  return ret;
}

//sets the data associated with node n to be d
MY_INLINE void node_set_data(SparseNode n, double d)
{
  if (null_node(n)) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "node_set_data: null node.\n");
    }
    return;
  }
  if (n.is_compact) {
    n.compact->data.data = (int)d;
  } else {
    n.precise->data.data = d;
  }
}

//sets the column associated with node n to be c
MY_INLINE void node_set_col(SparseNode n, unsigned int c)
{
  if (null_node(n)) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "node_set_col: null node.\n");
    }
    return;
  }
  if (n.is_compact) {
    n.compact->data.col = c;
  } else {
    n.precise->data.col = c;
  }
}

//frees the pointer that n has
//taking into account compactness
MY_INLINE void node_free(SparseNode n) {
  if (null_node(n)) {
    return;
  }
  if (n.is_compact) {
    free(n.compact);
  } else {
    free(n.precise);
  }
}

#endif // !defined(__CRM114_MATRIX_UTIL_H__)
