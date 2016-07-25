//	crm114_config.h -- Configuration for CRM114 library.

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
//
// Derived from engine version of crm114_config.h.


///////////////////////////////////////////////////////////////////
//	Some things here you can change with relative impunity.
//	Other things, not so much.  Where there are limiting factors
//	noted, please obey them or you may break something important.
//	And, of course, realize that this is LGPLed software with
//	NO WARRANTY - make any changes and that goes double.
///////////////////////////////////////////////////////////////////

#ifndef	__CRM114_CONFIG_H__
#define	__CRM114_CONFIG_H__

//     Do you want all the classifiers?  Or just the "production
//     ready ones"?   Comment the next line out if you want everything.
//#define PRODUCTION_CLASSIFIERS_ONLY
//
//
//     As used in the library:
#define STATISTICS_FILE_IDENT_STRING_MAX 1024
#define CLASSNAME_LENGTH 31
#define DEFAULT_HOW_MANY_CLASSES 2
#define DEFAULT_CLASS_SIZE (1<<23)   // 8 megabytes



// Markov family config

//    do we use Sparse Binary Polynomial Hashing (sensitive to both
//    sequence and spacing of individual words), Token Grab Bag, or
//    Token Sequence Sensitive?  Testing against the SpamAssassin
//    "hard" database shows that SBPH, TGB, and TGB2, are somewhat
//    more accurate than TSS, and about 50% more accurate than First
//    Order Only.  However, that's for English, and other natural
//    languages may show a different statistical distribution.
//
//    Choose ONE of the following:
//          SBPH, TGB2, TGB, TSS, or ARBITRARY_WINDOW_LEN:
//
//    *** DANGER, WILL ROBINSON ***  You MUST rebuild your .css files from
//    samples of text if you change this.
//
//
//     Sparse Binary Polynomial Hashing
#define SBPH
//
//     Token Grab Bag, noaliasing
//#define TGB2
//
//     Token Grab Bag, aliasing
//#define TGB
//
//     Token Sequence Sensitive
//#define TSS
//
//     First Order Only (i.e. single words, like SpamBayes)
//    Note- if you use FOO, you must turn off weights!!
//#define FOO
//
//     Generalized format for the window length.
//
//  DO NOT SET THIS TO MORE THAN 10 WITHOUT LENGTHENING hctable
//  the classifier modules !!!!!!  "hctable" contains the pipeline
//  hashing coefficients and needs to be extended to 2 * WINDOW_LEN
//
//     Generic window length code
//#define ARBITRARY_WINDOW_LENGTH
//
#define MARKOVIAN_WINDOW_LEN 5
//
#define OSB_BAYES_WINDOW_LEN 5
//
//      DO NOT set this to more than 5 without lengthening the
//      htup1 and htup2 tables in crm_unified_bayes.c
//
#define UNIFIED_BAYES_WINDOW_LEN 5
//
//      Unified tokenization pipeline length.
//          maximum window length _ever_.
#define UNIFIED_WINDOW_MAX 32
//
//          maximum number of iterations or phases of a pipeline,
//          maximum number of rows in a matrix of coefficients
#define UNIFIED_ITERS_MAX 64
//
//     Now, choose whether we want to use the "old" or the "new" local
//     probability calculation.  The "old" one works slightly better
//     for SBPH and much better for TSS, the "new" one works slightly
//     better for TGB and TGB2, and _much_ better for FOO
//
//     The current default (not necessarily optimal)
//     is Markovian SBPH, STATIC_LOCAL_PROBABILITIES,
//     LOCAL_PROB_DENOM = 16, and SUPER_MARKOV
//
//#define LOCAL_PROB_DENOM 2.0
#define LOCAL_PROB_DENOM 16.0
//#define LOCAL_PROB_DENOM 256.0
#define STATIC_LOCAL_PROBABILITIES
//#define LENGTHBASED_LOCAL_PROBABILITIES
//
//#define ENTROPIC_WEIGHTS
//#define MARKOV_WEIGHTS
#define SUPER_MARKOV_WEIGHTS
//#define BREYER_CHHABRA_SIEFKES_WEIGHTS
//#define BREYER_CHHABRA_SIEFKES_BASE7_WEIGHTS
//#define BCS_MWS_WEIGHTS
//#define BCS_EXP_WEIGHTS
//
//
//    Do we take only the maximum probability feature?
//
//#define USE_PEAK
//
//
//    Should we use stochastic microgrooming, or weight-distance microgrooming-
//    Make sure ONE of these is turned on.
//#define STOCHASTIC_AMNESIA
#define WEIGHT_DISTANCE_AMNESIA

#if (! defined (STOCHASTIC_AMNESIA) && ! defined (WEIGHT_DISTANCE_AMNESIA))
#error Neither STOCHASTIC_AMNESIA nor WEIGHT_DISTANCE_AMNESIA defined
#elif (defined (STOCHASTIC_AMNESIA) && defined (WEIGHT_DISTANCE_AMNESIA))
#error Both STOCHASTIC_AMNESIA and WEIGHT_DISTANCE_AMNESIA defined
#endif

//
//    define the default max chain length in a .css file that triggers
//    autogrooming, the rescale factor when we rescale, and how often
//    we rescale, and what chance (mask and key) for any particular
//    slot to get rescaled when a rescale is triggered for that slot chain.
//#define MARKOV_MICROGROOM_CHAIN_LENGTH 1024
#define MARKOV_MICROGROOM_CHAIN_LENGTH 256
//#define MARKOV_MICROGROOM_CHAIN_LENGTH 64
#define MARKOV_MICROGROOM_RESCALE_FACTOR .75
#define MARKOV_MICROGROOM_STOCHASTIC_MASK 0x0000000F
#define MARKOV_MICROGROOM_STOCHASTIC_KEY  0x00000001
#define MARKOV_MICROGROOM_STOP_AFTER 32    //  maximum number of buckets groom-zeroed

//   define the default number of buckets in a learning file hash table
//   (note that this should be a prime number, or at least one with a
//    lot of big factors)
//
//       this value (2097153) is one more than 2 megs, for a .css of 24 megs
//#define DEFAULT_SPARSE_SPECTRUM_FILE_LENGTH 2097153
//
//       this value (1048577) is one more than a meg, for a .css of 12 megs
//       for the Markovian, and half that for OSB classifiers
#define DEFAULT_SPARSE_SPECTRUM_FILE_LENGTH 1048577
#define DEFAULT_MARKOVIAN_SPARSE_SPECTRUM_FILE_LENGTH 1048577
#define DEFAULT_OSB_BAYES_SPARSE_SPECTRUM_FILE_LENGTH 524287 // Mersenne prime


//     How big is a feature bucket?  Is it a byte, a short, a long,
//     a float, whatever.  :)
//#define MARKOV_FEATUREBUCKET_VALUE_MAX 32767
#define MARKOV_FEATUREBUCKET_VALUE_MAX 1000000000
#define MARKOV_FEATUREBUCKET_HISTOGRAM_MAX 4096


// end Markov family config


//   define default internal debug level
#define DEFAULT_INTERNAL_TRACE_LEVEL 0

//   define default user debug level
#define DEFAULT_USER_TRACE_LEVEL 0

////
//         Winnow algorithm parameters here...
//
#define OSB_WINNOW_WINDOW_LEN 5
#define OSB_WINNOW_PROMOTION 1.23
#define OSB_WINNOW_DEMOTION 0.83
#define DEFAULT_WINNOW_SPARSE_SPECTRUM_FILE_LENGTH 1048577

//    For the hyperspace matcher, we need to define a few things.
//#define DEFAULT_HYPERSPACE_BUCKETCOUNT 1000000
#define DEFAULT_HYPERSPACE_BUCKETCOUNT 100
//   Stuff for bit-entropic configuration
//   Define the size of our alphabet, and how many bits per alph.
#define ENTROPY_ALPHABET_SIZE 2
#define ENTROPY_CHAR_SIZE 1
#define ENTROPY_CHAR_BITMASK 0x1
//       What fraction of the nodes in a bit-entropic file should be
//       referenceable from the FIR prior arithmetical encoding
//       lookaside table?  0.01 is 1% == average of 100 steps to find
//       the best node.  0.2 is 20% or 5 steps to find the best node.
#define BIT_ENTROPIC_FIR_LOOKASIDE_FRACTION 0.1
#define BIT_ENTROPIC_FIR_LOOKASIDE_STEP_LIMIT 128
#define BIT_ENTROPIC_FIR_PRIOR_BIT_WEIGHT 0.5
#define BIT_ENTROPIC_SHUFFLE_HEIGHT 1024  //   was 256
#define BIT_ENTROPIC_SHUFFLE_WIDTH 1024   //   was 256
#define BIT_ENTROPIC_PROBABILITY_NERF 0.0000000000000000001
//#define DEFAULT_BIT_ENTROPY_FILE_LENGTH 2000000
#define DEFAULT_BIT_ENTROPY_FILE_LENGTH 1000000

// Defines for the svm classifier
// All defines you should want to use without getting into
// the nitty details of the SVM are here.  For nitty detail
// defines, see crm114_matrix_util.h, crm114_svm_quad_prog.h,
// crm114_matrix.h, and crm114_svm_lib_fncts.h
#define MAX_SVM_FEATURES 100000       //per example
#define SVM_INTERNAL_TRACE_LEVEL 3    //the debug level when crm114__internal_trace is
                                      //on
#define SVM_ACCURACY 1e-3             //The accuracy to which to run the solver
                                      //This is the average margin violation NOT
                                      //accounted for by the slack variable.
#define SV_TOLERANCE 0.01             //An example is a support vector if
                                      //theta*y*x <= 1 + SV_TOLERANCE.
                                      //The smaller SV_TOLERANCE, the fewer
                                      //examples will be tagged as support
                                      //vectors.  This will make it faster to
                                      //learn new examples, but possibly less
                                      //accurate.
#define SVM_ADD_CONSTANT 1            //Define this to be 1 if you want a
                                      //constant offset in the classification
                                      //ie h(x) = theta*x + b where b is
                                      //the offset.  If you don't want
                                      //a constant offset (just h(x) = theta*x),
                                      //define this to be 0.
#define SVM_HOLE_FRAC 0.25            //Size of the "hole" left at the end of
                                      //the file to allow for quick appends
                                      //without having to forcibly unmap the
                                      //file.  This is as a fraction of the
                                      //size of the file without the hole.  So
                                      //setting it to 1 doubles the file size.
                                      //If you don't want a hole left, set
                                      //this to 0.
#define SVM_MAX_SOLVER_ITERATIONS 200 //absolute maximum number of loops the
                                      //solver is allowed
#define SVM_CHECK 100                 //every SVM_CHECK we look to see if
                                      //the accuracy is better than
                                      //SVM_CHECK_FACTOR*SVM_ACCURACY.
                                      //If it is, we exit the solver loop.
#define SVM_CHECK_FACTOR 2            //every SVM_CHECK we look to see if
                                      //the accuracy is better than
                                      //SVM_CHECK_FACTOR*SVM_ACCURACY.
                                      //If it is, we exit the solver loop.
//defines for SVM microgrooming
#define SVM_GROOM_OLD 10000           //we groom only if there are this many
                                      //examples (or more) not being used in
                                      //solving
#define SVM_GROOM_FRAC 0.9            //we keep this fraction of examples after
                                      //grooming
//defines for svm_smart_mode
#define SVM_BASE_EXAMPLES 1000        //the number of examples we need to see
                                      //before we train
#define SVM_INCR_FRAC 0.1             //if more than this fraction of examples
                                      //are appended, we do a fromstart rather
                                      //than use the incremental method.
// Defines for the PCA classifier
// All defines you should want to use without getting into
// the nitty details of the PCA are here.  For nitty detail
// defines, see crm114_matrix_util.h and crm114_pca_lib_fncts.h
#define PCA_INTERNAL_TRACE_LEVEL 3 //the debug level when crm114__internal_trace is on
#define PCA_ACCURACY 1e-8          //accuracy to which to run the solver
#define MAX_PCA_ITERATIONS 1000    //maximum number of solver iterations
#define PCA_CLASS_MAG 50           //the starting class magnitudes.  if this
                                   //is too small, the solver will double it
                                   //and resolve.  if it is too large, the
                                   //solver will be less accurate.
#define PCA_REDO_FRAC 0.001        //if we get this fraction of training
                                   //examples wrong with class mag enabled, we
                                   //retrain with class mag doubled.
#define PCA_MAX_REDOS 20           //The maximum number of redos allowed when
                                   //trying to find the correct class mag.
#define PCA_HOLE_FRAC 0.25         //Size of the "hole" left at the end of
                                   //the file to allow for quick appends
                                   //without having to forcibly unmap the file.
                                   //This is as a fraction of the size of the
                                   //file without the hole.  So setting it to
                                   //1 doubles the file size.  If you don't
                                   //want a hole left, set this to 0.
//defines for PCA microgrooming
#define PCA_GROOM_OLD 10000        //we groom only if there are this many
                                   //examples (or more) present
#define PCA_GROOM_FRAC 0.9         //we keep this fraction of examples after
                                   //grooming

//   DANGER DANGER DANGER DANGER DANGER
//   CHANGE THESE AT YOUR PERIL- YOUR .CSS FILES WILL NOT BE
//   FORWARD COMPATIBLE WITH ANYONE ELSES IF YOU CHANGE THESE.

//     how many classes per datablock can the library support?
#define CRM114_MAX_CLASSES 128
// FSCM uses two blocks / class
#define CRM114_MAX_BLOCKS  (2 * CRM114_MAX_CLASSES)
//     Maximum length of a stored regex (ugly!  But we need a max length
//     in the mapping.  GROT GROT GROT )
#define MAX_REGEX 4096

//
///    END OF DANGER DANGER DANGER DANGER
/////////////////////////////////////////////////////////////////////


////////////////////////////////////////////
//
//      Improved FSCM-specific parameters
//
/////////////////////////////////////////////

//   this is 2^18 + 1
//   This determines the tradeoff in memory vs. speed/accuracy.
//define FSCM_DEFAULT_PREFIX_TABLE_CELLS 262145
//
//   This is 1 meg + 1
#define FSCM_DEFAULT_PREFIX_TABLE_CELLS 1048577

//   How long are our prefixes?  Original prefix was 3 but that's
//   rather suboptimal for best speed.  6 looks pretty good for speed and
//   accuracy.
//   prefix length 6 and thickness 10 (200 multiplier) yields 29 / 4147
//
//#define FSCM_DEFAULT_CODE_PREFIX_LEN 3
#define FSCM_DEFAULT_CODE_PREFIX_LEN 6

//  The chain cache is a speedup for the FSCM match
//  It's indexed modulo the chainstart, with associativity 1.0
#define FSCM_CHAIN_CACHE_SIZE FSCM_DEFAULT_PREFIX_TABLE_CELLS

//   How much space to start out initially for the forwarding index area
#define FSCM_DEFAULT_FORWARD_CHAIN_CELLS 1048577

////////////////////////////////////////////
//
//     Neural Net parameters
//
////////////////////////////////////////////
#define NN_RETINA_SIZE 8192
#define NN_FIRST_LAYER_SIZE 8
#define NN_HIDDEN_LAYER_SIZE 8

//     Neural Net training setups
//
//     Note- convergence seems to work well at
//    alpha 0.2 init_noise 0.5 stoch_noise 0.1 gain_noise 0.00000001
//    alpha 0.2 init_noise 0.2 stoch_noise 0.1 gain_noise 0.00000001
//    alpha 0.2 init_noise 0.2 stoch_noise 0.05 gain_noise 0.00000001
//    alpha 0.2 init_noise 0.2 stoch_noise 0.05 gain_noise 0.00000001
//    alpha 0.2 init_noise 0.2 stoch_noise 0.05 gain_noise 2.0
//    alpha 0.2 init_noise 0.2 stoch_noise 0.05 gain_noise 2.0 zerotr 0.9999

#define NN_DEFAULT_ALPHA 0.2
//   Initialization noise magnitude
#define NN_INITIALIZATION_NOISE_MAGNITUDE 0.2
//   Stochastic noise magnitude
#define NN_DEFAULT_STOCH_NOISE 0.05
//   Gain noise magnitude
#define NN_DEFAULT_GAIN_NOISE 2.0
//   Zero-tracking factor - factor the weights move toward zero every epoch
#define NN_ZERO_TRACKING 0.9999
//   Threshold for back propagation
#define NN_INTERNAL_TRAINING_THRESHOLD 0.1
//  Just use 1 neuron excitation per token coming in.
#define NN_N_PUMPS 1
//  How many training cycles before we punt out
#define NN_MAX_TRAINING_CYCLES 500
//  When doing a "nuke and retry", allow this many training cycles.
#define NN_MAX_TRAINING_CYCLES_FROMSTART 5000
//  How often do we cause a punt (we punt every 0th epoch modulo this number)
#define NN_FROMSTART_PUNTING 10000000
//  After how many "not needed" cycles do we microgroom this doc away?
#define NN_MICROGROOM_THRESHOLD 1000000
//  use the sparse retina design?  No, it's not good.
#define NN_SPARSE_RETINA 0

//    End of configurable parameters.



#endif	// !__CRM114_CONFIG_H__
