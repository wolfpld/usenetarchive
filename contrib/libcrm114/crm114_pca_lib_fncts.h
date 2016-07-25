//	crm114_pca_lib_fncts.h - Principal Component Analysis

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


#ifndef __CRM114_PCA_LIB_FNCTS_H__
#define __CRM114_PCA_LIB_FNCTS_H__

#include "crm114_matrix_util.h"
#include "crm114_matrix.h"
#include "crm114_config.h"
#include <string.h>

#define PCA_DEBUG 1         //Debug mode defines - prints out minimal information
#define PCA_LOOP 8          //Prints out matrices and vectors - only use for small problems
                            //The intermediate DEBUG modes may enable debug printing for the
                            //matrix operations.  See crm114_matrix_util.h for details.

int CRM114__PCA_DEBUG_MODE;         //The debug mode for the PCA
extern int CRM114__MATR_DEBUG_MODE; //Debug mode for matrices.  CRM114__MATR_DEBUG_MODE = CRM114__PCA_DEBUG_MODE
                            //Defined in crm114_matrix_util.h

typedef struct {
  Vector *theta;     //first principal component
  double mudottheta; //decision point (unlike SVM this isn't necessarily 0)
} PCA_Solution;

PCA_Solution *run_pca(Matrix *M, Vector *init_pca);
ExpandingArray *pca_preprocess(Matrix *X, Vector *init_pca);
void crm114__pca_solve(Matrix *X, PCA_Solution **init_pca);
void crm114__pca_free_solution(PCA_Solution *sol);

#endif //crm114_pca_lib_fncts.h
