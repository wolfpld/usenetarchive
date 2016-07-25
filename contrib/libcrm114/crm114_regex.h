// Copyright 2001-2010 William S. Yerazunis.
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


#include <tre/regex.h>


extern int crm114__regncomp(regex_t *preg, const char *regex, long regex_len,
			    int cflags);
extern int crm114__regnexec(const regex_t *preg, const char *string,
			    long string_len, size_t nmatch, regmatch_t pmatch[],
			    int eflags);
extern size_t crm114__regerror(int errcode, const regex_t *preg, char *errbuf,
			       size_t errbuf_size);
extern void crm114__regfree(regex_t *preg);
