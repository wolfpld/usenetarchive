//	crm114_svm_lib_fncts.c - Support Vector Machine

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


#include "crm114_svm.h"

#define DEFINE_UNUSED_STATIC_FUNCTIONS 0

//static function declarations
#if DEFINE_UNUSED_STATIC_FUNCTIONS
static SVM_Solution *svm_solve_no_init_sol(Matrix *Xy);
#endif
static SVM_Solution *svm_solve_init_sol(Matrix *Xy, Vector *st_theta,
					double weight, int max_train_val);
static ExpandingArray *svm_preprocess(Matrix *X, Vector *old_theta);

/**********************************************************************
 *This method is taken from:
 * Training SVMs in Linear Time
 * Thorsten Joachims
 * ACM Conference on Knowledge Discovery and Data Mining 2006
 *
 *For labeled examples {(x_1, y_1), (x_2, y_2), ..., (x_n, y_n)}, the classic
 *SVM problem is
 * min_{theta, zeta >= 0} 0.5*theta*theta + C/n*sum_{i = 1}^n zeta_i
 *  s.t. for all i 1 <= i <= n, y_i*theta*x_i >= 1 - zeta_i
 *where zeta is the vector of slack variables and C is large and positive.
 *The classification of an example x is
 *               h(x) = sgn(theta*x)
 *Note that this formulation DOES NOT INCLUDE A CONSTANT VALUE.  If you want
 *a constant value (so that h(x) = sgn(theta*x + b)), you can create that by
 *adding an extra column to each example with the value +1.
 *
 *Now define a binary vector c of length n.  We will call this a "constraint
 *vector" and there are 2^n such vectors.
 *Let x_c = 1/n sum_{i=1}^n c_i*y_i*x_i for any c.
 *Then Joachims shows that the problem formulation
 * min_{theta, zeta >= 0} 0.5*theta*theta + C*zeta
 *  s.t. for all c \in {0, 1}^n, theta*x_c >= 1/n*||c||_1 - zeta
 *where ||c||_1 is the L1-norm (ie the number of 1's in c) is equivalent
 *to the problem given above.  In its dual form this is
 * max_{alpha >= 0} \sum_{c \in {0, 1}^n} 1/n*||c||_1*alpha_c -
                       0.5*sum_{c, c'} alpha_c alpha_c' x_c*x_c'
 *  s.t. sum_{c} alpha_c <= C (DUAL PROBLEM)
 *where theta = sum_c alpha_c*x_c and
 *zeta = max_c (1/n||c||_1 - theta*x_c
 *In QP terms (which requires a sign change since we minimize) the problem is
 * min_{alpha} 0.5*alpha*H*alpha + f*alpha
 *  s.t. A*alpha >= b
 * where H_{c, c'} = x_c*x_c' (k x k on the kth iteration)
 *       f_c = -1/n*||c||_1 (1 x k)
 *       A is k+1xk with the top row all -1's (corresponding to sum_{c} alpha_c)
 *         and the bottom kxk matrix the kxk identity matrix times -1
 *       b is the vector 1xk+1 vector with the first entry -SVM_MAX_X_VAL
 *         and the last k entries 0
 *
 *Clearly this form of the problem has an exponential number of contraints.
 *To solve it efficiently we use a cutting-plane method to decide which
 *constraints are important.  Specifically we do the following:
 * 1) Solve the DUAL PROBLEM over the current H, f, A, b
 *     see quad_prog.c for how that is done.
 * 2) Calculate theta and zeta from the alpha_c using the above equations
 * 3) Calculate the most violated constraint c
 *     c has a 1 in the i^th position if x_i is not classified correctly
 *     by a large enough problem
 * 4) Check how much this constraint is violated by.
 *     This corresponds to zeta' = 1/n*||c||_1 - theta*x_c
 *     If this error is within ACCURACY of the training error zeta
 *     from the last QP run (ie within ACCURACY of "as close as we could get")
 *     we return
 * 5) Update the arguments to the QP solver
 *     This requires:
 *     Adding a row and a column to H of the dot products with the new x_c
 *     Adding -1/n||c||_1 as the last entry of f
 *     Adding a row and column to A.  The top and bottom entries of the new
 *      column are -1
 *     Adding a new 0 entry to b
 *     We also save x_c so that we can recreate theta from the alpha_c's
 *
 *
 *INPUT: This method takes as input the data matrix X where
 * X_i = y_i*x_i
 * Here X_i is the ith row of X, y_i is the label (+/-1) of the ith
 *  example and x_i is the ith example.  Ie, if x_i is an example belonging to
 *  class -1, the ith row of X would be x_i multiplied by -1.  (In the case where
 *  x_i contains only positive entries, then the ith row of X would contain only
 *  negative entries.)
 *
 *OUTPUT: The function returns a solution struct.  This struct contains the
 * vector theta, which is the decision boundary: The classification of
 * an example x is
 *                      h(x) = theta*x
 * theta is a NON-SPARSE vector.  If X originally had a number of nonzero
 * columns, you need to convert theta into a sparse vector using the
 * colMap returned from preprocess.  If you call this function using the
 * wrapper function, solve, it will take care of this process for you and
 * return theta as a sparse vector.
 *
 * The solution struct also contains all of the support vectors, which allows
 * you to restart the learning with a new example without having to relearn
 * which of the old examples were support vectors.  Support vectors are those
 * vectors on or in violation of the margin (y_i*theta*x_i <= 1 => x_i is a
 * support vector).
 *
 * There is also data in the solution struct allowing the restart of the
 * solution.  The variable num_examples will be set to Xy->rows and
 * max_train_val = curr_max_train_val - sum_c alpha_c.  For how these are used
 * see the comment to svm_solve_init_solution.
 *
 * Xy will then only contain those vectors that ARE NOT support vectors.
 * Note that Xy WILL CHANGE.
 *
 *TYPES: We recommend that X be a SPARSE ARRAY to optimize memory caching
 * but testing has shown that making it a SPARSE LIST doesn't slow anything
 * down too much.  Not sure why you would want to do that though since X is
 * static and a SPARSE LIST would actually take more memory.
 * DO NOT CHANGE THE TYPES IN THE FUNCTION.  To make this as fast as possible,
 * I have moved away in some places from using structure-independent calls.
 * If you, for example, change theta to be anything but a MATR_PRECISE NON_SPARSE
 * vector it WILL cause a seg fault.
 *
 *ACCURACY: This runs until it finds a solution within some "accuracy"
 * parameter.  Here the accuracy on each iteration with new constraint vector
 * c, current slack variable zeta, and current decision boundary theta
 * is measured by the function:
 *      delta = ||c||_1/n - 1/n*sum_{i=1}^n c_i*theta*y_i*x_i - zeta
 * This is exactly the average over the margin violations of all of the
 * vectors minus the average margin violations already accounted for by the
 * slack variable.  In other words, accuracy is the *average margin violation
 * unaccounted for by the slack variable*.  Since this is MARGIN violation,
 * NOT necessarily a classification error (we would have to violate the margin
 * by more than 1 for a classification error), we can set the ACCURACY
 * parameter fairly high (ie 0.01 or 0.001) and still have good results.
 *
 *SV_TOLERANCE: An example x_i is tagged as a support vector if
 * theta*y_i*x_i <= 1 + SV_TOLERANCE.  In general, setting
 * SV_TOLERANCE = ACCURACY is approximately right since ACCURACY is kind of
 * how much play we have around the margin.  A low SV_TOLERANCE, will lead
 * to fast inclusion of new examples (because there are fewer support vectors
 * from old runs), but less accuracy.  One strategy might be to set
 * SV_TOLERANCE very low (even to 0), but rerun all seen examples every so
 * often.
 *
 *WARNINGS:
 *1) This function uses NON-SPARSE vectors of length X->cols
 *   to do fast dot products.  Therefore, X should not have a large
 *   number of fully zero columns.  If it is expected to, run
 *   preprocess first.
 *
 *2) X should contain NO ZERO ROWS.  If it may, run preprocess first.
 *
 *3) On return Xy contains only those vectors that are NOT support vectors.
 *   Xy WILL CHANGE.
 *
 *CALLING SVM_SOLVE: with a new matrix and a null old solution will
 * preprocess the matrix and feed it to this function correctly.
 ***********************************************************************/

/* not used currently, turned off to avoid compiler warning */
#if DEFINE_UNUSED_STATIC_FUNCTIONS

static SVM_Solution *svm_solve_no_init_sol(Matrix *Xy) {
  return svm_solve_init_sol(Xy, NULL, 0, SVM_MAX_X_VAL);
}

#endif

/*************************************************************************
 *Solves the SVM problem using an initial solution.  As far as I know
 *(I haven't really done that much research) this approach is novel.
 *
 *Note that all of the x_c's we calculated in our old solution still exist
 *if we simply assume 0's in the correct place for the c's.  The only
 *difference is that now we have more examples so the denominator changes.
 *Specifically if we had n_old examples before and n_new examples now
 *we need to update
 *                x_c -> n_old/(n_old+n_new)*x_c
 *Therefore, if the old decision boundary was theta, the new boundary is
 *               theta -> n_old/(n_old + n_new)*theta
 *
 *Now we break alpha, f, and H (see above comment for these definitions)
 *into two parts: the "old" parts, which we solved for before and the "new"
 *parts that we have yet to solve.  The QP becomes:
 * min_{alpha >= 0}  0.5*alpha_old*H_old*alpha_old + f_old*alpha_old
 *                 + 0.5*alpha_new*H_new*alpha_new + f_new*alpha_new
                   + alpha_new*H_{new, old}*alpha_old
 * s.t. sum_{c} alpha_c <= C
 *Clearly, the problem almost decouples - the only term involving alpha_new
 *and alpha_old is the last (note that H_{new, old} = X_{c, new}*X_{c, old}^T).
 *However, define
 * theta_new = alpha_new*X_{c, new} = sum_{new c} (alpha_c*x_c)
 * theta_old = X_{c, old}^T*alpha_old = sum_{old c} (alpha_c*x_c)
 *When we solved for alpha_old, we assumed that theta_new = 0.  How good was
 *this approximation?  Well, the full answer to the problem is
 * theta = theta_old + theta_new
 *If we assume that we are adding in just a few new examples to a problem we
 *have already pretty well learned, ||theta_new|| << ||theta_old||, making
 *this a good approximation.  Therefore, in the incremental learning, we
 *hold alpha_old constant.  This pulls out the terms just involving alpha_old
 *leaving us with the QP problem
 *
 * min_{alpha >= 0}  0.5*alpha_new*H_new*alpha_new + f_new*alpha_new
                   + alpha_new*H_{new, old}*alpha_old
 * s.t. sum_{new c} alpha_c <= C - sum_{old c} alpha_c
 *Now you might worry that the term alpha_new*H_{new, old}*alpha_old
 *is difficult to
 *calculate but in fact it is quite easy.  Consider:
 * H_{new, old}*alpha_old = X_new*X_old^T*alpha_old = X_new*theta_old
 *Therefore, just by saving theta_old (which we were doing anyway since it was
 *our old decision boundary!) we can simply fold the last term into the linear
 *term using
 * f' = f_new + X_new*theta_old.
 *Therefore, the only extra calculation we must do per iteration is
 * f_c' = f_c + x_c*theta_old.
 *This is a simple dot product.
 *
 *Now what if we didn't have a lot of examples to start with?  Does that mess
 *up this assumption?  Possibly.  Therefore, we also train on the old support
 *vectors.  This gives the old solution some "input" as well as dealing with
 *this problem - if our old solution incorporates very few examples they are
 *likely to be almost all support vectors.  Therefore, we will train on them
 *again.
 *
 *In addition, notice that each x_c is weighted by 1/n.  In this formulation
 *n is always the total number of examples seen, NOT the current number we
 *are training on.  That means that if our old decision boundary is theta_d,
 *theta_old = n_old/(n_old + n_new)*theta_d.  Thus if n_old is small and
 *n_new is large, our old solution will not influence the new solution much.
 *Similarly, as we argued above, if n_old is large and n_new is small, the
 *new solution is a small addition to the old one.
 *
 *There's one other issue: each time we go through this, we drop the maximum
 *allowed value for the sum of the alpha's.  If this value started fairly small
 *it can get to zero pretty quickly.  Therefore, we bottom it out at
 *SVM_MIN_X_VAL so that every new example will contribute something to the answer.
 *
 *A few notes about what the arguments to this function are:
 * Note that when we add more examples we need to "pretend" as though they
 * were there all along.  This means that any time in the old algorithm that
 * we divided by 1/n, n needs to increase to include the new examples.
 * Therefore, st_theta should actually NOT just be the old decision boundary
 * since that was calculated using the wrong n.  If the old decision boundary
 * was theta_d, st_theta should be
 *    st_theta = n_old/(n_old + n_new)*theta_d
 * In addition, in THIS algorithm anywhere we multiply by 1/n, we need to make
 * sure that n is n = n_old + n_new.  For that reason we pass in the "weight"
 * parameter where weight = 1.0/(n_old + n_new).
 *
 * We also have that sum_{c new} alpha_c <= C - sum_{c old} alpha_c.
 * Therefore, we need to remember the boundary on the alpha_c.  This is passed
 * in as max_sum = C - sum_{old c} alpha_c.  In summary:
 *
 *INPUT:
 * Xy: The matrix of the NEW examples and the OLD SUPPORT VECTORS multiplied
 *  by their label.
 * st_theta: The reweighted old decision boundary.  If the old decision
 *  boundary was theta_d calculated using n_old examples and we are adding in
 *  n_new examples, st_theta = n_old/(n_old + n_new)*theta_d.  If you have
 *  no old solution, use st_theta = NULL.
 * weight: If the previous solution was calculated on n_old examples and we
 *  are adding n_new examples, weight = 1.0/(n_old + n_new).  In other words,
 *  weight = 1.0/n where n is the TOTAL NUMBER OF EXAMPLES WE HAVE SEEN.
 * max_sum: The sum of all alpha's calculated in the old solution subtracted
 *  from the original maximum value max_sum = SVM_MAX_X_VAL - sum_{old c} alpha_c.
 *
 *
 *OUTPUT:
 * The function returns a solution struct.  See the comment to
 * svm_solve_no_init_sol for an explanation of that struct.
 *
 *TRAINING METHOD:
 * The incremental method is most error prone in the region
 * ||theta_new ~= theta_old||.  If you
 * have about the same number of old and new examples, it is almost certainly
 * better and not much (if any) slower to retrain the whole thing than to try
 * to use the incremental method to add those on.  This is ESPECIALLY TRUE if
 * the new examples are differently biased (ie many more negative or many more
 * positive) than the old example.
 * THE SVM IS MOST SENSITIVE TO THE MOST RECENTLY TRAIN THINGS!  If you are
 * using an incremental training method, try to mix positive and negative
 * examples as much as possible!
 *
 *TYPES, ACCURACY, SV_TOLERANCE: See the comment to svm_solve_no_init_sol.
 *
 *WARNINGS:
 *1) This function uses NON-SPARSE vectors of length X->cols
 *   to do fast dot products.  Therefore, X should not have a large
 *   number of fully zero columns.  If it is expected to, run
 *   preprocess first.
 *
 *2) X should contain NO ZERO ROWS.  If it may, run preprocess first.
 *
 *3) On return Xy contains only those vectors that are NOT support vectors.
 *   Xy WILL CHANGE.
 *
 *4) st_theta is NOT the old solution.  It is the old solution REWEIGHTED by
 *   n_old/(n_old + n_new).
 *
 *5) Xy should ONLY contain new examples and old support vectors.  It should
 *   NOT contain previously seen old non-support vectors if st_theta is
 *   non-null.
 *
 *CALLING SVM_SOLVE: with an old solution struct and a new matrix will compute
 * the correct arguments to this function and take care of the preprocessing.
 * We HIGHLY RECOMMEND that you do that.  This function is static because
 * the arguments to it are complicated!
 ****************************************************************************/

static SVM_Solution *svm_solve_init_sol(Matrix *Xy, Vector *st_theta,
					double weight, int max_sum) {
  unsigned int n, i, j;
  Vector *row, //a row of XC usually
    *xc, //the x_c we are adding
    //Lagrange multipliers - the solution of the QP problem
    *alpha = crm114__vector_make(0, SPARSE_LIST, MATR_PRECISE),
    //Solution to the SVM.  Should be NON_SPARSE for fastest execution.
    *theta = crm114__vector_make(Xy->cols, NON_SPARSE, MATR_PRECISE),
    //Linear term in the QP problem (-1/n*||c||_c + st_theta*x_c)
    *f = crm114__vector_make(0, NON_SPARSE, MATR_PRECISE),
    //The L1 norms of the c
    *l1norms = crm114__vector_make(0, NON_SPARSE, MATR_COMPACT),
    //The constraint vector for the QP problem.  The first term of
    //b is max_sum.  The rest are zeros
    *b = crm114__vector_make(1, SPARSE_LIST, MATR_PRECISE);
  double delta = SVM_ACCURACY + 1, zeta, d, s, dev;
  //The Hessian H_{c, c'} = x_c*x_c'
  //You could try to save space by making this compact and leaving out the 1/n^2
  //terms, but the numbers in H will quickly exceed 32 bit so it's probably
  //not worth it
  Matrix *H = crm114__matr_make(0, 0, NON_SPARSE, MATR_PRECISE),
    //The constraint matrix for the QP problem.  The top row of
    //A is k+1xk where k is the number of iterations.
    //The top row is all 1's for the constraint sum_{c}alpha_c <= C
    //The remaining kxk matrix is I_k (kxk identity) to represent alpha_c >= 0
    *A = crm114__matr_make_size(1, 0, SPARSE_ARRAY, MATR_COMPACT, SVM_EXP_MAX_IT),
    //The current x_c's we are considering.  We make this compact by actually
    //storing n*x_c.  This should be NON_SPARSE for fastest execution.
    *XC = crm114__matr_make(0, Xy->cols, NON_SPARSE, MATR_COMPACT);
  VectorIterator vit;
  int nz, loop_it = 0, offset;
  int *sv;
  SVM_Solution *sol;

  CRM114__MATR_DEBUG_MODE = CRM114__SVM_DEBUG_MODE;

  if (!alpha || !theta || !f || !l1norms || !b || !H || !A || !XC) {
    if (CRM114__SVM_DEBUG_MODE) {
      fprintf(stderr, "Error initializing svm solver.\n");
    }
    crm114__vector_free(alpha);
    crm114__vector_free(theta);
    crm114__vector_free(f);
    crm114__vector_free(l1norms);
    crm114__vector_free(b);
    crm114__matr_free(H);
    crm114__matr_free(A);
    crm114__matr_free(XC);
    return NULL;
  }


  // sv was a C99 variable-length array, but Microsoft is C89
  sv = malloc(Xy->rows * sizeof(int));
  n = Xy->rows;

  //set up the first row of b to be SVM_MAX_X_VAL (ie the constant C)
  //note that our QP solver takes constraints of the form A*x >= b
  //so everything is multiplied by -1
  vectorit_set_at_beg(&vit, b);
  if (max_sum > SVM_MAX_X_VAL) {
    max_sum = SVM_MAX_X_VAL;
  }
  if (max_sum < SVM_MIN_X_VAL) {
    max_sum = SVM_MIN_X_VAL;
  }
  if (CRM114__SVM_DEBUG_MODE >= SVM_SOLVER_DEBUG) {
    fprintf(stderr, "Using %d as limit for multipliers.\n", max_sum);
  }
  crm114__vectorit_insert(&vit, 0, -1.0*max_sum, b);
  if (CRM114__SVM_DEBUG_MODE >= SVM_SOLVER_DEBUG_LOOP) {
    fprintf(stderr, "Xy = \n");
    crm114__matr_print(Xy);
  }

  if (weight < MATRIX_EPSILON) {
    weight = 1.0/n;
  }

  while (delta > SVM_ACCURACY && loop_it < SVM_MAX_SOLVER_ITERATIONS) {
    if (!(loop_it % SVM_CHECK) && delta <= SVM_CHECK_FACTOR*SVM_ACCURACY) {
      //close enough
      break;
    }

    //run the QP problem
    if (H->rows > 0) {
      if (CRM114__SVM_DEBUG_MODE >= SVM_SOLVER_DEBUG) {
	fprintf(stderr, "Running quadratic programming problem.\n");
      }
      crm114__run_qp(H, A, f, b, alpha);
      if (CRM114__SVM_DEBUG_MODE >= SVM_SOLVER_DEBUG) {
	fprintf(stderr, "Returned from quadratic programming problem.\n");
      }
    }

    //calculate theta
    //time for loop is N*|W|
    //theta = st_theta + sum_c alpha_c*x_c
    vectorit_set_at_beg(&vit, alpha);
    if (st_theta) {
      crm114__vector_copy(st_theta, theta);
    } else {
      crm114__vector_zero(theta);
    }
    //sum_c alpha_c*x_c
    while(!vectorit_past_end(vit, alpha)) {
      row = matr_get_row(XC, vectorit_curr_col(vit, alpha));
      if (!row) {
	continue;
      }
      crm114__vector_add_multiple(theta, row, weight*vectorit_curr_val(vit, alpha),
			  theta);
      //for (i = 0; i < XC->cols; i++) {
      //theta->data.nsarray.precise[i] +=
      //  weight*(vit.pcurr->data.data)*(row->data.nsarray.compact[i]);
      //}
      vectorit_next(&vit, alpha);
    }

    //calculate which examples we aren't classifying
    //with a high enough margin
    //this gives us our new x_c and also the average margin
    //deviation over ALL the examples (the variable dev)
    crm114__matr_add_row(XC);
    xc = matr_get_row(XC, XC->rows-1); //will hold our x_c
    if (!xc) {
      //this indicates that something has gone really wrong
      //probably the original input was corrupted
      break;
    }
    s = 0;
    nz = 0;
    for (i = 0; i < n; i++) {
      //d = crm114__dot(theta, example i);
      d = 0;
      row = matr_get_row(Xy, i);
      if (!row) {
	continue;
      }

      d = crm114__dot(theta, row);
      if (d < 1) {
	//we violate the margin
	s += d; //add it to our average deviation
	crm114__vector_add(xc, row, xc); //and to x_c
	nz++; //number of ones in c
      }
      //keep track of the support vectors
      //namely those that are exactly at the margin
      //or in violation
      //we will save them in case we need to restart the learning
      if (d <= 1.0+SV_TOLERANCE) {
	//support vector!
	sv[i] = 1;
      } else {
	sv[i] = 0;
      }
    }

    //this is the average deviation from the margin
    dev = weight*s;

    //add a row and a column to H
    //corresponding to the new XC
    //and calculate zeta
    crm114__matr_add_col(H);
    crm114__matr_add_row(H);
    zeta = 0;
    vectorit_set_at_beg(&vit, f);

    //loop is |W|*N
    for (i = 0; i < H->rows; i++) {
      //d = (weight^2)*crm114__dot(matr_get_row(XC, i), xc);
      d = 0;
      //s = weight*crm114__dot(matr_get_row(XC, i), theta)
      s = 0;
      row = matr_get_row(XC, i);
      if (!row) {
	continue;
      }
      //it's more efficient to do both of these together
      //enough to have an impact on the running time
      //since xc, row, and theta are all initialized in this
      //function, we know what vector type they are
      //and can access the data directly
      for (j = 0; j < XC->cols; j++) {
	d += xc->data.nsarray.compact[j]*
	  row->data.nsarray.compact[j];
	s += theta->data.nsarray.precise[j]*
	  row->data.nsarray.compact[j];
      }
      d *= weight*weight;

      //these are inserts at the end of a sparse vector
      //will be fast
      //note that H is symmetrical (yay for positive semi-definiteness!)
      row = matr_get_row(H, H->rows-1);
      if (!row) {
	//bad problems
	break;
      }
      vectorit_set_at_end(&vit, row);
      crm114__vectorit_insert(&vit, i, d, row);
      row = matr_get_row(H, i);
      if (!row) {
	//disaster!
	break;
      }
      vectorit_set_at_end(&vit, row);
      crm114__vectorit_insert(&vit, H->cols-1, d, row);

      //now calculate zeta
      //this is zeta from solving the QP problem waaaay at the top
      //of the loop
      //it is more efficient to calculate it here, but we need to
      //remember not to incorporate our newest (as yet untrained on) x_c
      if ((int)i < (int)(H->rows - 2)) {
	d = weight*(crm114__vector_get(l1norms, i) - s);
	if (d > zeta) {
	  zeta = d;
	}
      }
    }

    //add a column to f
    //this is 1/n*||c||_1 + x_c dot theta_old
    crm114__vector_add_col(f);
    vectorit_set_at_end(&vit, f);
    if (st_theta) {
      d = weight*crm114__dot(st_theta, xc);
    } else {
      d = 0;
    }
    crm114__vectorit_insert(&vit, f->dim-1, -1.0*(nz)*weight + d, f);

    //add a column to l1norms
    //this is the number of non-zero entries in c
    crm114__vector_add_col(l1norms);
    vectorit_set_at_end(&vit, l1norms);
    crm114__vectorit_insert(&vit, l1norms->dim-1, nz, l1norms);

    //add a row and a column to A
    crm114__matr_add_col(A);
    crm114__matr_add_row(A);
    row = matr_get_row(A, 0);
    if (!row) {
      //uh oh
      break;
    }
    vectorit_set_at_end(&vit, row);
    crm114__vectorit_insert(&vit, A->cols-1, -1, row);
    row = matr_get_row(A, A->rows-1);
    if (!row) {
      //not good
      break;
    }
    vectorit_set_at_end(&vit, row);
    crm114__vectorit_insert(&vit, A->cols-1, 1, row);

    //add a column to b (last element is zero)
    crm114__vector_add_col(b);

    //add a column to alpha
    //note that the solution to the last iteration is an excellent
    //starting point for the next iteration for exactly the reasons
    //that this iterative method works
    //so just add this column and
    //don't reset alpha to be anything
    crm114__vector_add_col(alpha);

    //calculate the accuracy
    //this is the average deviation from the margin
    //not already accounted for by zeta
    //note that we "assume" that old examples we are not training
    //on STILL don't violate the margin
    delta = weight*nz - dev - zeta;

    //print out more debugging information
    if (CRM114__SVM_DEBUG_MODE >= SVM_SOLVER_DEBUG_LOOP) {
      fprintf(stderr, "theta = ");
      crm114__vector_print(theta);
      fprintf(stderr, "x_c = ");
      crm114__vector_print(xc);
      fprintf(stderr, "alpha = ");
      crm114__vector_print(alpha);
      fprintf(stderr, "zeta = %.10lf dev = %lf nz = %d, weight = %lf\n",
	      zeta, dev, nz, weight);
    }

    if (CRM114__SVM_DEBUG_MODE >= SVM_SOLVER_DEBUG) {
      fprintf(stderr, "%d: delta = %.10lf\n", loop_it, delta);
    }
    loop_it++;
  }

  if (delta > SVM_ACCURACY + MATRIX_EPSILON && CRM114__SVM_DEBUG_MODE) {
    fprintf(stderr, "Warning: SVM solver did not converge all the way.  Full convergence would have solved to an accuracy of %lf - we solved only to an accuracy of %lf.  If this is not accurate enough, increase SVM_MAX_SOLVER_ITERATIONS, decrease SVM_CHECK_FACTOR, or change your training method.\n", SVM_ACCURACY,
	    delta);
  }

  //free everything!
  crm114__vector_free(f);
  crm114__vector_free(l1norms);
  crm114__vector_free(b);
  crm114__matr_free(H);
  crm114__matr_free(A);
  crm114__matr_free(XC);

  //make the solution block
  sol = (SVM_Solution *)malloc(sizeof(SVM_Solution));
  sol->theta = theta;
  sol->num_examples = n;

  //store the support vectors
  sol->SV = crm114__matr_make_size(0, Xy->cols, Xy->type, Xy->compact, Xy->size);
  offset = 0;
  for (i = 0; i < n; i++) {
    if (sv[i]) {
      row = matr_get_row(Xy, i - offset);
      if (!row) {
	continue;
      }
      crm114__matr_shallow_row_copy(sol->SV, sol->SV->rows, row);
      crm114__matr_erase_row(Xy, i-offset);
      offset++;
    }
  }

  //figure out what the maximum value is next time
  vectorit_set_at_beg(&vit, alpha);
  sol->max_train_val = max_sum;
  while (!vectorit_past_end(vit, alpha)) {
    sol->max_train_val -= vectorit_curr_val(vit, alpha);
    vectorit_next(&vit, alpha);
  }
  if (sol->max_train_val > SVM_MAX_X_VAL) {
    sol->max_train_val = SVM_MAX_X_VAL;
  } else if (sol->max_train_val < SVM_MIN_X_VAL) {
    sol->max_train_val = SVM_MIN_X_VAL;
  }

  //free more stuff!
  crm114__vector_free(alpha);
  free(sv);

  return sol;
}


/**********************************************************************
 *Removes zero rows and columns from the matrix X.
 *If the number of columns of X is an integer (ie X->cols < 2^32)
 *then this runs in constant time O(ns).
 *
 *INPUT: Matrix X from which to remove zero rows and columns.
 * old_theta: the old decision boundary if you have it.  this will densify
 *  the columns of that boundary so that it can be used with the preprocessed
 *  X.  If you have no old solution, pass in NULL.
 *
 *OUTPUT: An expanding array colMap mapping between the renumbered
 * columns of X and the old columns of X.  Specifically colMap[i] = j
 * if the ith column of X AFTER preprocessing was the jth column BEFORE
 * preprocessing.  X remains sparse.  The same is true of old_theta if
 * you passed one in.
 **********************************************************************/

static ExpandingArray *svm_preprocess(Matrix *X, Vector *old_theta) {
  ExpandingArray *colMap;

  colMap = crm114__matr_remove_zero_rows(X);
  crm114__expanding_array_free(colMap);
  if (X->type == NON_SPARSE) {
    //don't densify X if X is non-sparse
    return NULL;
  }
  if (old_theta) {
    crm114__matr_shallow_row_copy(X, X->rows, old_theta);
  }
  colMap = crm114__matr_remove_zero_cols(X);
  if (old_theta) {
    crm114__matr_erase_row(X, X->rows-1);
  }
    return colMap;
}

/***************************************************************
 *Solve the SVM problem
 *
 *INPUT: Xy: the matrix of examples.  Examples from class 0 should
 *  be multiplied by the label +1 and examples from class 1 should be
 *  multiplied by the label -1.
 * sol_ptr: A pointer to the old SVM solution or a pointer to
 *  NULL if there is no old solution.
 *
 *OUTPUT: sol_ptr will contain a pointer to an SVM_Solution struct
 * which can be used to resolve the SVM with more examples or
 * to classify examples.  For an overview of the struct, see the OUTPUT
 * comment to solve_svm_no_init_sol.
 *
 * Xy = *Xy_ptr will contain the examples that are NOT support vectors.  If
 * there were support vectors in *sol_ptr that are no longer support
 * vectors these will have been added to Xy.  Similarly, all examples
 * in Xy that became support vectors will have moved to the solution struct.
 * If all examples are support vectors Xy = NULL.
 *
 *WARNINGS:
 *1) sol_ptr is a DOUBLE POINTER because the svm solver returns
 *   a pointer.  even if you have no previous svm solution sol_ptr
 *   should not be NULL - *sol_ptr should be NULL.
 *2) Note that each row of Xy should be *premultiplied* by the
 *   class label which MUST be +/-1 (classic SVM problem).  This algorithm
 *   does not explicitly add a constant value to the decision (ie it solves
 *   for theta such that h(x) = sgn(theta*x)).  If you want a constant
 *   value, you need to add a column of all +/-1 to each example.
 *3) Xy does NOT have to be preprocessed (ie have the all-zero
 *   rows and columns removed).  This function will do that for
 *   you.  If you do the preprocess ahead of time, this function
 *   will just redo that work.
 *4) On return Xy = *Xy_ptr will contain only those examples (perhaps
 *   including old support vectors from *sol_ptr) that are NOT
 *   support vectors.  The solution struct will contain the support vectors.
 *   Note that Xy WILL CHANGE and MAY BE NULL.  Note that sol WILL CHANGE.
 *   Examples will migrate between Xy_ptr and sol_ptr... don't expect them
 *   to _not_ move.
 *
 ****************************************************************************/

void crm114__svm_solve(Matrix **Xy_ptr, SVM_Solution **sol_ptr) {
  SVM_Solution *sol;
  ExpandingArray *colMap = NULL;
  int i, n_old_examples, max_train;
  VectorIterator vit;
  Vector *theta, *row;
  Matrix *Xy;
  double weight;

  CRM114__MATR_DEBUG_MODE = CRM114__SVM_DEBUG_MODE;

  if (!sol_ptr) {
    if (CRM114__SVM_DEBUG_MODE) {
      fprintf(stderr,
	      "crm114__svm_solve: unitialized sol_ptr.  If you have no previous svm solution, make *sol_ptr = NULL\n");
    }
      return;
  }

  if (!Xy_ptr) {
    if (CRM114__SVM_DEBUG_MODE) {
      fprintf(stderr,
	      "crm114__svm_solve: unitialized Xy_ptr.  If you have no new examples, make *Xy_ptr = NULL\n");
    }
    return;
  }

  sol = *sol_ptr;
  Xy = *Xy_ptr;

  if (sol) {
    //for what we do with the old solution, see the comment to
    //svm_solve_init_sol
    if (CRM114__SVM_DEBUG_MODE >= SVM_SOLVER_DEBUG) {
      fprintf(stderr, "Incorporating old solution\n");
    }
    theta = sol->theta;
    //n_old_examples is the number of non-support-vector old examples
    n_old_examples = sol->num_examples - sol->SV->rows;
    //add the old support vectors into our current matrix
    crm114__matr_append_matr(&Xy, sol->SV);
    max_train = sol->max_train_val;
    if (n_old_examples < 0 || !(Xy) || !(Xy->data)) {
      if (CRM114__SVM_DEBUG_MODE) {
	fprintf(stderr, "crm114__svm_solve: something is weird with the initial solution.  Why don't you try again with no initial solution?\n");
      }
      crm114__svm_free_solution(sol);
      *sol_ptr = NULL;
      return;
    }
  } else {
    if (!Xy) {
      if (CRM114__SVM_DEBUG_MODE) {
	fprintf(stderr, "One of *Xy_ptr and *sol_ptr must be non-null!\n");
      }
      return;
    }
    //no initial solution
    theta = NULL;
    n_old_examples = 0;
    max_train = SVM_MAX_X_VAL;
  }

  //debugging info
  if (CRM114__SVM_DEBUG_MODE >= SVM_SOLVER_DEBUG) {
    fprintf(stderr, "Xy is %d by %u with %d non-zero elements\n",
	    Xy->rows, Xy->cols, Xy->nz);
  }
  //get rid of zero rows and columns of Xy
  colMap = svm_preprocess(Xy, theta);
  if (!(Xy->rows) && !(n_old_examples)) {
    if (CRM114__SVM_DEBUG_MODE) {
      fprintf(stderr, "SVM solve: nothing to learn on.\n");
    }
    if (*sol_ptr) {
      crm114__svm_free_solution(*sol_ptr);
    }
    *sol_ptr = NULL;
    return;
  }
  //this is 1/(n_old + n_new) - ie 1/(total # of examples we've seen)
  weight = 1.0/(Xy->rows + n_old_examples);
  if (theta) {
    if (CRM114__SVM_DEBUG_MODE >= SVM_SOLVER_DEBUG) {
      fprintf(stderr, "sol->num_examples = %d, n_old_examples = %d, Xy->rows = %d\n", sol->num_examples, n_old_examples, Xy->rows);
      fprintf(stderr, "multiplying theta by %f\n",
	      sol->num_examples/(n_old_examples + (double)Xy->rows));
    }
    //reweight theta to include the new examples in n
    crm114__vector_multiply(theta, sol->num_examples/(n_old_examples+(double)Xy->rows),
		    theta);
  }

  //more debugging information...
  if (CRM114__SVM_DEBUG_MODE >= SVM_SOLVER_DEBUG) {
    fprintf(stderr, "After preprocess Xy is %d by %u\n", Xy->rows, Xy->cols);
  }

  if (CRM114__SVM_DEBUG_MODE >= SVM_SOLVER_DEBUG_LOOP) {
    fprintf(stderr, "Xy = \n");
    crm114__matr_print(Xy);
  }

  //run the solver!
  sol = svm_solve_init_sol(Xy, theta, weight, max_train);

  if (*sol_ptr) {
    //we don't need the old solution any more
    crm114__svm_free_solution(*sol_ptr);
  }

  if (!sol) {
    //uh oh, the solver choked on something
    //probably the data was corrupted
    if (Xy) {
      crm114__matr_free(Xy);
    }
    if (colMap) {
      crm114__expanding_array_free(colMap);
    }
    *Xy_ptr = NULL;
    *sol_ptr = NULL;
    if (CRM114__SVM_DEBUG_MODE) {
      fprintf(stderr, "SVM Solver Error.\n");
    }
    return;
  }

  //sol->num_examples = Xy->rows.  so tell it that it also had n_old_examples
  //it didn't see but were used to generate the older solution
  sol->num_examples += n_old_examples;
  theta = sol->theta;

  //ok, yes, we do a lot of debugging
  if (CRM114__SVM_DEBUG_MODE >= SVM_SOLVER_DEBUG) {
    fprintf(stderr, "Number support vectors: %d\n", sol->SV->rows);
  }


  //undo the densification if we did it
  //note that sol->SV and Xy are STILL SPARSE
  //they just have the densified column numbers
  //so we need to change that
  if (colMap) {

    //make theta sparse with the correct column numbers
    crm114__vector_convert_nonsparse_to_sparray(sol->theta, colMap);

    //give sol->SV the correct column numbers
    if (sol->SV->rows) {
      crm114__matr_add_ncols(sol->SV,
		     crm114__expanding_array_get(sol->SV->cols-1, colMap).compact->i+1
		     - sol->SV->cols);
      for (i = 0; i < sol->SV->rows; i++) {
	row = matr_get_row(sol->SV, i);
	if (!row) {
	  continue;
	}
	vectorit_set_at_end(&vit, row);
	while (!vectorit_past_beg(vit, row)) {
	  crm114__vectorit_set_col(vit,
			   crm114__expanding_array_get(vectorit_curr_col(vit, row),
					       colMap).compact->i, row);
	  vectorit_prev(&vit, row);
	}
      }
    } else {
      if (CRM114__SVM_DEBUG_MODE) {
	fprintf(stderr, "crm114__svm_solve: No support vectors recorded.  Run again with SV_TOLERANCE set higher if they are necessary.\n");
      }
    }

    //give Xy the correct column numbers
    if (Xy && Xy->rows) {
      crm114__matr_add_ncols(Xy,
		     crm114__expanding_array_get(Xy->cols-1, colMap).compact->i+1 -
		     Xy->cols);
      for (i = 0; i < Xy->rows; i++) {
	row = matr_get_row(Xy, i);
	if (!row) {
	  continue;
	}
	vectorit_set_at_end(&vit, row);
	while (!vectorit_past_beg(vit, row)) {
	  crm114__vectorit_set_col(vit,
			   crm114__expanding_array_get(vectorit_curr_col(vit, row),
					       colMap).
			   compact->i,
			   row);
	  vectorit_prev(&vit, row);
	}
      }
    } else {
      crm114__matr_free(Xy);
      Xy = NULL;
    }

    crm114__expanding_array_free(colMap);
  }

  if (Xy && !Xy->rows) {
    crm114__matr_free(Xy);
    Xy = NULL;
  }

  *sol_ptr = sol;
  *Xy_ptr = Xy;
}

/***********************SVM_Solution Functions******************************/

/***************************************************************************
 *Classify an example.
 *
 *INPUT: ex: example to classify
 * sol: SVM solution struct
 *
 *OUTPUT: +1/-1 label of the example
 ***************************************************************************/

int crm114__svm_classify_example(Vector *ex, SVM_Solution *sol) {
  double d;

  if (!ex || !sol || !sol->theta) {
    if (CRM114__SVM_DEBUG_MODE) {
      fprintf(stderr, "crm114__svm_classify_example: null argument.\n");
    }
    return 0;
  }

  d = crm114__dot(ex, sol->theta);

  if (d < 0) {
    return -1;
  }

  return 1;
}

/*****************************************************************************
 *Write a solution struct to a file in binary format.
 *
 *INPUT: sol: Solution to write
 * filename: file to write to
 *
 *OUTPUT: the amount written in bytes
 ****************************************************************************/

size_t crm114__svm_write_solution(SVM_Solution *sol, char *filename) {
  FILE *fp = fopen(filename, "wb");
  size_t size;
  if (!fp) {
    if (CRM114__SVM_DEBUG_MODE) {
      fprintf(stderr, "crm114__svm_write_solution: bad filename %s\n", filename);
    }
   return 0;
  }
  size = crm114__svm_write_solution_fp(sol, fp);
  fclose(fp);
  return size;
}

/*****************************************************************************
 *Write a solution struct to a file in binary format.
 *
 *INPUT: sol: Solution to write
 * fp: file to write to
 *
 *OUTPUT: the amount written in bytes
 ****************************************************************************/

size_t crm114__svm_write_solution_fp(SVM_Solution *sol, FILE *fp) {
  //write theta
  size_t size;

  if (!sol || !fp) {
    if (CRM114__SVM_DEBUG_MODE) {
      fprintf(stderr, "svm_wrte_solution: bad file pointer.\n");
    }
    return 0;
  }

  size = crm114__vector_write_bin_fp(sol->theta, fp);

  //write support vectors
  size += crm114__matr_write_bin_fp(sol->SV, fp);
  size += sizeof(int)*fwrite(&(sol->num_examples), sizeof(int), 1, fp);
  size += sizeof(int)*fwrite(&(sol->max_train_val), sizeof(int), 1, fp);
  return size;
}


/*****************************************************************************
 *Read a solution struct from a file in binary format.
 *
 *INPUT: filename: file to read from
 *
 *OUTPUT: the solution struct stored in the file or NULL if it couldn't be
 * read
 *
 *WARNINGS:
 *1) This file expects a file formatted as crm114__svm_write_solution creates.  If
 *   file is not formatted that way, results may vary.  It should not seg
 *   fault, but that's about all I can promise.
 ****************************************************************************/

SVM_Solution *crm114__svm_read_solution(char *filename) {
  SVM_Solution *sol;
  FILE *fp = fopen(filename, "rb");

  if (!fp) {
    if (CRM114__SVM_DEBUG_MODE) {
      fprintf(stderr, "crm114__svm_read_solution: bad filename %s\n", filename);
    }
    return NULL;
  }
  sol = crm114__svm_read_solution_fp(fp);
  fclose(fp);
  return sol;
}


/*****************************************************************************
 *Read a solution struct from a file in binary format.
 *
 *INPUT: fp: file to read from
 *
 *OUTPUT: the solution struct stored in the file or NULL if it couldn't be
 * read
 *
 *WARNINGS:
 *1) This file expects a file formatted as crm114__svm_write_solution creates.  If
 *   file is not formatted that way, results may vary.  It should not seg
 *   fault, but that's about all I can promise.
 ****************************************************************************/

SVM_Solution *crm114__svm_read_solution_fp(FILE *fp) {
  SVM_Solution *sol = (SVM_Solution *)malloc(sizeof(SVM_Solution));
  size_t unused;

  if (!fp) {
    if (CRM114__SVM_DEBUG_MODE) {
      fprintf(stderr, "crm114__svm_read_solution: bad file pointer.\n");
    }
    free(sol);
    return NULL;
  }

  sol->theta = crm114__vector_read_bin_fp(fp);
  if (!sol->theta) {
    if (CRM114__SVM_DEBUG_MODE) {
      fprintf(stderr, "read_solution: Bad file.\n");
    }
    free(sol);
    return NULL;
  }
  sol->SV = crm114__matr_read_bin_fp(fp);
  unused = fread(&(sol->num_examples), sizeof(int), 1, fp);
  unused = fread(&(sol->max_train_val), sizeof(int), 1, fp);
  return sol;
}

/***************************************************************************
 *Maps a solution from a block of memory in binary format (the same
 *format as would be written to a file using write.
 *
 *INPUT: addr: pointer to the address where the solution begins
 * last_addr: the last possible address that is valid.  NOT necessarily where
 *  the solution ends - just the last address that has been allocated in the
 *  chunk pointed to by *addr (ie, if *addr was taken from an mmap'd file
 *  last_addr would be *addr + the file size).
 *
 *OUTPUT: A solution STILL referencing the chunk of memory at *addr,
 *  but formated as an SVM_Solution or NULL if a properly formatted
 *  solution didn't start at *addr.
 * *addr: (pass-by-reference) points to the first memory location AFTER the
 *  full solution
 * *n_elts_ptr: (pass-by-reference) the number of elements actually read
 *
 *WARNINGS:
 * 1) *addr needs to be writable.  This will CHANGE VALUES stored at *addr and
 *    will seg fault if addr is not writable.
 * 2) last_addr does not need to be the last address of the solution
 *    but if it is before that, either NULL will be returned or an
 *    matrix with a NULL data value will be returned.
 * 3) if *addr does not contain a properly formatted solution, this function
 *    will not seg fault, but that is the only guarantee.
 * 4) you MUST call solution_free!
 * 5) *addr and CHANGES!
 * 6) the address returned by this IS NOT EQUAL to *addr as passed in.
 ***************************************************************************/

SVM_Solution *crm114__svm_map_solution(void **addr, void *last_addr) {
  SVM_Solution *sol = (SVM_Solution *)malloc(sizeof(SVM_Solution));

  sol->theta = crm114__vector_map(addr, last_addr);
  if (!sol->theta) {
    if (CRM114__SVM_DEBUG_MODE) {
      fprintf(stderr, "map_solution: Bad file.\n");
    }
    free(sol);
    return NULL;
  }

  sol->SV = crm114__matr_map(addr, last_addr);
  if (*addr > last_addr || (void *)((int *)*addr + 2) > last_addr) {
    if (CRM114__SVM_DEBUG_MODE) {
      fprintf(stderr, "map_solution: Bad file.\n");
    }
    crm114__svm_free_solution(sol);
    return NULL;
  }

  sol->num_examples = *((int *)(*addr));
  *addr = (int *)*addr + 1;
  sol->max_train_val = *((int *)(*addr));
  *addr = (int *)*addr + 1;
  return sol;
}

/*****************************************************************************
 *Free a solution struct.
 *
 *INPUT: sol: struct to free
 ****************************************************************************/

void crm114__svm_free_solution(SVM_Solution *sol) {
  if (sol) {
    if (sol->theta) {
      crm114__vector_free(sol->theta);
    }
    if (sol->SV) {
      crm114__matr_free(sol->SV);
    }
    free(sol);
  }
}
