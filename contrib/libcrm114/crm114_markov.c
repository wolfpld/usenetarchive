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

//  include some standard files
#include "crm114_sysincludes.h"

//  include any local crm114 configuration file
#include "crm114_config.h"

//  include the crm114 data structures file
#include "crm114_structs.h"

#include "crm114_lib.h"

#include "crm114_internal.h"


//   OSB/Markov classifier


//
//    How to learn OSB_Bayes style  - in this case, we'll include the single
//    word terms that may not strictly be necessary.
//

CRM114_ERR crm114_learn_features_markov (CRM114_DATABLOCK **db,
					 int icls,
					 const struct crm114_feature_row features[],
					 long featurecount)
{
  long i;
  // hash table (the class we're to learn into) and its length
  MARKOV_FEATUREBUCKET *hashes =
    (MARKOV_FEATUREBUCKET *)(&(*db)->data[0]
			     + (*db)->cb.block[icls].start_offset);
  unsigned int htlen = (*db)->cb.block[icls].allocated_size / sizeof(MARKOV_FEATUREBUCKET);
  //
  int sense;
  int microgroom;


  if (crm114__internal_trace)
    fprintf (stderr, "executing a Markov LEARN\n");

  if (db == NULL || features == NULL)
    return CRM114_BADARG;
  if (icls < 0 || icls > (*db)->cb.how_many_classes - 1)
    return CRM114_BADARG;
  if ((*db)->cb.classifier_flags & CRM114_REFUTE)
    return CRM114_BADARG;

  //            set our cflags, if needed.  The defaults are
  //            "case" and "affirm", (both zero valued).
  //            and "microgroom" disabled.
  sense = +1;
  if ((*db)->cb.classifier_flags & CRM114_ERASE)
    {
      sense = -sense;
      if (crm114__user_trace)
	fprintf (stderr, " unlearning\n");
    };
  microgroom = 0;
  if ((*db)->cb.classifier_flags & CRM114_MICROGROOM)
    {
      microgroom = 1;
      if (crm114__user_trace)
	fprintf (stderr, " enabling microgrooming.\n");
    };

  (*db)->cb.class[icls].documents += sense;
  if ((*db)->cb.class[icls].documents < 0)
    (*db)->cb.class[icls].documents = 0;

  //    and the big loop... go through all of the text.
  for (i = 0; i < featurecount; i++)
    {
      unsigned int hindex;
      unsigned int h1;
      unsigned int incrs;

      h1 = features[i].feature;
      hindex = h1 % htlen;

      // If the bucket we hash to is in use by some other feature
      // (hash value doesn't match), we have collided, and must search
      // forward for an unused bucket.
      incrs = 0;
      //          stop when we hit the correct slot, OR
      //          when we hit a zero-value (reusable) slot
      while ( ! (hashes[hindex].hash == h1 || hashes[hindex].value == 0))
	{
	  //
	  //       If microgrooming is enabled, and we've found a
	  //       chain that's too long, we groom it down.
	  //
	  if (microgroom && (incrs >= MARKOV_MICROGROOM_CHAIN_LENGTH))
	    {
	      int zeroedfeatures;

	      //   and do the groom.
	      zeroedfeatures = (int)crm114__markov_microgroom (hashes,
							       htlen,
							       h1);
	      // if microgroom couldn't help, we're hosed
	      if (zeroedfeatures == 0)
		return CRM114_CLASS_FULL;

	      (*db)->cb.class[icls].features -= zeroedfeatures;

	      //  since things may have moved after a
	      //  microgroom, restart our search
	      hindex = h1 % htlen;
	      incrs = 0;
	    }
	  else
	    {
	      //      check to see if we've incremented ourself all the
	      //      way around the .css file.  If so, we're full, and
	      //      can hold no more features (this is unrecoverable)
	      //
	      incrs++;
	      if (incrs >= htlen)
		{
		  return CRM114_CLASS_FULL;
		};
	      hindex++;
	      if (hindex >= htlen) hindex = 0;
	    }
	};

      if (crm114__internal_trace)
	{
	  if (hashes[hindex].value == 0)
	    {
	      fprintf (stderr,"New feature at %u\n", hindex);
	    }
	  else
	    {
	      fprintf (stderr, "Old feature at %u\n", hindex);
	    };
	};
      //      always rewrite hash, as it may be incorrect
      //    (on a reused bucket) or zero (on a fresh one)
      //
      //      watch out - sense may be both + or -, so check before
      //      adding it...
      //
      //     let the embedded feature counter sorta keep up...
      (*db)->cb.class[icls].features += sense;

      if (sense > 0 )
	{
	  //     Right slot, set it up
	  //
	  hashes[hindex].hash = h1;
	  if ( hashes[hindex].value + sense
	       >= MARKOV_FEATUREBUCKET_VALUE_MAX-1)
	    {
	      hashes[hindex].value = MARKOV_FEATUREBUCKET_VALUE_MAX - 1;
	    }
	  else
	    {
	      hashes[hindex].value += sense;
	    };
	};
      if ( sense < 0 )
	{
	  if (hashes[hindex].value <= (unsigned int)-sense )
	    {
	      hashes[hindex].value = 0;
	    }
	  else
	    {
	      hashes[hindex].value += sense;
	    };
	};
    };

  return (CRM114_OK);
}


//      How to do a OSB_Bayes CLASSIFY
//
CRM114_ERR crm114_classify_features_markov (const CRM114_DATABLOCK *db,
					    const struct crm114_feature_row features[],
					    long featurecount,
					    CRM114_MATCHRESULT *result)
{
  //      classify the sparse spectrum of this input
  //      as belonging to a particular type.
  int ifr;			// subscript in features
  int ncls = db->cb.how_many_classes;
  int icls;			// subscript in db->cb.blockhdr
  int use_chisquared;
  unsigned long total_learns;
  unsigned long total_features;

  // unsigned long fcounts[CRM114_MAX_CLASSES]; // total counts for feature normalize
  // unsigned long totalcount = 0;

  double cpcorr[CRM114_MAX_CLASSES];	// corpus correction factors
  double hits[CRM114_MAX_CLASSES];	// actual hits per feature per class
  long totalhits[CRM114_MAX_CLASSES];	// actual total hits per class
  double chi2[CRM114_MAX_CLASSES];	// chi-squared values (such as they are)
  int expected;				// expected hits for chi2.
  int unk_features;	     // total unknown features in the document
  double htf;			// hits this feature got.
  double ptc[CRM114_MAX_CLASSES];	// current running probability of this class
  double renorm = 0.0;
  double pltc[CRM114_MAX_CLASSES];	// current local probability of this class

  double top10scores[10];
  long top10polys[10];

  int i;



  if (crm114__internal_trace)
    fprintf (stderr, "executing a Markov CLASSIFY\n");

  if (db == NULL || features == NULL || result == NULL)
    return CRM114_BADARG;

  //            set our flags, if needed.

  use_chisquared = 0;
  if (db->cb.classifier_flags & CRM114_CHI2)
    {
      use_chisquared = 1;
      if (crm114__user_trace)
	fprintf (stderr, " using chi^2 chaining rule \n");
    };


  //  goodcount = evilcount = 1;   // prevents a divide-by-zero error.
  //cpgood = cpevil = 0.0;
  //ghits = ehits = 0.0 ;
  //psucc = 0.5;
  //pfail = (1.0 - psucc);
  //pic = 0.5;
  //pnic = 0.5;


  //      initialize our arrays for N classes
  for (i = 0; i < CRM114_MAX_CLASSES; i++)
    {
      // fcounts[i] = 0;    // check later to prevent a divide-by-zero
    			 // error on empty .css file
      cpcorr[i] = 0.0;   // corpus correction factors
      hits[i] = 0.0;     // absolute hit counts
      totalhits[i] = 0;     // absolute hit counts
      ptc[i] = 0.5;      // priori probability
      pltc[i] = 0.5;     // local probability

    };

  for (i = 0; i < 10; i++)
    {
      top10scores[i] = 0;
      top10polys[i] = 0;
    };
  //        --  probabilistic evaluator ---
  //     S = success; A = a testable attribute of success
  //     ns = not success, na = not attribute
  //     the chain rule we use is:
  //
  //                   P(A|S) P(S)
  //  P (S|A) =   -------------------------
  //             P(A|S) P(S) + P(A|NS) P(NS)
  //
  //     and we apply it repeatedly to evaluate the final prob.  For
  //     the initial a-priori probability, we use 0.5.  The output
  //     value (here, P(S|A) ) becomes the new a-priori for the next
  //     iteration.
  //
  //     Extension - we generalize the above to I classes as and feature
  //      F as follows:
  //
  //                         P(F|Ci) P(Ci)
  //    P(Ci|F) = ----------------------------------------
  //              Sum over all classes Ci of P(F|Ci) P(Ci)
  //
  //     We also correct for the unequal corpus sizes by multiplying
  //     the probabilities by a renormalization factor.  if Tg is the
  //     total number of good features, and Te is the total number of
  //     evil features, and Rg and Re are the raw relative scores,
  //     then the corrected relative scores Cg aqnd Ce are
  //
  //     Cg = (Rg / Tg)
  //     Ce = (Re / Te)
  //
  //     or  Ci = (Ri / Ti)
  //
  //     Cg and Ce can now be used as "corrected" relative counts
  //     to calculate the naive Bayesian probabilities.
  //
  //     Lastly, the issue of "over-certainty" rears it's ugly head.
  //     This is what happens when there's a zero raw count of either
  //     good or evil features at a particular place in the file; the
  //     strict but naive mathematical interpretation of this is that
  //     "feature A never/always occurs when in good/evil, hence this
  //     is conclusive evidence of good/evil and the probabilities go
  //     to 1.0 or 0.0, and are stuck there forevermore.  We use the
  //     somewhat ad-hoc interpretation that it is unreasonable to
  //     assume that any finite number of samples can appropriately
  //     represent an infinite continuum of spewage, so we can bound
  //     the certainty of any meausre to be in the range:
  //
  //        limit: [ 1/featurecount+2 , 1 - 1/featurecount+2].
  //
  //     The prior bound is strictly made-up-on-the-spot and has NO
  //     strong theoretical basis.  It does have the nice behavior
  //     that for feature counts of 0 the probability is clipped to
  //     [0.5, 0.5], for feature counts of 1 to [0.333, 0.666]
  //     for feature counts of 2 to [0.25, 0.75], for 3 to
  //     [0.2, 0.8], for 4 to [0.166, 0.833] and so on.
  //


  // sanity checks...  Uncomment for super-strict CLASSIFY.

  //	do we have at least 1 class?
  if (ncls <= 0)
    {
      return CRM114_BADARG;
    };
  //	do we have at least 1 success class and 1 failure class?
  //{
  //  int ns = 0;
  //  int nf = 0;

  //  for (i = 0; i < ncls; i++)
  //    if (db->cb.class_hdr[i].success)
  //      ns++;
  //    else
  //      nf++;
  //  if ( !(ns > 0 && nf > 0))
  //    return CRM114_BADARG;
  //}

  // end sanity checks

  // CLASSIFY with no arguments is a "success", if not found insane above
  if (ncls == 0)
    return (CRM114_OK);

//  for (i = 0; i < ncls; i++)
//    {
//      //    now, set up the normalization factor fcount[]
//      //      count up the total first
//      fcounts[i] = 0;
//      {
//        long k;
//
//        for (k = 1; k < hashlens[i]; k++)
//          fcounts [i] = fcounts[i] + hashes[i][k].value;
//      }
//      if (fcounts[i] == 0) fcounts[i] = 1;
//      totalcount = totalcount + fcounts[i];
//    };
  //
  //     calculate cpcorr (count compensation correction)
  //
  total_learns = 0;
  total_features = 0;
  for (i = 0; i < ncls; i++)
    {
      total_learns   += db->cb.class[i].documents;
      total_features += db->cb.class[i].features;
    };

  for (i = 0; i < ncls; i++)
    {
      //   disable cpcorr for now... unclear that it's useful.
      // cpcorr[i] = 1.0;

      if (use_chisquared)
	cpcorr[i] = 1.0;
      else
	//  new cpcorr - from Fidelis' work on evaluators.  Note that
	//   we renormalize _all_ terms, not just the min term.
	cpcorr [i] =  ((double)total_learns / (double) ncls) /
	  (double)db->cb.class[i].documents;

      if (crm114__internal_trace)
	fprintf(stderr, "total_learns:%lu class:%d learns:%d cpcorr:%f\n",
		total_learns, i, db->cb.class[i].documents, cpcorr[i]);
    };


  //
  //   now we can do the polynomials and add up points.
  unk_features = featurecount;

  for (ifr = 0; ifr < featurecount; ifr++)
    {
      // feature and subscript of matrix row that generated it
      unsigned int h1 = features[ifr].feature;
      int row         = features[ifr].row;

      //       Zero out "Hits This Feature"
      htf = 0.0;

      if (crm114__internal_trace)
	fprintf (stderr, "Polynomial %d has h1:%u\n", row, h1);

      for (icls = 0; icls < ncls; icls++)
	{
	  const MARKOV_FEATUREBUCKET *hashes =
	    (MARKOV_FEATUREBUCKET *)(&db->data[0]
				     + db->cb.block[icls].start_offset);
	  const long htlen =
	    db->cb.block[icls].allocated_size / sizeof(MARKOV_FEATUREBUCKET);
	  unsigned int hindex;
	  unsigned int lh;

	  hindex = h1 % htlen;
	  lh = hindex;
	  hits[icls] = 0;
	  while (hashes[lh].hash != h1 && hashes[lh].value != 0)
	    {
	      lh++;
	      if (lh >= htlen) lh = 0; // wraparound
	      if (lh == hindex) break;	// tried them all
	    };
	  if (hashes[lh].hash == h1)
	    {
	      //    Note - a strict interpretation of Bayesian
	      //    chain probabilities should use 0 as the initial
	      //    state.  However, because we rapidly run out of
	      //    significant digits, we use a much less strong
	      //    initial state.   Note also that any nonzero
	      //    positive value prevents divide-by-zero.
	      static int fw[] = {24, 14, 7, 4, 2, 1, 0};
#if 0
	      //use chi squared
	      // cubic weights seems to work well for chi^2...- Fidelis
	      static int chi_feature_weight[] = {125, 64, 27, 8, 1, 0};
#else
	      // disable chi squared
	      static int chi_feature_weight[] = {1, 1, 1, 1, 1, 1};
#endif
	      int feature_weight;
	      unsigned int wh;	// occurrences this feature this file, weighted
				// ..."weighted hits"

	      //
	      //    calculate the precursors to the local probabilities;
	      //    these are the hits[icls] array, and the htf total.

	      feature_weight = (use_chisquared
				? chi_feature_weight[row]
				: fw[row]);
	      wh = hashes[lh].value * feature_weight;
	      wh *= cpcorr [icls];    	// Correct with cpcorr
	      hits[icls] = wh;
	      htf += wh;            // hits-this-feature

	      totalhits[icls]++;
	    };
	};


      //      now update the probabilities.
      //
      //     NOTA BENE: there are a bunch of different ways to
      //      calculate local probabilities.  The text below
      //      refers to an experiment that may or may not make it
      //      as the "best way".
      //
      //      The hard part is this - what is the local in-class
      //      versus local out-of-class probability given the finding
      //      of a particular feature?
      //
      //      I'm guessing this- the validity is the differntial
      //      seen on the feature (that is, fgood - fevil )
      //      times the entropy of that feature as seen in the
      //      corpus (that is,
      //
      //              Pfeature*log2(Pfeature)
      //
      //      =
      //        totalcount_this_feature
      //            ---------------    * log2 (totalcount_this_feature)
      //         totalcount_all_features
      //
      //    (note, yes, the last term seems like it should be
      //    relative to totalcount_all_features, but a bit of algebra
      //    will show that if you view fgood and fevil as two different
      //    signals, then you end up with + and - totalcount inside
      //    the logarithm parenthesis, and they cancel out.
      //    (the 0.30102 converts "log10" to "log2" - it's not
      //    a magic number, it's just that log2 isn't in glibc)
      //

      //  HACK ALERT- this code here is still under development
      //  and should be viewed with the greatest possible
      //  suspicion.  :=)

      //    Now, some funky magic.  Our formula above is
      //    mathematically correct (if features were
      //    independent- something we conveniently ignore.),
      //    but because of the limited word length in a real
      //    computer, we can quickly run out of dynamic range
      //    early in a CLASSIFY (P(S) indistinguishable from
      //    1.00) and then there is no way to recover.  To
      //    remedy this, we use two alternate forms of the
      //    formula (in Psucc and Pfail) and use whichever
      //    form that yields the smaller probability to
      //    recalculate the value of the larger.
      //
      //    The net result of this subterfuge is a nonuniform
      //    representation of the probability, with a huge dynamic
      //    range in two places - near 0.0, and near 1.0 , which
      //    are the two places where we actually care.
      //
      //    Note upon note - we don't do this any more - instead we
      //    do a full renormalize and unstick at each local prob.
      //
      //   calculate renormalizer (the Bayesian formula's denomenator)

      if (use_chisquared)
	{
//
//	  //  Actually, for chisquared with ONE feature
//	  //  category (that being the number of weighted
//	  //  hits) we end up with not having to do
//	  //  anything here at all.  Instead, we look at
//	  //  total hits expected in a document of this
//	  //  length.
//	  //
//	  //  This actually makes sense, since the reality
//	  //  is that most texts have an expected value of
//	  //  far less than 1.0 for almost all featuess.
//	  //  and thus common chi-squared assumptions
//	  //  break down (like "need at least 5 in each
//	  //  category"!)
//
//	  float renorm;
//	  double expected;
//	  for ( icls = 0; icls < ncls; icls++)
//	    {
//	      // This is the first half of a BROKEN
//	      // chi-squared formula -
//
//	      // MeritProd =
//	      // Product (expected-observed)^2 / expected
//
//	      // Second half- when done with features, take the
//	      // featurecounth root of MeritProd.
//
//	      // Note that here the _lowest_ Merit is best fit.
//	      if (htf > 0 )
//		ptc[icls] = ptc[icls] *
//		  (1.0 + ((htf/ncls) - hits[icls])
//		   * (1.0 +(htf/ncls) - hits[icls]))
//		  / (2.0 + htf/ncls);
//
//	      // Renormalize to avoid really small
//	      //   underflow...  this is unnecessary with
//	      //   above better additive form
//
//	      renorm = 1.0;
//	      for (i = 0; i < ncls; i++)
//		renorm = renorm * ptc[i];
//	      for (i = 0; i < ncls; i++)
//		{
//		  ptc[i] = ptc[i] / renorm;
//		  fprintf (stderr, "I= %d, rn=%f, ptc[i] = %f\n",
//			   //   i, renorm,  ptc[i]);
//			   };
//
//		  //    Nota BENE: the above is not standard chi2
//		  //    here's a better one.
//		  //    Again, lowest Merit is best fit.
//		  if (htf > 0 )
//		    {
//		      expected = (htf + 0.000001) / (ncls + 1.0);
//		      ptc[i] = ptc[i] +
//			((expected - hits[i])
//			 * (expected - hits[i]))
//			/ expected;
//		    };
//		};
	}
      else     //  if not chi-squared, use Bayesian
	{
	  //   calculate local probabilities from hits
	  //

	  for (i = 0; i < ncls; i++)
	    {
	      pltc[i] = 0.5 +
		(( hits[i] - (htf - hits[i]))
		 / (LOCAL_PROB_DENOM * (htf + 1.0)));
	    };

	  //   Calculate the per-ptc renormalization numerators
	  renorm = 0.0;
	  for (i = 0; i < ncls; i++)
	    renorm = renorm + (ptc[i]*pltc[i]);

	  for (i = 0; i < ncls; i++)
	    ptc[i] = (ptc[i] * pltc[i]) / renorm;

	  //   if we have underflow (any probability == 0.0 ) then
	  //   bump the probability back up to 10^-308, or
	  //   whatever a small multiple of the minimum double
	  //   precision value is on the current platform.
	  //
	  for (i = 0; i < ncls; i++)
	    if (ptc[i] < 1000*DBL_MIN) ptc[i] = 1000 * DBL_MIN;

	  //
	  //      part 2) renormalize to sum probabilities to 1.0
	  //
	  renorm = 0.0;
	  for (i = 0; i < ncls; i++)
	    renorm = renorm + ptc[i];
	  for (i = 0; i < ncls; i++)
	    ptc[i] = ptc[i] / renorm;

	  for (i = 0; i < ncls; i++)
	    if (ptc[i] < 10*DBL_MIN) ptc[i] = 1000 * DBL_MIN;
	};


      if (crm114__internal_trace)
	{
	  for (i = 0; i < ncls; i++)
	    {
	      fprintf (stderr,
		       " poly: %d  filenum: %d, HTF: %7.0f, hits: %7.0f, Pl: %6.4e, Pc: %6.4e\n",
		       row, i, htf, hits[i], pltc[i], ptc[i]);
	    };
	};
    };

  expected = 1;
  //     Do the chi-squared computation.  This is just
  //           (expected-observed)^2  / expected.
  //     Less means a closer match.
  //
  if (use_chisquared)
    {
      double features_here, learns_here;
      double avg_features_per_doc, this_doc_relative_len;
      double actual;

      //    The next statement appears stupid, but we don't have a
      //    good way to estimate the fraction of features that
      //    will be "out of corpus".  A very *rough* guess is that
      //    about 2/3 of the learned document features will be
      //    hapaxes - that is, features not seen before, so we'll
      //    start with the 1/3 that we expect to see in the corpus
      //    as not-hapaxes.
      expected = (double)unk_features / 1.5 ;
      for (i = 0; i < ncls; i++)
	{
	  if (totalhits[i] > expected)
	    expected = totalhits[i] + 1;
	}

      for (i = 0; i < ncls; i++)
	{
	  features_here = db->cb.class[i].features;
	    learns_here = db->cb.class[i].documents;
	  avg_features_per_doc = 1.0 + features_here / ( learns_here + 1.0);
	  this_doc_relative_len = (double)unk_features / avg_features_per_doc;
	  // expected = 1 + this_doc_relative_len * avg_features_per_doc / 3.0;
	  // expected = 1 + this_doc_relative_len * avg_features_per_doc;
	  actual = totalhits[i];
	  chi2[i] = (expected - actual) * (expected - actual) / expected;
	  //     There's a real (not closed form) expression to
	  //     convert from chi2 values to probability, but it's
	  //     lame.  We'll approximate it as 2^-chi2.  Close enough
	  //     for government work.
	  ptc[i] = 1 / (pow (chi2[i], 2));
	  if (crm114__user_trace)
	    fprintf (stderr,
		     "CHI2: i: %d, feats: %lf, learns: %lf, avg fea/doc: %lf, rel_len: %lf, exp: %d, act: %lf, chi2: %lf, p: %lf\n",
		     i, features_here, learns_here,
		     avg_features_per_doc, this_doc_relative_len,
		     expected, actual, chi2[i], ptc[i] );
	};
    }

  //  One last chance to force probabilities into the non-stuck zone
  for (i = 0; i < ncls; i++)
    if (ptc[i] < 1000 * DBL_MIN) ptc[i] = 1000 * DBL_MIN;

  //   and one last renormalize for both bayes and chisquared
  renorm = 0.0;
  for (i = 0; i < ncls; i++)
    renorm = renorm + ptc[i];
  for (i = 0; i < ncls; i++)
    ptc[i] = ptc[i] / renorm;

  if (crm114__user_trace)
    {
      for (i = 0; i < ncls; i++)
	fprintf (stderr, "Probability of match for file %d: %f\n", i, ptc[i]);
    };

  // assemble result

  crm114__result_do_common(result, &db->cb, ptc);
  result->unk_features = unk_features;
  // copy per-class hits, chi2
  for (i = 0; i < ncls; i++)
    {
      result->class[i].hits = totalhits[i];
      if (use_chisquared)
	result->class[i].u.markov.chi2 = chi2[i];
    }

  return (CRM114_OK);
}


// Read/write a DB's data blocks from/to a text file.

int crm114__markov_learned_write_text_fp(const CRM114_DATABLOCK *db, FILE *fp)
{
  int b;		// block subscript (same as class subscript)
  unsigned int i;	// bucket subscript

  for (b = 0; b < db->cb.how_many_blocks; b++)
    {
      const MARKOV_FEATUREBUCKET *hashes =
	(MARKOV_FEATUREBUCKET *)(&db->data[0]
				 + db->cb.block[b].start_offset);
      const long htlen =
	db->cb.block[b].allocated_size / sizeof(MARKOV_FEATUREBUCKET);

      fprintf(fp, TN_BLOCK " %d\n", b);
      for (i = 0; i < htlen; i++)
	if (hashes[i].hash != 0)
	  fprintf(fp, "%u %u %u\n", i, hashes[i].hash, hashes[i].value);
      fprintf(fp, TN_END "\n");
    }

  return TRUE;			// writes always work, right?
}

int crm114__markov_learned_read_text_fp(CRM114_DATABLOCK **db, FILE *fp)
{
  int b;		// block subscript (same as class subscript)
  int chkb;		// block subscript read from text file

  for (b = 0; b < (*db)->cb.how_many_blocks; b++)
    {
      MARKOV_FEATUREBUCKET *hashes =
	(MARKOV_FEATUREBUCKET *)(&(*db)->data[0]
				 + (*db)->cb.block[b].start_offset);
      const long htlen =
	(*db)->cb.block[b].allocated_size / sizeof(MARKOV_FEATUREBUCKET);
      unsigned int hindex;	// bucket subscript read from text file
      MARKOV_FEATUREBUCKET tmpb;
      char line[200];

      // read and check block # line, except '\n'
      if (fscanf(fp, " " TN_BLOCK " %d", &chkb) != 1 || chkb != b)
	return FALSE;
      // read and discard the rest of the block # line
      if (fgets(line, sizeof(line), fp) == NULL
	  || line[0] != '\n')
	return FALSE;
      while (1)
	{
	  // read a line
	  if (fgets(line, sizeof(line), fp) == NULL)
	    return FALSE;
	  if (line[strlen(line) - 1] != '\n') // didn't get a whole line
	    return FALSE;
	  line[strlen(line) - 1] = '\0';    // remove \n
	  // is it an end mark?
	  if (strcmp(line, TN_END) == 0)
	    break;		// yes, done with this block
	  // not an end mark, must be bucket number and contents
	  if (sscanf(line, "%u %u %u", &hindex, &tmpb.hash, &tmpb.value) != 3)
	    return FALSE;
	  if (hindex >= htlen)	// input bucket number is past end of table
	    return FALSE;
	  hashes[hindex] = tmpb; // all OK, stuff the bucket
	}
    }

  return TRUE;
}
