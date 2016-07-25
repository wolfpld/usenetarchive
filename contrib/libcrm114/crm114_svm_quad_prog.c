//	crm114_svm_quad_prog.c - Support Vector Machine

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


#include "crm114_svm_quad_prog.h"

#define DEFINE_UNUSED_STATIC_FUNCTIONS 0

static void add_constraint(int toadd, Matrix *A, Matrix *Q, Matrix *R,
		    Matrix **Z_ptr);
static void delete_constraint(int toad, Matrix *A, Matrix *Q, Matrix *R,
		       Matrix **Z_ptr);
static int compute_lambda(Matrix *R, Matrix *Q, Vector *g);
static void back_sub(Matrix *U, Vector *b, Vector *ret);
static double find_direction(Matrix *Z, Matrix *G, Vector *g, Vector *p);
static void conjugate_gradient(Matrix **A, int nmatrices, int maxrows,
			Vector *b, Vector *x);
#if DEFINE_UNUSED_STATIC_FUNCTIONS
static void run_linear(Matrix *A, Vector *b, Vector *x);
static void gradient_descent(Matrix **A, int nmatrices, int maxrows, Vector *v,
		      Vector *x);
#endif

static int QP_DEBUG_MODE;


/******************************************************************
 *We use the active set method outlined in Gill and Murray 1977
 *"Numerically Stable Methods for Quadratic Programming"
 *Philip E. Gill and Walter Murray
 *Mathematical Programming 14 (1978) 349-372
 *North-Holland Publishing Company

 *We want to solve a constrained quadratic problem of the form
 *min_x f(x) = 0.5xGx + c\dot x
 *subject to Ax >= b
 *for POSITIVE DEFINITE G.

 *First consider the problem without the constraints.  We could just
 *solve for f'(x) = Gx + c = 0, which is a linear problem.
 *
 *Now add in the constraints.  We would like to get near f'(x) = 0
 *while still fulfilling all contraints on x. Therefore we use
 *an iterative method where we walk down the gradient, adding constraints
 *as they become active (ie constraint i is active if (Ax)_i = b_i) and
 *removing them as they become inactive ((Ax)_i > b_i).
 *
 *When a constraint is active, we only allow descent along directions
 *perpendicular to the constraint so that we will never violate it.
 *Specifically, at each iteration we solve for the direction p in
 * G'(x+p) + c' = 0
 *where G' and c' are projections of G and c along directions orthogonal
 *to the current active constraints.  This requires us to keep a projection

 *matrix Z such that G' = Z^T*G*Z and c' = Z*c.  In order to calculate this
 *projection matrix quickly, we also keep the QR factorization of the current
 *ACTIVE constraint matrix A'^T (where A' contains only the rows of A that are
 *that are currently active.  If we have k constraints then A' is k x n and
 *A'^T is n x k).  The QR factorization of A'^T is given by two matrices Q
 *and R such that
 *QA'^T = |R|
 *        |0_{n-k x k}|
 *where Q is nxn and the rows of Q are orthonormal (so Q*Q^T = I_nxn) and R
 *is kxk upper triangular.
 *Note that the product of the last n-k rows of Q with any row of A' is zero.
 *Therefore, the last n-k rows of Q are a projection matrix onto a space
 *orthogonal to any active constraints and the columns of Z are the last n-k
 *rows of Q.  It turns out we can update Q and R quickly when we add or remove
 *a row from A' (corresponding to a constraint becoming active or inactive).
 *For exacly how these updates work, see the add_constraint and
 *delete_constraint functions.
 *
 *The steps of the algorithm are then
 * 1) Find an initial starting point that fulfills the constraints.
 *    This is actually a linear minimization problem and one we do
 *    not actually solve.  If a starting point is passed in by the user
 *    (indicated by x != 0), we use that.  Note we do not check to make
 *    sure it fulfills all constraints.  If no starting point is passed in
 *    (x = 0), we assume that Ax >= b is always
 *    of the form passed in by the linear solver specifying that x >= 0
 *    and sum_x >= C and set x = C/(n+1) where G is nxn.  This is NOT
 *    a general method for finding a starting point and would need to
 *    be improved upon to make this a general solver.
 * 2) Solve for the direction p such that G'(x+p) + c' = 0.
 *    See find_direction for an overview of how we solve for p.
 * 3) If ||p|| ~= 0, solve for the Lagrange mulipliers on the active
 *    constraints. If these are all positive (ie, all of the active
 *    constraints are still active), return x.  Otherwise, pick an active
 *    constraint with a negative mulitplier and remove it and return to step 2.
 *    For an overview of how to find the Lagrange multipliers see the
 *    compute_lambda function.
 *    For an overview of how to remove a constraint see the delete_constraint
 *    function.
 * 3) Find how far we can go in direction p without violating a constraint.
 * 4) If we can move ||p|| in direction p without violating a constraint,
 *    update x -> x + p and return to step 2.
 * 5) Otherwise, move as far as possible in direction p and add the first
 *    constraint we hit to the set of active constraints.  Repeat from step 2.
 *    For an overview of how to add a constraint see the add_constraint
 *    function.
 *
 *INPUT: Matrices G, A, vectors c, b where the solution will be
 * min_x 0.5*x*G*x + c*b
 * s.t. Ax >= b
 * if x is non-zero it is assumed to be a starting point such that
 * Ax >= b
 *
 *OUTPUT: x fulfilling the equation above as pass by reference.
 *
 *TYPES: Really depends on how sparse G and A are.  Nothing in this function
 * takes its type from the arguments passed in so pick what you like.
 * However, the sparse arrays in this function will initialize with
 * the size passed in with A so even if A is not a sparse array, you need
 * to set its initial size.
 *
 *WARNING: I think I (finally) have all of the bugs out of this algorithm
 * but it does tend to be picky (especially if QP_LINEAR_ACCURACY is set to a
 * high number so the conjugate gradient solver doesn't give a great answer)
 * so I've put a maxmimum number of loops on it.
 ***********************************************************************/

void crm114__run_qp(Matrix *G, Matrix *A, Vector *c, Vector *b, Vector *x) {

  int tk = 0, n = A->cols, m = A->rows, oldtk = 0, neg_lambda, toadd, toremove,
    i, loop_it = 0, def_val, agz = 0;
  int *active, *constr;
  //gradient g = G*x + c
  Vector *g = crm114__vector_make_size(n, SPARSE_LIST, MATR_PRECISE, A->size),
    //direction G*(x+p) + c = 0
    *p = crm114__vector_make_size(n, SPARSE_LIST, MATR_PRECISE, A->size),
    *row;
  double alpha, r, dotp, length, bval;
  //R and Q are QR factorization of the current constraint matrix
  Matrix *R = crm114__matr_make_size(0,0, SPARSE_ARRAY, MATR_PRECISE, A->size),
    *Q = crm114__matr_make_size(n, n, NON_SPARSE, MATR_PRECISE, A->size),
    //projection matrix
    *Z = crm114__matr_make_size(n, n, SPARSE_ARRAY, MATR_PRECISE, A->size);
  //it is not actually necessary to keep the active constraint matrix
  //seperately from A.  However, I have left it commented out to make
  //it clear which constraints are currently active.
  //*currA = crm114__matr_make(0,0);
  VectorIterator vit;

  CRM114__MATR_DEBUG_MODE = QP_DEBUG_MODE;  //debug mode

  if (QP_DEBUG_MODE >= QP_DEBUG) {
    //print out the arguments
    fprintf(stderr, "Arguments are\nn = %d, m = %d\nG = \n", n, m);
    crm114__matr_print(G);
    fprintf(stderr, "A = \n");
    crm114__matr_print(A);
    fprintf(stderr, "f = ");
    crm114__vector_print(c);
    fprintf(stderr, "b = ");
    crm114__vector_print(b);
    fprintf(stderr, "x0 = ");
    crm114__vector_print(x);
  }

  // active and constr were C99 variable-length arrays, but Microsoft C is C89
  active = malloc(m * sizeof(int));
  constr = malloc(n * sizeof(int));

  for (i = 0; i < m; i++) {
    //active[i] = 1 if constraint i is active
    active[i] = 0;
  }
  for (i = 0; i < n; i++) {
    //constr[i] = the column of A'^T corresponding to constraint i if
    //i is active.  -1 if i is inactive.
    constr[i] = -1;
  }

  //initialize Q and Z to be the identity matrix
  //this is fast if Q and Z are sparse
  //because they were zero matrices before
  for (i = 0; i < n; i++) {
    matr_set(Q, i, i, 1);
    matr_set(Z, i, i, 1);
  }


  //finding an initial feasible point is a chore in and of itself
  //for what i'm doing, because i know the constraints, i know that
  //this vector works, but it may NOT in the general case
  //if you need a general solver, this step needs to be a linear
  //optimization problem

  if (crm114__vector_iszero(x)) {
    if (QP_DEBUG_MODE >= QP_DEBUG) {
      fprintf(stderr, "No inital guess.  Using default guess.\n");
    }
    //otherwise x is an initial starting point
    vectorit_set_at_beg(&vit, b);
    def_val = vectorit_curr_val(vit, b);
    for (i = 0; i < n; i++) {
      crm114__vector_set(x, i, def_val/(double)(n+1));
    }
  }

  while (loop_it < QP_MAX_ITERATIONS) {

    toadd = -1;
    toremove = -1;

    //calculate the gradient
    crm114__matr_vector(G, x, g); //sparse times non-sparse = fast
    crm114__vector_add(g, c, g); //sparse + non-sparse

    //find the direction
    //we want g = Gx + c = 0 (gradient)
    //so direction we need to go is p
    //where G(x+p) + c = 0
    //so Gp = -g
    //we will project only onto the directions
    //of inactive constraints (since we can't
    //move a constrained direction)
    //Z is the matrix we use to project onto these directions

    if (tk < n) {
      length = find_direction(Z, G, g, p);
    } else {
      length = 0;
    }

    if (QP_DEBUG_MODE >= QP_DEBUG_LOOP) {
      fprintf(stderr, "x = ");
      crm114__vector_print(x);
      if (tk < n) {
	fprintf(stderr, "p = ");
	crm114__vector_print(p);
      }
      fprintf(stderr, "length = %.10lf, tk = %d\n", length, tk);
    }

    if (length <= QP_LINEAR_ACCURACY) {
      if (tk > 0) {
	neg_lambda = compute_lambda(R, Q, g);
      } else {
	//no constraints are active
	//we reached the actual minimum - yay!
	neg_lambda = -1;
      }
      if (neg_lambda < 0) {
	//all currently "active" constraints actually are active
	//this is the solution
	break;
      } else {
	//an active constraint became inactive somewhere along
	//the way
	//remove it and see if we can move in that direction
	toremove = neg_lambda;
      }
    } else {
      alpha = MATRIX_INF; //alpha is the step size
      //find the step size
      vectorit_set_at_beg(&vit, b);
      for (i = 0; i < m; i++) {
	if (!active[i]) {
	  row = matr_get_row(A, i);
	  //part of p in direction of constraint
	  dotp = crm114__dot(row, p); //sparse times sparse
	  if (QP_DEBUG_MODE >= QP_LINEAR_SOLVER) {
	    fprintf(stderr, "dotp = %.10f\n", dotp);
	  }
	  if (dotp < -QP_LINEAR_ACCURACY) {
	    if (vectorit_curr_col(vit, b) != i) {
	      bval = 0;
	    } else {
	      bval = vectorit_curr_val(vit, b);
	    }
	    //step size for this constraint
	    r = (bval - crm114__dot(row, x))/dotp; //sparse times sparse
	    if (QP_DEBUG_MODE >= QP_LINEAR_SOLVER) {
	      fprintf(stderr, "bval = %f r = %f crm114__dot(row, x) = %.10f\n",
		      bval, r, crm114__dot(row, x));
	    }
	    if (r < alpha) {
	      //this is the most constrained constraint so far
	      alpha = r;
	      toadd = i;
	    }
	  }
	}
	if (vectorit_curr_col(vit, b) == i) {
	  vectorit_next(&vit, b);
	}
      }
      if (agz && alpha < MATRIX_EPSILON) {
	//we actually can't move along this constraint
	//this means we must have added the constraint at some
	//point in the past, then removed it, then tried to
	//add it again WITHOUT actually being able to move in this
	//direction.  This happens when the linear solver is not quite
	//accurate enough (through the fact that we consider MATRIX_EPSILON
	//to be zero I believe) and something that should be just above
	//zero is instead just below.  By the time we're looking at removing
	//constraints with such small leeway though, we've got a good
	//enough answer.  So give up.
	//
	//The agz term is in case our original x0 starting point is already
	//up against some constrained directions.  The first thing we do, then
	//is add those constraints.  In this case, we want to keep going even
	//though we are adding constraints along which we can't move at all -
	//after we have added the first alpha non-zero constraint, we need to
	//start checking for alpha = 0.
	break;
      }
      if (alpha > MATRIX_EPSILON) {
	agz = 1;
      }
      if (alpha > 1) {
	//we can move ||p|| in direction p
	//there is no new active constraint
	alpha = 1;
	toadd = -1;
      } else {
	//we must move less than ||p|| because we hit a constraint
	//move as far as we can and add the constraint
	crm114__vector_multiply(p, alpha, p); //sparse
	if (QP_DEBUG_MODE >= QP_LINEAR_SOLVER) {
	  fprintf(stderr, "alpha = %f\n", alpha);
	}
      }
      crm114__vector_add(x, p, x); //sparse + sparse
    }
    oldtk = tk;
    if (toadd >= 0)
      {
	if (QP_DEBUG_MODE >= QP_DEBUG_LOOP) {
	  fprintf(stderr, "Adding constraint %d\n", toadd);
	}
	//add constraint
	active[toadd] = 1;
	constr[tk] = toadd; //column tk contains toadd
	add_constraint(toadd, A, Q, R, &Z);
	tk++;
      }
    if (toremove >= 0)
      {
	if (QP_DEBUG_MODE >= QP_DEBUG_LOOP) {
	  fprintf(stderr, "Removing constraint %d to remove = %d\n",
		  constr[toremove], toremove);
	}
	//remove constraint
	active[constr[toremove]] = 0;
	delete_constraint(toremove, A, Q, R, &Z);
	//we need to keep track of which constraints correspond to which rows
	for (i = toremove; i < n-1; i++) {
	  constr[i] = constr[i+1];
	}
	tk--;
      }
    loop_it++;
  }

  if (QP_DEBUG_MODE && loop_it >= QP_MAX_ITERATIONS) {
    fprintf(stderr, "QP didn't converge. Returning.\n");
  }

  if (QP_DEBUG_MODE >= QP_DEBUG) {
    fprintf(stderr, "Max iterations was %d\n", loop_it);
  }
  crm114__matr_free(Q);
  crm114__matr_free(R);
  //crm114__matr_free(currA);
  crm114__matr_free(Z);
  crm114__vector_free(g);
  crm114__vector_free(p);
  free(active);
  free(constr);
}

/**************************************************************************
 *We need to add row toadd of A to the current set of active constraints
 *and update Q, R, and Z accordingly.
 *Specifically we are adding a column to A'^T making it n x k+1.  To make
 *this clear let A^k = A'^T before we add a column and A^{k+1} = A'^T after
 *we add a column.  We will use the same convention for Q, R, and Z.  Then
 *Q^k*A^{k+1} = |R^k Q*a_{1:k}  |
 *              | 0  Q*a_{k+1:n}|
 *where a is the row of A we are adding.
 *So R is almost upper triangular still except for its last column.  To
 *make it completely upper triangular, we use Givens rotations.  These are
 *used in the Gill and Murray paper, but I believe the original reference
 *is
 *Givens, Wallace.
 *"Computation of plane unitary rotations transforming a general matrix
 *to triangular form".
 *J. SIAM 6(1) (1958), pp. 26–50.
 *If you are just trying to understand them, though, I would recommend the
 *Wikipedia article.
 *
 *To make R upper triangular will require n-(k+1) rotations of the form
 *   | I_{i-1, i-1}  0    0       0_{n-i-1}   |
 *   |       0      c_i  s_i      0_{n-i-1}   |
 *   |       0      -s_i c_i      0_{n-i-1}   |
 *   |       0        0   0  I_{n-i-1, n-i-1} |
 *where
 * c_i = (Q*a)_(i-k-1)/r_i
 * s_i = r_{i-1}/r_i
 * r_i = sqrt(sum_{j = i:n-k+1} Q*a_{j-k-1}^2)
 *We multiply Q on the left by each of these matrices
 *each multiplication affects only 2 rows of Q, giving a total of n(n-tk)
 *time.
 *Note that Q remains orthonormal since these are rotation matrices.
 *
 *INPUT: toadd: the number of the row of A corresponding to the constriant
 *  we are adding
 * A: the full constraint matrix in A*x >= b
 *
 *OUTPUT: Updated QR factorization of A'^T (pass by reference)
 * Rotation matrix Z.  We actually delete and reallocate Z so we need a
 *  double pointer.
 *
 *TYPES: It should be easy to add a row and column to R and therefore
 * R should be some sort of sparse matrix.  It is upper triangular so it
 *  is already guaranteed to be ~50% zeros.
 * Q never changes size so it can be sparse or non-sparse.
 * Z needs to be freed and reallocated - this will be fastest with a
 *  SPARSE_ARRAY.
 **************************************************************************/

static void add_constraint(int toadd, Matrix *A, Matrix *Q, Matrix *R,
		    Matrix **Z_ptr) {
  Matrix Q2, *Z = *Z_ptr;
  unsigned int tk = R->rows, n = Q->rows, i, j, col;
  int rotate = 1, zsize = Z->size;

  Vector *Q2a = crm114__vector_make_size(n-tk, R->type, MATR_PRECISE, R->size),
    *trow = crm114__vector_make_size(n, Q->type, MATR_PRECISE, Q->size),
    *brow = crm114__vector_make_size(n, Q->type, MATR_PRECISE, Q->size), *arow;
  VectorType ztype = Z->type;
  double r2, c_i, s_i, r, uval, gamma;
  VectorIterator vit;

  if (QP_DEBUG_MODE >= QP_CONSTRAINTS) {
    fprintf(stderr, "Before adding constraint %d.\n", toadd);
    fprintf(stderr, "Q = \n");
    crm114__matr_print(Q);
    fprintf(stderr, "R = \n");
    crm114__matr_print(R);
    fprintf(stderr, "Z = \n");
    crm114__matr_print(*(Z_ptr));
  }

  //currA gets another column
  //crm114__matr_add_col(currA_ptr);
  //currA = *currA_ptr;
  //for (i = 0; i < currA->rows; i++) {
  //note that currA transposes A
  //currA->data[i][currA->cols-1] = A->data[toadd][i];
  //}

  //R^(k+1) is
  //|R^k     Q^k*A(toadd)_{1:k}      |
  //| 0  ||Q^k_2A(toadd)_{k+1:n-k}|| |
  crm114__matr_add_col(R); //sparse = fast
  crm114__matr_add_row(R);

  //set up Q2 (really just the bottom n-tk rows of Q)
  Q2.rows = n-tk;
  Q2.cols = n;
  Q2.compact = Q->compact;
  Q2.type = Q->type;
  Q2.size = Q->size;
  Q2.data = &(Q->data[tk]);
  arow = matr_get_row(A, toadd);
  crm114__matr_vector(&Q2, arow, Q2a);
  gamma = norm(Q2a);
  vectorit_set_at_end(&vit, matr_get_row(R, tk));
  if (fabs(gamma + crm114__vector_get(Q2a, 0)) < MATRIX_EPSILON) {
    //R is already upper triangular
    //and the first entry of Q2a is actually -Q2a
    //we won't rotate R at all, so this entry needs to have the
    //correct sign
    crm114__vectorit_insert(&vit, tk, crm114__vector_get(Q2a, 0), matr_get_row(R, tk));
    rotate = 0;
  } else {
    crm114__vectorit_insert(&vit, tk, gamma, matr_get_row(R, tk));
  }
  for (i = 0; i < tk; i++) {
    //this sets the last element of every row
    //so it's fast
    vectorit_set_at_end(&vit, matr_get_row(R, i));
    crm114__vectorit_insert(&vit, tk, crm114__dot(matr_get_row(Q, i), arow),
		    matr_get_row(R, i));
  }


  if (rotate) {
    //now we have to update Q
    vectorit_set_at_end(&vit, Q2a);
    //although we never need it, keep Q2a updated or things will get confusing!
    //note that Q2a above it's bottom non-zero element is certainly non-sparse
    //so we can work with that
    col = vectorit_curr_col(vit, Q2a);
    r = vectorit_curr_val(vit, Q2a);
    r2 = vectorit_curr_val(vit, Q2a)*vectorit_curr_val(vit, Q2a);
    while (col > 0 && col < Q2a->dim) {
      //when we hit zero we're done
      i = col + tk;
      vectorit_prev(&vit, Q2a);
      if (vectorit_curr_col(vit, Q2a) == col - 1) {
	uval = vectorit_curr_val(vit, Q2a);
      } else {
	uval = 0;
      }
      vectorit_next(&vit, Q2a);
      c_i = uval;
      s_i = r;
      r2 += uval*uval;
      crm114__vectorit_insert(&vit, col-1, r2, Q2a); //r2 is nonzero if we care
      vectorit_next(&vit, Q2a);
      crm114__vectorit_zero_elt(&vit, Q2a);
      vectorit_prev(&vit, Q2a);
      if (vectorit_curr_col(vit, Q2a) == col) {
	//non-sparse representation
	vectorit_prev(&vit, Q2a);
      }
      col = vectorit_curr_col(vit, Q2a); //should also be col--
      r = sqrt(r2);
      if (r > MATRIX_EPSILON) { //we still need this in case of full matrices
	c_i /= r;
	s_i /= r;

	//this affects two rows of Q
	//the i^th row and the one above it
	//this is done this way so it works for both
	//sparse and full matrices
	crm114__vector_multiply(matr_get_row(Q, i-1), c_i, trow);
	crm114__vector_add_multiple(trow, matr_get_row(Q, i), s_i, trow);
	crm114__vector_multiply(matr_get_row(Q, i), c_i, matr_get_row(Q, i));
	crm114__vector_add_multiple(matr_get_row(Q, i), matr_get_row(Q, i-1),
			    -1.0*s_i, matr_get_row(Q, i));
	crm114__matr_set_row(Q, i-1, trow);
      } else {
	r2 = 0; //avoid floating pt errors
      }
    }
  }
  //recalculate Z
  //now has one less row
  //easier to just free
  //and reallocate
  //same amount of time
  crm114__matr_free(*Z_ptr);
  *Z_ptr = crm114__matr_make_size(n, n - tk - 1, ztype, MATR_PRECISE, zsize);
  Z = *Z_ptr;

  //columns of Z are the last n-tk-1 (since tk is still the old number of
  //constraints) rows of the new Q in reverse order
  //this is fast for sparse matrices because we fill columns of j
  //in ascending order
  for (j = 0; j < n-tk-1; j++) {
    crm114__matr_set_col(Z, j, matr_get_row(Q, n - j - 1));
  }

  if (QP_DEBUG_MODE >= QP_CONSTRAINTS) {
    fprintf(stderr, "After adding constraint %d\n", toadd);
    fprintf(stderr, "Q = \n");
    crm114__matr_print(Q);
    fprintf(stderr, "R = \n");
    crm114__matr_print(R);
    fprintf(stderr, "Z = \n");
    crm114__matr_print(Z);
  }

  crm114__vector_free(Q2a);
  crm114__vector_free(trow);
  crm114__vector_free(brow);
}

/**************************************************************************
 *We need to remove row todel of A' (note that todel is the number of the row
 *in A' NOT A!!) and update Q, R, and Z accordingly.
 *Specifically we are removing a column from A'^T making it n x k-1.  To make
 *this clear let A^k = A'^T before we remove a column and A^{k-1} = A'^T after
 *we remove a column.  We will use the same convention for Q, R, and Z.  Then
 *Q^k*A^{k-1} = |R^k_0 R^k_1 ... R^k_{todel-1} R^k_{todel+1} ... R^k_k|
 *              | 0     0    .... 0                0         ...  0   |
 *Therefore R is almost still upper triangular (in fact if todel = k it will
 *remain upper triangular) but all columns with numbers greater than todel
 *have an extra element below the diagonal.  Therefore, we need to rotate
 *R to remove this element.  We use Givens rotations.  These are
 *used in the Gill and Murray paper, but I believe the original reference
 *is
 *Givens, Wallace.
 *"Computation of plane unitary rotations transforming a general matrix
 *to triangular form".
 *J. SIAM 6(1) (1958), pp. 26–50.
 *If you are just trying to understand them, though, I would recommend the
 *Wikipedia article.
 *
 *When we remove a column c from A', the matrix K = Q^kA^{k-1}
 *has n-c subdiagonal non-zero elements at K_{j+1, j} for j >= c
 *We need to rotate K to get rid of these elements
 *The rotated K is R and we multiply the rotation matrices into Q on the left.
 *The givens rotation for the j+1, j rotation is
 * | I_{j, j}  0    0      0_{n-j-2}    |
 * |     0    c_j  s_j     0_{n-j-2}    |
 * |     0    -s_j c_j     0_{n-j-2}    |
 * |     0      0   0  I_{n-j-2, n-j-2} |
 *where
 *c_j = R_{j-1, j}/r_j
 *s_j = R_{j,j}/r_j
 *r_j = sqrt(R_jj^2 + R_{j+1,j}^2)
 *Note that it is important to start with the c+1st rotation on the
 *right since that rotation changes the elements of R in the columns to the
 *right of c+1.
 *Each multiplication affects only 2 rows of Q so they take (n-c)(n-k) time.
 *
 *INPUT: todel: the number of the row of A' (!!!) corresponding to the
 * constriant we are deleting.  NOTE THAT todel IS NOT NECESSARILY THE ROW
 * NUMBER OF THE CONSTRAINT IN A.  Instead, it corresponds to the column
 * we need to remove from A and R.
 * A: the full constraint matrix in A*x >= b
 *
 *OUTPUT: Updated QR factorization of A'^T (pass by reference) and
 * the projection matrix Z.  We do not delete and reallocate Z here
 * so passing just a single pointer would have been fine, but it is done
 * this way for symmetry with add_constraint.
 *
 *TYPES: It should be easy to remove a row and column from R and therefore
 * R should be some sort of sparse matrix.  It is upper triangular so it
 *  is already guaranteed to be ~50% zeros.
 * Q never changes size so it can be sparse or non-sparse.
 * Z will have a column added to it so it is better if it is sparse.
 **************************************************************************/

static void delete_constraint(int todel, Matrix *A, Matrix *Q, Matrix *R,
		       Matrix **Z_ptr) {
  int j;
  Matrix *Z = *Z_ptr;
  Vector *trow = crm114__vector_make_size(Q->cols, Q->type, MATR_PRECISE, Q->size),
    *brow = crm114__vector_make_size(Q->cols, Q->type, MATR_PRECISE, Q->size),
    *trrow = crm114__vector_make_size(R->cols+1, R->type, MATR_PRECISE, R->size),
    *brrow = crm114__vector_make_size(R->cols+1, R->type, MATR_PRECISE, R->size);
  double r, c_j, s_j, tmp1, tmp2;

  if (QP_DEBUG_MODE >= QP_CONSTRAINTS) {
    fprintf(stderr, "Before deleting row %d\n", todel);
    fprintf(stderr, "Q = \n");
    crm114__matr_print(Q);
    fprintf(stderr, "R = \n");
    crm114__matr_print(R);
    fprintf(stderr, "Z = \n");
    crm114__matr_print(*(Z_ptr));
  }

  //update currA by removing the constraint
  //crm114__matr_remove_col(currA_ptr, todel);

  //Since we have to search each row for this column, this operation
  //is slow no matter what R is.  The removal is faster for a SPARSE_LIST
  //but the search is slower and vice versa for a SPARSE ARRAY and
  //a NON_SPARSE structure.
  crm114__matr_remove_col(R, todel);

  for (j = todel; j < R->cols; j++) {
    //both of these values can be picked from a sparse R quickly
    tmp1 = matr_get(R, j+1, j); //this is the first nonzero elt of row j+1
    tmp2 = matr_get(R, j, j); //this is the first nonzero elt of row j
    r = sqrt(tmp1*tmp1 + tmp2*tmp2);
    if (r <  MATRIX_EPSILON) {
      continue;
    }
    c_j = tmp2/r;
    s_j = tmp1/r;
    //this affects the j and j+1 rows of Q
    crm114__vector_multiply(matr_get_row(Q, j), c_j, trow);
    crm114__vector_add_multiple(trow, matr_get_row(Q, j+1), s_j, trow);
    crm114__vector_multiply(matr_get_row(Q, j+1), c_j, matr_get_row(Q, j+1));
    crm114__vector_add_multiple(matr_get_row(Q,j+1), matr_get_row(Q, j),
			-1.0*s_j, matr_get_row(Q, j+1));
    crm114__matr_set_row(Q, j, trow);

    //and the j and j+1 rows of R
    crm114__vector_multiply(matr_get_row(R, j), c_j, trrow);
    crm114__vector_add_multiple(trrow, matr_get_row(R, j+1), s_j, trrow);
    crm114__vector_multiply(matr_get_row(R, j+1), c_j, matr_get_row(R, j+1));
    crm114__vector_add_multiple(matr_get_row(R, j+1), matr_get_row(R, j),
			-1.0*s_j, matr_get_row(R, j+1));
    crm114__matr_set_row(R, j, trrow);
  }

  //remove the bottom row of all zeros
  //this will be very fast
  crm114__matr_remove_row(R, R->rows-1);

  //Z remains almost unchanged
  //note that in changing Q, we left the bottom n-k rows of Q alone.
  //however, we need to add in another column to Z since we've
  //lost a constraint
  //this column is the R->cols (k+1) row of Q
  crm114__matr_add_col(Z);
  crm114__matr_set_col(Z, Z->cols-1, matr_get_row(Q, R->cols)); //setting the last col
                                                        //is fast

  if (QP_DEBUG_MODE >= QP_CONSTRAINTS) {
    fprintf(stderr, "Q = \n");
    crm114__matr_print(Q);
    fprintf(stderr, "After R = \n");
    crm114__matr_print(R);
    fprintf(stderr, "Z = \n");
    crm114__matr_print(Z);
  }

  crm114__vector_free(trow);
  crm114__vector_free(brow);
  crm114__vector_free(trrow);
  crm114__vector_free(brrow);

}

/***********************************************************************
 *Compute the Langrange multipliers for the active constraints.
 *These are Lagrange multipliers so we must have that
 *f'(x) - A'^T*lambda = 0
 *Giving us that
 *A'^T*lambda = Gx + c = g
 *Since A'^T = Q^T*|R|
 *                 |0|
 *we have that R*lambda = Q_{1:k}*g
 *where Q_{1:k} are the first k rows (ie R->rows) of Q.
 *Since R is upper triangular we can solve this problem quickly using
 *back substition (see the function back_sub).
 *
 *INPUT: Q,R: QR factorization of A'^T
 *       g: Gradient G*x + c
 *
 *OUTPUT: The index of the first negative Lagrange multiplier or -1
 * if all Lagrange mulipliers are positive.
 *
 *TYPES: Anything should work.
 ***********************************************************************/
static int compute_lambda(Matrix *R, Matrix *Q, Vector *g) {
  int col = -1;
  Vector *c = crm114__vector_make_size(R->cols, Q->type, MATR_PRECISE, R->size),
    *lambda = crm114__vector_make_size(R->cols, R->type, MATR_PRECISE, R->size);
  VectorIterator vit;
  double minval = -1.0*QP_LINEAR_ACCURACY;

  crm114__matr_vector(Q, g, c); //sparse dot non-sparse = fast
  back_sub(R, c, lambda); //R is upper triangular so this is very fast
  crm114__vector_free(c);

  if (QP_DEBUG_MODE >= QP_DEBUG_LOOP) {
      fprintf(stderr, "lambda = ");
      crm114__vector_print(lambda);
  }

  //find any negative Lagrange multipliers
  vectorit_set_at_beg(&vit, lambda);
  while (!vectorit_past_end(vit, lambda)) {
    if (vectorit_curr_val(vit, lambda) < minval) {
      col = vectorit_curr_col(vit, lambda);
      minval = vectorit_curr_val(vit, lambda);
    }
    vectorit_next(&vit, lambda);
  }

  crm114__vector_free(lambda);
  return col;

}

/*************************************************************************
 *Uses back substitution to solve
 * U*x = b
 *where U is kxk upper triangular.
 *I don't know who originated this method, but it's in textbooks from the
 *1950s and I suspect it is much older than that.  If you just want a feel
 *for the method, I would recommend the Wikipedia page.
 *
 *The basic idea is that we can solve for the bottom element easily:
 *U_{kk}*x_k = b_k => x_k = b_k/U_kk
 *Then x_{k-1} can be solved for:
 *U_{k-1, k-1}x_{k-1} + U_{k-1, k}*x_k = b_{k-1}
 *=> x_{k-1} = (b_{k-1} - U_{k-1, k}*x_k)/U_{k-1, k-1}
 *In general, if we start with x = 0 and update as we go
 *x_i = b_i/U_{ii} - U_i*x
 *where U_i is the ith row of U.
 *
 *INPUT: U an invertible (ie no zero entries on the diagonal), upper
 * triangular matrix, b a vector
 *
 *OUTPUT: ret (pass by reference) such that
 * U*ret = b
 *
 *TYPES: There are dot products so if the matrices might be sparse,
 * it's a good idea to represent them as sparse.
 **************************************************************************/
static void back_sub(Matrix *U, Vector *b, Vector *ret) {
  int i;
  VectorIterator bit, rit;
  double bval;

  crm114__vector_zero(ret);

  vectorit_set_at_end(&bit, b);
  vectorit_set_at_end(&rit, ret);

  if (QP_DEBUG_MODE >= QP_CONSTRAINTS) {
    fprintf(stderr, "U = \n");
    crm114__matr_print(U);
    fprintf(stderr, "b = ");
    crm114__vector_print(b);
  }

  for (i = U->rows-1; i >= 0; i--) {
    if (vectorit_curr_col(bit, b) == i) {
      bval = vectorit_curr_val(bit, b);
      vectorit_prev(&bit, b);
    } else {
      bval = 0;
    }

    //note that U_ii is the first non-zero entry of U_i
    //so retrieving it is fast if U is sparse
    crm114__vectorit_insert(&rit, i,
		    (bval - crm114__dot(matr_get_row(U, i), ret))/matr_get(U, i, i),
		    ret);
    vectorit_prev(&rit,ret);
  }
}

/***********************************************************************
 *Finds the direction we should follow.
 *In an unconstrained problem this would simply be p such that
 * G(x + p) + c = 0 => Gp + g = 0
 *However, because of the constraints, we only want to move in certain
 *directions - namely those orthogonal to the active constraints.
 *Therefore, we use the projection matrix Z (recall that the columns of Z
 *are orthogonal to the currect active constraints) to only solve the above
 *equation in the directions orthogonal to the current constraints.
 *Namely we solve for p' in
 * Z^T*G*Z*(x+p') + Z^T*c = 0 => Z^T*G*Z*p' + Z^T*g = 0
 *and then project it back into the full space using
 *p = Z*p'
 *For how we solve the linear equation, see the conjugate_gradient
 *funtion.
 *
 *INPUT: Z: the projection matrix with columns orthogonal to current
 *  active constraints.
 * G: The Hessian s.t. we want G*(x+p) + c = 0
 * g: The gradient g = G*x + c
 *
 *OUTPUT: Returns the norm of the projected gradient Z^T*g
 * Returns the direction in p as pass by reference.
 *
 *TYPES: Use the types best fitted to each.  We will need to transpose Z
 * and multiply Z, Z^T, and G by a vector multiple times.
 ***********************************************************************/

static double find_direction(Matrix *Z, Matrix *G, Vector *g, Vector *p) {

  Matrix *Zt, *list[3];
  Vector *pa, *ga;
  double length;

  if (QP_DEBUG_MODE >= QP_DEBUG_LOOP) {
    fprintf(stderr, "g = ");
    crm114__vector_print(g);
  }

  if (Z->cols < G->cols) {
    //Z is actually a projection matrix
    //Go through this whole rigamarole
    Zt = crm114__matr_make_size(Z->cols, Z->rows, Z->type, MATR_PRECISE, Z->size);
    crm114__matr_transpose(Z, Zt);
    list[0] = Z;
    list[1] = G;
    list[2] = Zt;
    ga = crm114__vector_make_size(Z->cols, Z->type, MATR_PRECISE, Z->size);
    crm114__matr_vector(Zt, g, ga);
    pa = crm114__vector_make_size(Z->cols, Z->type, MATR_PRECISE, Z->size);
    conjugate_gradient(list, 3, G->rows, ga, pa);
    crm114__matr_vector(Z, pa, p);
    crm114__matr_free(Zt);
    crm114__vector_free(pa);
    length = norm2(ga);
    crm114__vector_free(ga);
    return length;
  }

  //Z is the identity
  //solve a simpler problem
  conjugate_gradient(&G, 1, G->rows, g, p);
  return norm2(g);
}


/*****************************************************************************
 *This solves A_{n-1}A_{n-2}...A_0x + b = 0
 *for symmetric, positive definite A = A_{n-1}A_{n-2}...A_0.
 *If A is symmetric, positive semi-definite and there is no solution
 *to the equation, the function detects that and returns the answer
 *from the iteration before it hit infinity.  This is the behavior
 *we want since the application of the constraints elsewhere will keep
 *the answer to the QP from actually going to infinity.
 *
 *The conjugate gradient method was originally proposed in
 *Hestenes, Magnus R.; Stiefel, Eduard (December 1952).
 *"Methods of Conjugate Gradients for Solving Linear Systems".
 *Journal of Research of the National Bureau of Standards 49 (6).
 *http://nvl.nist.gov/pub/nistpubs/jres/049/6/V49.N06.A08.pdf.
 *
 *However, if you're just trying to understand this algorithm, I would
 *recommend either the wikipedia page or
 *An Introduction to the Conjugate Gradient Method Without the Agonizing Pain
 *Jonathan Richard Shewchuk
 *CMU, 1994
 *http://www.cs.cmu.edu/~quake-papers/painless-conjugate-gradient.pdf
 *
 *The main idea of this algorithm is that you always move in "conjugate"
 *directions.  Two directions d_i and d_j are conjugate if
 * d_i*A*d_j = 0
 *This lets us converge to the correct solution in many fewer steps
 *than a straight gradient descent approach.  In addition, we never move
 *in more directions than there are - ie, the maximum number of iterations
 *of this algorithm is n where n is the length of x.
 *
 *One way of doing this would be to start with a set of basis vectors (ie
 *the unit vectors) and use a version of Gram-Schmidt to make them all
 *conjugate and then use those vectors to solve.  This requires generating
 *all n conjugate vectors, though.  So, instead, we use an iterative method
 *and hope that we don't need all n directions to get a good enough answer.
 *
 *The derivation for this algorithm is kind of advanced for a comment.
 *Therefore, I'll just give the update rules.  Use the papers above if
 *you want the derivation.
 *
 *Variables: x = solution (vector)
 * r = remainder = 0 - (A*x + b) (vector)
 * p = direction (vector)
 * a = stepsize (scalar)
 * b = constant used in calculations (Gram-Schmidt coefficient really)
 *Init: x_0 = 0, r_0 = -b, p_0 = r_0 = -b
 * a_i = r_i*r_i/(p_i*A*p_i)
 * x_{i+1} = x_i + a_i*p_i
 * r_{i+1} = r_i - a_i*A*p_i
 * b_{i+1} = r_{i+1}*r_{i+1}/r_i*r_i
 * p_{i+1} = r_{i+1} + b_{i+1}*p_i
 *
 *INPUT: A: Series of matrices such that A = A_{n-1}A_{n-2}...A_0
 *  (use this to avoid any actual matrix multiplication)
 * nmatrices: The number of matrices in A
 * maxrows: The maximum number of rows of any matrix in A
 * b: constant term
 *
 *OUTPUT: x, pass by reference, the solution to
 * A_{n-1}*A_{n-2}...*A_0*x + b = 0
 *
 *TYPES: All of the types in the function are based off the type of x.
 *****************************************************************************/

static void conjugate_gradient(Matrix **A, int nmatrices, int maxrows,
			Vector *b, Vector *x) {
  int dim = A[0]->cols; //should equal A[nmatrices-1]->rows!
  Vector *r = crm114__vector_make_size(dim, x->type, MATR_PRECISE, x->size),
    *p = crm114__vector_make_size(dim, x->type, MATR_PRECISE, x->size),
    *z = crm114__vector_make_size(dim, x->type, MATR_PRECISE, x->size),
    *ap = crm114__vector_make_size(dim, x->type, MATR_PRECISE, x->size),
    *last_x = crm114__vector_make_size(dim, x->type, MATR_PRECISE, x->size);
  double a = 0, beta = 0, lr, last_lr;
  int i;

  if (QP_DEBUG_MODE >= QP_LINEAR_SOLVER) {
    fprintf(stderr, "Arguments to conjugate gradient are:\n");
    for (i = 0; i < nmatrices; i++) {
      fprintf(stderr, "A[%d] = \n", i);
      crm114__matr_print(A[i]);
    }
    fprintf(stderr, "b = ");
    crm114__vector_print(b);
  }

  crm114__vector_zero(x); //x_0 = 0
  crm114__vector_multiply(b, -1.0, r); //r_0 = -b
  crm114__vector_copy(r, p); //p_0 = -b
  lr = norm2(r);
  last_lr = lr+1;

  i = 0;

  //this is the conjugate gradient method
  //it should never run more than x->dim times
  //note that this is no longer running to some accuracy
  //-it pretty much needs to either converge or blow up
  //or things get crazy
  while(lr > 0 && (norm2(x) < 1.0/MATRIX_EPSILON)
	&& i < x->dim) {
    crm114__matr_vector_seq(A, nmatrices, maxrows, p, z); //Ap_i (used a lot)
    a = crm114__dot(r, r)/crm114__dot(p, z); //a_i
    if (QP_DEBUG_MODE >= QP_LINEAR_SOLVER) {
      fprintf(stderr, "Iteration %d: a = %f, beta = %f norm2(r) = %.11lf\n",
	     i, a, beta, lr);
      fprintf(stderr, "x = ");
      crm114__vector_print(x);
      fprintf(stderr, "r = ");
      crm114__vector_print(r);
      fprintf(stderr, "p = ");
      crm114__vector_print(p);
      fprintf(stderr, "A*p = ");
      crm114__vector_print(z);
    }

    crm114__vector_copy(x, last_x);
    crm114__vector_add_multiple(x, p, a, x);
    crm114__vector_add_multiple(r, z, -1.0*a, r);
    last_lr = lr;
    lr = norm2(r);
    beta = lr/last_lr; //beta_{i+1}
    crm114__vector_multiply(p, beta, p); //beta_{i+1}p_i
    crm114__vector_add(p, r, p); //p_{i+1}
    i++;
  }

  if (QP_DEBUG_MODE >= QP_LINEAR_SOLVER) {
    fprintf(stderr, "Iteration %d: a = %f, beta = %f norm2(r) = %.11lf\nx = ",
	   i, a, beta, lr);
    crm114__vector_print(x);
  }

  if (norm2(x) >= 1.0/MATRIX_EPSILON) {
    //A was positive semi-definite
    //return with the correct things going to infinity
    //and it will all work out :)
    if (QP_DEBUG_MODE >= QP_DEBUG) {
      fprintf(stderr, "Singular matrix detected.\n");
    }
    if (i > 1) {
      crm114__vector_copy(last_x, x);
    }
  }

  if (QP_DEBUG_MODE >= QP_LINEAR_SOLVER) {
    fprintf(stderr, "Solution is x = ");
    crm114__vector_print(x);
  }

  crm114__vector_free(r);
  crm114__vector_free(p);
  crm114__vector_free(z);
  crm114__vector_free(ap);
  crm114__vector_free(last_x);
}

#if DEFINE_UNUSED_STATIC_FUNCTIONS

//solves equations of the form
// Ax + b = 0
//for the LEAST-SQUARES solution
//i am no longer using this
//but i've left it in
//since it is a useful function
static void run_linear(Matrix *A, Vector *b, Vector *x) {
  Matrix *T = crm114__matr_make(A->cols, A->rows, A->type, MATR_PRECISE), *list[2];
  Vector *bt = crm114__vector_make(A->cols, A->type, MATR_PRECISE);
  int maxrows;

  crm114__matr_transpose(A, T);
  list[0] = A;
  list[1] = T;
  if (T->rows > A->rows) {
    maxrows = T->rows;
  } else {
    maxrows = A->rows;
  }
  crm114__matr_vector(T, b,bt);
  conjugate_gradient(list, 2, maxrows, bt, x);
  crm114__matr_free(T);
  crm114__vector_free(bt);
}


//solves Ax + b = 0
//slowly
//use conjugate gradient instead
static void gradient_descent(Matrix **A, int nmatrices, int maxrows,
		      Vector *b, Vector *x) {
  Vector *r = crm114__vector_make(b->dim, (A[0]->type), MATR_PRECISE);

  crm114__vector_zero(x);

  crm114__vector_copy(b, r);

  if (QP_DEBUG_MODE >= QP_LINEAR_SOLVER) {
    fprintf(stderr, "A = ");
    crm114__matr_print(A[0]);
    fprintf(stderr, "b = ");
    crm114__vector_print(b);
  }

  while (norm2(r) > MATRIX_EPSILON) {
    if (QP_DEBUG_MODE >= QP_LINEAR_SOLVER) {
      fprintf(stderr, "norm2(r) = %f, r = ", norm2(r));
      crm114__vector_print(r);
      fprintf(stderr, "x = ");
      crm114__vector_print(x);
    }
    crm114__vector_multiply(r, -1.0, r);
    crm114__vector_add(x, r, x);
    crm114__matr_vector_seq(A, nmatrices, maxrows, x, r);
    crm114__vector_add(r, b, r);
  }
}

#endif

//#define SVM_QP_MAIN
#ifdef SVM_QP_MAIN

//sample main
//to test these functions
//and some of the matrix functions
int main (int argc, char **argv) {
  int i, j, xrows, xcols, issparse = 1;
  Vector *f, *qb, *qx, *v = crm114__vector_make(2, issparse),
    *w = crm114__vector_make(2, issparse),
    *u = crm114__vector_make(3, issparse);
  Matrix *X, *Xt, *G, *A, *M1 = crm114__matr_make(3, 2, issparse),
    *M2 = crm114__matr_make(2, 3, issparse),
    *list[2], *M2M1 = crm114__matr_make(2,2, issparse);
  FILE *in;
  int *t = NULL;
  double tmp;

  if (!t) {
    fprintf(stderr, "testing null %p\n", &t);
  }


  //testing crm114__matr_vector_seq
  matr_set(M1, 0, 0, 1.2);
  matr_set(M1, 0, 1, 3);
  matr_set(M1, 1, 0, 1.4);
  matr_set(M1, 1, 1, 4);
  matr_set(M1, 2, 0, 6);
  matr_set(M1, 2, 1, -2);

  matr_set(M2, 0, 0, -2);
  matr_set(M2, 0, 1, 1);
  matr_set(M2, 0, 2, 13);
  matr_set(M2, 1, 0, -5);
  matr_set(M2, 1, 1, 1.7);
  matr_set(M2, 1, 2, -2.5);

  crm114__vector_set(v, 0, 3);
  crm114__vector_set(v, 1, -2);
  list[0] = M1;
  list[1] = M2;

  fprintf(stderr, "M1 = \n");
  crm114__matr_print(M1);
  fprintf(stderr, "M2 = \n");
  crm114__matr_print(M2);
  fprintf(stderr, "v = ");
  crm114__vector_print(v);
  crm114__matr_vector(M1, v, u);
  fprintf(stderr, "u = ");
  crm114__vector_print(u);
  crm114__matr_vector_seq(list, 2, 3, v, w);
  fprintf(stderr, "M2*M1*v = ");
  crm114__vector_print(w);
  crm114__matr_multiply(M2, M1, M2M1);
  crm114__matr_vector(M2M1, v, w);
  fprintf(stderr, "(M2*M1)*v = ");
  crm114__vector_print(w);
  fprintf(stderr, "v = ");
  crm114__vector_print(v);
  fprintf(stderr, "M1 = \n");
  crm114__matr_print(M1);
  fprintf(stderr, "M2 = \n");
  crm114__matr_print(M2);
  crm114__matr_free(M1);
  crm114__matr_free(M2);
  crm114__matr_free(M2M1);
  crm114__vector_free(v);
  crm114__vector_free(w);
  crm114__vector_free(u);

  //testing crm114__run_qp
  if (argc < 2) {
    return 0;
  }
  in = fopen(argv[1], "r");
  if (!in) {
    fprintf(stderr, "Invalid file name");
    exit(1);
  }
  fscanf(in, "%d %d", &xrows, &xcols);
  X = crm114__matr_make(xrows, xcols, issparse);
  for (i = 0; i < xrows; i++) {
    for (j = 0; j < xcols; j++) {
      fscanf(in, "%lf", &tmp);
      matr_set(X, i, j, tmp);
    }
  }
  //last row is f
  f = crm114__vector_make(xrows, issparse);
  for (i = 0; i < xrows; i++) {
    fscanf(in, "%lf", &tmp);
    crm114__vector_set(f, i, -1.0*tmp);
  }
  fclose(in);
  //file should list feature vectors as rows
  Xt = crm114__matr_make(xcols, xrows, issparse);
  crm114__matr_transpose(X, Xt);
  G = crm114__matr_make(xrows, xrows, issparse);
  crm114__matr_multiply(X, Xt, G);
  //make constraint matrix
  A = crm114__matr_make(xrows+1, xrows, issparse);
  for (i = 0; i < xrows; i++) {
    matr_set(A, 0, i, -1.0);
  }
  for (i = 1; i <= xrows; i++) {
    matr_set(A, i, i-1, 1);
  }
  qb = crm114__vector_make(xrows+1, issparse);
  crm114__vector_set(qb, 0, -MAX_X_VAL);
  qx = crm114__vector_make(xrows, issparse);
  fprintf(stderr, "Running qp.\n");
  crm114__run_qp(G, A, f, qb, qx);
  fprintf(stderr, "QP solution is ");
  crm114__vector_print(qx);
  crm114__matr_free(X);
  crm114__matr_free(Xt);
  crm114__matr_free(G);
  crm114__matr_free(A);
  crm114__vector_free(f);
  crm114__vector_free(qb);
  crm114__vector_free(qx);
  return 0;
}

#endif
