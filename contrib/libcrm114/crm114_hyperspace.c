// Hyperspace classifier.  Copyright 2001-2010 William S. Yerazunis.
//
//   This file is part of the CRM114 Library.
//
// The CRM114 Library is free software: you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.
//
//   The CRM114 Library is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with the CRM114 Library.  If not, see <http://www.gnu.org/licenses/>.

//  This file is derived (er, "cargo-culted") from crm114_markov.c, so
//  some of the code may look really, really familiar.
//
//
//   This version uses multiple class blocks, rather than unifying
//   the classes together into one block.

//  include some standard files
#include "crm114_sysincludes.h"

//  include any local crm114 configuration file
#include "crm114_config.h"

//  include the crm114 data structures file
#include "crm114_structs.h"

#include "crm114_lib.h"

#include "crm114_internal.h"


//
//    How to learn Hyperspace style  - in this case, we'll include the single
//    word terms that may not strictly be necessary.
//
//
//    Note that Hyperspace is _slightly_ weird compared to the other
//    classifiers.  For one thing, it's nonlinear as heck.

CRM114_ERR crm114_learn_features_hyperspace (CRM114_DATABLOCK **db,
					     int whichclass,
					     const struct crm114_feature_row features[],
					     long featurecount)
{
  long i;

  // hash table (the class we're to learn into) and its length.
  //
  HYPERSPACE_FEATUREBUCKET *hashes;
  int microgroom;
  int nextslot;

  if (crm114__internal_trace)
    fprintf (stderr,
	     "starting learn, class %d, blocksize: %zu, used: %zu adding: %zu"
	     " bytes of features\n",
	     whichclass,
	     (*db)->cb.block[whichclass].allocated_size,
	     (*db)->cb.block[whichclass].size_used,
	     featurecount * sizeof (HYPERSPACE_FEATUREBUCKET));

    if (db == NULL || features == NULL)
    return CRM114_BADARG;

  if (whichclass < 0 || whichclass > (*db)->cb.how_many_classes - 1)
    return CRM114_BADARG;

  if ((*db)->cb.classifier_flags & CRM114_REFUTE)
    return CRM114_BADARG;

  //  GROT GROT GROT  we don't microgroom yet.
  microgroom = 0;
  if ((*db)->cb.classifier_flags & CRM114_MICROGROOM)
    {
      microgroom = 1;
      if (crm114__user_trace)
	fprintf (stderr, " enabling microgrooming.\n");
    };

  if ((*db)->cb.classifier_flags & CRM114_ERASE)
    {
      if (crm114__user_trace)
	fprintf (stderr,
		 "Sorry, ERASEing is not yet supported in hyperspace!\n");
      return (CRM114_BADARG);
    };

  //   Hyperspace "learning" (ignoring erasing) is really, really simple.
  //   Just append the features (without the feature-row numbers) to
  //   the class.  Use a 0 for a sentinel.
  //
  //   Note that crm114_learn_features() knows to always sort the incoming
  //   features, so we can just append them here without having to
  //   sort them again (and thus, within a document, the two-finger
  //   comparison will work).
  //
  //   Each of the following quantities is treated as a 32-bit unsigned int
  //   (including the zero sentinels, that mark the end of each doc):
  //
  //      feature feature  feature  feature... zero
  //
  //   GROT GROT GROT ???  For future implementation (not needed yet):
  //
  //     Erasing: finding a near-identical series and splicing it out.
  //     Do that later, not now.
  //

  //  Make sure we have enough space (features + sentinel)

  if ((*db)->cb.class[whichclass].features
      + (*db)->cb.class[whichclass].documents
      + featurecount + 1 >
      (*db)->cb.block[whichclass].allocated_size / sizeof (HYPERSPACE_FEATUREBUCKET))
    {
      size_t new_block_size;
      int errstate;
      if (crm114__user_trace)
	fprintf (stderr, "Sorry, insufficient space in db.  Must expand\n");
      //return (CRM114_CLASS_FULL);
      //
      //    Make this block big enough...
      //    We use a "double up" algorithm - that is, whenever a block
      //    is too small, we double it plus the size of the new information
      //    required to be stored.  So, on the average, a block is
      //    75% full, which is fine by us.

      new_block_size = 2 * (*db)->cb.block[whichclass].allocated_size
	+ featurecount * (int) sizeof (HYPERSPACE_FEATUREBUCKET)  ;

      errstate = crm114__resize_a_block (db, whichclass, new_block_size);
      if (errstate != CRM114_OK) return (errstate);

	
    };

  //  We have enough space, guaranteed.  So start doing our updates.
  //    note that data[0] = start of datablock

  hashes =
    (HYPERSPACE_FEATUREBUCKET *)
    (&((*db)->data[(*db)->cb.block[whichclass].start_offset]) );


  //     Append our features to the array & update feature count
  for (i = 0; i < featurecount; i++)
    {
      //  Nextslot is zero-based, which is why this next thing is NOT ...+1
      nextslot =
	(*db)->cb.class[whichclass].documents
	+ (*db)->cb.class[whichclass].features;
      hashes[nextslot].hash = features[i].feature;
      //  reserve the zero hash value as a sentinel; promote zeroes to +1
      if (hashes[nextslot].hash == 0)
	hashes[nextslot].hash = 1;
      (*db)->cb.class[whichclass].features ++;
    };

  //  put in the zero sentinel at the end and update learns count
  nextslot =
    (*db)->cb.class[whichclass].documents
    + (*db)->cb.class[whichclass].features;
  hashes[nextslot].hash = 0;
  (*db)->cb.class[whichclass].documents++;

  //   Update estimate of remaining space
  (*db)->cb.block[whichclass].size_used =
    ( (*db)->cb.class[whichclass].documents
      + (*db)->cb.class[whichclass].features)
    * (int)sizeof (HYPERSPACE_FEATUREBUCKET);


  if (crm114__internal_trace)
    {
      fprintf (stderr, "finishing learn\n");
      for (i = 0; i < (*db)->cb.how_many_classes; i++)
	fprintf (stderr, " ...class %ld, start %zu alloc %zu used %zu, learns: %d feats: %d\n",
		 i,
		 (*db)->cb.block[i].start_offset,
		 (*db)->cb.block[i].allocated_size,
		 (*db)->cb.block[i].size_used,
		 (*db)->cb.class[i].documents,
		 (*db)->cb.class[i].features);
      fprintf (stderr, " ... returning.\n");
    }

  //   All done, return OK.
  return (CRM114_OK);
}


//      How to do a Hyperspace CLASSIFY
//
//

CRM114_ERR crm114_classify_features_hyperspace(const CRM114_DATABLOCK *db,
					       const struct crm114_feature_row features[],
					       long featurecount,
					       CRM114_MATCHRESULT *result)
{

  int class = 0;    //   counter for which class we're running
  int upt, kpt;     //   unknown and known "fingers" for the two finger alg.
  int kandu, knotu, unotk;  // counters for the hyperspace distance;
  double distance;   //   featurespace distance between two documents
  double radiance;   //   total radiance of this class so far
  int hits;         //   total hits in this class so far
  HYPERSPACE_FEATUREBUCKET *ht;  // pointer to our current class known features
  int htlen;        //   how many known features in this class


  //  Sorting (done always) and uniquing (optional) are already done for us in
  //  crm114_classify_features.c, so we don't do them here.

  if (crm114__internal_trace)
    fprintf (stderr, "executing a CLASSIFY\n");

  //  GROT GROT GROT locally allocate the match result if it's not
  //  passed in?  I think so.
  if (db == NULL || features == NULL || result == NULL)
    return (CRM114_BADARG);

  //  Fill in the preamble part of the results block
  crm114__clear_copy_result (result, &db->cb);

  //  Ready to go!

  //   Big loop over all of the classes
  for (class = 0; class < db->cb.how_many_classes; class++)
    {
      //   get a pointer to the known features hash table
      ht = (HYPERSPACE_FEATUREBUCKET *) (&db->data[0]
		 + db->cb.block[class].start_offset);

      //   how many valid hashes + sentinels are there here?
      htlen =
	db->cb.class[class].documents
	+ db->cb.class[class].features;

      //   prepare to do two-finger on this class;
      upt = 0;   // the unknown document finger
      kpt = 0;   // the known document finger
      kandu = 0;
      knotu = unotk = 1;  // to make a perfect match still be finite.  WAS 10
                          // in previous versions of CRM114

      radiance = 0;
      hits = 0;
      if (crm114__internal_trace)
	fprintf (stderr,
	       "\nStarting class %d (%ld, %ld) knowns: %d unknowns: %ld\n",
	       class,
	       (long int)ht,
	       (long int) db->cb.block[class].allocated_size,
	       htlen, featurecount);

      while (kpt < htlen)
	{
	  // are we done with this known document?
	  if (ht[kpt].hash == 0 || upt >= featurecount)
	    {
	      // we're done two-fingering this doc, add up the radiance
	      distance = knotu + unotk;
	      radiance += (kandu * kandu) / distance;
	      //radiance += 10*FLT_MIN;   GROT GROT GROT radiance == 0 exactly?
	      // FLT instead of DBL because result struct radiance is float
	      hits += kandu;
	      if (crm114__internal_trace)
		fprintf (stderr, "Ending with kandu: %d  misses: %f\n",
		       kandu, distance);
	      //  and reset for the next document
	      kandu = 0;
	      knotu = unotk = 1; // change from 10 (previous version of CRM114)
	      kpt++; upt = 0;
	    }
	  else
	    {
	      // do a step of the two-finger algorithm
	      if (ht[kpt].hash > features[upt].feature)   // uknown is smaller
		{
		  unotk++;
		  upt++;
		}
	      else if (ht[kpt].hash < features[upt].feature)// known is smaller
		{
		  knotu++;
		  kpt++;
		}
	      else    // exact match
		{
		  kandu++;
		  upt++;
		  kpt++;
		};
	      if (upt >= featurecount) // end of unknown- skip to a known zero!
		{
		  while ( (kpt < htlen) && (ht[kpt].hash != 0))
		    {
		      kpt++; knotu++;
		      // fprintf (stderr, "s");
		    };
		}
	      //   end of two-finger algorithm
	    };
	};

      //  Done with this class, so now stuff this class's results into
      //  the results array.
      result->class[class].hits = hits;
      result->class[class].u.hyperspace.radiance = (float)radiance;
    };

  //   Build the output result!
  //
  //   Now all of the classes have been evaluated; we can calculate
  //   probability (which is normalized radiance) and then pR.
  //   Then we can get overall pR, as well as best match index
  {
    double tradiance;

    //  Set the unk_features to the # of incoming features.
    result->unk_features = (int)featurecount;

    //    get the normalization factor tradiance
    tradiance = 0.0;
    for (class = 0; class < db->cb.how_many_classes; class++)
      {
	tradiance += result->class[class].u.hyperspace.radiance;
      }

    //  If tradiance is zero, we get a DIVZERO problem... so
    //  we do a hacky fix here.
    if (tradiance < 1e-20) tradiance = 1e-20;

    //   calculate each class's normalized probability
    for (class = 0; class < db->cb.how_many_classes; class++)
      {
	result->class[class].prob =
	  result->class[class].u.hyperspace.radiance / tradiance;
      };

    // calc and set per-class pR, tsprob, overall_pR, bestmatch
    crm114__result_pR_outcome(result);
  };

  return (CRM114_OK);
}


// Read/write a DB's data blocks from/to a text file.

int crm114__hyperspace_learned_write_text_fp(const CRM114_DATABLOCK *db,
					     FILE *fp)
{
  int b;		// block subscript (same as class subscript)
  int i;		// feature/sentinel subscript

  for (b = 0; b < db->cb.how_many_blocks; b++)
    {
      HYPERSPACE_FEATUREBUCKET *ht; // pointer to our current class known features
      int htlen;        //   how many known features in this class

      //   get a pointer to the known features hash table
      ht = (HYPERSPACE_FEATUREBUCKET *) (&db->data[0]
					 + db->cb.block[b].start_offset);
      //   how many valid hashes + sentinels are here?
      htlen =
	db->cb.class[b].documents
	+ db->cb.class[b].features;

      fprintf(fp, TN_BLOCK " %d\n", b);
      for (i = 0; i < htlen; i++)
	fprintf(fp, "%u\n", ht[i].hash);
    }

  return TRUE;			// writes always work, right?
}

// Doesn't realloc() *db.  Shouldn't need to...
int crm114__hyperspace_learned_read_text_fp(CRM114_DATABLOCK **db, FILE *fp)
{
  int b;		// block subscript (same as class subscript)
  int chkb;		// block subscript read from text file
  int i;		// feature/sentinel subscript

  for (b = 0; b < (*db)->cb.how_many_blocks; b++)
    {
      HYPERSPACE_FEATUREBUCKET *ht; // pointer to our current class known features
      int htlen;        //   how many known features in this class

      //   get a pointer to the known features hash table
      ht = (HYPERSPACE_FEATUREBUCKET *) (&(*db)->data[0]
					 + (*db)->cb.block[b].start_offset);
      //   how many valid hashes + sentinels are here?
      htlen =
	(*db)->cb.class[b].documents
	+ (*db)->cb.class[b].features;

      if (fscanf(fp, " " TN_BLOCK " %d", &chkb) != 1 || chkb != b)
	return FALSE;
      for (i = 0; i < htlen; i++)
	if (fscanf(fp, " %u", &ht[i].hash) != 1)
	  return FALSE;
    }

  return TRUE;
}
