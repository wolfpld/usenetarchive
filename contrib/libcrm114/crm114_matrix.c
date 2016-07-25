//	crm114_matrix.c - Support Vector Machine

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

#include "crm114_matrix.h"
#include "crm114_matrix_util.h"

/*****************************************************************************
 *This is a matrix/vector library.
 *The intent of it is to keep all matrix operations out of the algorithms.
 *Therefore, sparse and full matrices are handled transparently inside these
 *functions.  In other words, once a matrix is declared sparse or full, you
 *can call any matrix operation on it and the operation will do the best
 *thing for the type of matrix.  If this were C++ (which, sadly, it is not)
 *matrix would be an abstract class with sparse and full implementing classes.
 *
 *Specifically there are 3 types of data structures for matrices currently
 *supported.  For an R x C matrix with S non-zero elements, these are:
 *
 *NON_SPARSE: The matrix is represented as a set of R pointers to arrays
 * of length C.  If more columns are added to the matrix, the array is
 * expanded to exactly the number of columns necessary.  The representation
 * is not sparse so that the data in the cth spot of the array is assumed
 * to be the data in the cth column of that row.  All data in a NON_SPARSE
 * matrix is accessible in O(1) time.
 *
 *SPARSE_ARRAY: The matrix is represented as a set of R pointers to arrays
 * of approximately length S/R.  Each element of the array stores a column
 * number and data element.  The arrays are arranged in order of increasing
 * column number.  When more non-zero elements are added to a row than the
 * array can hold, its size is doubled.  This is a sparse representation so
 * that any columns not mentioned in a row are assumed to be zero.  All data
 * in a SPARSE_ARRAY matrix is accessible in O(lg(C)) time.
 *
 *SPARSE_LIST: The matrix is represented as a set of R pointers to linked
 * lists.  Each element in the linked list stores a column number, data entry,
 * and pointers to its previous and next elements.  When a new non-zero
 * element is added, only one more node of the list is created.  This is a
 * sparse representation so that any columns not mentioned in a row are
 * assumed to be zero.  All data in a SPARSE_LIST matrix is accessible in
 * O(C) time.
 *
 *In addition, the type of data stored in a matrix is variable.  All columns
 *are assumed to be unsigned ints, but data can either be an int or a double.
 *The data type is specified by setting the MATR_COMPACT flag:
 *
 *MATR_COMPACT: A matrix set to be MATR_COMPACT contains integer data.
 * This matrix will take up less space than the equivalent MATR_PRECISE matrix.
 *
 *MATR_PRECISE: A matrix set to be MATR_PRECISE contains double data.
 * This matrix will take more space than the equivalent MATR_COMPACT matrix.
 *
 *Which representation to use depends very much on the application.  All of
 *the representations work together (ie you can add a SPARSE_ARRAY and a
 *SPARSE_LIST without a problem) so you can have many different types in
 *one application.

 *Time considerations:
 *All functions in this file should be commented with their big-oh running
 *time for the three different representations.  In general, though, very
 *sparse matrices that will change the number of non-zero elements often
 *are best represented as SPARSE_LISTS, while more static sparse matrices
 *should be SPARSE_ARRAYs.
 *
 *Space considerations (small considerations are left out):
 *
 *MATR_COMPACT, NON_SPARSE: Requires 4*R*C + 32*R bytes.
 *
 *MATR_PRECISE, NON_SPARSE: Requires 8*R*C + 32*R bytes.
 *
 *MATR_COMPACT, SPARSE_ARRAY: Requires up to 2*8*S + 64*R bytes (though usually
 * this is closer to 8*S bytes unless you get very unlucky in how the arrays
 * double in size.  Setting MATR_DEFAULT_VECTOR_SIZE or passing in the starting
 * size of the array can keep this relatively small).
 *
 *MATR_PRECISE, SPARSE_ARRAY: Requires up to 2*16*S + 64*R bytes (on this computer
 * anyway, a 12-byte struct is scaled up to 16 bytes).
 *
 *MATR_COMPACT, SPARSE_LIST: Requires 24*S + 88*R bytes.
 *
 *MATR_PRECISE, SPARSE_LIST: Requires 32*S + 88*R bytes
 ****************************************************************************/

//Static matrix function decalartions
static void matr_decrease_rows(Matrix *M, unsigned int r, int free_row);
static ExpandingArray *matr_remove_zero_cols_sort(Matrix *X,
						  SortingAlgorithms sorttype);
static void matr_write_sp(Matrix *M, FILE *out);
static void matr_write_ns(Matrix *M, FILE *out);

/*************************************************************************
 *Makes a zero matrix.
 *
 *INPUT: rows: number of rows in the matrix
 * cols: number of columns in the matrix
 * type: NON_SPARSE, SPARSE_ARRAY, or SPARSE_LIST specifying the data
 *  structure
 * compact: MATR_COMPACT or MATR_PRECISE specifying whether data is stored
 *  as an int or a double
 *
 *OUTPUT: A rows X cols matrix of all zeros with the type and compact
 * flags set correctly.  If the matrix is a sparse array, the arrays
 * will begin at size MATR_DEFAULT_VECTOR_SIZE.
 *
 *TIME:
 * NON_SPARSE: O(R*C)
 * SPARSE_ARRAY: O(R)
 * SPARSE_LIST: O(R)
 *************************************************************************/

Matrix *crm114__matr_make(unsigned int rows, unsigned int cols, VectorType type,
		  int compact) {
  return crm114__matr_make_size(rows, cols, type, compact, MATR_DEFAULT_VECTOR_SIZE);
}

/*************************************************************************
 *Makes a zero matrix.
 *
 *INPUT: rows: number of rows in the matrix
 * cols: number of columns in the matrix
 * type: NON_SPARSE, SPARSE_ARRAY, or SPARSE_LIST specifying the data
 *  structure
 * compact: MATR_COMPACT or MATR_PRECISE specifying whether data is stored as an
 *  int or a double
 * size: The starting size of the array for the SPARSE_ARRAY data type.
 *
 *OUTPUT: A rows X cols matrix of all zeros with the type and compact
 * flags set correctly.  If the matrix is a sparse array, the arrays
 * will begin at size MATR_DEFAULT_VECTOR_SIZE.
 *
 *TIME:
 * NON_SPARSE: O(R*C)
 * SPARSE_ARRAY: O(R)
 * SPARSE_LIST: O(R)
 *************************************************************************/

Matrix *crm114__matr_make_size(unsigned int rows, unsigned int cols, VectorType type,
		       int compact, int size) {
  Matrix *M;
  unsigned int i;

  M = (Matrix *)malloc(sizeof(Matrix));
  if (!M) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "Unable to allocate memory for matrix.\n");
    }
    return NULL;
  }
  M->rows = rows;
  M->cols = cols;
  M->type = type;
  M->compact = compact;
  M->size = size;
  M->was_mapped = 0;
  switch(type) {
  case NON_SPARSE:
    M->nz = rows*cols;
    break;
  case SPARSE_ARRAY:
    M->nz = 0;
    break;
  case SPARSE_LIST:
    M->nz = 0;
    break;
  default:
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_make: unrecognized type.\n");
    }
    free(M);
    return NULL;
  }

  if (rows > 0) {
    M->data = (Vector **)malloc(sizeof(Vector *)*rows);
    if (!M->data) {
      rows = 0;
    }
    for (i = 0; i < rows; i++) {
      //it would be nice if there were some way to make this
      //contiguous in memory
      //but i'm not sure how
      M->data[i] = crm114__vector_make_size(cols, type, compact, size);
      if (!M->data[i]) {
	//something went wrong
	break;
      }
    }
    if (i != M->rows) {
      M->rows = i;
      crm114__matr_free(M);
      return NULL;
    }
  } else {
    M->data = NULL;
  }
  return M;
}

/*************************************************************************
 *Sets a row of the matrix.
 *
 *INPUT: A: matrix in which to set a row.
 * r: row to set.
 * v: pointer to new row.
 *
 *TIME:
 * NON_SPARSE: O(C)
 * SPARSE_ARRAY: O(S/R)
 * SPARSE_LIST: O(S/R)
 *
 *WARNINGS:
 *1) This does NOT free the old row of A in case it is still in use
 *   somewhere.  It is up to you to do that.
 *************************************************************************/

void crm114__matr_set_row(Matrix *A, unsigned int r, Vector *v) {
  int oldnz;

  if (!A || !A->data || r >= A->rows || !A->data[r]) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_set_row: bad arguments.\n");
    }
    return;
  }
  oldnz = A->data[r]->nz;
  crm114__vector_copy(v, A->data[r]);
  A->nz += A->data[r]->nz - oldnz;
}

/*************************************************************************
 *Does a shallow copy to set the rth row of M to be v.  If M has fewer than
 *r rows, enough rows are added.  Note that this is a SHALLOW copy - M does
 *not create additional space for row r - it just sets M->data[r] = v.
 *
 *INPUT: M: matrix in which to set a row.
 * r: row to set.
 * v: pointer to new row.
 *
 *TIME: (if r = M->rows + t)
 * NON_SPARSE: O(t*C)
 * SPARSE_ARRAY: O(t)
 * SPARSE_LIST: O(t)
 *
 *WARNINGS:
 *1) M and v must be the same type.
 *2) When this is done M->data[r] and v have the SAME value.  Freeing one
 *   will free the other (for example).
 *************************************************************************/

void crm114__matr_shallow_row_copy(Matrix *M, unsigned int r, Vector *v) {
  int oldrows, i;
  if (!v || !M) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_shallow_row_copy: bad arguments.\n");
    }
    return;
  }

  if (v->type != M->type) {
    //this can be bad if v is sparse and M is non-sparse
    //since we try to set M->cols = v->dim and v->dim might be HUGE
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf
	(stderr,
	 "Attempt to do shallow row copy between different vector types.\n");
    }
    return;
  }

  oldrows = M->rows;

  if (r >= M->rows) {
    //add a row or 3
    M->data = (Vector **)realloc(M->data, sizeof(Vector *)*(r+1));
    if (!M->data) {
      //oh, oh something is really wrong
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "Unable to grow M in shallow_row_copy.\n");
      }
      M->rows = 0;
      return;
    }
    M->rows = r+1;
  }

  for (i = oldrows; i < M->rows; i++) {
    M->data[i] = crm114__vector_make_size(M->cols, M->type, M->compact, M->size);
  }

  if (v->dim > M->cols) {
    crm114__matr_add_ncols(M, v->dim - M->cols);
  }

  crm114__vector_free(M->data[r]);
  M->data[r] = v;
  v->dim = M->cols;
  M->nz += v->nz;
}



/*************************************************************************
 *Sets a column of the matrix.
 *
 *INPUT: A: matrix in which to set a column.
 * r: column to set.
 * v: pointer to new column.
 *
 *TIME:
 * NON_SPARSE: O(R)
 * SPARSE_ARRAY:
 *  Generally: O(R*lg(S/R)) (few zeros in v) O(S) (many zeros in v)
 *  First/Last column: O(R)
 * SPARSE_LIST:
 *  Generally: O(S)
 *  First/Last column: O(R)
 *************************************************************************/

void crm114__matr_set_col(Matrix *A, unsigned int c, Vector *v) {
  int oldnz;
  int i, col, lastcol = -1;
  VectorIterator vit;

  if (!v || !A || !A->data || c >= A->cols) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_set_col: bad arguments.\n");
    }
    return;
  }

  if (v->dim != A->rows) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_set_col: dimension mismatch.\n");
    }
    return;
  }

  if (CRM114__MATR_DEBUG_MODE >= MATR_OPS) {
    fprintf(stderr, "setting column %d of \n", c);
    crm114__matr_print(A);
    fprintf(stderr, "to be\n");
    crm114__vector_print(v);
  }

  vectorit_set_at_beg(&vit, v);
  while (!vectorit_past_end(vit, v)) {
    col = vectorit_curr_col(vit, v);
    for (i = lastcol+1; i < col; i++) {
      oldnz = A->data[i]->nz;
      crm114__vector_set(A->data[i], c, 0);
      A->nz += A->data[i]->nz - oldnz;
    }
    oldnz = A->data[col]->nz;
    crm114__vector_set(A->data[col], c, vectorit_curr_val(vit, v));
    A->nz += A->data[vectorit_curr_col(vit, v)]->nz - oldnz;
    lastcol = col;
    vectorit_next(&vit, v);
  }
  for (i = lastcol+1; i < A->rows; i++) {
    oldnz = A->data[i]->nz;
    crm114__vector_set(A->data[i], c, 0);
    A->nz += A->data[i]->nz - oldnz;
  }
}

/*************************************************************************
 *Adds a row to the bottom of the matrix.
 *
 *INPUT: M: matrix to which to add a row.
 *
 *TIME:
 * NON_SPARSE: O(C) (if realloc succeeds) O(R) + O(C) (if realloc fails)
 * SPARSE_ARRAY: O(1) (if realloc succeds) O(R) (if realloc fails)
 * SPARSE_LIST: O(1) (if realloc succeeds) O(R) (if realloc fails)
 *************************************************************************/

void crm114__matr_add_row(Matrix *M) {

  if (!M) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_add_row: null matrix.\n");
    }
    return;
  }

  //reallocate the memory for M
  M->data = (Vector **)realloc(M->data, sizeof(Vector *)*(M->rows+1));
  if (!M->data) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "Unable to add more rows to matrix.\n");
    }
    M->rows = 0;
    M->nz = 0;
    return;
  }
  M->data[M->rows] = crm114__vector_make_size(M->cols, M->type, M->compact, M->size);
  M->rows++;
}

/*************************************************************************
 *Adds n rows to the bottom of the matrix.
 *
 *INPUT: M: matrix to which to add rows.
 * n: number of rows to add
 *
 *TIME:
 * NON_SPARSE: O(n*C) (if realloc succeeds) O(R) + O(n*C) (if realloc fails)
 * SPARSE_ARRAY: O(n) (if realloc succeds) O(R) + O(n) (if realloc fails)
 * SPARSE_LIST: O(n) (if realloc succeeds) O(R) + O(n) (if realloc fails)
 *************************************************************************/

void crm114__matr_add_nrows(Matrix *M, unsigned int n) {
  unsigned int i;

  if (!M || n <= 0) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_add_nrows: bad arguments.\n");
    }
    return;
  }

  M->data = (Vector **)realloc(M->data, sizeof(Vector *)*(M->rows+n));
  if (!M->data) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "Unable to add more rows to matrix.\n");
    }
    M->rows = 0;
    M->nz = 0;
    return;
  }
  for (i = M->rows; i < M->rows+n; i++) {
    M->data[i] = crm114__vector_make_size(M->cols, M->type, M->compact, M->size);
  }
  M->rows += n;
}

/*************************************************************************
 *Adds a column to the right of the matrix.
 *
 *INPUT: M: matrix to which to add columns.
 *
 *TIME:
 * NON_SPARSE: O(R) (if realloc succeeds often) O(R*C) (if realloc fails often)
 * SPARSE_ARRAY: O(R)
 * SPARSE_LIST: O(R)
 *************************************************************************/

void crm114__matr_add_col(Matrix *M) {
  unsigned int i, j;

  if (!M) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_add_col: null matrix.\n");
    }
    return;
  }

  if (M->data) {
    for (i = 0; i < M->rows; i++) {
      crm114__vector_add_col(M->data[i]);
      if (!M->data[i]) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "Unable to add more columns to matrix.\n");
	}
	for (j = 0; j < i; j++) {
	  crm114__vector_free(M->data[j]);
	}
	for (j = i+1; j < M->rows; j++) {
	  crm114__vector_free(M->data[j]);
	}
	free(M->data);
	M->data = NULL;
	M->cols = 0;
	M->nz = 0;
	return;
      }
    }
  }

  M->cols++;
}

/*************************************************************************
 *Adds n columns to the right of the matrix.
 *
 *INPUT: M: matrix to which to add columns.
 * n: number of columns to add
 *
 *TIME:
 * NON_SPARSE: O(n*R) (if realloc succeeds often) O(n*R*C) (if realloc fails)
 * SPARSE_ARRAY: O(R)
 * SPARSE_LIST: O(R)
 *************************************************************************/

void crm114__matr_add_ncols(Matrix *M, unsigned int n) {
  unsigned int i, j;

  if (!M || n <= 0) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_add_ncols: bad arguments.\n");
    }
    return;
  }

  if (M->data) {
    for (i = 0; i < M->rows; i++) {
      crm114__vector_add_ncols(M->data[i], n);
      if (!M->data[i]) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "Unable to add more columns to matrix.\n");
	}
	for (j = 0; j < i; j++) {
	  crm114__vector_free(M->data[j]);
	}
	for (j = i+1; j < M->rows; j++) {
	  crm114__vector_free(M->data[j]);
	}
	free(M->data);
	M->data = NULL;
	M->cols = 0;
	M->nz = 0;
	return;
      }
    }
  }
  M->cols += n;
}

/*************************************************************************
 *Removes a row from the matrix and frees the row.
 *
 *INPUT: M: matrix from which to remove a row.
 * r: row to remove
 *
 *TIME:
 * NON_SPARSE: O(R)
 * SPARSE_ARRAY: O(R)
 * SPARSE_LIST: O(R) + O(S/R)
 *************************************************************************/

void crm114__matr_remove_row(Matrix *M, unsigned int r) {
  matr_decrease_rows(M, r, 1);
}

/*************************************************************************
 *Removes a row from the matrix and DOES NOT free the row.
 *
 *INPUT: M: matrix from which to remove a row.
 * r: row to remove
 *
 *TIME:
 * NON_SPARSE: O(R)
 * SPARSE_ARRAY: O(R)
 * SPARSE_LIST: O(R) + O(S/R)
 *************************************************************************/

void crm114__matr_erase_row(Matrix *M, unsigned int r) {
  matr_decrease_rows(M, r, 0);
}

//private function to actually do the work of removing a row
//and erasing if (if desired)
static void matr_decrease_rows(Matrix *M, unsigned int r, int free_row) {
  Vector *tptr = NULL;
  unsigned int i;

  if (!M || !M->data) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "matr_decrease_rows: null matrix.\n");
    }
    return;
  }

  if (r >= M->rows) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr,
	      "matr_decrease_rows: attempt to remove non-existant row.\n");
    }
    return;
  }

  if (M->rows == 0) {
    return;
  }

  if (M->rows == 1) {
    if (M->data) {
      if (free_row) {
	crm114__vector_free(M->data[0]);
      }
      free(M->data);
      M->data = NULL;
    }
    M->rows = 0;
    return;
  }

  if (M->data[r]) {
    M->nz -= M->data[r]->nz;
  }

  if (r < M->rows-1) {
    tptr = M->data[M->rows-1];
  } else if (free_row) {
    crm114__vector_free(M->data[r]);
  }
  M->data = (Vector **)realloc(M->data, sizeof(Vector *)*(M->rows-1));
  if (!M->data) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "Unable to reduce rows of matrix.\n");
    }
    M->rows = 0;
    M->nz = 0;
    return;
  }
  if (r < M->rows-1 && free_row) {
    crm114__vector_free(M->data[r]);
  }
  for (i = r; i < M->rows-2; i++) {
    M->data[i] = M->data[i+1];
  }
  if (r < M->rows-1) {
    M->data[M->rows-2] = tptr;
  }
  M->rows--;
}

/*************************************************************************
 *Removes a column from the matrix.
 *
 *INPUT: M: matrix from which to remove a column.
 * c: column to remove
 *
 *TIME:
 * NON_SPARSE: O(R)
 * SPARSE_ARRAY: O(R*lg(S/R))
 * SPARSE_LIST: O(S)
 *************************************************************************/

void crm114__matr_remove_col(Matrix *M, unsigned int c) {
  unsigned int i, j;
  int oldnz;

  if (!M || !M->data || M->cols == 0) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_remove_col: null matrix.\n");
    }
    return;
  }

  if (c >= M->cols) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr,
	      "crm114__matr_remove_col: attempt to remove non-existent column.\n");
    }
    return;
  }


  //it would be much faster if we didn't need to preserve the order of the
  //columns
  //but we do
  for (i = 0; i < M->rows; i++) {
    if (!M->data[i]) {
      //??
      continue;
    }
    oldnz = M->data[i]->nz;
    crm114__vector_remove_col(M->data[i], c);
    if (!M->data[i]) {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "Unable to remove columns from matrix.\n");
      }
      for (j = 0; j < i; j++) {
	crm114__vector_free(M->data[j]);
      }
      for (j = i+1; j < M->rows; j++) {
	crm114__vector_free(M->data[j]);
      }
      free(M->data);
      M->data = NULL;
      M->cols = 0;
      M->nz = 0;
      return;
    }
    M->nz += M->data[i]->nz - oldnz;
  }
  M->cols--;
}

/*************************************************************************
 *Removes rows that are all zeros from the matrix.
 *
 *INPUT: M: matrix from which to remove rows.
 *
 *TIME: (if there are Z zero rows)
 * NON_SPARSE: O(ZR)
 * SPARSE_ARRAY: O(ZR)
 * SPARSE_LIST: O(ZR) + O(ZS/R)
 *************************************************************************/

ExpandingArray *crm114__matr_remove_zero_rows(Matrix *X) {
  unsigned int i, offset, row, lim;
  ExpandingArray *rowMap = crm114__make_expanding_array(MATR_DEFAULT_VECTOR_SIZE,
						MATR_COMPACT);
  CompactExpandingType cet;
  ExpandingType et;
  Vector *r;

  if (!X || !X->data) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "matr_remoev_zero_rows: null matrix.\n");
    }
    return NULL;
  }

  offset = 0;
  lim = X->rows;
  for (i = 0; i < lim; i++) {
    row = i - offset;
    r = matr_get_row(X, row);
    if (!r) {
      continue;
    }
    if (crm114__vector_iszero(r)) {
      crm114__matr_remove_row(X, row);
      offset++;
    } else {
      cet.i = i;
      et.compact = &cet;
      crm114__expanding_array_insert(et, rowMap);
    }
  }

  return rowMap;
}

/*************************************************************************
 *Removes columns that are all zeros from a sparse matrix.
 *
 *INPUT: M: matrix from which to remove columns.
 *
 *OUTPUT: An expanding array A of unsigned ints such that if c is a column
 * number of M after the removal, A[c] is the column number of M before the
 * removal.  A will allow you to reconstruct M.
 *
 *TIME:
 * The running time of this function is complicated.  If there are fewer
 * than QSORT_COUNTING_CUTOFF (we recommend you set this ~8*10^7) elements,
 * we sort the columns of X using qsort.  The running time for this is
 * (on average) where C is the number of non-zero columns:
 * NON_SPARSE: --
 * SPARSE_ARRAY: O(Slg(S)) + O(Slg(C))
 * SPARSE_LIST: O(Slg(S)) + O(Slg(C))
 *
 * If there are greater than QSORT_COUNTING_CUTOFF elements, we sort the
 * columns of X using a counting sort.  The running time if F is the total
 * number of columns is then
 * NON_SPARSE: --
 * SPARSE_ARRAY: O(F/MATRIX_SORT_MAX_ALLOC_SIZE*RS)
 * SPARSE_LIST: O(F/MATRIX_SORT_MAX_ALLOC_SIZE*RS)
 * This depends on the total number of columns because of the amount of
 * memory we can allocate.  If columns are considered unsigned ints, then
 * F/MATRIX_SORT_MAX_ALLOC_SIZE is usually ~40
 *
 * In general, empirical evidence shows that for small to medium size
 * matrices qsort is faster and for larger matrices counting sort is faster.
 * For any matrix that fits in memory, neither should take more than 5 minutes.
 *
 *MEMORY:
 * Running QSort requires having a copy of all of the elements in the
 * matrix while running counting sort requires an array of size
 * MATRIX_SORT_MAX_ALLOC_SIZE.  Because countint sort is topped at MATRIX_SORT_MAX_ALLOC_SIZE,
 * if the matrix has a very large number of elements, we recommend you use
 * counting sort.
 *************************************************************************/

ExpandingArray *crm114__matr_remove_zero_cols(Matrix *X) {
  if (!X || !X->data) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_remove_zero_cols: null matrix.\n");
    }
    return NULL;
  }

  if (!(X->cols) || !(X->nz)) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_remove_zero_cols: X has nothing to sort.\n");
    }
    return NULL;
  }

  if (X->nz < QSORT_COUNTING_CUTOFF) {
    return matr_remove_zero_cols_sort(X, QSORT);
  }
  return matr_remove_zero_cols_sort(X, COUNTING);
}

//"private" function to select the sorting algorithm
//and actually remove the columns
static ExpandingArray *matr_remove_zero_cols_sort(Matrix *X,
						  SortingAlgorithms sorttype) {
  unsigned int size, offset, j, i, startcol = 0, index, col, lastcol;
  int iterations, *coliszero = NULL;
  ExpandingArray *colMap = NULL;
  VectorIterator vit;
  Vector *row;
  CompactExpandingType cet;
  ExpandingType et;
  int front, back;


  if (!X || !X->data) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_remove_zero_cols: null matrix.\n");
    }
    return NULL;
  }

  if (X->type == NON_SPARSE) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf
	(stderr,
	 "Called crm114__matr_remove_zero_cols on non-sparse matrix.  Returning.\n");
    }
    //we don't want to try renumbering columns on a non-sparse matrix
    //what a headache!
    return NULL;
  }


  switch (sorttype) {
  case COUNTING:
    {
      //O(S*lg_{10^8}(X->cols))
      //now remove the zero columns
      //if X->cols is less than MATRIX_SORT_MAX_ALLOC_SIZE/sizeof(int)
      //this is a single loop
      //Basically, we are doing a counting sort on the columns of X.
      size = X->cols;
      if (size > MATRIX_SORT_MAX_ALLOC_SIZE/sizeof(int)) {
	//don't run out of memory
	//instead do this several times
	//note that since cols is an integer
	//X->cols <= 2^32 ~= 4.3*10^9
	//i recommend setting MATRIX_SORT_MAX_ALLOC_SIZE = 4*10^8
	//this uses only 400 MB of memory and
	//we won't ever do this loop more than 43 times
	//So it's still fast
	size = MATRIX_SORT_MAX_ALLOC_SIZE/sizeof(int);
      }
      colMap = crm114__make_expanding_array(MATR_DEFAULT_VECTOR_SIZE, MATR_COMPACT);
      if (!colMap) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf
	    (stderr,
	     "Unable to allocate memory for counting sort.  Giving up.\n");
	}
	return NULL;
      }
      iterations = (int)((X->cols-1)/(double)size+1);
      if (CRM114__MATR_DEBUG_MODE >= MATR_DEBUG) {
	fprintf(stderr, "Removing zero columns will take %d iterations, each of length %d.\n", iterations, size);
      }
      coliszero = (int *)malloc(sizeof(int)*size);
      offset = 0;
      for (j = 0; j < iterations; j++) {
	startcol = j*size;
	if (startcol >= X->cols) {
	  //just a check
	  startcol = (j-1)*size;
	  break;
	}
	if (CRM114__MATR_DEBUG_MODE >= MATR_DEBUG) {
	  fprintf(stderr, "startcol = %u\n", startcol);
	}
	for (i = 0; i < size; i++) {
	  coliszero[i] = 1;
	}

	//locate the zero columns
	for (i = 0; i < X->rows; i++) {
	  row = matr_get_row(X, i);
	  if (!row) {
	    continue;
	  }
	  vectorit_set_at_beg(&vit, row);
	  crm114__vectorit_find(&vit, startcol, row);
	  vectorit_prev(&vit, row);
	  while ((vectorit_past_beg(vit, row) ||
		  vectorit_curr_col(vit, row) < (long)startcol + (long)size) &&
		 !vectorit_past_end(vit, row)) {
	    if (!vectorit_past_beg(vit, row) &&
		vectorit_curr_col(vit, row) >= startcol) {
	      coliszero[vectorit_curr_col(vit, row)-startcol] = 0;
	    }
	    vectorit_next(&vit, row);
	  }
	}

	//calculate the offset for every column and store it in coliszero
	//also update colMap
	for (i = 0; i < size; i++) {
	  if (!coliszero[i]) {
	    cet.i = i + startcol;
	    et.compact = &cet;
	    crm114__expanding_array_insert(et, colMap);
	  } else {
	    offset++;
	  }
	  coliszero[i] = offset;
	}

	//renumber the columns of X
	for (i = 0; i < X->rows; i++) {
	  row = matr_get_row(X, i);
	  if (!row) {
	    continue;
	  }
	  vectorit_set_at_beg(&vit, row);
	  crm114__vectorit_find(&vit, startcol, row);
	  while (!vectorit_past_end(vit, row) &&
		 (vectorit_past_beg(vit, row) ||
		  vectorit_curr_col(vit, row) < (long)startcol + (long)size)) {
	    if (!vectorit_past_beg(vit, row) &&
		vectorit_curr_col(vit, row) >= startcol) {
	      crm114__vectorit_set_col(vit, vectorit_curr_col(vit, row)
			       - coliszero[vectorit_curr_col(vit, row)-startcol],
			       row);
	    }
	    vectorit_next(&vit, row);
	  }
	}
      }

      index = X->cols - startcol-1;
      //tell X it has fewer columns now
      X->cols -= coliszero[index];
      for (i = 0; i < X->rows; i++) {
	X->data[i]->dim -= coliszero[index];
      }

      if (CRM114__MATR_DEBUG_MODE >= MATR_DEBUG) {
	fprintf(stderr, "There were %u zero columns.\n", coliszero[index]);
      }

      free(coliszero);
      return colMap;
    }
  case MERGE:
    {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr,
		"Merge sort not yet implemented.  Using counting sort.\n");
      }
      return crm114__matr_remove_zero_cols(X);
    }
  case QSORT:
    {
      //ok, let's hope we can fit two versions of X into memory!
      //put everything into colMap
      if (CRM114__MATR_DEBUG_MODE >= MATR_DEBUG) {
	fprintf(stderr, "Allocating %d elements for sort.\n", X->nz);
      }
      colMap = crm114__make_expanding_array(X->nz, MATR_COMPACT);
      if (!colMap) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "Unable to allocate enough space for qsort.  Using counting sort.\n");
	}
	return matr_remove_zero_cols_sort(X, COUNTING);
      }
      for (i = 0; i < X->rows; i++) {
	row = matr_get_row(X, i);
	if (!row) {
	  continue;
	}
	vectorit_set_at_beg(&vit, row);
	while (!vectorit_past_end(vit, row)) {
	  col = vectorit_curr_col(vit, row);
	  cet.i = col;
	  et.compact = &cet;
	  crm114__expanding_array_insert(et, colMap);
	  vectorit_next(&vit, row);
	}
      }
      qsort(&(colMap->data.compact[colMap->first_elt]),
	    colMap->n_elts, sizeof(CompactExpandingType),
	    crm114__compact_expanding_type_int_compare);
      if (CRM114__MATR_DEBUG_MODE >= MATR_DEBUG) {
	fprintf(stderr, "Finished qsort.\n");
      }

      //make colMap unique
      lastcol = crm114__expanding_array_get(0, colMap).compact->i;
      offset = 0;
      for (i = 1; i < colMap->n_elts; i++) {
	et = crm114__expanding_array_get(i, colMap);
	if (et.compact->i == lastcol) {
	  offset++;
	} else {
	  lastcol = et.compact->i;
	  if (CRM114__MATR_DEBUG_MODE >= MATR_OPS_MORE) {
	    fprintf(stderr, "Replacing column %d (%u) with column %d (%u)\n",
		   i - offset, crm114__expanding_array_get(i-offset, colMap).compact->i,
		   i, crm114__expanding_array_get(i, colMap).compact->i);
	  }
	  crm114__expanding_array_set(et, i - offset, colMap);
	}
      }

      colMap->n_elts -= offset;
      colMap->last_elt -= offset;
      crm114__expanding_array_trim(colMap);
      if (CRM114__MATR_DEBUG_MODE >= MATR_DEBUG) {
	fprintf(stderr, "Renumbering columns.  Total columns = %d last_elt = %d\n", colMap->n_elts, colMap->last_elt);
      }
      //renumber the columns of X
     for (i = 0; i < X->rows; i++) {
       row = matr_get_row(X, i);
       if (!row) {
	 continue;
       }
       vectorit_set_at_beg(&vit, row);
       index = 0;
       while (!vectorit_past_end(vit, row)) {
	 front = colMap->first_elt;
	 back = colMap->last_elt;
	 col = vectorit_curr_col(vit, row);
	 index = (front + back)/2;
	 while (colMap->data.compact[index].i != col) {
	   if (colMap->data.compact[index].i < col) {
	     front = index+1;
	   } else if (colMap->data.compact[index].i > col) {
	     back = index-1;
	   }
	   index = (front + back)/2;
	 }
	 index -= colMap->first_elt;
	 if (CRM114__MATR_DEBUG_MODE >= MATR_OPS_MORE) {
	   fprintf(stderr, "index = %d, colMap[index] = %u, actual col = %u\n",
		  index,
		  crm114__expanding_array_get(index, colMap).compact->i,
		  vectorit_curr_col(vit, row));
	 }
	 crm114__vectorit_set_col(vit, index, row);
	 vectorit_next(&vit, row);
       }
     }

     //tell X it has fewer columns
     X->cols = colMap->n_elts;
     for (i = 0; i < X->rows; i++) {
       X->data[i]->dim = colMap->n_elts;
     }

     return colMap;
     // return matr_remove_zero_cols_sort(X, COUNTING);
    }
  default:
    {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "Invalid sorting type.\n");
      }
      return NULL;
    }

  }
}

/*************************************************************************
 *Appends one matrix to another using shallow copies.  In essence, this just
 *does pointer arithmetic so that the last from->rows of *to_ptr = from
 *and from no longer has anything in it.
 *
 *INPUT: to_ptr: pointer to a pointer to the matrix to which to append.
 *  if to_ptr is NULL, that's bad.  if *to_ptr is null, a new matrix will be
 *  created.
 * from: matrix to append.  on return, from will contain no rows
 *
 *OUTPUT: A matrix in *to_ptr such that the last from->rows rows of to_ptr
 * are the rows of from in reverse order.  from will have 0 rows.
 *
 *TIME: (R and C refer to from)
 * NON_SPARSE: O(R*C)
 * SPARSE_ARRAY: O(R)
 * SPARSE_LIST: O(R)
 *************************************************************************/

void crm114__matr_append_matr(Matrix **to_ptr, Matrix *from) {
  Matrix *to;
  unsigned int i, oldrows;
  Vector *row;

  if (!to_ptr) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr,
	      "crm114__matr_append_matr: pointer to 'to' matrix unitialized.\n");
    }
    return;
  }

  to = *to_ptr;

  if (from && from->rows > 0) {
    if (!to) {
      to = crm114__matr_make_size(from->rows, from->cols, from->type, from->compact,
			  from->size);
      oldrows = 0;
    } else {
      oldrows = to->rows;
      crm114__matr_add_nrows(to, from->rows);
    }
    if (!to || (from->rows && !(to->data))) {
      //something is wrong
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "crm114__matr_append_matr: error in creating new matrix.  your from matrix appears corrupted.\n");
      }
      if (to) {
	to->rows = 0;
	to->nz = 0;
      }
      return;
    }

    for (i = oldrows; i < to->rows; i++) {
      row = matr_get_row(from, from->rows-1);
      crm114__matr_shallow_row_copy(to, i, row);
      crm114__matr_erase_row(from, from->rows-1);
    }
  }

  *to_ptr = to;
}


/*************************************************************************
 *Multiplies a matrix by a vector.
 *
 *INPUT: M: matrix
 * v: row vector
 *
 *OUTPUT: ret = M*v.  If ret has more rows than M or v, only the first
 * R rows (where M is R x C) will be relevant.  If M has more rows than
 * ret, M will be treated as a D x C matrix where D is the number of rows
 * of ret.
 *
 *TIME: (M is R x C with S non-zero elements, v has s non-zero elements)
 * Both NON_SPARSE: O(R*C)
 * M NON_SPARSE, v SPARSE: O(R*s)
 * M SPARSE, v NON_SPARSE: O(S)
 * Both SPARSE: O(S + R*s)
 *
 *WARNINGS:
 *1) v and ret CANNOT point to the same vector.
 *2) If v->dim > M->cols or M->cols > v->dim, the missing numbers will be
 *   treated as zeros.
 **************************************************************************/

void crm114__matr_vector(Matrix *M, Vector *v, Vector *ret) {
  unsigned int i, rows;
  VectorIterator vit;
  double d;

  if (!M || !M->data || !v || !ret) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_vector: null arguments.\n");
    }
    return;
  }

  rows = ret->dim;

  if (CRM114__MATR_DEBUG_MODE >= MATR_OPS) {
    fprintf(stderr, "crm114__matr_vector: multiplying\n");
    crm114__matr_print(M);
    fprintf(stderr, "by\n");
    crm114__vector_print(v);
    fprintf(stderr, "putting in\n");
    crm114__vector_print(ret);
  }

  if (M->rows < rows) {
    rows = M->rows;
  }

  if (ret->type == SPARSE_ARRAY) {
    //this is fast
    //and prevents us from moving the whole array around later
    crm114__vector_zero(ret);
  }

  vectorit_set_at_beg(&vit, ret);

  for (i = 0; i < rows; i++) {
    d = crm114__dot(M->data[i], v);
    if (fabs(d) < MATRIX_EPSILON && i == vectorit_curr_col(vit, ret)) {
      crm114__vectorit_zero_elt(&vit, ret);
      continue;
    }
    crm114__vectorit_insert(&vit, i, d, ret);
    vectorit_next(&vit, ret);
    if (CRM114__MATR_DEBUG_MODE >= MATR_OPS_MORE) {
      fprintf(stderr, "ret = ");
      crm114__vector_print(ret);
    }
  }
}

/*************************************************************************
 *Multiplies a sequence of matrices by a vector.
 *
 *INPUT: A: List of matrices
 * nmatrices: number of matrices in A
 * maxrows: the maximum number of rows any matrix in the list A has
 * w: the vector to multiply by
 *
 *OUTPUT: z = A_{n-1}*A_{n-2}*...*A_0*w.
 *
 *TIME: nmatrices*TIME(crm114__matr_vector)
 *
 *WARNINGS:
 *1) w and z CANNOT point to the same vector.
 **************************************************************************/

void crm114__matr_vector_seq(Matrix **A, int nmatrices, unsigned int maxrows,
		     Vector *w, Vector *z) {
  int i;
  Vector *tmp1, *tmp2, *ctmp;

  if (!A || !w || !z) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_vector_seq: null arguments.\n");
    }
    return;
  }

  tmp1 = crm114__vector_make_size(maxrows, z->type, z->compact, z->size);
  tmp2 = crm114__vector_make_size(maxrows, z->type, z->compact, z->size);

  if (nmatrices == 0) {
    return;
  }

  if (nmatrices == 1) {
    crm114__matr_vector(A[0], w, z);
    crm114__vector_free(tmp1);
    crm114__vector_free(tmp2);
    return;
  }


  crm114__vector_copy(w, tmp1);

  ctmp = tmp1;

  for (i = 0; i < nmatrices; i++) {
    if (!(i%2)) {
      crm114__matr_vector(A[i], tmp1, tmp2);
      ctmp = tmp2;
    } else {
      crm114__matr_vector(A[i], tmp2, tmp1);
      ctmp = tmp1;
    }
  }

  crm114__vector_copy(ctmp, z);

  crm114__vector_free(tmp1);
  crm114__vector_free(tmp2);
}

/*************************************************************************
 *Transposes a matrix.
 *
 *INPUT: A: Matrix to transpose.

 *OUTPUT: T = A^T, transpose of A.
 *
 *TIME:
 * NON_SPARSE: O(R*C)
 * SPARSE_ARRAY: O(R) + O(S)
 * SPARSE_LIST: O(S)
 *
 *WARNINGS:
 *1) A and T CANNOT point to the same matrix.
 **************************************************************************/

void crm114__matr_transpose(Matrix *A, Matrix *T) {
  unsigned int i;
  VectorIterator vit, trit;

  if (!A || !T || !A->data || !T->data) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_transpose: null matrix.\n");
    }
    return;
  }

  if (A->rows != T->cols || A->cols != T->rows) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_transpose: transposed matrix incorrect size.\n");
    }
    return;
  }

  if (T->type != NON_SPARSE && T->nz > 0) {
    //clear out T
    for (i = 0; i < T->rows; i++) {
      crm114__vector_zero(T->data[i]);
    }
    T->nz = 0;
  }

  for (i = 0; i < A->rows; i++) {
    vectorit_set_at_beg(&vit, A->data[i]);
    while (!vectorit_past_end(vit, A->data[i])) {
      vectorit_set_at_end(&trit, T->data[vectorit_curr_col(vit, A->data[i])]);
      crm114__vectorit_insert(&trit, i, vectorit_curr_val(vit, A->data[i]),
		      T->data[vectorit_curr_col(vit, A->data[i])]);
      vectorit_next(&vit, A->data[i]);
    }
  }
}

/*************************************************************************
 *Checks if a matrix is all zeros.
 *
 *INPUT: A: matrix to check.

 *OUTPUT: 1 if matrix is all zeros, 0 else.
 *
 *TIME:
 * NON_SPARSE: O(R*C)
 * SPARSE_ARRAY: O(R)
 * SPARSE_LIST: O(R)
 *
 **************************************************************************/

int crm114__matr_iszero(Matrix *M) {
  unsigned int i;

  if (!M || !M->data) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_iszero: null matrix.\n");
    }
    return 1;
  }

  for (i = 0; i < M->rows; i++) {
    if (!crm114__vector_iszero(M->data[i])) {
      return 0;
    }
  }

  return 1;
}

/***************************************************************************
 *Converts a NON_SPARSE matrix to a SPARSE_ARRAY using colMap.
 *
 *INPUT: M: matrix to convert
 * colMap: array such that if c is a column of M when M is non-sparse,
 *  that column will have value colMap[c] when M is sparse.  this can be
 *  used to "undo" removing zero columns IF you convert to a NON_SPARSE
 *  matrix after you do so.
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: --
 * SPARSE_LIST: --
 *************************************************************************/

void crm114__matr_convert_nonsparse_to_sparray(Matrix *M, ExpandingArray *colMap) {
  int i;
  Vector *row;

  if (!M || !colMap) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "matr_convert: null arguments.\n");
    }
    return;
  }

  if (M->type != NON_SPARSE) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr,
	      "Attempt to convert an already sparse matrix to sparse.\n");
    }
    return;
  }

  M->type = SPARSE_ARRAY;
  M->nz = 0;
  M->size = M->cols;

  for (i = 0; i < M->rows; i++) {
    row = matr_get_row(M, i);
    if (!row) {
      continue;
    }
    M->nz += row->nz;
    crm114__vector_convert_nonsparse_to_sparray(row, colMap);
  }
}

/*************************************************************************
 *Prints a matrix to stderr putting back the zeros so the full matrix can
 * be seen.  If you want to print a matrix in sparse form, use crm114__matr_write
 * with the file pointer stdout.
 *
 *INPUT: M: matrix to print.
 *
 *TIME:
 * NON_SPARSE: O(R*C)
 * SPARSE_ARRAY: O(R*C)
 * SPARSE_LIST: O(R*C)
 *
 **************************************************************************/

void crm114__matr_print(Matrix *M) {
  unsigned int i;
  Vector *row;

  if (!M) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_print: null matrix.\n");
    }
    return;
  }

  if (M->rows == 0 || M->cols == 0) {
    //empty matrix
    fprintf(stderr, "[]");
    return;
  }

  for (i = 0; i < M->rows; i++) {
    row = matr_get_row(M, i);
    if (row) {
      crm114__vector_print(row);
    }
  }
}

/*************************************************************************
 *Writes a matrix to a file using a sparse representation for the sparse
 * matrices and non-sparse representation for the non-sparse ones.
 *
 *INPUT: M: matrix to write.
 * filename: file to write to.
 *TIME:
 * NON_SPARSE: O(R*C)
 * SPARSE_ARRAY: O(S)
 * SPARSE_LIST: O(S)
 *
 **************************************************************************/

void crm114__matr_write(Matrix *M, char *filename) {
  FILE *out = fopen(filename, "w");

  if (!out) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "Bad file name in crm114__matr_write: %s\n", filename);
    }
    return;
  }

  crm114__matr_write_fp(M, out);

  fclose(out);
}

/*************************************************************************
 *Writes a matrix to a file using a sparse representation for the sparse
 * matrices and non-sparse representation for the non-sparse ones.
 *
 *INPUT: M: matrix to write.
 * fp: file to write to.
 *TIME:
 * NON_SPARSE: O(R*C)
 * SPARSE_ARRAY: O(S)
 * SPARSE_LIST: O(S)
 *
 **************************************************************************/

void crm114__matr_write_fp(Matrix *M, FILE *out) {

  if (!M || !out) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_write: null arguments.\n");
    }
    return;
  }
  if (M->type == NON_SPARSE) {
    matr_write_ns(M, out);
  } else {
    matr_write_sp(M, out);
  }

}

//"private" functions for writing the different types of matrices
static void matr_write_sp(Matrix *M, FILE *out) {
  unsigned int i;
  VectorIterator vit;

  if (!M || !out || !M->data) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_write: null arguments.\n");
    }
    return;
  }

  for (i = 0; i < M->rows; i++) {
    vectorit_set_at_beg(&vit, M->data[i]);
    while (!vectorit_past_end(vit, M->data[i])) {
      fprintf(out, "%u %u %lf\n", i, vectorit_curr_col(vit, M->data[i]),
	      vectorit_curr_val(vit, M->data[i]));
      vectorit_next(&vit, M->data[i]);
    }
  }
}

static void matr_write_ns(Matrix *M, FILE *out) {
  unsigned int i;

  if (!M || !out || !M->data) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_write: null arguments.\n");
    }
    return;
  }

  for (i = 0; i < M->rows; i++) {
    crm114__vector_write_ns_fp(M->data[i], out);
    fprintf(out, "\n");
  }
}

#define PRINTF_DOUBLE_FMT "%.20g"
#define SCANF_DOUBLE_FMT "%lg"

#define TN_MATR "matrix"
#define TN_VECTOR "vector"
#define TN_NON_SPARSE "NON_SPARSE"
#define TN_SPARSE_ARRAY "SPARSE_ARRAY"
#define TN_SPARSE_LIST "SPARSE_LIST"
#define TN_COMPACT "compact"
#define TN_PRECISE "precise"
#define TN_ROWS "rows"
#define TN_COLS "cols"
#define TN_NZ "nz"
#define TN_DIM "dim"

/*
  C99 has printf type "a", which prints a double in hex, and so
  represents it exactly.  Unfortunately, Microsoft doesn't have it.
  So we have to use some decimal representation of floating point
  numbers, which is not necessarily exact.

  At least we pick one that prints all the significant digits of the
  approximation.
*/

static char *vector_type_to_string(VectorType type)
{
  switch (type)
    {
    case NON_SPARSE:
      return TN_NON_SPARSE;
    case SPARSE_ARRAY:
      return TN_SPARSE_ARRAY;
    case SPARSE_LIST:
      return TN_SPARSE_LIST;
    }
  return "garbage";
}

static int string_to_vector_type(char str[], VectorType *type)
{
  int ret;

  ret = 1;
  if (strcmp(str, TN_NON_SPARSE) == 0)
    *type = NON_SPARSE;
  else if (strcmp(str, TN_SPARSE_ARRAY) == 0)
    *type = SPARSE_ARRAY;
  else if (strcmp(str, TN_SPARSE_LIST) == 0)
    *type = SPARSE_LIST;
  else
    ret = 0;

  return ret;
}

static int string_to_compact(char str[], int *compact)
{
  int ret;

  ret = 1;
  if (strcmp(str, TN_COMPACT) == 0)
    *compact = 1;
  else if (strcmp(str, TN_PRECISE) == 0)
    *compact = 0;
  else
    ret = 0;

  return ret;
}

// Write a matrix to a text file that's portable storage.
void crm114__matr_write_text_fp(char name[], Matrix *M, FILE *fp)
{
  unsigned int i;

  fprintf(fp, TN_MATR " %s %s %s " TN_ROWS " %u " TN_COLS " %u\n",
	  name,
	  vector_type_to_string(M->type),
	  M->compact ? TN_COMPACT : TN_PRECISE,
	  M->rows,
	  M->cols);

  for (i = 0; i < M->rows; i++) {
    char rowtxt[50];		// row subscript as text, for vector name

    sprintf(rowtxt, "%u", i);
    crm114__vector_write_text_fp(rowtxt, M->data[i], fp);
  }
}

/*************************************************************************
 *Writes a matrix to a file using a binary representation.
 *
 *INPUT: M: matrix to write.
 * filename: file to write to.
 *TIME:
 * NON_SPARSE: O(R*C)
 * SPARSE_ARRAY: O(S)
 * SPARSE_LIST: O(S)
 *
 **************************************************************************/

size_t crm114__matr_write_bin(Matrix *M, char *filename) {
  size_t size;
  FILE *fp = fopen(filename, "wb");
  if (!fp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_write_bin: bad filename %s", filename);
    }
    return 0;
  }
  size = crm114__matr_write_bin_fp(M, fp);
  fclose(fp);
  return size;
}

/*************************************************************************
 *Writes a matrix to a file using a binary representation.
 *
 *INPUT: M: matrix to write.
 * fp: file to write to.
 *TIME:
 * NON_SPARSE: O(R*C)
 * SPARSE_ARRAY: O(S)
 * SPARSE_LIST: O(S)
 *
 **************************************************************************/

size_t crm114__matr_write_bin_fp(Matrix *M, FILE *fp) {
  size_t size;
  unsigned int i;
  Vector *row;

  if (!M || !fp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_write: null arguments.\n");
    }
    return 0;
  }

  size = sizeof(Matrix)*fwrite(M, sizeof(Matrix), 1, fp);
  for (i = 0; i < M->rows; i++) {
    row = matr_get_row(M, i);
    if (row) {
      size += crm114__vector_write_bin_fp(row, fp);
    }
  }
  return size;
}

size_t crm114__matr_size(Matrix *M) {
  size_t size;
  int i;
  Vector *row;

  if (M) {
    size = sizeof(Matrix);
    if (M->data)
      for (i = 0; i < M->rows; i++)
	if ((row = matr_get_row(M, i)) != NULL)
	  size += crm114__vector_size(row);
  }
  else
    size = 0;

  return size;
}

// read a matrix from a text file
Matrix *crm114__matr_read_text_fp(char expected_name[], FILE *fp)
{
  Matrix *M;
  char matr_name[50 + 1];
  char type_string[20 + 1];
  char compact_string[10 + 1];
  unsigned int i;

  if ((M = calloc(1, sizeof(Matrix))) == NULL)
    return NULL;
  if (fscanf(fp, " " TN_MATR " %50s %20s %10s " TN_ROWS " %u " TN_COLS " %u",
	     matr_name, type_string, compact_string, &M->rows, &M->cols) != 5)
    goto err;
  if (strcmp(matr_name, expected_name) != 0)
    goto err;
  if ( !string_to_vector_type(type_string, &M->type))
    goto err;
  if ( !string_to_compact(compact_string, &M->compact))
    goto err;

  if (M->rows > 0)
    if ((M->data = calloc(M->rows, sizeof(Vector *))) == NULL)
      goto err;
  for (i = 0; i < M->rows; i++) {
    char rowtxt[50];

    sprintf(rowtxt, "%u", i);
    if ((M->data[i] = crm114__vector_read_text_fp(rowtxt, fp)) == NULL)
      goto err;
  }

  return M;

 err:
  crm114__matr_free(M);
  return NULL;
}

/*************************************************************************
 *Reads a matrix from a file using a binary representation.
 *
 *INPUT: filename: file to read from.
 *
 *OUTPUT: Matrix in the file or NULL if the file is incorrectly formatted.
 *
 *TIME:
 * NON_SPARSE: O(R*C)
 * SPARSE_ARRAY: O(S)
 * SPARSE_LIST: O(S)
 *
 *WARNINGS:
 * 1) This expects a file formatted as crm114__matr_write_bin would write it.  If it
 *    detects the file is wrong, it may return NULL, but it may not.  Check
 *    the output!
 **************************************************************************/

Matrix *crm114__matr_read_bin(char *filename) {
  Matrix *M;

  FILE *fp = fopen(filename, "rb");
  if (!fp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_read_bin: bad filename %s", filename);
    }
    return NULL;
  }
  M = crm114__matr_read_bin_fp(fp);
  fclose(fp);
  return M;
}

/*************************************************************************
 *Reads a matrix from a file using a binary representation.
 *
 *INPUT: fp: file to read from.
 *
 *OUTPUT: Matrix in the file or NULL if the file is incorrectly formatted.
 *
 *TIME:
 * NON_SPARSE: O(R*C)
 * SPARSE_ARRAY: O(S)
 * SPARSE_LIST: O(S)
 *
 *WARNINGS:
 * 1) This expects a file formatted as crm114__matr_write_bin would write it.  If it
 *    detects the file is wrong, it may return NULL, but it may not.  Check
 *    the output!
 **************************************************************************/

Matrix *crm114__matr_read_bin_fp(FILE *fp) {
  Matrix *M = (Matrix *)malloc(sizeof(Matrix));
  unsigned int i;
  size_t amount_read;

  if (!fp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "matr_read: bad file pointer.\n");
    }
    free(M);
    return NULL;
  }

  amount_read = fread(M, sizeof(Matrix), 1, fp);
  M->was_mapped = 0;

  if (!amount_read) {
    free(M);
    return NULL;
  }

  M->data = (Vector **)malloc(sizeof(Vector *)*M->rows);
  if (!M->data && M->rows > 0) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "matr_read: Unable to malloc space for matrix.\n");
    }
    M->rows = 0;
    M->nz = 0;
    return M;
  }

  for (i = 0; i < M->rows; i++) {
    M->data[i] = crm114__vector_read_bin_fp(fp);
    if (!M->data[i]) {
      //oh oh bad file
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "matr_read: Bad file.\n");
      }
      break;
    }
    if (CRM114__MATR_DEBUG_MODE >= MATR_OPS_MORE) {
      fprintf(stderr, "read row %u feof = %d dim = %d nz = %d\n", i, feof(fp),
	     M->data[i]->dim, M->data[i]->nz);
      crm114__vector_write_sp_fp(matr_get_row(M, i), stderr);
    }
  }
  if (i != M->rows) {
    M->rows = i;
    crm114__matr_free(M);
    M = NULL;
  }
  return M;
}

/*****************************************************************************
 *Converts data stored at *addr into a matrix.
 *
 *INPUT: addr: a pointer to the address at which the matrix is stored.
 * last_addr: the last possible address that is valid.  NOT necessarily where
 *  the list ends - just the last address that has been allocated in the
 *  chunk pointed to by *addr (ie, if *addr was taken from an mmap'd file
 *  last_addr would be *addr + the file size).
 *
 *OUTPUT: A matrix STILL referencing the chunk of memory pointed to by *addr
 *  although with its OWN, newly malloc'd row list.
 * *addr: (pass-by-reference) points to the first address AFTER the full
 *   matrix.
 *WARNINGS:
 * 1) *addr needs to be writable.  This will CHANGE VALUES stored at *addr and
 *    will seg fault if addr is not writable.
 * 2) last_addr does not need to be the last address of the list
 *    but if it is before that, either NULL will be returned or a
 *    matrix with a NULL data value will be returned.
 * 3) if *addr does not contain a properly formatted matrix, this function
 *    will not seg fault, but that is the only guarantee.
 * 4) you MUST call crm114__matr_free on this matrix AS WELL AS freeing memory
 *    stored at *addr.
 * 5) *addr CHANGES!
 * 6) This was one of the last functions I added to the library and one of the
 *    likeliest to cause memory errors and seg faults.  I've done a good bit
 *    of testing on it, but this function and memory-mapped objects in
 *    general are the likeliest to break the library.  I appologize.
 ****************************************************************************/

Matrix *crm114__matr_map(void **addr, void *last_addr) {
  Matrix *M;
  unsigned int i;

  if (!addr || !*addr || !last_addr) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_map: null arguments.\n");
    }
    return NULL;
  }

  if ((void *)((Matrix *)*addr + 1) > last_addr) {
    return NULL;
  }

  M = (Matrix *)(*addr);
  *addr = M + 1;
  M->was_mapped = 1;

  M->data = (Vector **)malloc(sizeof(Vector *)*M->rows);
  if (!M->data && M->rows > 0) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__matr_map: unable to allocate space for matrix.\n");
    }
    M->rows = 0;
    M->nz = 0;
    return M;
  }
  for (i = 0; i < M->rows; i++) {
    M->data[i] = crm114__vector_map(addr, last_addr);
    if (!M->data[i]) {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "crm114__matr_map: bad file.\n");
      }
      break;
    }
  }
  if (i != M->rows) {
    M->rows = i;
    crm114__matr_free(M);
    M = NULL;
  }
  return M;
}


/*************************************************************************
 *Frees all memory associated with a matrix.
 *
 *INPUT: M: matrix to free.
 *
 *TIME:
 * NON_SPARSE: O(R)
 * SPARSE_ARRAY: O(R)
 * SPARSE_LIST: O(S)
 *
 **************************************************************************/

void crm114__matr_free(Matrix *M) {
  unsigned int i;

  if (!M) {
    return;
  }

  if (M->data) {
    for (i = 0; i < M->rows; i++) {
      crm114__vector_free(M->data[i]);
    }
    free(M->data);
  }

  if (!M->was_mapped) {
    free(M);
  }
}

/**************************************************************************
 *The vector class works with the matrix class.  All matrices are arrays
 *of pointers to vectors.  In general, the actual work is done in the vector
 *class.
 *
 *For the times given below, we assume vectors are of length c with s
 *non-zero elements.
 **************************************************************************/

//Static vector function declarations
static void vector_make_nsarray_data(Vector *v, int compact);
static void vector_make_sparray_data(Vector *v, int compact, int init_size);
static void vector_make_splist_data(Vector *v, int compact);
static void vector_add_col_ns(Vector *v);
static void vector_add_ncols_ns(Vector *v, unsigned int n);
static inline void vector_add_fast(Vector *sp, Vector *ns, Vector *ret);
static inline double dot_log(Vector *sp, Vector *ns);
static inline double dot_fast(Vector *sp, Vector *ns);
static inline void vector_add_multiple_fast(Vector *base, Vector *toadd,
					    double factor);
static size_t vector_write_bin_ns(Vector *v, FILE *fp);
static void vector_read_bin_ns(Vector *v, FILE *fp);


/*************************************************************************
 *Makes a zero vector.
 *
 *INPUT: dim: number of rows/columns in the vector
 * type: NON_SPARSE, SPARSE_ARRAY, or SPARSE_LIST specifying the data
 *  structure
 * compact: MATR_COMPACT or MATR_PRECISE specifying whether data is stored
 *  as an int or a double
 *
 *OUTPUT: A vector of dimension dim of all zeros with the type and compact
 * flags set correctly.  If the vector is a sparse array, the array
 * will begin at size MATR_DEFAULT_VECTOR_SIZE.
 *
 *TIME:
 * NON_SPARSE: O(C)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(1)
 *************************************************************************/

Vector *crm114__vector_make(unsigned int dim, VectorType type, int compact) {
  return crm114__vector_make_size(dim, type, compact, MATR_DEFAULT_VECTOR_SIZE);
}

/*************************************************************************
 *Makes a zero vector.
 *
 *INPUT: dim: number of rows/columns in the vector
 * type: NON_SPARSE, SPARSE_ARRAY, or SPARSE_LIST specifying the data
 *  structure
 * compact: MATR_COMPACT or MATR_PRECISE specifying whether data is stored
 *  as an int or a double
 * size: the starting size of the array if the vector is a SPARSE_ARRAY
 *
 *OUTPUT: A vector of dimension dim of all zeros with the type and compact
 * flags set correctly.  If the vector is a sparse array, the array
 * will begin at size size.
 *
 *TIME:
 * NON_SPARSE: O(C)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(1)
 *************************************************************************/

Vector *crm114__vector_make_size(unsigned int dim, VectorType type, int compact,
			 int size) {
  Vector *v = (Vector *)malloc(sizeof(Vector));
  v->dim = dim;
  v->type = type;
  v->compact = compact;
  v->size = size;
  v->was_mapped = 0;

  switch(type) {
  case NON_SPARSE:
    vector_make_nsarray_data(v, compact);
    break;
  case SPARSE_ARRAY:
    vector_make_sparray_data(v, compact, size);
    break;
  case SPARSE_LIST:
    vector_make_splist_data(v, compact);
    break;
  default:
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_make: unrecognized type.\n");
    }
    free(v);
    return NULL;
  }

  return v;
}

//"private" functions for dealing with making the
//different types of data structures
static void vector_make_nsarray_data(Vector *v, int compact) {
  unsigned int i;

  if (!v) {
    return;
  }

  v->nz = v->dim;
  if (v->dim > 0) {
    if (compact) {
      v->data.nsarray.compact = (int *)malloc(sizeof(int)*v->dim);
      if (!v->data.nsarray.compact) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "Unable to malloc data for non-sparse vector.\n");
	}
	return;
      }
    } else {
      v->data.nsarray.precise = (double *)malloc(sizeof(double)*v->dim);
      if (!v->data.nsarray.precise) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "Unable to malloc data for non-sparse vector.\n");
	}
	return;
      }
    }
    for (i = 0; i < v->dim; i++) {
      crm114__vector_set(v, i, 0);
    }
  } else {
    v->data.nsarray.precise = NULL; //pointers are same size doesn't matter
  }
}

static void vector_make_sparray_data(Vector *v, int compact, int size) {

  if (!v) {
    return;
  }

  if (size < 0) {
    size = 0;
    v->size = 0;
  }

  v->nz = 0;
  v->data.sparray = crm114__make_expanding_array(size, compact);
  if (!v->data.sparray && CRM114__MATR_DEBUG_MODE) {
    fprintf(stderr, "warning: no space malloc'd for sparse array vector.\n");
  }
}

static void vector_make_splist_data(Vector *v, int compact) {

  if (!v) {
    return;
  }

  v->nz = 0;
  v->data.splist = crm114__make_list(compact);
  if (!v->data.splist && CRM114__MATR_DEBUG_MODE) {
    fprintf(stderr, "warning: no space malloc'd for sparse list vector.\n");
  }
}

/*************************************************************************
 *Copies one vector to another.
 *
 *INPUT: from: vector to copy from
 *
 *OUTPUT: to = from, a copy of the vector from
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(s)
 * SPARSE_LIST: O(s)
 *
 *WARNINGS:
 *1) from and to cannot point to the same vector (why would you want to do
 *   that anyway?)
 *************************************************************************/

void crm114__vector_copy(Vector *from, Vector *to) {
  VectorIterator fit, toit;

  if (!to || !from) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_copy: null arguments.\n");
    }
    return;
  }

  if (to->type == SPARSE_ARRAY) {
    //this is constant time
    //and prevents us having to move the array
    //around as we zero elements
    crm114__vector_zero(to);
  }

  vectorit_set_at_beg(&fit, from);
  vectorit_set_at_beg(&toit, to);
  while (!vectorit_past_end(fit, from) &&
	 vectorit_curr_col(fit, from) < to->dim) {
    if (vectorit_curr_col(toit, to) < vectorit_curr_col(fit, from)) {
      crm114__vectorit_zero_elt(&toit, to);
      continue;
    }
    crm114__vectorit_insert(&toit, vectorit_curr_col(fit, from),
		    vectorit_curr_val(fit, from), to);
    while (vectorit_curr_col(toit, to) <= vectorit_curr_col(fit, from)) {
      vectorit_next(&toit, to);
    }
    vectorit_next(&fit, from);
  }
  while (!vectorit_past_end(toit, to)) {
    crm114__vectorit_zero_elt(&toit, to);
  }
}

/*************************************************************************
 *Sets an element of a vector.
 *
 *INPUT: v: vector in which to set an element
 * i: element to set
 * d: value to set element to
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY:
 *  Generally: ammortized O(lg(s)) if d != 0, O(s) if d = 0
 *  First/Last element: ammortized O(1)
 * SPARSE_LIST:
 *  Generally: O(s)
 *  First/Last element: O(1)
 *************************************************************************/
void crm114__vector_set(Vector *v, unsigned int i, double d) {
  VectorIterator vit;

  if (!v) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_set: null vector.\n");
    }
    return;
  }

  if (i >= v->dim) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_set: out of range column %u.\n", i);
    }
    return;
  }

  if (v->type == NON_SPARSE) {
    if (v->compact) {
      if (v->data.nsarray.compact) {
	v->data.nsarray.compact[i] = (int)d;
      } else if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "crm114__vector_set: null vector.\n");
      }
    } else {
      if (v->data.nsarray.precise) {
	v->data.nsarray.precise[i] = d;
      } else if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "crm114__vector_set: null vector.\n");
      }
    }
    return;
  }

  vectorit_set_at_beg(&vit, v);
  crm114__vectorit_insert(&vit, i, d, v);

}

/*************************************************************************
 *Gets an element of a vector.
 *
 *INPUT: v: vector from which to get an element
 * i: element to get
 *
 *OUTPUT: The element in the ith column of v.
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY:
 *  Generally: O(lg(s))
 *  First/Last element: O(1)
 * SPARSE_LIST:
 *  Generally: O(s)
 *  First/Last element: O(1)
 *************************************************************************/

double crm114__vector_get(Vector *v, unsigned int i) {
  VectorIterator vit;

  if (!v) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_get: null vector.\n");
    }
    return 0;
  }

  if (i >= v->dim) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_get: out of range column %u.\n", i);
    }
    return 0;
  }

  if (v->type == NON_SPARSE) {
    if (v->compact) {
      if (v->data.nsarray.compact) {
	return v->data.nsarray.compact[i];
      } else {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vector_get: null vector.\n");
	}
	return 0;
      }
    } else {
      if (v->data.nsarray.precise) {
	return v->data.nsarray.precise[i];
      } else {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vector_get: null vector.\n");
	}
	return 0;
      }
    }
  }

  vectorit_set_at_beg(&vit, v);
  crm114__vectorit_find(&vit, i, v);

  if (vectorit_curr_col(vit, v) == i) {
    return vectorit_curr_val(vit, v);
  }

  return 0;
}

/*************************************************************************
 *Zero out a vector.
 *
 *INPUT: v: vector to zero
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(s)
 *************************************************************************/

void crm114__vector_zero(Vector *v) {
  unsigned int i;

  if (!v) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_zero: null vector.\n");
    }
    return;
  }

  switch (v->type) {
  case NON_SPARSE:
    {
      if (!(v->data.nsarray.compact) &&
	  !(v->data.nsarray.precise)) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vector_zero: null vector.\n");
	}
	return;
      }
      for (i = 0; i < v->dim; i++) {
	if (v->compact) {
	  v->data.nsarray.compact[i] = 0;
	} else {
	  v->data.nsarray.precise[i] = 0;
	}
      }
      break;
    }
  case SPARSE_ARRAY:
    {
      if (!v->data.sparray) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vector_zero: null vector.\n");
	}
	return;
      }
      crm114__expanding_array_clear(v->data.sparray);
      v->nz = 0;
      break;
    }
  case SPARSE_LIST:
    {
      if (!v->data.splist) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vector_zero: null vector.\n");
	}
	return;
      }

      crm114__list_clear(v->data.splist);
      v->nz = 0;
      break;
    }
  default:
    {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "crm114__vector_zero: unrecognized type.\n");
      }
    }
  }
}

/*************************************************************************
 *Add two vectors.
 *
 *INPUT: v1: first vector
 * v2: second vector
 *
 *OUTPUT: ret = v1 + v2.  note that ret CAN point to v1 or v2
 *
 *TIME:
 * Both NON_SPARSE: O(c)
 * One NON_SPARSE, one SPARSE, ret points to the NON_SPARSE: O(s)
 * Both SPARSE: O(s_1) + O(s_2)
 *************************************************************************/

void crm114__vector_add(Vector *v1, Vector *v2, Vector *ret) {
  VectorIterator vit1, vit2, *vitr;
  unsigned int col1, col2, colr, col;
  double d;

  if (!v1 || !v2 || !ret) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_add: null arguments.\n");
    }
    return;
  }

  if (v1->type != NON_SPARSE && v2->type == NON_SPARSE && ret == v2) {
    vector_add_fast(v1, v2, ret);
    return;
  }

  if (v1->type == NON_SPARSE && v2->type != NON_SPARSE && ret == v1) {
    vector_add_fast(v2, v1, ret);
    return;
  }

  if (CRM114__MATR_DEBUG_MODE >= MATR_OPS) {
    fprintf(stderr, "Adding\n\t");
    crm114__vector_print(v1);
    fprintf(stderr, "and\n\t");
    crm114__vector_print(v2);
    fprintf(stderr, "putting in\n\t");
    crm114__vector_print(ret);
  }

  if (ret->type == SPARSE_ARRAY && ret != v1 && ret != v2) {
    //zero out ret
    crm114__vector_zero(ret);
  }

  vectorit_set_at_beg(&vit1, v1);
  vectorit_set_at_beg(&vit2, v2);
  if (v1 == ret) {
    vitr = &vit1;
  }
  if (v2 == ret) {
    vitr = &vit2;
  }
  if (v1 != ret && v2 != ret) {
    vitr = (VectorIterator *)malloc(sizeof(VectorIterator));
    vectorit_set_at_beg(vitr, ret);
  }

  while (!vectorit_past_end(*vitr, ret) ||
	 (!vectorit_past_end(vit1, v1) &&
	  vectorit_curr_col(vit1, v1) < ret->dim) ||
	 (!vectorit_past_end(vit2, v2) &&
	  (vectorit_curr_col(vit2, v2) < ret->dim))) {

    col1 = vectorit_curr_col(vit1, v1);
    col2 = vectorit_curr_col(vit2, v2);
    colr = vectorit_curr_col(*vitr, ret);
    if (CRM114__MATR_DEBUG_MODE >= MATR_OPS_MORE) {
      fprintf(stderr, "col1 = %d, col2 = %d, colr = %d\n", col1, col2, colr);
    }
    if ((colr < col1 || col1 >= v1->dim) &&
	(colr < col2 || col2 >= v2->dim)) {
      crm114__vectorit_zero_elt(vitr, ret);
      continue;
    }

    if (col1 == col2 && col1 < v1->dim && col2 < v2->dim) {
      d = vectorit_curr_val(vit1, v1) + vectorit_curr_val(vit2, v2);
      col = col1;
      if (v1 != ret) {
	vectorit_next(&vit1, v1);
      }
      if (v2 != ret) {
	vectorit_next(&vit2, v2);
      }
    } else if (col1 < col2 || col2 == v2->dim) {
      col = col1;
      d = vectorit_curr_val(vit1, v1);
      if (v1 != ret) {
	vectorit_next(&vit1, v1);
      }
    } else {
      col = col2;
      d = vectorit_curr_val(vit2, v2);
      if (v2 != ret) {
	vectorit_next(&vit2, v2);
      }
    }

    if (fabs(d) < MATRIX_EPSILON) {
      crm114__vectorit_zero_elt(vitr, ret);
    } else {
      crm114__vectorit_insert(vitr, col, d, ret);
      vectorit_next(vitr, ret);
    }
  }

  if (v1 != ret && v2 != ret) {
    free(vitr);
  }
}


/*************************************************************************
 *Multiply a vector by a scalar.
 *
 *INPUT: v: vector to multiply
 * s: scalar to multiply by
 *
 *OUTPUT: ret = s*v.  ret CAN point to the same vector as v.
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_LIST: O(s)
 * SPARSE_ARRAY: O(s)
 *************************************************************************/

void crm114__vector_multiply(Vector *v, double s, Vector *ret) {
  VectorIterator vit, *vitr;
  unsigned int col, colr;

  if (!v || !ret) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_multiply: null arguments.\n");
    }
    return;
  }

  if (CRM114__MATR_DEBUG_MODE >= MATR_OPS) {
    fprintf(stderr, "multiplying\n\t");
    crm114__vector_print(v);
    fprintf(stderr, "by %lf, putting in\n\t", s);
    crm114__vector_print(ret);
  }

  if (fabs(s) < MATRIX_EPSILON || crm114__vector_iszero(v)) {
    //zero out vector
    if (CRM114__MATR_DEBUG_MODE >= MATR_OPS) {
      fprintf(stderr, "zeroing ret.\n");
    }
    crm114__vector_zero(ret);
    return;
  }

  if (ret != v && ret->type == SPARSE_ARRAY) {
    crm114__vector_zero(ret);
  }

  vectorit_set_at_beg(&vit, v);
  if (ret == v) {
    vitr = &vit;
  } else {
    vitr = (VectorIterator *)malloc(sizeof(VectorIterator));
    vectorit_set_at_beg(vitr, ret);
  }

  while (!vectorit_past_end(*vitr, v) ||
	 (!vectorit_past_end(vit, v) &&
	  vectorit_curr_col(vit, v) < ret->dim)) {

    col = vectorit_curr_col(vit, v);
    colr = vectorit_curr_col(*vitr, ret);

    if (CRM114__MATR_DEBUG_MODE >= MATR_OPS_MORE) {
      fprintf(stderr, "col = %d, colr = %d ret = ", col, colr);
      crm114__vector_print(ret);
    }
    if (colr < col || col == v->dim) {
      crm114__vectorit_zero_elt(vitr, ret);
      continue;
    }

    crm114__vectorit_insert(vitr, col, s*vectorit_curr_val(vit, v), ret);
    vectorit_next(&vit, v);
    while (vectorit_curr_col(*vitr, ret) <= col) {
      vectorit_next(vitr, ret);
    }
  }

  if (ret != v) {
    free(vitr);
  }
}

/*************************************************************************
 *Dot two vectors.
 *
 *INPUT: v1: first vector
 * v2: second vector
 *
 *OUTPUT: v1 dot v2.
 *
 *TIME:
 * Both NON_SPARSE: O(c)
 * One NON_SPARSE, one SPARSE: O(s)
 * Both SPARSE: O(s_1) + O(s_2)
 *************************************************************************/

double crm114__dot(Vector *v1, Vector *v2) {
  VectorIterator vit1, vit2;
  unsigned int col1, col2;
  double ret = 0;

  if (!v1 || !v2) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "dot: null arguments.\n");
    }
    return 0;
  }

  if (v1->type != NON_SPARSE && v2->type == NON_SPARSE) {
    return dot_fast(v1, v2);
  }

  if (v1->type == NON_SPARSE && v2->type != NON_SPARSE) {
    return dot_fast(v2, v1);
  }

  if (v1->type == SPARSE_ARRAY && v2->nz <= 0.1*v1->nz) {
    return dot_log(v2, v1);
  }

  if (v2->type == SPARSE_ARRAY && v1->nz <= 0.1*v2->nz) {
    return dot_log(v1, v2);
  }

  vectorit_set_at_beg(&vit1, v1);
  vectorit_set_at_beg(&vit2, v2);

  while (!vectorit_past_end(vit1, v1) && !vectorit_past_end(vit2, v2)) {
    col1 = vectorit_curr_col(vit1, v1);
    col2 = vectorit_curr_col(vit2, v2);
    if (col1 < col2) {
      vectorit_next(&vit1, v1);
      continue;
    }
    if (col2 < col1) {
      vectorit_next(&vit2, v2);
      continue;
    }
    ret += vectorit_curr_val(vit1, v1)*vectorit_curr_val(vit2, v2);
    vectorit_next(&vit1, v1);
    vectorit_next(&vit2, v2);
  }
  return ret;
}

void crm114__vector_add_multiple(Vector *base, Vector *toadd,
			 double factor, Vector *ret) {

  VectorIterator vit1, vit2, *vitr;
  unsigned int col1, col2, colr, col;
  double d;

  if (!base || !toadd || !ret) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_add_multiple: null arguments.\n");
    }
    return;
  }

  if (fabs(factor) < MATRIX_EPSILON) {
    if (ret != base) {
      crm114__vector_copy(base, ret);
    }
    return;
  }

  if (base->type == NON_SPARSE && ret == base) {
    vector_add_multiple_fast(base, toadd, factor);
    return;
  }

  if (CRM114__MATR_DEBUG_MODE >= MATR_OPS) {
    fprintf(stderr, "Adding to \n\t");
    crm114__vector_print(base);
    fprintf(stderr, "Multiplying\n\t");
    crm114__vector_print(toadd);
    fprintf(stderr, "by %lf and putting in\n\t", factor);
    crm114__vector_print(ret);
  }

  if (ret->type == SPARSE_ARRAY && ret != base && ret != toadd) {
    //zero out ret
    crm114__vector_zero(ret);
  }

  vectorit_set_at_beg(&vit1, base);
  vectorit_set_at_beg(&vit2, toadd);
  if (base == ret) {
    vitr = &vit1;
  }
  if (toadd == ret) {
    vitr = &vit2;
  }
  if (base != ret && toadd != ret) {
    vitr = (VectorIterator *)malloc(sizeof(VectorIterator));
    vectorit_set_at_beg(vitr, ret);
  }

  while (!vectorit_past_end(*vitr, ret) ||
	 (!vectorit_past_end(vit1, base) &&
	  vectorit_curr_col(vit1, base) < ret->dim) ||
	 (!vectorit_past_end(vit2, toadd) &&
	  (vectorit_curr_col(vit2, toadd) < ret->dim))) {

    col1 = vectorit_curr_col(vit1, base);
    col2 = vectorit_curr_col(vit2, toadd);
    colr = vectorit_curr_col(*vitr, ret);
    if (CRM114__MATR_DEBUG_MODE >= MATR_OPS_MORE) {
      fprintf(stderr, "col1 = %d, col2 = %d, colr = %d\n", col1, col2, colr);
    }
    if ((colr < col1 || col1 >= base->dim) &&
	(colr < col2 || col2 >= toadd->dim)) {
      crm114__vectorit_zero_elt(vitr, ret);
      continue;
    }

    if (col1 == col2 && col1 < base->dim && col2 < toadd->dim) {
      d = vectorit_curr_val(vit1, base) + factor*vectorit_curr_val(vit2, toadd);
      col = col1;
      if (base != ret) {
	vectorit_next(&vit1, base);
      }
      if (toadd != ret) {
	vectorit_next(&vit2, toadd);
      }
    } else if (col1 < col2 || col2 == toadd->dim) {
      col = col1;
      d = vectorit_curr_val(vit1, base);
      if (base != ret) {
	vectorit_next(&vit1, base);
      }
    } else {
      col = col2;
      d = factor*vectorit_curr_val(vit2, toadd);
      if (toadd != ret) {
	vectorit_next(&vit2, toadd);
      }
    }

    if (fabs(d) < MATRIX_EPSILON) {
      crm114__vectorit_zero_elt(vitr, ret);
    } else {
      crm114__vectorit_insert(vitr, col, d, ret);
      vectorit_next(vitr, ret);
    }
  }

  if (base != ret && toadd != ret) {
    free(vitr);
  }
}

/*************************************************************************
 *Check if a vector is all zeros.
 *
 *INPUT: v: vector to check
 *
 *OUTPUT: 1 if v is all zeros, 0 else
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(1)
 *************************************************************************/

int crm114__vector_iszero(Vector *v) {
  unsigned int i;

  if (!v) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_iszero: null vector.\n");
    }
    return 1;
  }

  switch (v->type) {
  case NON_SPARSE:
    {
      for (i = 0; i < v->dim; i++) {
	if (crm114__vector_get(v, i)) {
	  return 0;
	}
      }
      return 1;
    }
  case SPARSE_ARRAY:
    {
      if (!v->data.sparray) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vector_iszero: null vector.\n");
	}
	return 1;
      }
      return v->data.sparray->n_elts == 0;
    }
  case SPARSE_LIST:
    {
      if (!v->data.splist) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vector_iszero: null vector.\n");
	}
	return 1;
      }
      return crm114__list_is_empty(v->data.splist);
    }
  default:
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_iszero: unrecognized type.\n");
    }
    return 1;
  }
}

/*************************************************************************
 *Check if two vectors have the same content (regardless of representation).
 *
 *INPUT: v1, v2: vectors to check
 *
 *OUTPUT: 1 if v1 = v2 in the content sense, 0 else
 *
 *TIME:
 * NON_SPARSE, SPARSE: O(c) + O(s)
 * SPARSE, SPARSE: O(s_1) + O(s_2)
 *************************************************************************/

int crm114__vector_equals(Vector *v1, Vector *v2) {
  VectorIterator vit1, vit2;
  unsigned int col1, col2;

  if (v1 == v2) {
    return 1;
  }

  if (!v1 || !v2) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_equals: null arguments.\n");
    }
    return 0;
  }


  vectorit_set_at_beg(&vit1, v1);
  vectorit_set_at_beg(&vit2, v2);
  while ((!vectorit_past_end(vit1, v1) &&
	  vectorit_curr_col(vit1, v1) < v2->dim) ||
	 (!vectorit_past_end(vit2, v2) &&
	  vectorit_curr_col(vit2, v2) < v1->dim)) {
    col1 = vectorit_curr_col(vit1, v1);
    col2 = vectorit_curr_col(vit2, v2);
    if (col1 != col2) {
      //check for non-sparse representations having 0's where
      //sparse representations have no entry
      if (v1->type == NON_SPARSE && col1 < col2 &&
	  fabs(vectorit_curr_val(vit1, v1)) < MATRIX_EPSILON) {
	vectorit_next(&vit1, v1);
	continue;
      }
      if (v2->type == NON_SPARSE && col2 < col1 &&
	  fabs(vectorit_curr_val(vit2, v2)) < MATRIX_EPSILON) {
	vectorit_next(&vit2, v2);
	continue;
      }
      return 0;
    }
    if (vectorit_curr_val(vit1, v1) != vectorit_curr_val(vit2, v2)) {
      return 0;
    }
    vectorit_next(&vit1, v1);
    vectorit_next(&vit2, v2);
  }

  return 1;
}



//"private" function for adding a sparse and a non-sparse
//vector quickly
static inline void vector_add_fast(Vector *sp, Vector *ns, Vector *ret) {
  VectorIterator vit;
  unsigned int col;

  if (!sp || !ns || !ret) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_add: null arguments.\n");
    }
    return;
  }

  if (ret != ns || sp->type == NON_SPARSE || ns->type != NON_SPARSE) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "vector_add_fast: Wrong sparseness.\n");
    }
    crm114__vector_add(sp, ns, ret);
    return;
  }

  vectorit_set_at_beg(&vit, sp);
  while (!vectorit_past_end(vit, sp)) {
    col = vectorit_curr_col(vit, sp);
    crm114__vector_set(ret, col, vectorit_curr_val(vit, sp) + crm114__vector_get(ns, col));
    vectorit_next(&vit, sp);
  }
}

static inline void vector_add_multiple_fast(Vector *base, Vector *toadd,
					    double factor) {
  VectorIterator vit;
  int j;

  if (!base || !toadd) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "add multiple: null arguments.\n");
    }
    return;
  }

  if (base->type != NON_SPARSE) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr,
	      "Warning: Called add_multiple_fast with wrong sparseness.\n");
    }
  }
  if (fabs(factor) < MATRIX_EPSILON) {
    return;
  }

  //a common combination that we want to be screaming fast
  if (toadd->type == SPARSE_ARRAY && toadd->compact && !(base->compact) &&
      toadd->data.sparray && base->type == NON_SPARSE &&
      base->data.nsarray.precise) {
    for (j = toadd->data.sparray->first_elt; j <= toadd->data.sparray->last_elt;
	 j++) {
      base->data.nsarray.precise[toadd->data.sparray->data.compact[j].s.col] +=
	factor*toadd->data.sparray->data.compact[j].s.data;
    }
    return;
  }

  vectorit_set_at_beg(&vit, toadd);
  while (!vectorit_past_end(vit, toadd)) {
    crm114__vector_set(base, vectorit_curr_col(vit, toadd),
	       crm114__vector_get(base, vectorit_curr_col(vit, toadd)) +
	       factor*vectorit_curr_val(vit, toadd));
    vectorit_next(&vit, toadd);
  }
}

//"private" function for dotting a sparse and a non-sparse vector
//quickly
static inline double dot_fast(Vector *sp, Vector *ns) {
  VectorIterator vit;
  double ret = 0;
  int j;

  if (!sp || !ns) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "dot: null arguments.\n");
    }
    return 0;
  }

  if (ns->type != NON_SPARSE) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "Warning: Called dot_fast with incorrect sparseness.\n");
    }
  }

  //this particular combination of types comes up often
  //we want it to be as fast as possible
  //so use ugly code
  if (sp->type == SPARSE_ARRAY && sp->compact && !(ns->compact) &&
      sp->data.sparray && ns->type == NON_SPARSE && ns->data.nsarray.precise) {
    for (j = sp->data.sparray->first_elt; j <= sp->data.sparray->last_elt; j++){
      ret += sp->data.sparray->data.compact[j].s.data*
	ns->data.nsarray.precise[sp->data.sparray->data.compact[j].s.col];
    }
    return ret;
  }

  //this is still fairly fast for all other cases
  //and much prettier =D
  vectorit_set_at_beg(&vit, sp);
  while (!vectorit_past_end(vit, sp)) {
    ret += vectorit_curr_val(vit, sp)*
      crm114__vector_get(ns, vectorit_curr_col(vit, sp));
    vectorit_next(&vit, sp);
  }
  return ret;
}

//private function for dotting a sparse array and another much sparser
//vector quickly using a binary search in the sparse array.
static inline double dot_log(Vector *sp, Vector *ns) {
  VectorIterator vit, nit;
  double ret = 0;

  if (!sp || !ns) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "dot: null arguments.\n");
    }
    return 0;
  }

  if (ns->type != SPARSE_ARRAY) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "Warning: Called dot_log with incorrect sparseness.\n");
    }
    //return crm114__dot(sp, ns);
  }

  if (sp->type == NON_SPARSE) {
    //this is faster
    return dot_fast(ns, sp);
  }

  vectorit_set_at_beg(&vit, sp);
  vectorit_set_at_beg(&nit, ns);
  while (!vectorit_past_end(vit, sp)) {
    crm114__vectorit_find(&nit, vectorit_curr_col(vit, sp), ns);
    if (vectorit_curr_col(nit, ns) == vectorit_curr_col(vit, sp)) {
      ret += vectorit_curr_val(vit, sp)*vectorit_curr_val(nit, ns);
    }
    vectorit_next(&vit, sp);
  }

  return ret;
}



/*************************************************************************
 *Add a column to the end of the vector.
 *
 *INPUT: v: vector to add a column to
 *
 *TIME:
 * NON_SPARSE: O(1) (realloc succeeds) O(c) (realloc fails)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(1)
 *************************************************************************/

void crm114__vector_add_col(Vector *v) {
  if (!v) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_add_col: null vector.\n");
    }
    return;
  }

  if (v->type != NON_SPARSE) {
    v->dim++;
    //well, this is easy
    return;
  }
  vector_add_col_ns(v);
}

//"private" function to add a column to
//a non-sparse vector
static void vector_add_col_ns(Vector *v) {
  NSData tmpdata;

  if (!v) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_add_col: null vector.\n");
    }
    return;
  }

  if (v->compact) {
    if (!(v->was_mapped) ||
	(v->was_mapped &&
	 (void *)(v + 1) != (void *)(v->data.nsarray.compact))) {
      v->data.nsarray.compact = (int *)realloc(v->data.nsarray.compact,
					       sizeof(int)*(v->dim+1));
    } else {
      tmpdata.compact = v->data.nsarray.compact;
      v->data.nsarray.compact = (int *)malloc(sizeof(int)*(v->dim+1));
      if (v->data.nsarray.compact) {
	memcpy(v->data.nsarray.compact, tmpdata.compact, sizeof(int)*v->dim);
      }
    }
    if (!v->data.nsarray.compact) {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "Error adding a column to non-sparse vector.\n");
      }
      v->dim = 0;
      v->nz = 0;
      return;
    }
  } else {
    if (!(v->was_mapped) ||
	(v->was_mapped &&
	 (void *)(v + 1) != (void *)(v->data.nsarray.precise))) {
      v->data.nsarray.precise = (double *)realloc(v->data.nsarray.precise,
						  sizeof(double)*(v->dim+1));
    } else {
      tmpdata.precise = v->data.nsarray.precise;
      v->data.nsarray.precise = (double *)malloc(sizeof(double)*(v->dim+1));
      if (v->data.nsarray.precise) {
	memcpy(v->data.nsarray.precise, tmpdata.precise, sizeof(double)*v->dim);
      }
    }
    if (!v->data.nsarray.precise) {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "Error adding a column to non-sparse vector.\n");
      }
      v->dim = 0;
      v->nz = 0;
      return;
    }
  }
  v->dim++;
  crm114__vector_set(v, v->dim-1, 0);
}

/*************************************************************************
 *Add n columns to the end of the vector.
 *
 *INPUT: v: vector to add columns to
 *
 *TIME:
 * NON_SPARSE: O(1) (realloc succeeds) O(c) (realloc fails)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(1)
 *************************************************************************/

void crm114__vector_add_ncols(Vector *v, unsigned int n) {
  if (!v) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_add_ncols: null vector.\n");
    }
    return;
  }

  if (n <= 0) {
    return;
  }

  if (v->type != NON_SPARSE) {
    v->dim += n;
  } else {
    vector_add_ncols_ns(v, n);
  }
}

//"private" function to add n columns to a non-sparse vector
static void vector_add_ncols_ns(Vector *v, unsigned int n) {
  unsigned int i;
  NSData tmpdata;

  if (!v) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_add_ncols: null vector.\n");
    }
    return;
  }

  if (n <= 0){
    return;
  }

  if (v->compact) {
    if (!(v->was_mapped) ||
	(v->was_mapped &&
	 (void *)(v + 1) != (void *)(v->data.nsarray.compact))) {
      v->data.nsarray.compact = (int *)realloc(v->data.nsarray.compact,
					       sizeof(int)*(v->dim+n));
    } else {
      tmpdata.compact = v->data.nsarray.compact;
      v->data.nsarray.compact = (int *)malloc(sizeof(int)*(v->dim+n));
      if (v->data.nsarray.compact) {
	memcpy(v->data.nsarray.compact, tmpdata.compact, sizeof(int)*v->dim);
      }
    }
    if (!v->data.nsarray.compact) {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "Error adding a column to non-sparse vector.\n");
      }
      v->dim = 0;
      v->nz = 0;
      return;
    }
  } else {
    if (!(v->was_mapped) ||
	(v->was_mapped &&
	 (void *)(v + 1) != (void *)(v->data.nsarray.precise))) {
      v->data.nsarray.precise = (double *)realloc(v->data.nsarray.precise,
						  sizeof(double)*(v->dim+n));
    } else {
      tmpdata.precise = v->data.nsarray.precise;
      v->data.nsarray.precise = (double *)malloc(sizeof(double)*(v->dim+n));
      if (v->data.nsarray.precise) {
	memcpy(v->data.nsarray.precise, tmpdata.precise, sizeof(double)*v->dim);
      }
    }
    if (!v->data.nsarray.precise) {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "Error adding a column to non-sparse vector.\n");
      }
      v->dim = 0;
      v->nz = 0;
      return;
    }
  }
  v->dim += n;
  for (i = v->dim-n; i < v->dim; i++) {
    crm114__vector_set(v, i, 0);
  }
}

/*************************************************************************
 *Remove a column from a vector.
 *
 *INPUT: v: vector from which to remove a column
 * c: column to remove
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(s)
 * SPARSE_LIST: O(s)
 *************************************************************************/

void crm114__vector_remove_col(Vector *v, unsigned int c) {
  VectorIterator vit;
  int remove = 0, i;
  double d;

  if (!v) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_remove_col: null vector.\n");
    }
    return;
  }

  if (c >= v->dim) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr,
	      "crm114__vector_remove_col: attempt to remove nonexistant column.\n");
    }
    return;
  }

  vectorit_set_at_beg(&vit, v);
  crm114__vectorit_find(&vit, c, v);
  if (vectorit_curr_col(vit, v) == c) {
    remove = 1;
  }
  //just make sure we're pointing after or at c
  while (vectorit_curr_col(vit, v) < c) {
    vectorit_next(&vit, v);
  }

  if (v->type == NON_SPARSE) {
    if (v->dim == 1) {
      if (v->compact && v->data.nsarray.compact) {
	free(v->data.nsarray.compact);
      } else if (v->data.nsarray.precise) {
	free(v->data.nsarray.precise);
      }
      v->data.nsarray.precise = NULL;
      v->dim = 0;
      return;
    }
    d = crm114__vector_get(v, v->dim-1);
    if (v->compact) {
      if (!(v->was_mapped) ||
	  (v->was_mapped &&
	   (void *)(v + 1) != (void *)(v->data.nsarray.compact))) {
	v->data.nsarray.compact = (int *)realloc(v->data.nsarray.compact,
						 sizeof(int)*(v->dim-1));
      } else if (v->dim-1 <= 0) {
	//otherwise v->data is mapped in memory and all is good
	v->data.nsarray.compact = NULL;
      }
      if (!v->data.nsarray.compact && v->dim > 0) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "Error removing a column from non-sparse vector.\n");
	}
	v->dim = 0;
	v->nz = 0;
	return;
      }
    } else {
      if (!(v->was_mapped) ||
	  (v->was_mapped &&
	   (void *)(v + 1) != (void *)(v->data.nsarray.compact))) {
	v->data.nsarray.precise = (double *)realloc(v->data.nsarray.precise,
						    sizeof(double)*(v->dim-1));
      } else if (v->dim-1 <= 0) {
	v->data.nsarray.precise = NULL;
      }
      if (!v->data.nsarray.precise && v->dim > 0) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "Error removing a column from non-sparse vector.\n");
	}
	v->dim = 0;
	v->nz = 0;
	return;
      }
    }
    if (v->dim >= 2) {
      for (i = c; i < v->dim-2; i++) {
	crm114__vector_set(v, i, crm114__vector_get(v, i+1));
      }
    }
    if ((v->dim >= 1 && c < v->dim-1)) {
      crm114__vector_set(v, v->dim-2, d);
    }
    v->dim--;
    return;
  }

  if (remove) {
    crm114__vectorit_zero_elt(&vit, v);
  }

  while (!vectorit_past_end(vit, v)) {
    crm114__vectorit_set_col(vit, vectorit_curr_col(vit, v) - 1, v);
    vectorit_next(&vit, v);
  }

  v->dim--;
}

/*************************************************************************
 *Squared distance between two vectors.
 *
 *INPUT: v1: first vector
 * v2: second vector
 *
 *OUTPUT: ||v1 - v2||^2
 *
 *TIME:
 * Both NON_SPARSE: O(c)
 * One NON_SPARSE, one SPARSE: O(s) + O(c)
 * Both SPARSE: O(s_1) + O(s_2)
 *************************************************************************/

double crm114__vector_dist2(Vector *v1, Vector *v2) {
  VectorIterator vit1, vit2;
  unsigned int col1, col2;
  double ret = 0, d;


  if (!v1 || !v2) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_dist2: null arguments.\n");
    }
    return -1;
  }

  if (v1->dim != v2->dim) {
    //uh oh
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_dist2: dimension mismatch\n");
    }
    return -1;
  }

  vectorit_set_at_beg(&vit1, v1);
  vectorit_set_at_beg(&vit2, v2);

  while (!vectorit_past_end(vit1, v1) || !vectorit_past_end(vit2, v2)) {

    col1 = vectorit_curr_col(vit1, v1);
    col2 = vectorit_curr_col(vit2, v2);

    if (col1 == col2) {
      d = vectorit_curr_val(vit1, v1) - vectorit_curr_val(vit2, v2);
      vectorit_next(&vit1, v1);
      vectorit_next(&vit2, v2);
    } else if (col1 < col2) {
      d = vectorit_curr_val(vit1, v1);
      vectorit_next(&vit1, v1);
    } else {
      d = vectorit_curr_val(vit2, v2);
      vectorit_next(&vit2, v2);
    }
    ret += d*d;
  }
  return ret;
}


/*************************************************************************
 *Converts a NON_SPARSE vector to a sparse array using colMap.
 *
 *INPUT: v: vector to convert
 * colMap: array such that if c is a column of v when v is non-sparse,
 *  that column will have value colMap[c] when v is sparse.  this can be
 *  used to "undo" removing zero columns.
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: --
 * SPARSE_LIST: --
 *************************************************************************/

void crm114__vector_convert_nonsparse_to_sparray(Vector *v, ExpandingArray *colMap) {
  Vector tmpv;
  int i;
  VectorIterator vit;
  ExpandingType et;

  if (!v || !colMap) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "vector_convert: null arguments.\n");
    }
    return;
  }

  if (v->type != NON_SPARSE) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "Attempt to convert already sparse vector to sparse.\n");
    }
    return;
  }

  et = crm114__expanding_array_get(v->dim-1, colMap);
  if (!et.precise || !et.compact) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "vector_convert: colMap doesn't have enough entries.\n");
    }
    return;
  }

  tmpv.type = NON_SPARSE;
  tmpv.size = v->size;
  tmpv.dim = v->dim;
  tmpv.nz = v->nz;
  tmpv.compact = v->compact;
  tmpv.data = v->data;

  v->type = SPARSE_ARRAY;
  v->size = v->dim;
  v->dim = et.compact->i+1;
  vector_make_sparray_data(v, v->compact, v->size);

  if (!v->data.sparray || (v->compact && !v->data.sparray->data.compact) ||
      (!(v->compact) && !v->data.sparray->data.precise)) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf
	(stderr,
	 "vector_convert: unable to convert vector.  It appears corrupted.\n");
    }
    v->type = tmpv.type;
    v->size = tmpv.size;
    v->dim = tmpv.dim;
    v->data = tmpv.data;
    return;
  }

  vectorit_set_at_beg(&vit, v);
  for (i = 0; i < tmpv.dim; i++) {
    et = crm114__expanding_array_get(i, colMap);
    if (!et.precise || !et.compact) {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr,
		"vector_convert: colMap doesn't have enough entries.\n");
      }
      if (v->compact) {
	free(v->data.sparray->data.compact);
      } else {
	free(v->data.sparray->data.precise);
      }
      v->type = tmpv.type;
      v->size = tmpv.size;
      v->dim = tmpv.dim;
      return;
    }
    crm114__vectorit_insert(&vit, et.compact->i, crm114__vector_get(&tmpv, i), v);
  }

  if (tmpv.compact) {
    free(tmpv.data.nsarray.compact);
  } else {
    free(tmpv.data.nsarray.precise);
  }
}

/*************************************************************************
 *Print a vector to stderr.  Puts the zeros back in the vector.
 *
 *INPUT: v: vector to print
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(c)
 * SPARSE_LIST: O(c)
 *************************************************************************/

void crm114__vector_print(Vector *v) {
  VectorIterator vit;
  int lastcol = -1, i, col;

  if (!v) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_print: null vector.\n");
    }
    return;
  }

  fprintf(stderr, "[");
  vectorit_set_at_beg(&vit, v);
  while (!vectorit_past_end(vit, v)) {
    col = vectorit_curr_col(vit, v);
    for (i = lastcol+1; i < col; i++) {
      fprintf(stderr, "%20.10lf", 0.0);
    }
    fprintf(stderr, "%20.10lf", vectorit_curr_val(vit, v));
    lastcol = col;
    vectorit_next(&vit, v);
  }

  for (i = lastcol+1; i < v->dim; i++) {
    fprintf(stderr, "%20.10lf", 0.0);
  }

  fprintf(stderr, "]\n");
}

/*************************************************************************
 *Write a vector to a file.  Writes everything in non-sparse format.
 *
 *INPUT: v: vector to write
 * filename: file to write vector to
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(c)
 * SPARSE_LIST: O(c)
 *************************************************************************/

void crm114__vector_write_ns(Vector *v, char *filename) {
  FILE *out = fopen(filename, "w");

  if (!out) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_write_ns: Bad file name %s\n", filename);
    }
    return;
  }

  crm114__vector_write_ns_fp(v, out);

  fclose(out);
}

/*************************************************************************
 *Write a vector to a file.  Writes everything in non-sparse format.
 *
 *INPUT: v: vector to write
 * out: pointer to file to write vector to
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(c)
 * SPARSE_LIST: O(c)
 *************************************************************************/

void crm114__vector_write_ns_fp(Vector *v, FILE *out) {
  int lastcol = -1, i, col;
  VectorIterator vit;

  if (!v || !out) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_write_ns_fp: null arguments.\n");
    }
    return;
  }

  vectorit_set_at_beg(&vit, v);
  while (!vectorit_past_end(vit, v)) {
    col = vectorit_curr_col(vit, v);
    for (i = lastcol+1; i < col; i++) {
      fprintf(out, "0 ");
    }
    lastcol = col;
    fprintf(out, "%f ", vectorit_curr_val(vit, v));
    vectorit_next(&vit, v);
  }
  for (i = lastcol+1; i < v->dim; i++) {
    fprintf(out, "0 ");
  }
}

/*************************************************************************
 *Write a vector to a file.  Writes everything (including non-sparse vectors!)
 *in sparse format.
 *
 *INPUT: v: vector to write
 * filname: file to write vector to
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(s)
 * SPARSE_LIST: O(s)
 *************************************************************************/

void crm114__vector_write_sp(Vector *v, char *filename) {
  FILE *out;

  out = fopen(filename, "w");
  if (!out) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_write_sp: bad output filename %s\n", filename);
    }
    return;
  }

  crm114__vector_write_sp_fp(v, out);

  fclose(out);
}

/*************************************************************************
 *Write a vector to a file.  Writes everything in sparse format.
 *
 *INPUT: v: vector to write
 * out: pointer to file to write vector to
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(s)
 * SPARSE_LIST: O(s)
 *************************************************************************/

void crm114__vector_write_sp_fp(Vector *v, FILE *out) {
  VectorIterator vit;

  if (!v || !out) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_write_sp_fp: null arguments.\n");
    }
    return;
  }

  vectorit_set_at_beg(&vit, v);
  while (!vectorit_past_end(vit, v)) {
    fprintf(out, "%u %lf\n", vectorit_curr_col(vit, v),
	    vectorit_curr_val(vit, v));
    vectorit_next(&vit, v);
  }
}

// write a vector to a text file that's portable storage
void crm114__vector_write_text_fp(char name[], Vector *v, FILE *fp)
{
  VectorIterator vit;

  fprintf(fp, TN_VECTOR " %s %s %s " TN_NZ " %d " TN_DIM " %u\n",
	  name,
	  vector_type_to_string(v->type),
	  v->compact ? TN_COMPACT : TN_PRECISE,
	  v->nz,
	  v->dim);
  vectorit_set_at_beg(&vit, v);
  while (!vectorit_past_end(vit, v)) {
    fprintf(fp, "%u " PRINTF_DOUBLE_FMT "\n", vectorit_curr_col(vit, v),
	    vectorit_curr_val(vit, v));
    vectorit_next(&vit, v);
  }
}

/*************************************************************************
 *Write a vector to a file.  Writes everything in binary format.
 *
 *INPUT: v: vector to write
 * filename: file to write vector to
 *
 *OUTPUT: number of bytes written.
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(s)
 * SPARSE_LIST: O(s)
 *************************************************************************/

size_t crm114__vector_write_bin(Vector *v, char *filename) {
  size_t size;

  FILE *fp = fopen(filename, "wb");
  if (!fp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_write_bin: Bad file name %s.\n", filename);
    }
    return 0;
  }
  size = crm114__vector_write_bin_fp(v, fp);
  fclose(fp);
  return size;
}

/*************************************************************************
 *Write a vector to a file.  Writes everything in binary format.
 *
 *INPUT: v: vector to write
 * fp: file to write vector to
 *
 *OUTPUT: number of bytes written
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(s)
 * SPARSE_LIST: O(s)
 *************************************************************************/

size_t crm114__vector_write_bin_fp(Vector *v, FILE *fp) {
  size_t size = sizeof(Vector)*fwrite(v, sizeof(Vector), 1, fp);

  if (!v || !fp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_write_bin_fp: null arguments.\n");
    }
    return 0;
  }

  switch(v->type) {
  case NON_SPARSE:
    {
      size += vector_write_bin_ns(v, fp);
      return size;
    }
  case SPARSE_ARRAY:
    {
      size += crm114__expanding_array_write(v->data.sparray, fp);
      return size;
    }
  case SPARSE_LIST:
    {
      size += crm114__list_write(v->data.splist, fp);
      return size;
    }
  default:
    {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "crm114__vector_write_bin_fp: unrecognized type\n");
      }
      return size;
    }
  }
}

//"private" function to write non-sparse vector to file in binary
static size_t vector_write_bin_ns(Vector *v, FILE *fp) {
  if (!v || !fp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_write_ns: null arguments.\n");
    }
    return 0;
  }
  if (v->type != NON_SPARSE) {
    return crm114__vector_write_bin_fp(v, fp);
  }

  if (v->compact) {
    return sizeof(int)*fwrite(v->data.nsarray.compact, sizeof(int), v->dim, fp);
  }

  return sizeof(double)*fwrite(v->data.nsarray.precise, sizeof(double), v->dim,
			       fp);
}

// read a vector from a text file
Vector *crm114__vector_read_text_fp(char expected_name[], FILE *fp)
{
  char vector_name[50 + 1];
  char type_str[20 + 1];
  char compact_str[10 + 1];
  Vector tmpv;
  Vector *v;
  VectorIterator vit;
  int i;
  unsigned int col;
  double d;

  if (fscanf(fp, " " TN_VECTOR " %s %s %s " TN_NZ " %d " TN_DIM " %u",
	     vector_name,
	     type_str,
	     compact_str,
	     &tmpv.nz,
	     &tmpv.dim) != 5)
    return NULL;
  if (strcmp(vector_name, expected_name) != 0
      || !string_to_vector_type(type_str, &tmpv.type)
      || !string_to_compact(compact_str, &tmpv.compact))
    return NULL;
  if ((v = crm114__vector_make_size(tmpv.dim, tmpv.type, tmpv.compact, 0))
      == NULL)
    return NULL;

  vectorit_set_at_beg(&vit, v);
  for (i = 0; i < tmpv.nz; i++) {
    if (fscanf(fp, " %u " SCANF_DOUBLE_FMT, &col, &d) != 2) {
      crm114__vector_free(v);
      return NULL;
    }
    crm114__vectorit_insert(&vit, col, d, v);
  }

  return v;
}

/*************************************************************************
 *Read a vector from a binary file.
 *
 *INPUT: filename: file to read vector from
 *
 *OUTPUT: vector in file or NULL if the file is incorrectly formatted
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(s)
 * SPARSE_LIST: O(s)
 *
 *WARNINGS:
 *1) This expects a binary file formatted as crm114__vector_write_bin does.  If
 *   the file is incorrectly formatted, this may return NULL or it may
 *   return some weird interpretation.  Check the output!
 *************************************************************************/

Vector *crm114__vector_read_bin(char *filename) {
  Vector *v;
  FILE *fp = fopen(filename, "rb");

  if (!fp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_read_bin: Bad file name %s.\n", filename);
    }
    return NULL;
  }

  v = crm114__vector_read_bin_fp(fp);
  fclose(fp);
  return v;
}

/*************************************************************************
 *Read a vector from a binary file.
 *
 *INPUT: fp: file to read vector from
 *
 *OUTPUT: vector in file or NULL if the file is incorrectly formatted
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(s)
 * SPARSE_LIST: O(s)
 *
 *WARNINGS:
 *1) This expects a binary file formatted as crm114__vector_write_bin does.  If
 *   the file is incorrectly formatted, this may return NULL or it may
 *   return some weird interpretation.  Check the output!
 *************************************************************************/

Vector *crm114__vector_read_bin_fp(FILE *fp) {
  Vector tmpv, *v;
  size_t amount_read;

  amount_read = fread(&tmpv, sizeof(Vector), 1, fp);
  if (!(amount_read)) {
    return NULL;
  }
  v = crm114__vector_make_size(tmpv.dim, tmpv.type, tmpv.compact, 0);
  if (!v) {
    return NULL;
  }
  v->nz = tmpv.nz;

  switch(v->type) {
  case NON_SPARSE:
    {
      vector_read_bin_ns(v, fp);
      return v;
    }
  case SPARSE_ARRAY:
    {
      if (v->nz && !v->data.sparray) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf
	    (stderr,
	     "warning: no space allocated for non-zero sparse array vector.\n");
	}
	v->nz = 0;
	return v;
      }
      crm114__expanding_array_read(v->data.sparray, fp);
      return v;
    }
  case SPARSE_LIST:
    {
      if (v->nz && !(v->data.splist)) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf
	    (stderr,
	     "warning: no space allocated for non-zero sparse list vector.\n");
	}
	v->nz = 0;
	return v;
      }
      v->nz = crm114__list_read(v->data.splist, fp, v->nz);
      return v;
    }
  default:
    {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "crm114__vector_read_bin_fp: unrecognized type.\n");
      }
      return v;
    }
  }
}

//private function to read a non-sparse vector from a binary file
static void vector_read_bin_ns(Vector *v, FILE *fp) {
  size_t amount_read = 0;

  if (v->type != NON_SPARSE) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "Called vector_read_bin_ns on non-sparse vector.\n");
    }
    return;
  }

  if (v->compact) {
    if (v->data.nsarray.compact) {
      amount_read = fread(v->data.nsarray.compact, sizeof(int), v->dim, fp);
    }
  } else {
    if (v->data.nsarray.precise) {
      amount_read = fread(v->data.nsarray.precise, sizeof(double), v->dim, fp);
    }
  }
  if (v->dim && !amount_read) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "Warning: nothing was read into non-sparse vector.\n");
    }
    v->dim = 0;
  }
}

/*****************************************************************************
 *Converts data stored at *addr into a vector.
 *
 *INPUT: addr: a pointer to the address at which the vector is stored.
 * last_addr: the last possible address that is valid.  NOT necessarily where
 *  the list ends - just the last address that has been allocated in the
 *  chunk pointed to by *addr (ie, if *addr was taken from an mmap'd file
 *  last_addr would be *addr + the file size).
 *
 *OUTPUT: A vector STILL referencing the chunk of memory pointed to by *addr
 *  although with its OWN, newly malloc'd row list.
 * *addr: (pass-by-reference) points to the first address AFTER the full
 *   vector.
 *WARNINGS:
 * 1) *addr needs to be writable.  This will CHANGE VALUES stored at *addr and
 *    will seg fault if addr is not writable.
 * 2) last_addr does not need to be the last address of the list
 *    but if it is before that, either NULL will be returned or a
 *    vector with a NULL data value will be returned.
 * 3) if *addr does not contain a properly formatted vector, this function
 *    will not seg fault, but that is the only guarantee.
 * 4) you MUST call crm114__vector_free on this vector AS WELL AS freeing memory
 *    stored at *addr.
 * 5) *addr CHANGES!
 * 6) This was one of the last functions I added to the library and one of the
 *    likeliest to cause memory errors and seg faults.  I've done a good bit
 *    of testing on it, but this function and memory-mapped objects in
 *    general (specifically I'd be most suspicious of SPARSE_LIST's and
 *    NON_SPARSE vectors since they aren't actually mapped anywhere in the
 *    SVM implementation although, of course, I did test them separately)
 *    are the likeliest to break the library.  I appologize.
 ****************************************************************************/

Vector *crm114__vector_map(void **addr, void *last_addr) {
  Vector *v;

  if (!addr || !*addr || !last_addr || *addr >= last_addr) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_map: null arguments.\n");
    }
    return NULL;
  }

  if ((void *)((Vector *)*addr + 1) > last_addr) {
    return NULL;
  }

  //tmpv = (Vector *)(*addr);
  v = (Vector *)(*addr);
  *addr = v + 1;
  v->was_mapped = 1;

  switch (v->type) {
  case NON_SPARSE:
    {
      if (v->compact) {
	if (v->dim > 0 && (void *)((int *)*addr + v->dim) <= last_addr) {
	  v->data.nsarray.compact = (int *)(*addr);
	  *addr = (int *)*addr + v->dim;
	} else {
	  if (v->dim && CRM114__MATR_DEBUG_MODE) {
	    fprintf
	      (stderr,
	       "warning: no space allocated for non-sparse vector data.\n");
	  }
	  v->data.nsarray.compact = NULL;
	}
	return v;
      }
      if (v->dim > 0 && (void *)((double *)*addr + v->dim) <= last_addr) {
	v->data.nsarray.precise = (double *)(*addr);
	*addr = (double *)*addr + v->dim;
      } else {
	if (v->dim && CRM114__MATR_DEBUG_MODE) {
	  fprintf
	    (stderr,
	     "warning: no space allocated for non-sparse vector data.\n");
	}
	v->data.nsarray.precise = NULL;
      }
      return v;
    }
  case SPARSE_ARRAY:
    {
      v->data.sparray = crm114__expanding_array_map(addr, last_addr);
      if (v->nz && !v->data.sparray) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf
	    (stderr,
	     "warning: no space allocated for non-zero sparse array vector.\n");
	}
	v->nz = 0;
      }
      return v;
    }
  case SPARSE_LIST:
    {
      v->data.splist = crm114__list_map(addr, last_addr, &(v->nz));
      if (!v->data.splist && CRM114__MATR_DEBUG_MODE) {
	fprintf
	  (stderr,
	   "warning: no space allocated for non-zero sparse list vector.\n");
      }
      return v;
    }
  default:
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vector_map: unrecognized type.\n");
    }
    return v;
  }
}

/***************************************************************************
 *Copies a vector into a chunk of memory. to will not be completely identical
 * to from since pointer values will change, etc.  Rather to is a contiguous
 * form of from in memory.  to can be treated on return as a vector and
 * should be freed with crm114__vector_free as well as freeing the chunk of memory
 * it is part of.  As with memmove this function does not actually "move"
 * anything out of from.
 *
 *INPUT: to: a block of memory with enough memory to hold the entire vector
 *  stored in from.
 * from: the vector to be copied from.
 *
 *OUTPUT: A pointer to the first address AFTER the data was copied.  In
 * other words this returns to + size(from) where size(from) is the size
 * in bytes of the full vector from.
 *
 *WARNINGS:
 * 1) to needs to be writable.  This will CHANGE VALUES stored at to and
 *    will seg fault if to is not writable.
 * 2) this does NOT CHECK FOR OVERFLOW.  to must have enough memory
 *    already to contain from or this can cause a seg fault.
 * 3) unlike with memmove, this is not a completely byte-by-byte copy.
 *    instead, to is a copy of the vector from stored contiguously at to
 *    with the same functionality as from.  in other words, to can be
 *    treated as a vector.
 * 4) you should call crm114__vector_free on to unless you are CERTAIN you
 *    have not changed it.  calling crm114__vector_free on an unchanged list
 *    will not do anything.
 * 5) This was one of the last functions I added to the library and one of the
 *    likeliest to cause memory errors and seg faults.  I've done a good bit
 *    of testing on it, but this function and memory-mapped objects in
 *    general are the likeliest to break the library.  I appologize.
 ***************************************************************************/

void *crm114__vector_memmove(Vector *to, Vector *from) {
#define PA_C 1
#define PA_EMULATED 0
#define POINTER_ARITHMETIC PA_C
  *to = *from;
  to->was_mapped = 1;
  switch (from->type) {
  case NON_SPARSE:
    {
      if (from->compact && from->data.nsarray.compact) {
	to->data.nsarray.compact = (int *)(to + 1);
	(void)memmove(to + 1, from->data.nsarray.compact,
			 sizeof(int)*from->dim);
#if POINTER_ARITHMETIC == PA_C
	return ((int *)(to + 1) + from->dim);
#else
	return ((char *)(to + 1) + sizeof(int)*from->dim);
#endif
      }
      if (!(from->compact) && from->data.nsarray.precise) {
	to->data.nsarray.precise = (double *)(to + 1);
	(void)memmove(to + 1, from->data.nsarray.precise,
			 sizeof(double)*from->dim);
#if POINTER_ARITHMETIC == PA_C
	return ((double *)(to + 1) + from->dim);
#else
	return ((char *)(to + 1) + sizeof(double)*from->dim);
#endif
      }
      return to + 1;
    }
  case SPARSE_ARRAY:
    {
      if (from->data.sparray) {
	to->data.sparray = (ExpandingArray *)(to + 1);
	*to->data.sparray = *from->data.sparray;
	to->data.sparray->was_mapped = 1;
	if (from->compact && from->data.sparray->data.compact) {
	  (void)memmove
	    ((char *)(to + 1) + sizeof(ExpandingArray),
	     &(from->data.sparray->data.compact[from->data.sparray->first_elt]),
	     sizeof(CompactExpandingType)*(from->data.sparray->n_elts));
#if POINTER_ARITHMETIC == PA_C
	  return ((CompactExpandingType *)((ExpandingArray *)(to + 1) + 1)
		  + from->data.sparray->n_elts);
#else
	  return ((char *)(to + 1) + sizeof(ExpandingArray) +
		  sizeof(CompactExpandingType)*(from->data.sparray->n_elts));
#endif
	}
	if (!(from->compact) && from->data.sparray->data.precise) {
	  (void)memmove
	    ((char *)(to + 1) + sizeof(ExpandingArray),
	     &(from->data.sparray->data.precise[from->data.sparray->first_elt]),
	     sizeof(PreciseExpandingType)*(from->data.sparray->n_elts));
#if POINTER_ARITHMETIC == PA_C
	  return ((PreciseExpandingType *)((ExpandingArray *)(to + 1) + 1)
		  + from->data.sparray->n_elts);
#else
	  return ((char *)(to + 1) + sizeof(ExpandingArray) +
		  sizeof(PreciseExpandingType)*(from->data.sparray->n_elts));
#endif
	}
	return (char *)(to + 1) + sizeof(ExpandingArray);
      }
      return to + 1;
    }
  case SPARSE_LIST:
    {
      if (from->data.splist) {
	to->data.splist = (SparseElementList *)(to + 1);
	return crm114__list_memmove((SparseElementList *)(to + 1), from->data.splist);
      }
      return to + 1;
    }
  default:
    {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "crm114__vector_memmove: unrecognized type.\n");
      }
      return NULL;
    }
  }
#undef POINTER_ARITHMETIC
#undef PA_EMULATED
#undef PA_C
}

/***************************************************************************
 *The full size of the vector in bytes.  This is the size a binary file will
 *be or a chunk of contiguous memory needs to be to contain v.
 *
 *INPUT: v: vector from which to get the size.
 *
 *OUTPUT: The size in bytes of v.  If you want to copy v into contiguous
 * memory, for example, the following bit of code would work:
 *   copy = malloc(sizeof(v))
 *   crm114__vector_memmove(copy, v)
 ****************************************************************************/
size_t crm114__vector_size(Vector *v) {

  if (!v) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "warning: null vector in crm114__vector_size.\n");
    }
    return 0;
  }

  switch (v->type) {
  case NON_SPARSE:
    {
      if (v->compact && v->data.nsarray.compact) {
	return sizeof(Vector) + v->dim*sizeof(int);
      }
      if (!(v->compact) && v->data.nsarray.precise) {
	return sizeof(Vector) + v->dim*sizeof(double);
      }
      return sizeof(Vector);
    }
  case SPARSE_ARRAY:
    {
      if (v->data.sparray) {
	if (v->compact && v->data.sparray->data.compact) {
	  return sizeof(Vector) + sizeof(ExpandingArray) +
	    sizeof(CompactExpandingType)*v->data.sparray->n_elts;
	}
	if (!(v->compact) && v->data.sparray->data.precise) {
	  return sizeof(Vector) + sizeof(ExpandingArray) +
	    sizeof(PreciseExpandingType)*v->data.sparray->n_elts;
	}
	return sizeof(Vector) + sizeof(ExpandingArray);
      }
      return sizeof(Vector);
    }
  case SPARSE_LIST:
    {
      if (v->data.splist) {
	if (v->compact) {
	  return sizeof(Vector) + sizeof(SparseElementList) +
	    sizeof(CompactSparseNode)*v->nz;
	}
	return sizeof(Vector) + sizeof(SparseElementList) +
	  sizeof(PreciseSparseNode)*v->nz;
      }
      return sizeof(Vector);
    }
  default:
    {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "crm114__vector_size: unrecognized type.\n");
      }
      return 0;
    }
  }
}

/*************************************************************************
 *Frees all memory associated with a vector and the vector itself.
 *
 *INPUT: v: vector to free
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(s)
 *************************************************************************/

void crm114__vector_free(Vector *v) {
  if (!v) {
    return;
  }
  switch (v->type) {
  case NON_SPARSE:
    {
      if (v->compact) {
	if (v->data.nsarray.compact &&
	    (!(v->was_mapped) ||
	     (void *)(v + 1) != (void *)(v->data.nsarray.compact))) {
	  free(v->data.nsarray.compact);
	}
      } else {
	if (v->data.nsarray.precise &&
	    (!(v->was_mapped) ||
	     (void *)(v + 1) != (void *)(v->data.nsarray.precise))) {
	  free(v->data.nsarray.precise);
	}
      }
      break;
    }
  case SPARSE_ARRAY:
    {
      if (v->was_mapped) {
	//if the array grew in size, it's possible that v was mapped
	//but the array was not
	crm114__expanding_array_free_data(v->data.sparray);
      } else {
	crm114__expanding_array_free(v->data.sparray);
      }
      break;
    }
  case SPARSE_LIST:
    {
      crm114__list_clear(v->data.splist);
      if (!(v->was_mapped)) {
	free(v->data.splist);
      }
      break;
    }
  default:
    {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "crm114__vector_free: unrecognized type.\n");
      }
      break;
    }
  }

  if (!v->was_mapped) {
    free(v);
  }
}

/**************************************************************************
 *The vector iterator class allows you to move along a vector without
 *knowing anything about its underlying data structure.  This is very
 *much modeled after C++ iterators.
 *
 *All vector iterators are bi-directional.
 *************************************************************************/

//Static vector iterator function declarations
static void vectorit_set(VectorIterator vit, double d, Vector *v);
static void vectorit_insert_before(VectorIterator *vit, unsigned int c,
				   double d, Vector *v);
static void vectorit_insert_after(VectorIterator *vit, unsigned int c,
				  double d, Vector *v);

//"private" function to set an element of a vector using an iterator
//this is private because where the iterator will point if d = 0 is
//data structure dependent.  to set an element of a vector, you should
//use crm114__vectorit_zero_elt or crm114__vectorit_insert.
static void vectorit_set(VectorIterator vit, double d, Vector *v) {
  SparseNode n;

  if (!v) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vectorit_insert: null vector.\n");
    }
    return;
  }

  switch (v->type) {
  case SPARSE_LIST:
    {
      if (!v->data.splist) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vectorit_insert: null vector.\n");
	}
	return;
      }
      n.is_compact = v->compact;
      n.precise = vit.pcurr;
      n.compact = vit.ccurr;
      if (!null_node(n)) {
	node_set_data(n, d);
	if (fabs(d) < MATRIX_EPSILON) {
	  //get rid of this value
	  crm114__list_remove_elt(v->data.splist, n);
	  v->nz--;
	}
      }
      break;
    }
  case SPARSE_ARRAY:
    {
      if (!v->data.sparray) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vectorit_insert: null vector.\n");
	}
	return;
      }
      if (vit.nscurr >= v->data.sparray->first_elt &&
	  vit.nscurr <= v->data.sparray->last_elt) {
	if (v->compact && v->data.sparray->data.compact) {
	  v->data.sparray->data.compact[vit.nscurr].s.data = (int)d;
	} else if (!(v->compact) && v->data.sparray->data.precise) {
	  v->data.sparray->data.precise[vit.nscurr].s.data = d;
	} else {
	  if (CRM114__MATR_DEBUG_MODE) {
	    fprintf(stderr, "crm114__vectorit_insert: null vector.\n");
	  }
	  return;
	}
	if (fabs(d) < MATRIX_EPSILON) {
	  crm114__expanding_array_remove_elt(vit.nscurr - v->data.sparray->first_elt,
				     v->data.sparray);
	  v->nz--;
	}
      }
      break;
    }
  case NON_SPARSE:
    {
      if (vit.nscurr >= 0 && vit.nscurr < v->dim) {
	if (v->compact) {
	  if (!(v->data.nsarray.compact)) {
	    if (CRM114__MATR_DEBUG_MODE) {
	      fprintf(stderr, "crm114__vectorit_insert: null vector.\n");
	    }
	    return;
	  }
	  v->data.nsarray.compact[vit.nscurr] = (int)d;
	} else {
	  if (!(v->data.nsarray.precise)) {
	    if (CRM114__MATR_DEBUG_MODE) {
	      fprintf(stderr, "crm114__vectorit_insert: null vector.\n");
	    }
	    return;
	  }
	  v->data.nsarray.precise[vit.nscurr] = d;
	}
      }
      break;
    }
  default:
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "vectorit_set: unrecognized type.\n");
    }
    return;
  }
}



/*************************************************************************
 *Set the element vit points to to zero.  vit will then point to the
 *NEXT element or past_end.  Note that if v is NON_SPARSE, this STILL
 *moves vit so as to have the same behavior with every data structure.
 *
 *INPUT: vit: the iterator
 * v: the vector vit is traversing
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY: O(s)
 * SPARSE_LIST: O(1)
 *
 *WARNINGS:
 *1) Regardless of data type, vit points to the NEXT ELEMENT after the
 *   zero'd one!
 *************************************************************************/

void crm114__vectorit_zero_elt(VectorIterator *vit, Vector *v) {
  VectorIterator tmpit;
  unsigned int currcol = vectorit_curr_col(*vit, v);

  if (!v || !vit) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vectorit_zero_elt: null arguments.\n");
    }
    if (vit) {
      vit->nscurr = -1;
    }
    return;
  }

  vectorit_copy(*vit, &tmpit);
  vectorit_next(&tmpit, v);
  vectorit_set(*vit, 0, v);

  //we need to wind up pointing to the next element
  //depending on how the delete is done
  //this is either the same element we were pointing at (prev(tmpit))
  //or the next element (tmpit)
  //so we check both
  vectorit_copy(tmpit, vit);
  vectorit_prev(&tmpit, v);
  if ((!vectorit_past_beg(tmpit, v) &&
       vectorit_curr_col(tmpit, v) > currcol)) {
    vectorit_copy(tmpit, vit);
  }
}

/*************************************************************************
 *Set the column of the element vit points to.  NOT IMPLEMENTED for
 *NON_SPARSE vectors.
 *
 *INPUT: vit: the iterator
 * c: the new column value
 * v: the vector vit is traversing
 *
 *TIME:
 * NON_SPARSE: NOT IMPLEMENTED
 * SPARSE_ARRAY: O(1)
 * SPARSE_LIST: O(1)
 *
 *WARNINGS:
 *1) This does NOT move elements in v around.  If changing the column
 *   number would mess up the order of the elements, then this prints
 *   an error message and DOES NOT DO IT.  So check first.
 *************************************************************************/

void crm114__vectorit_set_col(VectorIterator vit, unsigned int c, Vector *v) {
  SparseNode n;

  if (!v) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vectorit_set_col: null vector.\n");
    }
    return;
  }

  switch (v->type) {
  case NON_SPARSE:
    {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr,
		"crm114__vectorit_set_col: not implemented for non-sparse matrices.\n");
      }
      return;
    }
  case SPARSE_LIST:
    {
      n.is_compact = v->compact;
      n.precise = vit.pcurr;
      n.compact = vit.ccurr;
      if (null_node(n)) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr,
		  "crm114__vectorit_set_col: Attempt to set uninitiated iterator.\n");
	}
	return;
      }
      if (!null_node(prev_node(n)) && node_col(prev_node(n)) >= c) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vectorit_set_col: invalid column number in list.\n");
	}
	return;
      }

      if (!null_node(next_node(n)) && node_col(next_node(n)) <= c) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vectorit_set_col: invalid column number in list.\n");
	}
	return;
      }

      node_set_col(n, c);
      break;
    }
  case SPARSE_ARRAY:
    {
      if (!v->data.sparray || (v->compact && !(v->data.sparray->data.compact))
	  || (!(v->compact) && !(v->data.sparray->data.precise))) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vectorit_set_col: null vector.\n");
	}
	return;
      }
      if (vit.nscurr < v->data.sparray->first_elt ||
	  vit.nscurr > v->data.sparray->last_elt) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr,
		  "crm114__vectorit_set_col: Attempt to set uninitiated iterator.\n");
	}
	return;
      }
      if ((vit.nscurr-1 >= v->data.sparray->first_elt &&
	   ((v->compact &&
	     v->data.sparray->data.compact[vit.nscurr-1].s.col >= c) ||
	    (!v->compact &&
	     v->data.sparray->data.precise[vit.nscurr-1].s.col >= c))) ||
	  (vit.nscurr+1 <= v->data.sparray->last_elt &&
	   ((v->compact &&
	     v->data.sparray->data.compact[vit.nscurr+1].s.col <= c) ||
	    (!v->compact &&
	     v->data.sparray->data.precise[vit.nscurr+1].s.col <= c)))) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr,
		  "crm114__vectorit_set_col: invalid column number in array at space %ld column number is %u and it is ", vit.nscurr, c);
	  if (vit.nscurr - 1 >= v->data.sparray->first_elt) {
	    if ((v->compact &&
		 v->data.sparray->data.compact[vit.nscurr-1].s.col >= c)) {
	      fprintf(stderr, " less than space %ld with the column number %u before it.\n",
		      vit.nscurr-1, v->data.sparray->data.compact[vit.nscurr-1] .s.col);
	    }
	    if (!(v->compact) &&
		v->data.sparray->data.precise[vit.nscurr-1].s.col >= c) {
	      fprintf(stderr, " less than space %ld with the column number %u before it.\n",
		      vit.nscurr-1, v->data.sparray->data.precise[vit.nscurr-1].s.col);
	    }
	  }
	  if (vit.nscurr + 1 <= v->data.sparray->last_elt) {
	    if ((v->compact &&
		 v->data.sparray->data.compact[vit.nscurr+1].s.col <= c)) {
	      fprintf(stderr, " less than the column number %u before it.\n",
		      v->data.sparray->data.compact[vit.nscurr+1].s.col);
	    }
	    if (!(v->compact) &&
		v->data.sparray->data.precise[vit.nscurr+1].s.col <= c) {
	      fprintf(stderr, " greater than the column number %u before it.\n",
		      v->data.sparray->data.precise[vit.nscurr+1].s.col);
	    }
	  }
	}
	return;
      }
      if (v->compact) {
	v->data.sparray->data.compact[vit.nscurr].s.col = c;
      } else {
	v->data.sparray->data.precise[vit.nscurr].s.col = c;
      }
      break;
    }
  default:
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vectorit_set_col: unrecognized type.\n");
    }
    return;
  }
}



/*************************************************************************
 *Insert a new value or set an old value of v using the iterator.
 *
 *INPUT: vit: an iterator that serves as an initial guess as to where
 * the value should be inserted.
 * c: the column
 * d: the data
 * v: the vector vit is traversing
 *
 *OUTPUT: vit will point to the element that has been inserted or the NEXT
 * element if d = 0 and the vector is SPARSE.  If you insert a zero element
 * into an already 0 spot, vit will point to EITHER the PREVIOUS or the
 * NEXT element if the vector is SPARSE.
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY:
 *  Generally: O(s)
 *  Setting an element, no initial guess: amortized O(lg(s))
 *  Setting first/last element or good initial guess: O(1)
 *  Inserting at front or end of vector: amortized O(1)
 * SPARSE_LIST:
 *  Generally: O(s)
 *  Good initial guess, inserting or setting first/last element: O(1)
 *
 *WARNINGS:
 *1) If d = 0, the iterator will point to the NEXT ELEMENT only if the
 *   vector is SPARSE!
 *2) Where the vector points if d = 0 and that spot was ALREADY 0 and the
 *   vector is SPARSE is undetermined - it will either be BEFORE or AFTER
 *   the spot you tried to insert.  This is a weird case.
 *************************************************************************/

void crm114__vectorit_insert(VectorIterator *vit, unsigned int c, double d, Vector *v) {

  if (!v || !vit) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vectorit_insert: null arguments.\n");
    }
    if (vit) {
      vit->nscurr = -1;
    }
    return;
  }

  if (c >= v->dim) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vectorit_insert: invalid column number %u.\n", c);
    }
    return;
  }

  if (v->type == NON_SPARSE) {
    //we have constant time access to elements
    if (v->compact) {
      if (!(v->data.nsarray.compact)) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vectorit_insert: null vector.\n");
	}
	return;
      }
      v->data.nsarray.compact[c] = d;
    } else {
      if (!(v->data.nsarray.precise)) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vectorit_insert: null vector.\n");
	}
	return;
      }
      v->data.nsarray.precise[c] = d;
    }
    vit->nscurr = c;
    return;
  }

  crm114__vectorit_find(vit, c, v);

  if (!vectorit_past_end(*vit, v) && !vectorit_past_beg(*vit, v) &&
      (vectorit_curr_col(*vit, v) == c)) {
    if (fabs(d) < MATRIX_EPSILON && vectorit_curr_col(*vit, v) == c) {
      if (CRM114__MATR_DEBUG_MODE >= MATR_OPS_MORE) {
	fprintf(stderr, "zeroing column %u type = %d\n",
		vectorit_curr_col(*vit, v), v->type);
      }
      crm114__vectorit_zero_elt(vit, v);
      return;
    }
  }
  if (vectorit_curr_col(*vit, v) == c) {
    if (CRM114__MATR_DEBUG_MODE >= MATR_OPS_MORE) {
      fprintf(stderr, "setting column %u to be %f type = %d\n",
	     vectorit_curr_col(*vit, v), d, v->type);
    }
    vectorit_set(*vit, d, v);
  } else if (fabs(d) > MATRIX_EPSILON) {
    if (vectorit_past_beg(*vit, v) || vectorit_curr_col(*vit, v) < c) {
      if (CRM114__MATR_DEBUG_MODE >= MATR_OPS_MORE) {
	if (vectorit_past_beg(*vit, v)) {
	  fprintf(stderr, "inserting %lf in first position.\n", d);
	} else {
	  fprintf(stderr, "inserting %lf after column %u type = %d\n",
		  d, vectorit_curr_col(*vit, v), v->type);
	}
      }
      vectorit_insert_after(vit, c, d, v);
    } else {
      if (CRM114__MATR_DEBUG_MODE >= MATR_OPS_MORE) {
	fprintf(stderr, "inserting %lf before column %u type = %d\n",
	       d, vectorit_curr_col(*vit, v), v->type);
      }
      vectorit_insert_before(vit, c, d, v);
    }
  }
}

/*************************************************************************
 *Find a column in v.
 *
 *INPUT: vit: an iterator that serves as an initial guess as to where
 * the column is in the data structure.
 * c: the column
 * v: the vector vit is traversing
 *
 *OUTPUT: vit will point to the element with column number c if it exists.
 * If it does NOT exist, vit will point to either the last element with
 * a column number less than c or the first element with a column number
 * greater than c.  If c is less than all column numbers in the vector,
 * vit may be past_beg.  Similarly, if c is greater than all column numbers,
 * vit may be past_end.
 *
 *TIME:
 * NON_SPARSE: O(1)
 * SPARSE_ARRAY:
 *  Generally: O(lg(s))
 *  Before/after/equals first/last element or good initial guess: O(1)
 * SPARSE_LIST:
 *  Generally: O(s)
 *  Before/after/equals first/last element or good initial guess: O(1)
 *************************************************************************/

void crm114__vectorit_find(VectorIterator *vit, unsigned int c, Vector *v) {

  SparseNode n;

  if (!v || !vit) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vectorit_find: null arguments.\n");
    }
    if (vit) {
      vit->nscurr = -1;
    }
    return;
  }

  switch (v->type) {
  case NON_SPARSE:
    {
      vit->nscurr = c;
      break;
    }
  case SPARSE_ARRAY:
    {
      if (!v->data.sparray) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vectorit_find: null vector.\n");
	}
	return;
      }
      vit->nscurr = crm114__expanding_array_search(c,
					   vit->nscurr -
					   v->data.sparray->first_elt,
					   v->data.sparray)
	+ v->data.sparray->first_elt;
      break;
    }
  case SPARSE_LIST:
    {
      if (!v->data.splist) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vectorit_find: null vector.\n");
	}
	return;
      }
      n.is_compact = v->compact;
      n.compact = vit->ccurr;
      n.precise = vit->pcurr;
      n = crm114__list_search(c, n, v->data.splist);
      if (null_node(n)) {
	if (null_node((v->data.splist->head)) ||
	    c < node_col((v->data.splist->head))) {
	  n = v->data.splist->head;
	} else {
	  n = v->data.splist->tail;
	}
      }
      if (v->compact) {
	vit->ccurr = n.compact;
      } else {
	vit->pcurr = n.precise;
      }
      break;
    }
  default:
    {
      vit->nscurr = -1;
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "crm114__vectorit_find: unrecognized type.\n");
      }
      return;
    }
  }
}

//always call crm114__vectorit_insert NOT these!
//these are for the use of crm114__vectorit_insert
//they assume that c fits correctly before/after vit
//and they do not check for it
//if you are correctly inserting before or after the iterator passed
//to insert, it will recognize that and call these in O(1) time
//these functions are "private"
static void vectorit_insert_before(VectorIterator *vit, unsigned int c,
				   double d, Vector *v) {
  PreciseSparseElement pnewelt;
  PreciseExpandingType pet;
  CompactSparseElement cnewelt;
  CompactExpandingType cet;
  ExpandingType ne;
  SparseElement newelt;
  SparseNode n;

  if (!v || !vit) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vectorit_insert: null arguments.\n");
    }
    if (vit) {
      vit->nscurr = -1;
    }
    return;
  }

  if (v->compact) {
    cnewelt.data = (int)d;
    cnewelt.col = c;
    cet.s = cnewelt;
    ne.compact = &cet;
    newelt.compact = &cnewelt;
  } else {
    pnewelt.data = d;
    pnewelt.col = c;
    pet.s = pnewelt;
    ne.precise = &pet;
    newelt.precise = &pnewelt;
  }

  if (c >= v->dim) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "vectorit_insert_before: bad index.\n");
    }
    return;
  }

  switch (v->type) {
  case NON_SPARSE:
    {
      vit->nscurr = c;
      if (v->compact) {
	if (!(v->data.nsarray.compact)) {
	  if (CRM114__MATR_DEBUG_MODE) {
	    fprintf(stderr, "crm114__vectorit_insert: null vector.\n");
	  }
	  return;
	}
	v->data.nsarray.compact[c] = (int)d;
      } else {
	if (!(v->data.nsarray.precise)) {
	  if (CRM114__MATR_DEBUG_MODE) {
	    fprintf(stderr, "crm114__vectorit_insert: null vector.\n");
	  }
	  return;
	}
	v->data.nsarray.precise[c] = d;
      }
      break;
    }
  case SPARSE_ARRAY:
    {
      if (!v->data.sparray) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vectorit_insert: null vector.\n");
	}
	return;
      }
      if (vit->nscurr > v->data.sparray->last_elt && v->data.sparray->n_elts) {
	vectorit_set_at_end(vit, v);
	vectorit_insert_after(vit, c, d, v);
	return;
      }
      vit->nscurr -= v->data.sparray->first_elt;
      vit->nscurr = crm114__expanding_array_insert_before(ne, vit->nscurr,
						  v->data.sparray);
      v->nz++;
      break;
    }
  case SPARSE_LIST:
    {
      if (!v->data.splist) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vectorit_insert: null vector.\n");
	}
	return;
      }
      if (!crm114__list_is_empty(v->data.splist) &&
	  c > node_col(v->data.splist->tail)) {
	//this is easier
	vectorit_set_at_end(vit, v);
	vectorit_insert_after(vit, c, d, v);
	return;
      }
      n.is_compact = v->compact;
      n.compact = vit->ccurr;
      n.precise = vit->pcurr;
      if (null_node(n)) {
	n = (v->data.splist->head);
      }
      n = crm114__list_insert_before(newelt, n, v->data.splist);
      if (v->compact) {
	vit->ccurr = n.compact;
      } else {
	vit->pcurr = n.precise;
      }
      v->nz++;
      break;
    }
  default:
    vit->nscurr = -1;
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "vectorit_insert_before: unrecognized type.\n");
    }
    break;
  }

}

static void vectorit_insert_after(VectorIterator *vit, unsigned int c,
				  double d, Vector *v) {
  PreciseSparseElement pnewelt;
  PreciseExpandingType pet;
  CompactSparseElement cnewelt;
  CompactExpandingType cet;
  ExpandingType et;
  SparseElement newelt;
  SparseNode n;

  if (!v || !vit) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__vectorit_insert: null arguments.\n");
    }
    if (vit) {
      vit->nscurr = -1;
    }
    return;
  }

  if (v->compact) {
    cnewelt.data = (int)d;
    cnewelt.col = c;
    cet.s = cnewelt;
    et.compact = &cet;
    newelt.compact = &cnewelt;
  } else {
    pnewelt.data = d;
    pnewelt.col = c;
    pet.s = pnewelt;
    et.precise = &pet;
    newelt.precise = &pnewelt;
  }

  if (c >= v->dim) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "vectorit_insert_after: bad index.\n");
    }
    return;
  }

  switch (v->type) {
  case NON_SPARSE:
    {
      vit->nscurr = c;
      if (v->compact) {
	if (!(v->data.nsarray.compact)) {
	  if (CRM114__MATR_DEBUG_MODE) {
	    fprintf(stderr, "crm114__vectorit_insert: null vector.\n");
	  }
	  return;
	}
	v->data.nsarray.compact[c] = (int)d;
      } else {
	if (!(v->data.nsarray.precise)) {
	  if (CRM114__MATR_DEBUG_MODE) {
	    fprintf(stderr, "crm114__vectorit_insert: null vector.\n");
	  }
	  return;
	}
	v->data.nsarray.precise[c] = d;
      }
      break;
    }
  case SPARSE_ARRAY:
    {
      if (!v->data.sparray) {
	if (CRM114__MATR_DEBUG_MODE) {
	  fprintf(stderr, "crm114__vectorit_insert: null vector.\n");
	}
	return;
      }

      if (vit->nscurr < v->data.sparray->first_elt && v->data.sparray->n_elts) {
	vectorit_set_at_beg(vit, v);
	vectorit_insert_before(vit, c, d, v);
	return;
      }
      vit->nscurr -= v->data.sparray->first_elt;
      vit->nscurr = crm114__expanding_array_insert_after(et, vit->nscurr,
						 v->data.sparray);
      v->nz++;
      break;
    }
  case SPARSE_LIST:
    {
      if (!v->data.splist) {
	if (CRM114__MATR_DEBUG_MODE) {
	    fprintf(stderr, "crm114__vectorit_insert: null vector.\n");
	}
	return;
      }
      if (!crm114__list_is_empty(v->data.splist) &&
	  c < node_col(v->data.splist->head)) {
	//this is easier
	vectorit_set_at_beg(vit, v);
	vectorit_insert_before(vit, c, d, v);
	return;
      }
      n.is_compact = v->compact;
      n.compact = vit->ccurr;
      n.precise = vit->pcurr;
      if (null_node(n)) {
	n = (v->data.splist->tail);
      }
      n = crm114__list_insert_after(newelt, n, v->data.splist);
      if (v->compact) {
	vit->ccurr = n.compact;
      } else {
	vit->pcurr = n.precise;
      }
      v->nz++;
      break;
    }
  default:
    {
      vit->nscurr = -1;
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "vectorit_insert_after: unrecognized type.\n");
      }
      return;
    }
  }
}

//This function SHOULD NOT BE USED
//for anything but checking answers
//If you want to multiply matrices together, implement somthing smarter
//than this.
void crm114__matr_multiply(Matrix *M1, Matrix *M2, Matrix *ret) {
  unsigned int i, j, k;
  double s;

  if (M1->rows < ret->rows) {
    fprintf(stderr, "crm114__matr_multiply: Attempt to multiply more rows than matrix has.\n");
    return;
  }
  if (M2->cols < ret->cols) {
    fprintf(stderr, "crm114__matr_multiply: Attempt to multiply more columns than matrix has.\n");
    return;
  }
  if (M1->cols != M2->rows) {
    fprintf(stderr, "crm114__matr_multiply: Mismatch in inner dimensions.\n");
    return;
  }


  for (i = 0; i < ret->rows; i++) {
    for (j = 0; j < ret->cols; j++) {
      s = 0;
      for (k = 0; k < M1->cols; k++) {
	s += matr_get(M1, i, k)*matr_get(M2, k, j);
      }
      matr_set(ret, i, j, s);
    }
  }
}
