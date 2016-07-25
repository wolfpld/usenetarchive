// Conversion hack: read/write SVM data from/to a CRM114_DATABLOCK
// instead of a file (C library stream).  Contains two sets of
// functions:

// Direct replacements for some C library file functions, but deal with
// a CRM114_DATABLOCK instead of a file.  All use a pointer analogous to
// a FILE *.

// Vector write/read binary, to/from a CRM114_DATABLOCK instead of a
// file.  Copied and hacked from vector_(write|read)_bin*().  These
// call the first set (C library replacements).


#include "crm114_matrix.h"
#include "crm114_svm.h"


// subset of fopen()
void crm114__dbopen(CRM114_DATABLOCK *db, struct data_state *dsp)
{
  dsp->data = &db->data[db->cb.block[0].start_offset];
  dsp->size = db->cb.block[0].allocated_size;
  dsp->position = 0;
  dsp->eof = 0;
}

// fclose()
int crm114__dbclose(struct data_state *dsp)
{
  dsp->data = NULL;
  dsp->size = 0;
  dsp->position = 0;
  dsp->eof = 0;
  return 0;
}

// ftell()
long crm114__dbtell(struct data_state *dsp)
{
  return dsp->position;
}

// fseek()
// uses C library values for whence
int crm114__dbseek(struct data_state *dsp, long offset, int whence)
{
  int ret = -1;			// error

  // be careful about signed vs. unsigned arithmetic
  switch (whence)
    {
    case SEEK_SET:
      if (offset >= 0 && (unsigned long)offset <= dsp->size)
	{
	  dsp->position = offset;
	  ret = 0;		// success
	}
      break;
    case SEEK_CUR:
#if 1
      // Ideally long long (signed) would be at least one bit wider
      // than size_t (unsigned), but the same width should do in
      // practice.  Long long is guaranteed to be at least 64 bits;
      // data blocks are unlikely to get bigger than 2^63 bytes.  And
      // size_t is unlikely to be wider than long long.
      {
	long long new_position;

	new_position = (long long)dsp->position + offset;
	if (new_position >= 0 && (size_t)new_position <= dsp->size)
	  {
	    dsp->position = new_position;
	    ret = 0;
	  }
      }
#else
      // this version doesn't count on long long
      if (offset >= 0)
	{
	  if ((unsigned long)offset <= dsp->size - dsp->position)
	    {
	      dsp->position += offset;
	      ret = 0;
	    }
	}
      else
	if ((unsigned long)(-offset) <= dsp->position)
	  {
	    dsp->position -= -offset;
	    ret = 0;
	  }
#endif
      break;
    case SEEK_END:
      if (offset <= 0 && (unsigned long)(-offset) <= dsp->size)
	{
	  dsp->position = dsp->size - -offset;
	  ret = 0;
	}
      break;
    default:
      break;
    }

  if (ret == 0)
    dsp->eof = 0;

  return ret;
}

// feof()
int crm114__dbeof(struct data_state *dsp)
{
  return (dsp->eof);
}

// fread()
size_t crm114__dbread(void *ptr, size_t size, size_t nmemb,
	      struct data_state *dsp)
{
  size_t nmemb1;
  size_t bytes;

  if (dsp->position < dsp->size) // both size_t: unsigned
    {
      nmemb1 = (dsp->size - dsp->position) / size;
      if (nmemb1 > nmemb)
	nmemb1 = nmemb;
      bytes = nmemb1 * size;
      (void)memmove(ptr, &dsp->data[dsp->position], bytes);
      dsp->position += bytes;
    }
  else
    nmemb1 = 0;

  if (nmemb1 < nmemb)
    dsp->eof = 1;

  return nmemb1;
}

// fwrite(), sort of: refuses to write past EOF, trying to sets EOF
// it would be bad to write off the end of our data block
size_t crm114__dbwrite(const void *ptr, size_t size, size_t nmemb,
	       struct data_state *dsp)
{
  size_t nmemb1;
  size_t bytes;

  if (dsp->position < dsp->size) // both size_t: unsigned
    {
      nmemb1 = (dsp->size - dsp->position) / size;
      if (nmemb1 > nmemb)
	nmemb1 = nmemb;
      bytes = nmemb1 * size;
      (void)memmove(&dsp->data[dsp->position], ptr, bytes);
      dsp->position += bytes;
    }
  else
    nmemb1 = 0;

  if (nmemb1 < nmemb)
    dsp->eof = 1;

  return nmemb1;
}

// Here's one not in the C library: write some zeroes (but not too many).
size_t crm114__dbwrite_zeroes(size_t size, size_t nmemb,
			      struct data_state *dsp)
{
  size_t nmemb1;
  size_t bytes;

  if (dsp->position < dsp->size) // both size_t: unsigned
    {
      nmemb1 = (dsp->size - dsp->position) / size;
      if (nmemb1 > nmemb)
	nmemb1 = nmemb;
      bytes = nmemb1 * size;
      (void)memset(&dsp->data[dsp->position], '\0', bytes);
      dsp->position += bytes;
    }
  else
    nmemb1 = 0;

  if (nmemb1 < nmemb)
    dsp->eof = 1;

  return nmemb1;
}


// Now functions that use the above to move vectors in and out of a data block.

/*************************************************************************
 *Write a vector to a db.  Writes everything in binary format.
 *
 *INPUT: v: vector to write
 * db: CRM114_DATABLOCK to write vector to
 *
 *OUTPUT: number of bytes written.
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(s)
 * SPARSE_LIST: O(s)
 *************************************************************************/

size_t crm114__db_vector_write_bin(Vector *v, CRM114_DATABLOCK *db) {
  size_t size;
  struct data_state ds;

  crm114__dbopen(db, &ds);
  size = crm114__db_vector_write_bin_dsp(v, &ds);
  crm114__dbclose(&ds);
  return size;
}

//"private" function to write non-sparse vector to db in binary
static size_t db_vector_write_bin_ns(Vector *v, struct data_state *dsp) {
  if (!v || !dsp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "db_vector_write_bin_ns: null arguments.\n");
    }
    return 0;
  }
  if (v->type != NON_SPARSE) {
    return crm114__db_vector_write_bin_dsp(v, dsp);
  }

  if (v->compact) {
    return sizeof(int)*crm114__dbwrite(v->data.nsarray.compact, sizeof(int), v->dim,
			       dsp);
  }

  return sizeof(double)*crm114__dbwrite(v->data.nsarray.precise, sizeof(double), v->dim,
			       dsp);
}

//writes a node to a block of data
static inline size_t db_node_write(SparseNode n, struct data_state *dsp) {
  if (null_node(n) || !dsp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "db_node_write: null arguments.\n");
    }
  }
  if (n.is_compact) {
    return sizeof(CompactSparseNode)*crm114__dbwrite(n.compact,
					     sizeof(CompactSparseNode), 1, dsp);
  }

  return sizeof(PreciseSparseNode)*crm114__dbwrite(n.precise,
					   sizeof(PreciseSparseNode), 1, dsp);
}

/*************************************************************************
 *Write a vector to an "open" block of data.  Writes everything in binary format.
 *
 *INPUT: v: vector to write
 * dsp: struct data_state * to write vector through
 *
 *OUTPUT: number of bytes written
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(s)
 * SPARSE_LIST: O(s)
 *************************************************************************/

size_t crm114__db_vector_write_bin_dsp(Vector *v, struct data_state *dsp) {
  size_t size;

  if (!v || !dsp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "db_vector_write: null arguments.\n");
    }
    return 0;
  }

  size = sizeof(Vector)*crm114__dbwrite(v, sizeof(Vector), 1, dsp);

  switch(v->type) {
  case NON_SPARSE:
    {
      size += db_vector_write_bin_ns(v, dsp);
      return size;
    }
  case SPARSE_ARRAY:
    {
      size += crm114__db_expanding_array_write(v->data.sparray, dsp);
      return size;
    }
  case SPARSE_LIST:
    {
      size += crm114__db_list_write(v->data.splist, dsp);
      return size;
    }
  default:
    {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "crm114__db_vector_write_bin_dsp: unrecognized type\n");
      }
      return size;
    }
  }
}

/*************************************************************************
 *Read a vector from a binary CRM114_DATABLOCK.
 *
 *INPUT: filename: db to read vector from
 *
 *OUTPUT: vector from file or NULL if the file is incorrectly formatted
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(s)
 * SPARSE_LIST: O(s)
 *
 *WARNINGS:
 *1) This expects binary data formatted as crm114__db_vector_write_bin does.  If
 *   the data is incorrectly formatted, this may return NULL or it may
 *   return some weird interpretation.  Check the output!
 *************************************************************************/

Vector *crm114__db_vector_read_bin(CRM114_DATABLOCK *db) {
  Vector *v;
  struct data_state ds;

  crm114__dbopen(db, &ds);
  v = crm114__db_vector_read_bin_dsp(&ds);
  crm114__dbclose(&ds);
  return v;
}

//private function to read a non-sparse vector from a binary block of data
static void db_vector_read_bin_ns(Vector *v, struct data_state *dsp) {
  size_t amount_read = 0;

  if (v->type != NON_SPARSE) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "Called db_vector_read_bin_ns on non-sparse vector.\n");
    }
    return;
  }

  if (v->compact) {
    if (v->data.nsarray.compact) {
      amount_read = crm114__dbread(v->data.nsarray.compact, sizeof(int), v->dim, dsp);
    }
  } else {
    if (v->data.nsarray.precise) {
      amount_read = crm114__dbread(v->data.nsarray.precise, sizeof(double), v->dim,
			   dsp);
    }
  }
  if (v->dim && !amount_read) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "Warning: nothing was read into non-sparse vector.\n");
    }
    v->dim = 0;
  }
}

//reads a node from a block of data
static inline SparseNode db_node_read(int is_compact, struct data_state *dsp) {
  SparseNode n = make_null_node(is_compact);
  size_t nr;

  if (!dsp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "db_node_read: bad file pointer.\n");
    }
    return n;
  }

  if (n.is_compact) {
    n.compact = (CompactSparseNode *)malloc(sizeof(CompactSparseNode));
    nr = crm114__dbread(n.compact, sizeof(CompactSparseNode), 1, dsp);
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
  nr = crm114__dbread(n.precise, sizeof(PreciseSparseNode), 1, dsp);
  if (!nr) {
    //end of file
    free(n.precise);
    return make_null_node(is_compact);
  }
  n.precise->next = NULL;
  n.precise->prev = NULL;
  return n;
}

/*************************************************************************
 *Read a vector from an "open" binary block of data.
 *
 *INPUT: dsp: data block to read vector from
 *
 *OUTPUT: vector from data or NULL if the data is incorrectly formatted
 *
 *TIME:
 * NON_SPARSE: O(c)
 * SPARSE_ARRAY: O(s)
 * SPARSE_LIST: O(s)
 *
 *WARNINGS:
 *1) This expects a binary file formatted as crm114__db_vector_write_bin does.  If
 *   the file is incorrectly formatted, this may return NULL or it may
 *   return some weird interpretation.  Check the output!
 *************************************************************************/

Vector *crm114__db_vector_read_bin_dsp(struct data_state *dsp) {
  Vector tmpv, *v;
  size_t amount_read;

  amount_read = crm114__dbread(&tmpv, sizeof(Vector), 1, dsp);
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
      db_vector_read_bin_ns(v, dsp);
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
      crm114__db_expanding_array_read(v->data.sparray, dsp);
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
      v->nz = crm114__db_list_read(v->data.splist, dsp, v->nz);
      return v;
    }
  default:
    {
      if (CRM114__MATR_DEBUG_MODE) {
	fprintf(stderr, "crm114__db_vector_read_bin_dsp: unrecognized type.\n");
      }
      return v;
    }
  }
}

/***************************************************************************
 *Writes A to a block of data in binary format.
 *
 *INPUT: A: the array to write
 * dsp: analogous to FILE *
 *
 *WARNINGS:
 *1) A is written in a BINARY format.  Use crm114__expanding_array_read to recover
 *   A.
 ***************************************************************************/

size_t crm114__db_expanding_array_write(ExpandingArray *A, struct data_state *dsp) {
  size_t size;

  if (!A || !dsp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__db_expanding_array_write: null arguments.\n");
    }
    return 0;
  }

  size = sizeof(ExpandingArray)*crm114__dbwrite(A, sizeof(ExpandingArray),
					 1, dsp);
  if (A->length && A->length >= A->first_elt) {
    if (A->compact && A->data.compact) {
      return size + sizeof(CompactExpandingType)*
	crm114__dbwrite(&(A->data.compact[A->first_elt]), sizeof(CompactExpandingType),
	       A->n_elts, dsp);
    }
    if (!(A->compact) && A->data.precise) {
      return size + sizeof(PreciseExpandingType)*
	crm114__dbwrite(&(A->data.precise[A->first_elt]), sizeof(PreciseExpandingType),
	       A->n_elts, dsp);
    }
  }

  return size;
}

/***************************************************************************
 *Reads A from a block of data in binary format.
 *
 *INPUT: A: an expanding array.  if A contains any data it will be freed
 *  and overwritten.
 * dsp: analogous to FILE *
 *
 *WARNINGS:
 *1) If dsp does not contain a properly formatted expanding array as written
 *   by the function crm114__expanding_array_write, this function will do its best,
 *   but the results may be very bizarre.  Check for an empty return.
 ***************************************************************************/

void crm114__db_expanding_array_read(ExpandingArray *A, struct data_state *dsp) {
  size_t amount_read;

  if (!A || !dsp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__db_expanding_array_read: null arguments.\n");
    }
    return;
  }

  if (A->compact && A->data.compact && !(A->was_mapped)) {
    free(A->data.compact);
  } else if (!(A->compact) && A->data.precise && !(A->was_mapped)) {
    free(A->data.precise);
  }
  amount_read = crm114__dbread(A, sizeof(ExpandingArray), 1, dsp);
  A->was_mapped = 0;
  if (!amount_read) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__db_expanding_array_read: bad file.\n");
    }
    return;
  }

  if (A->length >= A->n_elts &&
      A->first_elt < A->length && A->first_elt >= 0) {
    if (A->compact) {
      A->data.compact = (CompactExpandingType *)
	malloc(sizeof(CompactExpandingType)*A->length);
      amount_read = crm114__dbread(&(A->data.compact[A->first_elt]),
			  sizeof(CompactExpandingType), A->n_elts, dsp);
    } else {
      A->data.precise = (PreciseExpandingType *)
	malloc(sizeof(PreciseExpandingType)*A->length);
      amount_read = crm114__dbread(&(A->data.precise[A->first_elt]),
			  sizeof(PreciseExpandingType), A->n_elts, dsp);
    }
    if (amount_read < A->n_elts && CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr,
	      "crm114__db_expanding_array_read: fewer elts read in than expected.\n");
    }
  } else {
    if (CRM114__MATR_DEBUG_MODE && A->n_elts) {
      fprintf(stderr, "crm114__db_expanding_array_read: A cannot contain all of its elements.  This is likely a corrupted file.\n");
    }
    A->length = 0;
    A->n_elts = 0;
    A->first_elt = 0;
    A->last_elt = -1;
    A->data.precise = NULL;
  }
}

/***************************************************************************
 *Writes l to a block of data in binary format.
 *
 *INPUT: l: the array to write
 * fp: pointer to file to write l in
 *
 *WARNINGS:
 *1) l is written in a BINARY format.  Use crm114__list_read to recover
 *   l.
 ***************************************************************************/

size_t crm114__db_list_write(SparseElementList *l, struct data_state *dsp) {
  SparseNode curr;
  size_t size;

  if (!l || !dsp) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__list_write: null arguments.\n");
    }
    return 0;
  }

  size = sizeof(SparseElementList)*crm114__dbwrite(l, sizeof(SparseElementList),
					  1, dsp);
  curr = l->head;
  while (!null_node(curr)) {
    size += db_node_write(curr, dsp);
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

int crm114__db_list_read(SparseElementList *l, struct data_state *dsp, int n_elts) {
  SparseNode n, pn;
  int i;

  if (!l || !dsp || n_elts < 0) {
    if (CRM114__MATR_DEBUG_MODE) {
      fprintf(stderr, "crm114__list_write: null arguments.\n");
    }
    return 0;
  }

  if (!crm114__list_is_empty(l)) {
    crm114__list_clear(l);
  }

  l->last_addr = NULL;

  (void)crm114__dbread(l, sizeof(SparseElementList), 1, dsp);
  if (n_elts <= 0) {
    return 0;
  }
  l->head = db_node_read(l->compact, dsp);
  pn = l->head;
  for (i = 1; i < n_elts; i++) {
    if (null_node(pn)) {
      break;
    }
    n = db_node_read(l->compact, dsp);
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
      fprintf(stderr, "crm114__db_list_read: Couldn't read in enough elements.\n");
    }
  }
  l->tail = pn;
  return i;
}
