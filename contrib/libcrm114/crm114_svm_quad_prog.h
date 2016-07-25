//	crm114_svm_quad_prog.h - Support Vector Machine

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

#ifndef __CRM114_SVM_QUAD_PROG_H__
#define __CRM114_SVM_QUAD_PROG_H__

#include "crm114_matrix_util.h"
#include "crm114_matrix.h"

extern int CRM114__MATR_DEBUG_MODE; //debugging mode. see crm114_matrix_util.h
                                    //for possible values.

#define QP_DEBUG 2              //basic information about the qp solver

#define QP_DEBUG_LOOP 3         //prints some information during each qp loop
                                //useful if the svm is getting stuck during a QP
                                //problem

#define QP_LINEAR_SOLVER 4      //prints some information during each cg loop
                                //useful to discover if the run goes on forever
                                //because the cg isn't converging
                                //(usually indicates a bug in the add or remove
                                //constraint functions!)

#define QP_CONSTRAINTS 5        //prints out information about adding and
                                //removing constraints during the qp solver


//the accuracy to which we run conjugate_gradient
//this should be a pretty small number!!!
#define QP_LINEAR_ACCURACY 1e-10
//we should never exceed this but here just in case
#define QP_MAX_ITERATIONS 1000

void crm114__run_qp(Matrix *G, Matrix *A, Vector *c, Vector *b, Vector *x);

//int main(int argc, char **argv);

#endif // !defined(__CRM114_SVM_QUAD_PROG_H__)
