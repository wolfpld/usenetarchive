//	crm114_svm.h - Support Vector Machine

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

#ifndef __CRM114_SVM_H__
#define __CRM114_SVM_H__

#include <string.h>
#include "crm114_sysincludes.h"
#include "crm114_config.h"
#include "crm114_structs.h"
#include "crm114_lib.h"
#include "crm114_internal.h"
#include "crm114_svm_lib_fncts.h"
#include "crm114_datalib.h"


//this is the full SVM block we use while learning
//for classifying, appending, etc, we usually only
//read or write part of the block.
typedef struct {
  int n_old,         //the number of examples in oldXy
                     //ie, the number of examples we have seen but aren't
                     //training on currently
    has_solution,    //1 if the block contains a solution, 0 else
    n0, n1,          //number of examples in classes 0 (n0) and 1 (n1)
    n0f, n1f,        //number of total features in classes 0 (n0f) and 1 (n1f)
    map_size;        //the size of the mapping from rows to vectors
  SVM_Solution *sol; //previous solution if it exists or NULL
  Matrix *oldXy;     //examples we've seen but aren't training on or NULL
  Matrix *newXy;     //examples we haven't trained on yet or NULL
} svm_block;

#endif // !defined(__CRM114_SVM_H__)
