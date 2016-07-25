//	crm114_matrix_util.c - Support Vector Machine

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


#include "crm114_matrix_util.h"

/************************************************************************
 *Expanding array and linked list functions.  Mostly for use with the
 *matrix library, but possibly more general.
 ************************************************************************/

/***********************Expanding Array Functions***************************/

//Static expanding array function declarations
static void expand(ExpandingArray *A, int newsize);

/***************************************************************************
 *Make a new expanding array with nothing in it
 *
 *INPUT: init_size: the initial size of the array
 * compact: COMPACT or PRECISE, deciding the size of the data in the array
 *
 *OUTPUT: an expanding array
 ***************************************************************************/

ExpandingArray *crm114__make_expanding_array(int init_size, int compact) {
  ExpandingArray *A = (ExpandingArray *)malloc(sizeof(ExpandingArray));

  if (!A) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "Could not create expanding array.\n");
    }
    return NULL;
  }

  if (init_size < 0) {
    init_size = 0;
  }

  A->length = init_size;

 if (!compact) {
    A->data.precise =
      (PreciseExpandingType *)malloc(sizeof(PreciseExpandingType)*init_size);
    A->compact = 0;
    if (!A->data.precise) {
      A->length = 0;
    }
  } else {
    A->data.compact =
      (CompactExpandingType *)malloc(sizeof(CompactExpandingType)*init_size);
    A->compact = 1;
    if (!A->data.compact) {
      A->length = 0;
    }
  }
  A->last_elt = -1;
  A->first_elt = 0; //the first time we do an insert_before we center the array
  A->n_elts = 0;
  A->was_mapped = 0;
  return A;
}


/***************************************************************************
 *Puts an element into the next open spot in the array, doubling the size
 *of the array if needed.
 *
 *INPUT: d: element to insert
 * A: array in which to insert the element
 *
 *WARNINGS:
 *1) This just puts an element into the next open spot in A.  If d is some
 *   sort of sparse element, this never checks to make sure A is still in
 *   ascending column order.  Use the matrix functions to insert things
 *   instead if you want that order preserved.
 ***************************************************************************/

void crm114__expanding_array_insert(ExpandingType d, ExpandingArray *A) {
  if (!A) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__expanding_array_insert: null array.\n");
    }
    return;
  }

  A->last_elt++;
  if (A->last_elt >= A->length) {
    if (A->length == 0) {
      A->length = 1;
    }
    expand(A, 2*A->length);
    if (!(A->length)) {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf
	  (stderr,
	   "crm114__expanding_array_insert: unable to expand array enough to do insert.\n");
      }
      return;
    }
  }

  if (A->compact) {
    A->data.compact[A->last_elt] = *(d.compact);
  } else {
    A->data.precise[A->last_elt] = *(d.precise);
  }
  A->n_elts++;
}

/***************************************************************************
 *Inserts an element into position c of A where c is relative to first_elt.
 *If something is already at position c, this over-writes it.  If c is negative,
 *the element is inserted before the first element.
 *
 *INPUT: d: element to insert
 * c: column in A (relative to first_elt!) in which to insert d.  c CAN be
 *  negative, in which case this will insert the element an appropriate
 *  number of places before zero
 * A: array in which to insert the element
 *
 *WARNINGS:
 *1) c is relative to first_elt.  So if the first element is in place 3
 *   of A and c is 1, d will be inserted in place 4 of A.  To insert d in
 *   place 1 of A, c needs to be -3.
 *2) This just puts an element into spot c in A.  If d is some
 *   sort of sparse element, this never checks to make sure A is still in
 *   ascending column order.  Use the matrix functions to insert things
 *   instead if that is important.
 *3) If there is already an entry at c, d will overwrite it.
 ***************************************************************************/

void crm114__expanding_array_set(ExpandingType d, int c, ExpandingArray *A) {

  int newsize, offset, mid, i;

  if (!A) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__expanding_array_set: null array.\n");
    }
    return;
  }

  if (A->first_elt == 0 && c == A->n_elts) {
    //if we only ever insert at the end of the array
    //then we don't want to do this funny middle thing
    //the first time we do an insert_before, first_elt gets set to non-zero
    //and all is good
    crm114__expanding_array_insert(d, A);
    return;
  }

  if (c+A->first_elt >= A->length || c+A->first_elt < 0) {
    if (fabs(c+A->first_elt) < 2*A->length) {
      if (A->length == 0) {
	A->length = 1;
      }
      newsize = 2*A->length;
    } else {
      newsize = (fabs(c+A->first_elt)+1);
    }
    expand(A, newsize);
    if (!(A->length)) {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf
	  (stderr,
	   "crm114__expanding_array_insert: unable to expand array enough to do insert.\n");
      }
      return;
    }

    //with insert we try to keep things centered
    //so we move everything
    //and recenter the array
    mid = A->n_elts/2;
    offset = A->length/2 - mid;
    for (i = A->last_elt; i >= A->first_elt; i--) {
      if (A->compact) {
	A->data.compact[i-A->first_elt+offset] = A->data.compact[i];
      } else {
	A->data.precise[i-A->first_elt+offset] = A->data.precise[i];
      }
    }
    A->last_elt += offset-A->first_elt;
    A->first_elt = offset;
  }

  if (A->compact) {
    A->data.compact[A->first_elt+c] = *(d.compact);
  } else {
    A->data.precise[A->first_elt+c] = *(d.precise);
  }
  if (c+A->first_elt > A->last_elt) {
    A->last_elt = c + A->first_elt;
    A->n_elts++;
  }

  if (c+A->first_elt < A->first_elt) {
    A->first_elt += c;
    A->n_elts++;
  }
}

//"private" function to change the array size
//on failure this sets A->length = 0
static void expand(ExpandingArray *A, int newsize) {
  ExpandingArray tmp;
  int i;

  if (!A) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "expand: null array.\n");
    }
    return;
  }

  if (CRM114__MATR_DEBUG_MODE >= MATR_OPS) {
    fprintf(stderr, "Expanding array to size %d\n", newsize);
  }

  A->length = newsize;

  if (!A->was_mapped) {
    if (A->compact) {
      A->data.compact = (CompactExpandingType *)
	realloc(A->data.compact, sizeof(CompactExpandingType)*newsize);
      if (!A->data.compact) {
	A->length = 0;
      }
    } else {
      A->data.precise = (PreciseExpandingType *)
	realloc(A->data.precise, sizeof(PreciseExpandingType)*newsize);
      if (!A->data.precise) {
	A->length = 0;
      }
    }
  } else {
    A->was_mapped = 0; //the data for A needs to be freed now!
    if (A->compact) {
      tmp.data.compact = A->data.compact;
      A->data.compact = (CompactExpandingType *)
	malloc(sizeof(CompactExpandingType)*newsize);
      if (!A->data.compact) {
	A->length = 0;
	return;
      }
      for (i = A->first_elt; i < A->last_elt; i++) {
	if (i >= newsize) {
	  //we might be making A smaller
	  break;
	}
	A->data.compact[i] = tmp.data.compact[i];
      }
    } else {
      tmp.data.precise = A->data.precise;
      A->data.precise = (PreciseExpandingType *)
	malloc(sizeof(PreciseExpandingType)*newsize);
      if (!A->data.precise) {
	A->length = 0;
	return;
      }
      for (i = A->first_elt; i < A->last_elt; i++) {
	if (i >= newsize) {
	  break;
	}
	A->data.precise[i] = tmp.data.precise[i];
      }
    }
  }
}


/***************************************************************************
 *Trims the expanding array to size first_elt + n_elts.  I would have liked
 *to have this function trim the array to size n_elts, but I can't figure
 *out how to just free the first first_elt elements of an array.
 *
 *INPUT: A: array to trim
 *
 *WARNINGS:
 *1) This frees only memory above last_elt (ie trims A to size first_elt
 *   + n_elts).  If first_elt != 0 this DOES NOT free all of the unused
 *   memory associated with A.
 ***************************************************************************/

void crm114__expanding_array_trim(ExpandingArray *A) {

  if (!A) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__expanding_array_trim: null array.\n");
    }
    return;
  }

  if (A->length == A->last_elt+1) {
    return;
  }

  expand(A, A->last_elt+1);

}

/***************************************************************************
 *Returns the element at position c of A where c is relative to first_elt.
 *
 *INPUT: c: element to get.  c is relative to first_elt!
 * A: array from which to get the element
 *
 *OUTPUT: the element at position c or NULL if c is less than 0 or
 * greater than the number of elements in A
 *
 *WARNINGS:
 *1) c is relative to first_elt.  So if the first element is in place 3
 *   of A and c is 1, this returns the element in place 4 of A.
 *2) Check for a NULL return.
 ***************************************************************************/

ExpandingType crm114__expanding_array_get(int c, ExpandingArray *A) {
  ExpandingType et;
  if (!A || !(A->length)) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__expanding_array_get: null array.\n");
    }
    et.precise = NULL;
    return et;
  }

  if (c + A->first_elt > A->last_elt || c < 0) {
    et.precise = NULL;
    return et;
  }
  if (A->compact) {
    et.compact = &A->data.compact[c+A->first_elt];
    return et;
  } else {
    et.precise = &A->data.precise[c + A->first_elt];
    return et;
  }
}

/***************************************************************************
 *Search for an element with column c of A, assuming A is ordered by ascending
 *columns and that its elements are SparseElement's.
 *
 *INPUT: c: column to search for.  this has nothing to do with the column
 * of A - rather it is the column of a sparse element in a matrix!
 * init: initial guess of the index at which c appears (relative to first_elt)
 * A: array to search
 *
 *OUTPUT: the index of an element with column c or, if no such element exists,
 * the index of the last element with a column number less than c or the
 * first element with a column number greater than c.  If the array is empty
 * this returns -1.
 *
 *WARNINGS:
 *1) init is relative to first_elt.
 *2) if c does not appear in the array, the return may be the last element
 *   before c would appear OR the first element after.
 *3) the search assumes A is arranged in ascending column order.  if it is
 *   not, the result will probably be wrong.
 ***************************************************************************/

int crm114__expanding_array_search(unsigned int c, int init, ExpandingArray *A) {
  int i, front, back, num_it = 0;

  if (!A) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__expanding_array_search: null array.\n");
    }
    return -1;
  }

  i = init+A->first_elt;
  front = A->first_elt;
  back = A->last_elt;

  if (back < front) {
    return -1;
  }

  if (i < front) {
    i = front;
  }

  if (i > back) {
    i = back;
  }

  if ((A->compact && !(A->data.compact)) ||
      (!(A->compact) && !(A->data.precise))) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__expanding_array_search: null array.\n");
    }
    return -1;
  }

  //check i itself
  if (i >= A->first_elt && i <= A->last_elt &&
      ((A->compact && c == A->data.compact[i].s.col) ||
       (!A->compact && c == A->data.precise[i].s.col))) {
    return i - A->first_elt;
  }

  //check the beginning and the end
  if ((A->compact && c >= A->data.compact[A->last_elt].s.col) ||
      (!A->compact && c >= A->data.precise[A->last_elt].s.col)) {
    return A->last_elt-A->first_elt;
  }
  if ((A->compact && c <= A->data.compact[A->first_elt].s.col) ||
      (!A->compact && c <= A->data.precise[A->first_elt].s.col)) {
    return 0;
  }

  //check before and after the current element
  if (i > A->first_elt && i <= A->last_elt &&
      ((A->compact && c < A->data.compact[i].s.col) ||
       (!A->compact && c < A->data.precise[i].s.col)) &&
      ((A->compact && c >= A->data.compact[i-1].s.col) ||
       (!A->compact && c >= A->data.precise[i-1].s.col))) {
    return i-1-A->first_elt;
  }
  if (i >= A->first_elt && i < A->last_elt &&
      ((A->compact && c > A->data.compact[i].s.col) ||
       (!A->compact && c > A->data.precise[i].s.col)) &&
      ((A->compact && c <= A->data.compact[i+1].s.col) ||
       (!A->compact && c <= A->data.precise[i+1].s.col))) {
    return i+1-A->first_elt;
  }

  while (((A->compact && A->data.compact[i].s.col != c) ||
	  (!(A->compact) && A->data.precise[i].s.col != c)) &&
	 front <= back) {
    i = (front + back)/2;
    if ((A->compact && A->data.compact[i].s.col < c) ||
	(!A->compact && A->data.precise[i].s.col < c)) {
      front = i+1;
    } else if ((A->compact && A->data.compact[i].s.col > c) ||
	       (!A->compact && A->data.precise[i].s.col > c)) {
      back = i-1;
    }
    num_it++;
  }

  if (CRM114__MATR_DEBUG_MODE >= MATR_OPS_MORE) {
    fprintf(stderr, "After full search (%d iterations) returned %d, init = %d, last_elt = %d, first_elt = %d\n",
	   num_it, i, init+A->first_elt, A->last_elt, A->first_elt);
  }
  return i - A->first_elt;
}


/***************************************************************************
 *Insert an element into the array.  This function does the least amount
 *of shifting so that if before < n_elts/2, all the elements will move back
 *one place and otherwise they will move forward one place.
 *
 *INPUT: ne: element to insert
 * before: the index of the element ne should be inserted before (relative to
 *  first_elt)
 * A: the array in which to insert
 *
 *OUTPUT: the ABSOLUTE index in A of the new element.  this index is NOT
 * relative to first_elt!  we do this this way, because the location of
 * first_elt may change during this function.
 *
 *WARNINGS:
 *1) the return is an ABSOLUTE index NOT RELATIVE to first_elt.
 *2) the function does the least amount of shifting so it may change
 *   last_elt OR it may change first_elt.
 ***************************************************************************/

int crm114__expanding_array_insert_before(ExpandingType ne, int before,
				   ExpandingArray *A) {
  int i;
  ExpandingType tmp;
  CompactExpandingType cet;
  PreciseExpandingType pet;

  if (!A) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__expanding_array_insert_before: null array.\n");
    }
    return -1;
  }

  if (before < 0) {
    before = 0;
  }

  if (before < A->n_elts/2) {
    //this changes indexing
    tmp = crm114__expanding_array_get(0, A);
    if (tmp.precise) {
      if (A->compact) {
	cet = *(tmp.compact);
	tmp.compact = &cet;
      } else {
	pet = *(tmp.precise);
	tmp.precise = &pet;
      }
      crm114__expanding_array_set(tmp, -1, A);
    }
    for (i = 1; i < before; i++) {
      tmp = crm114__expanding_array_get(i+1, A);
      if (tmp.precise) {
	if (A->compact) {
	  cet = *(tmp.compact);
	  tmp.compact = &cet;
	} else {
	  pet = *(tmp.precise);
	  tmp.precise = &pet;
	}
	crm114__expanding_array_set(tmp, i, A);
      }
    }
    crm114__expanding_array_set(ne, before, A);
  } else {
    for (i = A->n_elts-1; i >= before; i--) {
      tmp = crm114__expanding_array_get(i, A);
      if (tmp.precise) {
	if (A->compact) {
	  cet = *(tmp.compact);
	  tmp.compact = &cet;
	} else {
	  pet = *(tmp.precise);
	  tmp.precise = &pet;
	}
	crm114__expanding_array_set(tmp, i+1, A);
      }
    }
    crm114__expanding_array_set(ne, before, A);
  }
  return before+A->first_elt;
}

/***************************************************************************
 *Insert an element into the array.  This function does the least amount
 *of shifting so that if after < n_elts/2, all the elements will move back
 *one place and otherwise they will move forward one place.
 *
 *INPUT: ne: element to insert
 * after: the index of the element ne should be inserted after (relative to
 *  first_elt)
 * A: the array in which to insert
 *
 *OUTPUT: the ABSOLUTE index in A of the new element.  this index is NOT
 * relative to first_elt!  we do this this way, because the location of
 * first_elt may change during this function.
 *
 *WARNINGS:
 *1) the return is an ABSOLUTE index NOT RELATIVE to first_elt.
 *2) the function does the least amount of shifting so it may change
 *   last_elt OR it may change first_elt.
 ***************************************************************************/

int crm114__expanding_array_insert_after(ExpandingType ne, int after,
				  ExpandingArray *A) {
  return crm114__expanding_array_insert_before(ne, after+1, A);
}

/***************************************************************************
 *Remove an element from the array.  This function does the least amount
 *of shifting so that if elt < n_elts/2, all the elements will move back
 *one place and otherwise they will move forward one place.
 *
 *INPUT: elt: the index (relative to first_elt) of the element to be removed
 * A: the array from which to remove elt
 *
 *WARNINGS:
 *1) the function does the least amount of shifting so it may change
 *   last_elt OR it may change first_elt.
 ***************************************************************************/

void crm114__expanding_array_remove_elt(int elt, ExpandingArray *A) {
  int i;

  if (!A) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "expanding_remove_elt: null array.\n");
    }
    return;
  }

  if (elt < A->n_elts/2) {
    //move everything behind it closer
    for (i = elt-1; i >= 0; i--) {
      crm114__expanding_array_set(crm114__expanding_array_get(i, A), i+1, A);
    }
    A->first_elt++;
  } else {
    for (i = elt+1; i < A->n_elts; i++) {
      crm114__expanding_array_set(crm114__expanding_array_get(i, A), i-1, A);
    }
    A->last_elt--;
  }
  A->n_elts--;
}

/***************************************************************************
 *Clears all elements of A.
 *
 *INPUT: A: the array to clear
 *
 *WARNINGS:
 *1) this does not free any of the memory associated with A.  to do that
 *   call crm114__expanding_array_free.
 ***************************************************************************/

void crm114__expanding_array_clear(ExpandingArray *A) {
  if (!A) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__expanding_array_clear: null array.\n");
    }
    return;
  }

  A->last_elt = -1;
  if (A->first_elt > 0) {
    A->first_elt = A->length/2;
  }
  A->n_elts = 0;
}

/***************************************************************************
 *Writes A to a file in binary format.
 *
 *INPUT: A: the array to write
 * fp: pointer to file to write A in
 *
 *WARNINGS:
 *1) A is written in a BINARY format.  Use crm114__expanding_array_read to recover
 *   A.
 ***************************************************************************/

size_t crm114__expanding_array_write(ExpandingArray *A, FILE *fp) {
  size_t size;

  if (!A || !fp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__expanding_array_write: null arguments.\n");
    }
    return 0;
  }

  size = sizeof(ExpandingArray)*fwrite(A, sizeof(ExpandingArray),
					 1, fp);
  if (A->length && A->length >= A->first_elt) {
    if (A->compact && A->data.compact) {
      return size + sizeof(CompactExpandingType)*
	fwrite(&(A->data.compact[A->first_elt]), sizeof(CompactExpandingType),
	       A->n_elts, fp);
    }
    if (!(A->compact) && A->data.precise) {
      return size + sizeof(PreciseExpandingType)*
	fwrite(&(A->data.precise[A->first_elt]), sizeof(PreciseExpandingType),
	       A->n_elts, fp);
    }
  }

  return size;
}

/***************************************************************************
 *Reads A from a file in binary format.
 *
 *INPUT: A: an expanding array.  if A contains any data it will be freed
 *  and overwritten.
 * fp: pointer to file to read A from
 *
 *WARNINGS:
 *1) If fp does not contain a properly formatted expanding array as written
 *   by the function crm114__expanding_array_write, this function will do its best,
 *   but the results may be very bizarre.  Check for an empty return.
 ***************************************************************************/

void crm114__expanding_array_read(ExpandingArray *A, FILE *fp) {
  size_t amount_read;

  if (!A || !fp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__expanding_array_read: null arguments.\n");
    }
    return;
  }

  if (A->compact && A->data.compact && !(A->was_mapped)) {
    free(A->data.compact);
  } else if (!(A->compact) && A->data.precise && !(A->was_mapped)) {
    free(A->data.precise);
  }
  amount_read = fread(A, sizeof(ExpandingArray), 1, fp);
  A->was_mapped = 0;
  if (!amount_read) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__expanding_array_read: bad file.\n");
    }
    return;
  }

  if (A->length >= A->n_elts &&
      A->first_elt < A->length && A->first_elt >= 0) {
    if (A->compact) {
      A->data.compact = (CompactExpandingType *)
	malloc(sizeof(CompactExpandingType)*A->length);
      amount_read = fread(&(A->data.compact[A->first_elt]),
			  sizeof(CompactExpandingType), A->n_elts, fp);
    } else {
      A->data.precise = (PreciseExpandingType *)
	malloc(sizeof(PreciseExpandingType)*A->length);
      amount_read = fread(&(A->data.precise[A->first_elt]),
			  sizeof(PreciseExpandingType), A->n_elts, fp);
    }
    if (amount_read < A->n_elts && CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr,
	      "crm114__expanding_array_read: fewer elts read in than expected.\n");
    }
  } else {
    if (CRM114__MATR_DEBUG_MODE && A->n_elts) {
      fprintf(stderr, "crm114__expanding_array_read: A cannot contain all of its elements.  This is likely a corrupted file.\n");
    }
    A->length = 0;
    A->n_elts = 0;
    A->first_elt = 0;
    A->last_elt = -1;
    A->data.precise = NULL;
  }
}

/***************************************************************************
 *Maps an expanding array from a block of memory in binary format (the same
 *format as would be written to a file using .
 *
 *INPUT: addr: a pointer to the address where the expanding array begins
 * last_addr: the last possible address that is valid.  NOT necessarily where
 *  the expanding array ends - just the last address that has been allocated
 *  in the chunk pointed to by *addr (ie, if *addr was taken from an mmap'd file
 *  last_addr would be addr + the file size).
 *
 *OUTPUT: An expanding array STILL referencing the chunk of memory at *addr,
 *  but formated as an expanding array or NULL if a properly formatted
 *  expanding array didnt' start at addr.
 * *addr: (pass-by-reference) now points to the next address AFTER the full
 *   expanding array.
 *
 *WARNINGS:
 * 1) *addr needs to be writable.  This will CHANGE VALUES stored at *addr and
 *    will seg fault if *addr is not writable.
 * 2) last_addr does not need to be the last address of the expanding array
 *    but if it is before that, either NULL will be returned or an
 *    expanding array with a NULL data value will be returned.
 * 3) if *addr does not contain a properly formatted array, this function
 *    will not seg fault, but that is the only guarantee.
 * 4) call crm114__expanding_array_free_data on this output UNLESS you are SURE
 *    you have made no changes!  if the array expands its data, you need
 *    to free that.  DO NOT call crm114__expanding_array_free.
 * 5) *addr CHANGES!
 ***************************************************************************/

ExpandingArray *crm114__expanding_array_map(void **addr, void *last_addr) {
  ExpandingArray *A;

  if (!addr || !*addr || !last_addr || *addr >= last_addr) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__expanding_array_map: null arguments.\n");
    }
    return NULL;
  }

  if ((void *)((ExpandingArray *)*addr + 1) > last_addr) {
    //bad
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__expanding_array_map: not enough memory for array.\n");
    }
    return NULL;
  }

  A = (ExpandingArray *)(*addr);
  *addr = A + 1;
  A->length = A->n_elts; //we only have this much space in the file
  A->last_elt = A->n_elts-1;
  A->first_elt = 0;
  A->was_mapped = 1;

  if (A->length >= A->n_elts && A->first_elt < A->length && A->first_elt >= 0
      && ((A->compact &&
	   (void *)((CompactExpandingType *)*addr + A->n_elts) <= last_addr) ||
	  (!(A->compact) &&
	   (void *)((PreciseExpandingType *)*addr + A->n_elts) <= last_addr))) {
    if (A->compact) {
      A->data.compact = (CompactExpandingType *)(*addr);
      *addr = A->data.compact + A->n_elts;
    } else {
      A->data.precise = (PreciseExpandingType *)(*addr);
      *addr = A->data.precise + A->n_elts;
    }
  } else {
    if (CRM114__MATR_DEBUG_MODE && A->n_elts) {
      fprintf(stderr, "crm114__expanding_array_map: array cannot contain all of its elements. This is likely a corrupted file.\n");
    }
    A->length = 0;
    A->n_elts = 0;
    A->first_elt = 0;
    A->last_elt = -1;
    A->data.precise = NULL;
  }

  return A;
}

/***************************************************************************
 *Frees the data associated A.
 *
 *INPUT: A: the array with the data to free
 *
 *WARNINGS:
 *1) this does not free A, only the data associated with it.
 ***************************************************************************/

void crm114__expanding_array_free_data(ExpandingArray *A) {
  if (!A || A->was_mapped) {
    return;
  }
  if (A->compact && A->data.compact) {
    free(A->data.compact);
  } else if (A->data.precise) {
    free(A->data.precise);
  }
}

/***************************************************************************
 *Frees all memory associated with A.
 *
 *INPUT: A: the array to free
 ***************************************************************************/

void crm114__expanding_array_free(ExpandingArray *A) {
  if (A && A->was_mapped) {
    //the data stored in A was mapped in
    //we shouldn't free it
    free(A);
    return;
  }
  crm114__expanding_array_free_data(A);
  if (A) {
    free(A);
  }
}

/***********************Linked List Functions***************************/

//Linked list static function declarations
static inline size_t node_write(SparseNode n, FILE *fp);
static inline SparseNode node_read(int is_compact, FILE *fp);
static inline SparseNode node_map(int is_compact, void **addr, void *last_addr);

/***************************************************************************
 *Make a new list with nothing in it
 *
 *INPUT: compact: COMPACT or PRECISE, deciding the size of the data in the list
 *
 *OUTPUT: a linked list
 ***************************************************************************/

SparseElementList *crm114__make_list(int compact) {
  SparseElementList *l = (SparseElementList *)malloc(sizeof(SparseElementList));
  if (!l) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "Could not create a sparse element list.\n");
    }
    return NULL;
  }
  l->compact = compact;
  l->head = make_null_node(compact);
  l->tail = make_null_node(compact);
  l->last_addr = NULL;
  return l;
}

/***************************************************************************
 *Search for an element with column c of l, assuming l is ordered by ascending
 *columns and that its elements are SparseElement's.
 *
 *INPUT: c: column to search for.
 * init: initial guess
 * l: list to search
 *
 *OUTPUT: a pointer to the element with column c or, if no such element exists,
 * a pointer to the last element with a column number less than c or the
 * first element with a column number greater than c.  If the array is empty
 * this returns a null node.
 *
 *WARNINGS:
 *1) if c does not appear in the array, the return may be the last element
 *   before c would appear OR the first element after.
 *2) the search assumes l is arranged in ascending column order.  if it is
 *   not, the result will probably be wrong.
 ***************************************************************************/

SparseNode crm114__list_search(unsigned int c, SparseNode init, SparseElementList *l)
{
  SparseNode curr = init;

  if (!l) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__list_search: null list.\n");
    }
    return make_null_node(l->compact);
  }

  if (crm114__list_is_empty(l)) {
    return make_null_node(l->compact);
  }

  if (c <= node_col(l->head)) {
    return l->head;
  }

  if (c >= node_col(l->tail)) {
    return l->tail;
  }

  while (!null_node(curr) && node_col(curr) < c) {
    curr = next_node(curr);
  }

  while (!null_node(curr) && node_col(curr) > c) {
    curr = prev_node(curr);
  }

  return curr;
}

/***************************************************************************
 *Insert an element into a list.
 *
 *INPUT: newelt: element to be inserted
 * before: pointer to element before which newelt should be inserted
 * l: list in which to insert
 *
 *OUTPUT: a pointer to the element that has been inserted
 ***************************************************************************/

SparseNode crm114__list_insert_before(SparseElement newelt, SparseNode before,
			SparseElementList *l) {
  CompactSparseNode *cn;
  PreciseSparseNode *pn;
  SparseNode n;

  if (!l) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__list_insert_before: null list.\n");
    }
    return make_null_node(l->compact);
  }

  n.is_compact = l->compact;
  n.compact = NULL;
  n.precise = NULL;

  if (l->compact) {
    cn = (CompactSparseNode *)malloc(sizeof(CompactSparseNode));
    n.compact = cn;
    cn->data = *(newelt.compact);
    if (crm114__list_is_empty(l)) {
      //empty list
      cn->prev = NULL;
      l->head.compact = cn;
      l->tail.compact = cn;
    } else {
      cn->prev = before.compact->prev;
      if (cn->prev) {
	cn->prev->next = cn;
      } else {
	l->head.compact = cn;
      }
      before.compact->prev = cn;
    }
    cn->next = before.compact;
  } else {
    pn = (PreciseSparseNode *)malloc(sizeof(PreciseSparseNode));
    n.precise = pn;
    pn->data = *(newelt.precise);
    if (crm114__list_is_empty(l)) {
      //empty list
      pn->prev = NULL;
      l->head.precise = pn;
      l->tail.precise = pn;
    } else {
      pn->prev = before.precise->prev;
      if (pn->prev) {
	pn->prev->next = pn;
      } else {
	l->head.precise = pn;
      }
      before.precise->prev = pn;
    }
    pn->next = before.precise;
  }
  return n;
}

/***************************************************************************
 *Insert an element into a list.
 *
 *INPUT: newelt: element to be inserted
 * after: pointer to element after which newelt should be inserted
 * l: list in which to insert
 *
 *OUTPUT: a pointer to the element that has been inserted
 ***************************************************************************/

SparseNode crm114__list_insert_after(SparseElement ne, SparseNode after,
			     SparseElementList *l) {
  CompactSparseNode *cn;
  PreciseSparseNode *pn;
  SparseNode n;

  if (!l) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__list_insert_after: null list.\n");
    }
    return make_null_node(l->compact);
  }

  n.is_compact = l->compact;
  n.compact = NULL;
  n.precise = NULL;

  if (l->compact) {
    cn = (CompactSparseNode *)malloc(sizeof(CompactSparseNode));
    n.compact = cn;
    cn->data = *(ne.compact);
    if (crm114__list_is_empty(l)) {
      //empty list
      cn->next = NULL;
      l->head.compact = cn;
      l->tail.compact = cn;
    } else {
      cn->next = after.compact->next;
      if (cn->next) {
	cn->next->prev = cn;
      } else {
	l->tail.compact = cn;
      }
      after.compact->next = cn;
    }
    cn->prev = after.compact;
  } else {
    pn = (PreciseSparseNode *)malloc(sizeof(PreciseSparseNode));
    n.precise = pn;
    pn->data = *(ne.precise);
    if (crm114__list_is_empty(l)) {
      //empty list
      pn->next = NULL;
      l->head.precise = pn;
      l->tail.precise = pn;
    } else {
      pn->next = after.precise->next;
      if (pn->next) {
	pn->next->prev = pn;
      } else {
	l->tail.precise = pn;
      }
      after.precise->next = pn;
    }
    pn->prev = after.precise;
  }
  return n;
}

/***************************************************************************
 *Clear a list, freeing each element.
 *
 *INPUT: l: list to clear
 ***************************************************************************/

void crm114__list_clear(SparseElementList *l) {
  SparseNode curr, next;
  int i;

  if (!l) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__list_clear: null list.\n");
    }
    return;
  }

  curr = l->head;

  i = 0;

  while (!null_node(curr)) {
    next = next_node(curr);
    if (!(l->last_addr)) {
      node_free(curr);
    } else {
      if (l->compact && ((void *)curr.compact < (void *)l ||
			 (void *)curr.compact >= l->last_addr)) {
	node_free(curr);
      }
      if (!(l->compact) && ((void *)curr.precise < (void *)l ||
			    (void *)curr.precise >= l->last_addr)) {
	node_free(curr);
      }
    }
    curr = next;
    i++;
  }
  l->head = make_null_node(l->compact);
  l->tail = make_null_node(l->compact);
}

/***************************************************************************
 *Remove an element from the list.
 *
 *INPUT: l: list from which to remove an element
 * toremove: pointer to the element to be removed
 ***************************************************************************/

void crm114__list_remove_elt(SparseElementList *l, SparseNode toremove) {

  if (!l) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__list_remove_elt: null list.\n");
    }
    return;
  }

  if (null_node(toremove)) {
    return;
  }

  if (!null_node(prev_node(toremove))) {
    if (l->compact) {
      toremove.compact->prev->next = toremove.compact->next;
    } else {
      toremove.precise->prev->next = toremove.precise->next;
    }
  } else {
    if (l->compact) {
      l->head.compact = toremove.compact->next;
    } else {
      l->head.precise = toremove.precise->next;
    }
  }
  if (!null_node(next_node(toremove))) {
    if (l->compact) {
      toremove.compact->next->prev = toremove.compact->prev;
    } else {
      toremove.precise->next->prev = toremove.precise->prev;
    }
  } else {
    if (l->compact) {
      l->tail.compact = toremove.compact->prev;
    } else {
      l->tail.precise = toremove.precise->prev;
    }
  }

  if (l->compact) {
    if (!(l->last_addr) || (void *)toremove.compact < (void *)l ||
	(void *)toremove.compact >= l->last_addr) {
      node_free(toremove);
    }
  } else {
    if (!(l->last_addr) || (void *)toremove.precise < (void *)l ||
	(void *)toremove.precise >= l->last_addr) {
      node_free(toremove);
    }
  }
}

/***************************************************************************
 *Check if a list is empty.
 *
 *INPUT: l: list to check.
 *
 *OUTPUT: 1 if l is empty, 0 else
 ***************************************************************************/

int crm114__list_is_empty(SparseElementList *l) {
  if (!l) {
    return 1;
  }
  return null_node((l->head));
}

/***************************************************************************
 *Writes l to a file in binary format.
 *
 *INPUT: l: the array to write
 * fp: pointer to file to write l in
 *
 *WARNINGS:
 *1) l is written in a BINARY format.  Use crm114__list_read to recover
 *   l.
 ***************************************************************************/

size_t crm114__list_write(SparseElementList *l, FILE *fp) {
  SparseNode curr;
  size_t size;

  if (!l || !fp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__list_write: null arguments.\n");
    }
    return 0;
  }

  size = sizeof(SparseElementList)*fwrite(l, sizeof(SparseElementList),
					  1, fp);
  curr = l->head;
  while (!null_node(curr)) {
    size += node_write(curr, fp);
    curr = next_node(curr);
  }
  return size;
}

/***************************************************************************
 *Reads l from a file in binary format.
 *
 *INPUT: l: a sparse element list.  if l contains any data it will be freed
 *  and overwritten.
 * fp: pointer to file to read l from
 * n_elts: the number of elements (nodes) to read into the list
 *
 *OUTPUT: the number of elements actually read.
 *
 *WARNINGS:
 *1) If fp does not contain a properly formatted list as written
 *   by the function crm114__list_write, this function will do its best,
 *   but the results may be very bizarre.  Check for an empty return.
 ***************************************************************************/

int crm114__list_read(SparseElementList *l, FILE *fp, int n_elts) {
  SparseNode n, pn;
  int i;
  size_t unused;

  if (!l || !fp || n_elts < 0) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__list_write: null arguments.\n");
    }
    return 0;
  }

  if (!crm114__list_is_empty(l)) {
    crm114__list_clear(l);
  }

  l->last_addr = NULL;

  unused = fread(l, sizeof(SparseElementList), 1, fp);
  if (n_elts <= 0) {
    return 0;
  }
  l->head = node_read(l->compact, fp);
  pn = l->head;
  for (i = 1; i < n_elts; i++) {
    if (null_node(pn)) {
      break;
    }
    n = node_read(l->compact, fp);
    if (null_node(n)) {
      break;
    }
    if (l->compact) {
      pn.compact->next = n.compact;
      n.compact->prev = pn.compact;
    } else {
      pn.precise->next = n.precise;
      n.precise->prev = pn.precise;
    }
    pn = n;
  }
  if (i != n_elts) {
    if (!null_node(pn)) {
      if (l->compact) {
	pn.compact->next = NULL;
      } else {
	pn.precise->next = NULL;
      }
    }
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__list_read: Couldn't read in enough elements.\n");
    }
  }
  l->tail = pn;
  return i;
}

/***************************************************************************
 *Maps an list from a block of memory in binary format (the same
 *format as would be written to a file using crm114__list_write.
 *
 *INPUT: addr: pointer to the address where the list begins
 * last_addr: the last possible address that is valid.  NOT necessarily where
 *  the list ends - just the last address that has been allocated in the
 *  chunk pointed to by *addr (ie, if *addr was taken from an mmap'd file
 *  last_addr would be *addr + the file size).
 * n_elts_ptr: a pointer to a value containing the number of elements
 *  in l that should be read.  on return, this value is the number of
 *  elements that actually were read.
 *
 *OUTPUT: A list STILL referencing the chunk of memory at *addr,
 *  but formated as a list or NULL if a properly formatted
 *  list didn't start at *addr.
 * *addr: (pass-by-reference) points to the first memory location AFTER the
 *  full list
 * *n_elts_ptr: (pass-by-reference) the number of elements actually read
 *
 *WARNINGS:
 * 1) *addr needs to be writable.  This will CHANGE VALUES stored at *addr and
 *    will seg fault if addr is not writable.
 * 2) last_addr does not need to be the last address of the list
 *    but if it is before that, either NULL will be returned or an
 *    matrix with a NULL data value will be returned.
 * 3) if *addr does not contain a properly formatted list, this function
 *    will not seg fault, but that is the only guarantee.
 * 4) you should call crm114__list_clear on this list unless you are CERTAIN you
 *    have not changed the list.  calling crm114__list_clear on an unchanged list
 *    will not do anything.
 * 5) *addr and *n_elts_ptr CHANGE!
 ***************************************************************************/

SparseElementList *crm114__list_map(void **addr, void *last_addr, int *n_elts_ptr) {
  SparseElementList *l;
  SparseNode n, pn;
  int n_elts = *n_elts_ptr, i;

  if (!addr || !*addr || !last_addr || n_elts < 0 || *addr >= last_addr) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__list_map: null arguments.\n");
    }
    *n_elts_ptr = 0;
    return NULL;
  }

  if ((void *)((SparseElementList *)*addr + 1) > last_addr) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__list_map: not enough memory for list.\n");
    }
    *n_elts_ptr = 0;
    return NULL;
  }

  l = (SparseElementList *)(*addr);
  *addr = l + 1;
  l->head = node_map(l->compact, addr, last_addr);
  pn = l->head;
  for (i = 1; i < n_elts; i++) {
    if (null_node(pn)) {
      break;
    }
    n = node_map(l->compact, addr, last_addr);
    if (null_node(n)) {
      break;
    }
    if (l->compact) {
      pn.compact->next = n.compact;
      n.compact->prev = pn.compact;
    } else {
      pn.precise->next = n.precise;
      n.precise->prev = pn.precise;
    }
    pn = n;
  }
  if (i != n_elts) {
    if (!null_node(pn)) {
      if (l->compact) {
	pn.compact->next = NULL;
      } else {
	pn.precise->next = NULL;
      }
    }
    *n_elts_ptr = i;
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__list_map: Couldn't read in enough elements.\n");
    }
  }
  l->last_addr = *addr;
  l->tail = pn;
  return l;
}

/***************************************************************************
 *Copies a list into a chunk of memory. to will not be completely identical
 * to from since pointer values will change and the value of last_addr will
 * change.  to is, and can be treated as, a contiguous-memory form of from.
 * it is a SparseElementList and crm114__list_clear should be called before free'ing
 * the chunk of memory to belongs in.  As with memmove this function does not
 * actually "move" anything out of from.
 *
 *INPUT: to: a block of memory with enough memory to hold the entire list
 *  stored in from.
 * from: the list to be copied from.
 *
 *OUTPUT: A pointer to the first address AFTER the data was copied.  In
 * other words this returns (char *)to + size(from) where size(from) is the size
 * in bytes of the full list from.
 *
 *WARNINGS:
 * 1) to needs to be writable.  This will CHANGE VALUES stored at to and
 *    will seg fault if to is not writable.
 * 2) this does NOT CHECK FOR OVERFLOW.  to must have enough memory
 *    already to contain from or this can cause a seg fault.
 * 3) unlike with memmove, this is not a completely byte-by-byte copy.
 *    instead, to is a copy of the list from stored contiguously at to
 *    with the same functionality as from.  in other words, to can be
 *    treated as a list.
 * 4) you should call crm114__list_clear on to unless you are CERTAIN you
 *    have not changed it.  calling crm114__list_clear on an unchanged list
 *    will not do anything.
 * 5) like memmove, this actually copies, not moves.  it DOES NOT FREE from.
 ***************************************************************************/

void *crm114__list_memmove(SparseElementList *to, SparseElementList *from) {
  void *curr;
  SparseNode n, tn, tpn;
  int i;

  if (!from || !to) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__list_memmove: null arguments.\n");
    }
    return to;
  }

  *to = *from;
  curr = (void *)(to + 1);
  n = from->head;


  if (null_node(to->head)) {
    return curr;
  }
  if (from->compact) {
    to->head.compact = (CompactSparseNode *)curr;
    curr = (void *)((CompactSparseNode *)curr + 1);
    *(to->head.compact) = *(n.compact);
    to->head.precise = NULL;
  } else {
    to->head.precise = (PreciseSparseNode *)curr;
    curr = (void *)((PreciseSparseNode *)curr + 1);
    *(to->head.precise) = *(n.precise);
    to->head.compact = NULL;
  }

  tpn = to->head;
  n = next_node(n);
  i = 1;
  tn.is_compact = from->compact;
  tpn.is_compact = from->compact;
  while (!null_node(n)) {
    if (from->compact) {
      tn.compact = (CompactSparseNode *)curr;
      curr = (void *)((CompactSparseNode *)curr + 1);
      tn.compact->data = n.compact->data;
      tn.compact->prev = tpn.compact;
      tn.compact->next = NULL;
      tn.precise = NULL;
      tpn.compact->next = tn.compact;
    } else {
      tn.precise = (PreciseSparseNode *)curr;
      curr = (void *)((PreciseSparseNode *)curr + 1);
      tn.precise->data = n.precise->data;
      tn.precise->prev = tpn.precise;
      tn.precise->next = NULL;
      tn.compact = NULL;
      tpn.precise->next = tn.precise;
    }
    n = next_node(n);
    tpn = tn;
    i++;
  }
  to->tail = tpn;
  to->last_addr = curr;
  return curr;
}


//writes a node to a file
static inline size_t node_write(SparseNode n, FILE *fp) {
  if (null_node(n) || !fp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "node_write: null arguments.\n");
    }
  }
  if (n.is_compact) {
    return sizeof(CompactSparseNode)*fwrite(n.compact,
					    sizeof(CompactSparseNode), 1, fp);
  }

  return sizeof(PreciseSparseNode)*fwrite(n.precise,
					  sizeof(PreciseSparseNode), 1, fp);
}

//reads a node from a file
static inline SparseNode node_read(int is_compact, FILE *fp) {
  SparseNode n = make_null_node(is_compact);
  size_t nr;

  if (!fp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "node_read: bad file pointer.\n");
    }
    return n;
  }

  if (n.is_compact) {
    n.compact = (CompactSparseNode *)malloc(sizeof(CompactSparseNode));
    nr = fread(n.compact, sizeof(CompactSparseNode), 1, fp);
    if (!nr) {
      //end of file
      free(n.compact);
      return make_null_node(is_compact);
    }
    n.compact->next = NULL;
    n.compact->prev = NULL;
    return n;
  }

  n.precise = (PreciseSparseNode *)malloc(sizeof(PreciseSparseNode));
  nr = fread(n.precise, sizeof(PreciseSparseNode), 1, fp);
  if (!nr) {
    //end of file
    free(n.precise);
    return make_null_node(is_compact);
  }
  n.precise->next = NULL;
  n.precise->prev = NULL;
  return n;
}

//maps a node in from memory
//on finish, addr points to the address AFTER the node that was mapped in
//if there was not enough memory between *addr and last_addr, *addr will
//point AFTER LAST_ADDR and a null_node will be returned.
static inline SparseNode node_map(int is_compact, void **addr, void *last_addr){
  SparseNode n = make_null_node(is_compact);

  if (*addr >= last_addr) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "node_map: no memory.\n");
    }
    return n;
  }

  if (n.is_compact) {
    n.compact = (CompactSparseNode *)(*addr);
    *addr = n.compact + 1;
    if (*addr > last_addr) {
      return make_null_node(is_compact);
    }
    n.compact->next = NULL;
    n.compact->prev = NULL;
    return n;
  }

  n.precise = (PreciseSparseNode *)(*addr);
  *addr = n.precise + 1;
  if (*addr > last_addr) {
    return make_null_node(is_compact);
  }
  n.precise->next = NULL;
  n.precise->prev = NULL;
  return n;
}

//qsort comparison functions

//a function for use with qsort that compares CompactExpandingTypes
//by their integer values
int crm114__compact_expanding_type_int_compare(const void *a, const void *b) {
  CompactExpandingType *ceta = (CompactExpandingType *)a,
    *cetb = (CompactExpandingType *)b;

  if (ceta->i < cetb->i) {
    return -1;
  }

  if (ceta->i > cetb->i) {
    return 1;
  }

  return 0;
}

//function to be passed to qsort that compares two PreciseExpandingType's by
//the value.  the sort will be in INCREASING value order.
int crm114__precise_sparse_element_val_compare(const void *a, const void *b) {
  PreciseSparseElement *ra = (PreciseSparseElement *)a,
    *rb = (PreciseSparseElement *)b;
  if (ra->data < rb->data) {
    return -1;
  }
  if (ra->data > rb->data) {
    return 1;
  }
  return 0;
}

//function to be passed to qsort that compares two PreciseExpandingType's by
//col number.  the sort will be in DECREASING row order
int crm114__precise_sparse_element_col_compare(const void *a, const void *b) {
  PreciseSparseElement *ra = (PreciseSparseElement *)a,
    *rb = (PreciseSparseElement *)b;
  if (ra->col > rb->col) {
    return -1;
  }
  if (ra->col < rb->col) {
    return 1;
  }
  return 0;
}
