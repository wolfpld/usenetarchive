#ifndef __CRM114_DATALIB_H__
#define __CRM114_DATALIB_H__

// conversion hack
// Direct replacements for some C library file functions, but deal
// with a CRM114_DATABLOCK instead of a file.  All use a pointer
// analogous to a FILE *.  Also, vector read/write functions that call
// the others, to "read/write" vectors from/to a CRM114_DATABLOCK.

// all we need is CRM114_DATABLOCK and stdio.h (SEEK_CUR, etc.), but
// everything's tied to everything
#include <string.h>
#include "crm114_sysincludes.h"
#include "crm114_config.h"
#include "crm114_structs.h"
#include "crm114_lib.h"
#include "crm114_internal.h"

// Replacement for FILE -- describe state of "open" data block.
struct data_state
{
  char *data;	       // pointer to "file", a block of data in memory
  size_t size;	       // size of "file"
  size_t position;     // offset from beginning
  int eof;	       // T/F: somebody tried to go too far
};

// subset of fopen()
void crm114__dbopen(CRM114_DATABLOCK *db, struct data_state *dsp);
// fclose()
int crm114__dbclose(struct data_state *dsp);
// ftell()
long crm114__dbtell(struct data_state *dsp);
// fseek()
// uses C library values for whence
int crm114__dbseek(struct data_state *dsp, long offset, int whence);
// feof()
int crm114__dbeof(struct data_state *dsp);
// fread()
size_t crm114__dbread(void *ptr, size_t size, size_t nmemb,
	      struct data_state *dsp);
// fwrite(), sort of: refuses to write past EOF, trying to sets EOF
// it would be bad to write off the end of our data block
size_t crm114__dbwrite(const void *ptr, size_t size, size_t nmemb,
	       struct data_state *dsp);
// Here's one not in the C library: write some zeroes (but not too many).
size_t crm114__dbwrite_zeroes(size_t size, size_t nmemb,
			      struct data_state *dsp);
size_t crm114__db_vector_write_bin(Vector *v, CRM114_DATABLOCK *db);
size_t crm114__db_vector_write_bin_dsp(Vector *v, struct data_state *dsp);
Vector *crm114__db_vector_read_bin(CRM114_DATABLOCK *db);
Vector *crm114__db_vector_read_bin_dsp(struct data_state *dsp);
size_t crm114__db_expanding_array_write(ExpandingArray *A, struct data_state *dsp);
void crm114__db_expanding_array_read(ExpandingArray *A, struct data_state *dsp);
size_t crm114__db_list_write(SparseElementList *l, struct data_state *dsp);
int crm114__db_list_read(SparseElementList *l, struct data_state *dsp, int n_elts);


#endif	// !defined(__CRM114_DATALIB_H__)
