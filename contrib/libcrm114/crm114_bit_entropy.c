//	crm114_bit_entropy.c - entropy classification utilities

// Copyright 2001-2010 William S. Yerazunis.
//
//   This file is part of the CRM114 Library.
//
//   The CRM114 Library is free software: you can redistribute it and/or modify
//   it under the terms of the GNU Lesser General Public License as published 
//   by the Free Software Foundation, either version 3 of the License, or
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

// crm114 library configuration
#include "crm114_config.h"

// crm114 library structures, etc.
#include "crm114_structs.h"

#include "crm114_lib.h"

#include "crm114_internal.h"


//////////////////////////////////////////////////////////////////
//
//            Entropic Classification Basics
//
//    This is an implementation of an entropic bitwise lattice Markov
//    classifier.  This is very roughly based on some ideas in the DMC
//    (Dynamic Markov Compressor) data compression technique of Gordon
//    Cormack et al and inspired by Andrej Bratko's use of it in spam
//    filtering ( Gordon originally proposed DMC for compression and
//    then Andrej used it for spam filtering.  Matthew Young-Lai
//    then proposed a technique for merging nodes which is limited
//    to re-merging the "cloned" nodes in Gordon's work.  Our merging
//    method here has no such limitation.)
//
//    This particular implementation is entirely separate, similar in
//    tht it uses a Markov chain, but does not use cloning.  Rather it
//    uses cross- linking, in a minimum-entropy format, and supports
//    "open-ended" Markov chains, rather than the toroidal lattices
//    required in Cormack's DMC.
//
//    For those of you into references as "closest prior", see:
//    1) Cormack, Gordon V. et al, Data Compression using Dynamic Markov
//       Modeling (Computer Journal #6, 1987
//    2) Bratko, Andrej, Markov Modeling for Spam Filtering (?) TREC
//       2005
//    3) Matthew Young-Lai,  Adding state merging to the DMC data
//       compression algorithm, Information Processing Letters 70
//       (1999) 223-228.
//
//
//    The basic concept is to construct a Markov chain that describes
//    the sequential bit pattern of a message, and then use that chain
//    to compress the incoming unknown text; the smaller the entropy
//    the better the quality of match.  Yes, this sounds like
//    a horrible idea, but Andrej Bratko at TREC 2005 has shown that
//    it does work.  One advantage is that it can work on files that
//    have no obvious tokenizations, like JPGs.
//
//    Like every other Markov chain, there's a start node, and then
//    for each observed 1 or 0 there's an observed freqency of 1 or 0,
//    and the associated next node for a 1 or 0.  The quality of match
//    is just the sums of the entropy (*) of each path taken.
//
//    More correctly, entropy is the weighted sum for an ensemble of
//    signals, and is -Psignal * log2 (Psignal), and the sum over an
//    ensemble of possible texts yields the entropy which is the
//    average bit rate.  Whichever Markov chain shows less entropy
//    is the one that matches best.  However, in this case we're
//    only given one example, which is the text in consideration,
//    and so the probability of each transition is 1.0000 exactly
//    (as we have only a single sample of the unknown).  Thus, we
//    actually sum the bits required; that is, log2 (Psig).
//
//    Nota Bene 1: in order to make this code a little bit more
//    general, we have each node accept an alphabet of N different
//    symbols, where N defaults to 2 (that is, bitwise) but we'll
//    write the code to be more general than that; it's a compile-time
//    change rather than a hack-the-code change.  States *are* always
//    numbered starting at zero, though, so consider the alphabet to
//    always be an ENUM.
//
//    Nota Bene 2: If you happen to be familiar with the other CRM114
//    classifiers, note that this classifier does NOT use the tokenizer
//    regex system, nor the SBPH or the OSB feature set.  Instead,
//    all that state information is captured by where you are in the
//    Markov chain, and the concept of "token" is replaced by the
//    concept of "alphabet".  By default, right now the "alphabet"
//    is single bits.
//
//    Nota Bene 3: As currently designed, the learning algorithm
//    defaults to initialize with a toroidal perfect shuffle- thus the
//    default model always takes constant memory and you can't run off
//    the "edge" of the model.  We still need to fix it so that
//    "crosslink" will allow a jump to another more appropriate (lower
//    entropy) part of the braid; that's not there yet.
//
//    Nota Bene 4: As the toroidal shuffle didn't give extremely good
//    results, there's also a "unique state" classifier, set by the
//    flag < unique >.  This generates a *loopless* chain where each
//    text learned produces a single linear chain; this means we
//    follow whatever model has already had the same bit pattern, but
//    whenever we diverge, we start allocating new bit nodes
//    immediately.  This works decently but uses up a lot of nodespace
//    quickly.  It also means that you can run off the "edge" of the
//    model because it's not closed like the toroidal shuffle.  By
//    default, running off the edge causes a restart at the START node
//    (node 1, as it turns out; node 0 is a
//    bookkeeping/anchor/sentinel node).  This works, but doesn't give
//    very high accuracy as the reSTARTs are not time-aligned with the
//    model; they might even occur in the middle of a byte.
//
//    Nota Bene 5: In the quest for better accuracy, instead of doing
//    a reSTART when we're classifying and fall off the edge of a
//    'unique' model, we branch to a node that has a very similar
//    prior history.  How this is done is seen below but it boils down
//    to finding a node that already exists that has had a very
//    similar prior history bit pattern.  This can actually be done in
//    near-constant (and pretty small!) time; the magic to do this is
//    all described below.
//
//    Nota Bene 6: In the quest for smaller (and faster!) models and
//    the ability to not run out of nodes, we have the <crosslink>
//    flag, which allows the LEARNing process to do the same branching
//    that is done when we run off the end of the model chain during
//    classification.  More specifically, before allocating a new
//    node, we look for the closest prior history in already-allocated
//    nodes, and if it's "close enough" (as set by a threshold), we
//    make that the next node in our chain.  This produces Markov
//    models with both acyclic joins and repeating loops, and
//    effectively prevents the model from growing without bound, as
//    the model reuses nodes quite effectively (sometimes more than a
//    300:1 reuse!).  It also produces a model that is quite accurate
//    when trained with SSTTT.  Although we can argue that ROCAC isn't
//    the best possible figure of merit, SSTTT can yield very good
//    ROCAC values (0.03 per cent); the terminal error rate on the
//    TREC06 public corpus can be as low as 3 errors per 10,000 (that
//    is, three errors in the final ten thousand messages, of the
//    92Kmsgs corpus.)
//


///////////////////////////////////////////////////////////////////
//
//         Algorithm for Classification:
//
//    Follow the Markov chain from START with total entropy of 0 to
//    begin, then walk the Markov chain taking each branch as
//    instructed by the successive bits of incoming text.  Along the
//    path, accumulate the compressed size as defined by - log2 (Pt)
//    for the current transition.  Here, we define probability for a
//    transition Pt = Nt + c / (Nt + N~t + 2c).  Note that for c = 0
//    this can yield probabilities == 0.00 exactly, which will yield
//    an infinite entropy (log of zero is... not a good thing).
//    Gordon Cormack's paper indicates a "small" value of c is good
//    for shorter texts; unfortunately there's no mention of what
//    "small" means.
//
//    If you run off the end of the chain (some learning algorithms
//    generate terminating chains, others always generate loops) then
//    do one of : (1) stop accumulating entropy; you can't go any
//    further; (2) go back to the START node and keep accumulating, or
//    (3) jump to the node with the closest prior history and keep
//    accumulating.
//
//    Do this on each bitwise markov file; the file which accumulates
//    the lowest compressed size is the best matching file.
//

///////////////////////////////////////////////////////////////////
//
//          Algorithm for Learning - Overview
//
//    Before you learn in the first text, initialize the data
//    structure with a START node with NULL exit nodes for each of the
//    possible exits.  If you want, you can put something bigger or
//    more complex in, but it's not necessary.  If you're running with
//    constant storage, allocate the rest of the storage, preformat
//    them as a linked list of free nodes, and off you go.
//
//    Follow the chain, incrementing "seen" counts as you go.  If
//    you're learning a file, and you end up at a node exit that's
//    a NULL (meaning no node is yet linked in for the current bit
//    pattern) then grab a node off the free list (or malloc one).
//    See below under "advanced methods" for how to get more
//    nodes by merging very similar nodes together.
//
//    Whenever a node is seen with both a sufficiently large number of
//    seens, and a sufficiently balanced number of differing symbols,
//    "clone" the node so that the Markov chain branches on the
//    previous node.  This is to split the chain where the current
//    state is insufficient to model the underlying process.  This
//    only can work if the Markov model somehow contains a merge;
//    otherwise each node can only be reached from a single other node
//    (and so on, all the way back to the START node).
//
//    In Cormack's version, when you run out of free nodes, you must
//    reinitialize the storage and rebuild the model (using only the
//    last part of the corpus data), but there are ways to merge
//    similar nodes (see "advanced methods" below) to get more free
//    nodes without throwing away data.
//
//    Note - there is a "classical" method for building a hidden
//    Markov model, but it's O(n^2) in the number of nodes in the
//    output model!  It basically uses hillclimbing techniques to form
//    the convex hull and then climbing to the local top of the hull
//    repeatedly.  That's fine for small HMMs but here, when we may
//    have a million nodes, that's way too slow.
//
//       Method 1 - for both initial-loop or terminated Markov chains
//
//        Follow the Markov chain from START, incrementing the seen-0
//        and seen-1 bits of the Markov chain.  When you hit a 0 or 1
//        choice pointing to NULL, grab an unused node off of the free
//        list.  If the free list is empty, use an advanced method to
//        get more.
//

////////////////////////////////////////////////////////////////////
//
//       Advanced Method - Cloning a Node
//
//    If a node has more than one infeeder, there is always the
//    possibility that the node really should be two nodes.  This
//    could happen either because of a prior merge, a prior clone, or
//    because the initial node setup had a loop in it (which it may or
//    may not; it can work both ways.)
//
//    A node should be cloned when there is a sufficient number of
//    incoming events to be statistically significant, and the
//    outgoing events are sufficiently balanced (i.e. the current node
//    is of sufficiently low predictive value - that is, it's high
//    entropy no matter what you do ) to lend credence to the idea
//    that this one node really represents two states and splitting
//    the node is a good idea.
//
//    Of course, there's no reason (or way!) to clone a node that has
//    only one infeeder even if the node state itself has low
//    predictive power; in this case what's really happening is that
//    the real corpus' hidden Markov model is diverging at a prior
//    node.  There simply isn't any predictive state available.
//
//    Topic for further work - given only a single infeeder, maybe we
//    should back propagate the "split" up the chain of single priors
//    until a state is found with at least two infeeders, and clone
//    *that* node and all successor nodes down to the current node.
//    Note that if we hit the START node we don't do this as there's
//    no way to clone START.
//
//    The simple and obvious way to clone a node of low predictive
//    power is to keep track of the previous node while doing a
//    training traverse, and only clone off the path from the previous
//    node.  This means creating a new node with the same successor
//    nodes as the current node (or not; it depends on whether the
//    current node has a successor or is "end of the line"), and has
//    only a single infeeder, which we know is at best a single line
//    of prior states of arbitrary length, and at worst a series of
//    nodes that all looked similar at some time in the past.
//


//////////////////////////////////////////////////////////////////
//
//           Advanced methods - getting free nodes by merging nodes
//
//  NOTA BENE: the code to implement merging is not debugged because the
//   open-graph training with crosslinking works so well.  Consider
//    this stuff more as "design notes for the future".
//
//
//    merging nodes - in order to represent loops in the markov model,
//    it's sometimes necessary to loop back into prior Markov states.
//    Finding the appropriate prior state can be a hassle.  Cormack and
//    Lai both postulate a "way" to find the appropriate prior state,
//    but don't describe an efficient way to do so.
//
//    Here's a shortcut, using arithmetic to get an infinite impulse
//    response out of a floating point number; the already established
//    node with the closest floating point number to the current
//    generated floating point number is the "closest node".  This is
//    a different version of "arithmetical coding", but it can be
//    lossy (in a controlled way).  See the Guazzo coding method in
//    Cormack's paper - note that we do NOT use Guazzo coding for
//    generating output but rather just for quickly finding the node
//    whth the closest prior state for us to jump to when we run
//    off the edge of a model.
//
//    Clearly, one could weight the prior bit by a factor of 1/2
//    (really 1/(2^1)), and weight the prior float by 1 - (1/2) in
//    which case the prior float's mantissa is just the last N bits,
//    in reverse order.  Unfortunately, this also gaurantees that
//    prior history utterly disappears as soon as it rolls out of the
//    number of bits in the matissa of the local floating point
//    operation.
//
//    More interesting is larger values of the exponent; for example a
//    value of 2 yields a half-bit fractional representation, so an
//    error of 1 bit seen N steps in the past is overrideable by two
//    correct bits at N+1 and N+2 bits in the past.  For example, the
//    with an exponent of 2, the most recent bit has significance
//    1/4 = 0.250, while the second-most-recent is 3/4 * 1/4 = 3/16 =
//    .1875, and the bit before that is 3/4 * 3/16 = 9/64 = .04687,
//    and so on.
//
//    Higher exponents just stretch out this series even further.
//    There is no need to assume only an integer exponent.  Indeed,
//    it's only because of the finite precision of the computer that
//    this a finite impulse response (FIR) filter; infinite precision
//    would give infinite impulse response and "total recall" of prior
//    states in the Markov process - but also the ability for some
//    long-ago series of perfectly correct steps to blow away the more
//    recent entropy.  For this reason, we usually use an exponent of
//    1, which yields a prior bit weight of 1/2 (whch is just the
//    floating-point mantissa as described above).  
//
//    Values of the exponent less than one yield a situation where the
//    range of possible outcome FIR numbers becomes discontinuous;
//    that is, significant chunks of the range become unreachable by
//    any possible prior bit series.  Consider the FIR value 0.5000
//    with exponent of 0.5.  The most recent bit then has weighting of
//    0.70711, with only .29289 of weight available for all following
//    bits.  The maximum value achieveable for a pattern 0111111.. is
//    then .29289, while the minimum value achieveable for pattern
//    100000... is 0.70711, so the range of possible FIR numbers does
//    not contain any value between .29289 and .70711.  Recursively,
//    similar subranges cease to be accessible as well.
//
//    The exponent for "exact ambivalence" between an error in state N
//    versus two errors in state N-1 and N-2 is around 1.385; the
//    exponent for ambivalence between an error in state N versus
//    errors in states N-1, N-2, and N-3 is about 1.13 .  Note that both of
//    these numbers yield rather short "memories" in the FIR filter
//    before the Nth prior bit underflows and is lost completely.
//
//    By using the FIR coding of prior state, we need only keep a
//    table of the FIR path floating point number generated on the
//    path that first generated this node (which can be sorted,
//    yielding a _very_ fast node merging capability; just make one
//    pass through the sorted table looking for the two closest
//    floating point numbers.)  Those close pairings of nodes are good
//    merge candidates.
//
//    It's probably worth checking these merge candidates to see if
//    they have similar local probabilities - that is, if they have similar
//    0-counts and 1-counts.  If one node goes 0 mostly and the other
//    goes 1 mostly, then there's a good chance that this isn't a good
//    merge.  How to weight FIR prior history match versus the local
//    outbound probability is a good question.
//
//    Another merge candidate is to look at nodes which have identical
//    0 or 1 destinations.  However, finding this requires either an
//    extensible list of back pointers or brute-force search (albeit
//    over a very small subset of the entire Marov Chain).
//
//    If we're running in <unique> mode (that is, nodes are allocated
//    on each unique path, and no node has ancestors with any
//    possibility of significantly deviating FIR values, We can also
//    back-calculate the approximate "feeder" node FIR values that
//    could lead to a particular node, given the decreasing exponent.
//    For example, if the current node's FIR is .3700000, then it can
//    only have been reached by a 0-branch from a node whose prior was
//    (2 * .3700000 = .7400000 ) plus or minus any crosslink-error
//    threshold (which is typically under 1 in a million or less).
//    So, we can just use our FIR lookaside table to find all possible
//    ancestors to this node.
//
//    When merging two nodes, we (speaking in the 0/1 tense; larger
//    alphabets are an obvious extension) we perform the following steps:
//
//    * update the to-be-kept node with the sums of the outbound 0-seen
//      and 1-seen counts.  Without any loss, we can choose the
//      to-be-kept node is the node with the lower index number.
//    * we keep whichever output chain had the highest counts of each
//      of the 0-seen and 1-seen categories
//    * we set the FIR prior history to the weighted average of the
//      two prior weighted averages
//    * for the two output nodes which were _not_ kept (a 0-chain and
//      a 1-chain) mark these chains for eligibility for garbage
//      if we choose active collection;
//    * put the merged-away node on the free list.
//    * make an update pass thorugh the nodeset; any node outlet pointing to
//      the merged-away node is reset to point to the to-be-kept node.
//
//    We may choose to merge more than one node when we encounter
//    an empty free list.  Each merge gives us one node for sure, plus
//    the possibility of many others.
//
//    At some point, a garbage collection is necessary to reclaim nodes
//    that no longer have a prior node.  We can do this either by:
//    * reference counts - each node can keep an accurate reference count
//      of nodes that can transfer to this node.  Since there are only
//      three events that can cause the reference count to change (those
//      being "allocate from free list == refcount = 1", "clone node
//      refcount = split incomings, and  "merge with another node
//      == refcount1 + refcount2" then the refcounts can be maintained at
//      relatively low cost.
//    * we can run a "classic" mark and sweep.  However, there's a
//      better way...
//    * we could add back pointers in each node.  Unfortunately, that
//      means the size of a node could grow rather large.  There is
//      still a better way....
//    * We could add chaining pointers on the _output_ of each seen
//      slot.  The "to" node gets a single slot added, to point to
//      the first node that can transfer in; each such node points
//      to the next node that can also transfer to the target node.
//      Preferably, the insertions are done in order so the list
//      remains sorted.
//
//    When learning a new text, we also compute the running FIR prior
//    floating point value, and on each node we encounter in our path,
//    we update the local prior state FIR value according to the
//    weighted average of the new text (with weight 1) and the prior
//    texts (with weight = total times this node has been visited in
//    the past = sum of the 0-seen and 1-seen output values ).  For
//    this reason, node FIR prior encodings may change so it's necessary
//    for the sorted FIR vector to contain not just the FIR values, but
//    also backpointers to the FIR nodes that held them.
//
//     Improvement: rather than a separate sorted list of FIR values,
//     embed the FIR value in the cell, along with the "next less"
//     and "next greater" indices.  Thus, each cell contains within it
//     pointers to it's most likely merge candidates.
//
//     Secondary improvement - when allocating a new node from the
//     free list, how does one efficiently link the new node into the
//     next-less and next-greater lists?  Sequential search clearly
//     works, but is inefficient.  Clearly a binary (or near-binary)
//     tree would assist this but it's a programming hassle to
//     maintain the two data structures in synchrony.
//
//     Alternatively, a fixed-size table of nodes at or close to
//     various "thresholds" can be maintained; when a node changes
//     it's FIR value, it only takes constant time to round the FIR to
//     a table entry value and check to see if the new value is closer
//     to the "perfect" value of FIR for that table entry versus the
//     node that is currently in that slot.  If it's better (not just
//     as good, "better", to minimize thrashing) then the new node
//     index is written into the table; otherwise the old node stays.
//
//     Note that nothing prevents a node from being in the table more
//     than once, nor is there any correctness "need" for the table to
//     be exactly right.  Instead, the lookaside table is just a
//     speedup that helps a lot most of the time.  When a node changes
//     value, it will move (some) so incorrectness is part of the
//     game.  If you really want, you can move the node to a new
//     lookaside table slot, and then look at the previous next-less
//     and next-greater nodes, and choose one of them to go into
//     the table, replacing the (now moved) node.  (NB: in implementation,
//     it turns out that the FIR lookaside table is *critical* for
//     reasonable performance; any bug in that code that causes
//     weak inital guesses will slow the classifier down by a factor
//     of 10 or more.
//

////////////////////////////////////////////////////////////////////////
//
//    Futher Hacks and Improvements: resuming a CLASSIFY past NULL.
//
//    If the classifier runs off the end of the known Markov chains,
//    what do we do?  Just drop the following text?  Possibly - but
//    there is an alternative.
//
//    We can use the FIR arithmetical coding to resume the Markov
//    chain at a near-optimal position.  The difference between where
//    we are and where we resume should be as small as possible;
//    fortunately the FIR-next-least and FIR-next-greater indices in the
//    current node give the two nearest side-steps at the current Markov
//    node; we can then explore up and down the FIR chain to find a
//    preexisting node that actually has a further non-NULL chain.
//    The entropy increase due to this side-step outside the Markov
//    chain is not entirely computable from the FIR values, because
//    the prior information that's already rolled off the least significant
//    bit of the FIR calculation.
//
//    (note: this is currently the default action)

////////////////////////////////////////////////////////////////////
//
//        IMPLEMENTATION AND STORAGE DETAILS
//
//    Data storage format for bitwise Markov classifiers.  Note that
//    we always use indices rather than pointers as we want the data
//    structure to be stored and reloaded from disk via mmap.
//
//    We have several "global static" values, which are stored in
//    the meta-start node which is always node 0:
//
//      * the index of the START node (should always be 1) - stored
//        in alphabet-zero slot.
//
//      * the index of the node with the lowest FIR value - node 0 is
//      * always "lower than the lowest"
//
//      * the index of the node with the highest FIR value - node 0 is
//      * also always the node "higher than the highest".
//
//    If we have a FIR lookaside speedup table, that also needs to
//    have space allocated.
//
//    Each normal node contains a header containing:
//
//      * the floating-point FIR value that codes the prior states
//
//      * the index to the node with the next larger FIR value
//
//      * the index to the node with the next smallest FIR value
//
//    These FIR prior values are updated whenever a node is
//    visited during learning.  This may mean minor motion up and
//    down the FIR chain.
//
//    Each node also contains BITMARKOV_ALPHABET_SIZE slots; each slot
//    contains the following (so there's one such slot for each
//    member of the alphabet A).  There are two kinds of data stored in
//    each slot; they are related but not interdependent.  The forward
//    markov slot data is used during classification; the backward slots
//    are used only during maintenance.
//
//     Forward Markov slots:
//
//      * how many times this alphabet member was seen as the input at
//        this node.
//
//      * when it's seen, what the index of the next node to transition to
//        is (index, not pointer, for position-independent code)
//
//     Backward slots for maintenance (clone and merge)
//
//      * index of the first incoming node that can transfer to this
//        node when that prior node saw this member of the alphabet A
//        (note that this is NOT the alphabet member that the current
//        node is seeing.  This just gives an array of
//        backlinks for each member of the alphabet).  NULL if there's
//        no known encounters of an incoming with the most recent prior
//        for this alphabet member.
//
//      * index iof the _next_ incoming node that can transfer to the
//        same outgoing node as this node.  In combination with the
//        prior index above, this forms a linked list of all of the
//        nodes that can reach a particular node.  As we don't need
//        to traverse this list in any particular order, it's strictly
//        a "stack" (newest added at the front).
//
//
//    Using 64-bit storage for the floating-point FIR values and
//    for the visit counts, and 32-bit storage for all indices, then
//    assuming we're using a binary alphabet, we have:
//
//    Per-cell
//    FIR prior float = 8 bytes
//    FIR up and down links - 2 * 4 bytes = 8 bytes
//    Slot within cell
//    forward count - 8 bytes * 2  = 16 bytes
//    forward index - 4 bytes * 2  = 8 bytes
//    backward first - 4 bytes * 2 = 8 bytes
//    backward next - 4 bytes * 2 = 8 bytes
//
//    Total - 56 bytes storage per nodem, 18K nodes per megabyte.
//    Note that the absolute minimum size is 24 bytes/node with no
//    advanced features, so we're about a factor of two worse in
//    space, and for that we get near-linear in size for both learning
//    and classification.
//
//    If we add in a 0.1% overhead size for the FIR prior lookaside
//    accelleration table (containing 1 index per 4-byte word) then
//    for every 4 bytes (1 index) of FIR prior lookaside there are
//    4Kbytes (about 73) FIR value cells, so with the FIR prior
//    lookaside and an assumption of roughly uniform FIR prior values,
//    we need to traverse an average of 1/4 of 73 or about 18 markov
//    chain cells.  With a 1.0% overhead (1 byte per 100 bytes, or 1
//    index slot per 400 bytes) then there is 1 index slot per 7
//    markov chain cells, and (again assuming a uniform distribution
//    of FIR priors) an average of just 1.8 markov chain cells visited
//    per FIR lookup.
//
//    If the distribution of FIR values is too nonuniform, an adaptive
//    grid method may be used to smooth it out.  But we'll climb that
//    mountain when we come to it.
//
//    Note to self: don't use the mathlib LOG function!
//    It's far too slow.  We'll custom craft our own that uses
//    linear interpolation over a lookup table.
//

///////////////////////////////////////////////////////////////////
//
//     An interesting information-theoretic philosophical point - if
//     we only have a million nodes in the database, then at most we
//     encode 20 bits worth of state.  A 500-megabyte file like Andrej
//     uses would have at most 62 million nodes (12 bytes/node not
//     with short counts), or 41 million nodes = 27 bits to define
//     state.  Since the FIR-prior value encodes at least 64 bits, can
//     go to 96 bits (GCC 4.0 "long double") and possibly 128 (if your
//     compiler supports "-mlong-double-128") shouldn't we be sticking
//     with the FIR-prior and simply *ignoring* the actual links, as
//     they (and the state nodes) encode far less information?
//
//     Submitted as an "ultimate" pare-down - the memory contains
//     ONLY the times-previously-seen information.  It's a big array,
//     indexed by the most significant N bits of the prior, with the
//     high-order bit being the transition now being executed.  All
//     that's there in the array is the number of times this transition
//     has been executed in LEARN mode; everything else is embedded
//     in the state (which _is_ the index).
//
//     That optimization would concieveably allow one to write an
//     embedded form of bit-entropic classification in as little as
//     two to four megabytes of space per class; in short, something
//     that would fit on an embedded system.
//


//////////////////////////////////////////////////////////////////
//
//       File Structure and Layout for Bit Entropy
//
//   The file starts with a header block, of size 4 kybtes; the first
//   four longwords give the indexes (in longwords, for 32-bit alignment)
//   of the start and length of the FIR lookaside table and the node list.
//


/////////////////////////////////////////////////////////////////
//
//    **** NOTE - THIS IS ALL UNNECESSARY AND DEPRECATED IN THE CURRENT CODE!
//
//        Per-node memory layout
//
//    We have three pointers in the node slot; each of these is replicated
//    for each member of the alphabet in the Markov graph.
//    *  The .nextcell says what the next cell is when we see this alph
//    *  The .backfirst cell says what the first cell is that could
//          get us here when it saw an alph
//    *  The .samenext cell says what the next cell that also gets you
//          you to the same nextcell when you see the same alph.
//
//    Yes, this is slightly redundant, but it's very fast.  (but note the
//    approximation above that would save us all this memory!)
//
//    A diagram here helps to understand the slotting structure
//
//    node
//     .nextcell   >------------------->--> node
//     .backfirst <---\     />------->/        .nextcell >---------->
//     .samenext -|    \---------------------< .backfirst
//                |       /
//     node     <-|      /
//       .nextcell  >---/
//
//
//     Note- "backfirst" and "samenext" are redundant given the FIRlat
//     system; we can always find referrers to the current node by
//     looking at the possible origins - a node of FIR value 0.260 can
//     only come from the alpha link of a node of FIR value 0.520 ; a
//     node of value 0.779 can only have come from the 1-link of a FIR
//     of .558 so we can just search the FIR around there.



// Functions to calculate class data size, number of nodes, number of
// firlat entries.

// fixed-size part of bit entropy class data
// the 1024 is just some padding
#define FIXED_HEADER (ENTROPY_RESERVED_HEADER_LEN * sizeof(long) + 1024)

// minimum number of nodes
#define MIN_NODES 4

// a nodeslen is the number of nodes in a class
// a firlatlen is the number of firlat entries in a class

static size_t calc_size(int nodeslen, int firlatlen)
{
  size_t nodesbytes;
  size_t firlatbytes;
  size_t class_size;

  nodesbytes = nodeslen * (sizeof (ENTROPY_FEATUREBUCKET));
  firlatbytes = firlatlen * sizeof (long);
  class_size = FIXED_HEADER + firlatbytes + nodesbytes;

  return class_size;
}

static int flen(int nodeslen)
{
  return (int)(nodeslen * BIT_ENTROPIC_FIR_LOOKASIDE_FRACTION);
}

size_t crm114__bit_entropy_default_class_size(const CRM114_CONTROLBLOCK *p_cb)
{
  int nodeslen;
  int firlatlen;

  nodeslen = ((p_cb->classifier_flags & CRM114_UNIQUE)
	      ? DEFAULT_BIT_ENTROPY_FILE_LENGTH
	      // enough for standard toroid, plus padding
	      : (BIT_ENTROPIC_SHUFFLE_HEIGHT * BIT_ENTROPIC_SHUFFLE_WIDTH)
	      + 1000);
  firlatlen = flen(nodeslen);
  return calc_size(nodeslen, firlatlen);
}

// Given a pre-allocated block of some size for class data, how many
// nodes and firlat entries can fit in it, leaving room for header and
// padding?
//
// n     nodes
// f     firlat entries
// N     sizeof(bucket) -- size of node
// F     sizeof(long) -- size of firlat entry
// h     header size (bytes)
// z     1 / lookaside_fraction
// s     total size (bytes)
//
//                          s = h + Nn + Ff + 1024
//               s - 1024 - h = Nn + Ff
//                            = Nn + F(n/z)
//                            = Nn + (F/z)n
//                            = (N + F/z)n
// (s - 1024 - h) / (N + F/z) = n
//
// Returns true for success, false for class too small.
// Minimum number of nodes is MIN_NODES.

static int bit_entropy_class_size_to_lengths(size_t class_size,
					     int *p_nodeslen,
					     int *p_firlatlen)
{
  double recip;		// reciprocal of lookaside fraction, z above
  double divisor;	// N + F/z -- see algebra above
  int nodeslen;	// return value
  int firlatlen;	// return value
  size_t size;		// size calculated from nodeslen and firlatlen
  size_t n1;		// temp: proposed next nodeslen
  size_t f1;		// temp: proposed next firlatlen
  size_t size1;		// temp: proposed next class size
  int ret;

  if (class_size >= FIXED_HEADER)	// avoid underflow
    {
      recip = 1.0 / (double)BIT_ENTROPIC_FIR_LOOKASIDE_FRACTION;
      divisor =  sizeof(ENTROPY_FEATUREBUCKET) + sizeof(long) / recip;
      nodeslen = (class_size - FIXED_HEADER) / divisor; // Presto! Approximately.
      firlatlen = flen(nodeslen);

      // correct any floating-point slop
      size = calc_size(nodeslen, firlatlen);
      if (size < class_size)
	while (1)
	  {
	    n1 = nodeslen + 1;
	    f1 = flen(n1);
	    size1 = calc_size(n1, f1);
	    // don't go too far, and don't wrap around integer type
	    if ( !(size1 <= class_size && size1 > size))
	      break;
	    nodeslen = n1;
	    firlatlen = f1;
	    size = size1;
	  }
      else
	// Asymmetric.  Whether adjust up or down, must end up <= class_size.
	while (nodeslen > MIN_NODES && size > class_size)
	  {
	    nodeslen--;
	    firlatlen = flen(nodeslen);
	    size = calc_size(nodeslen, firlatlen);
	  }

      *p_nodeslen = nodeslen;
      *p_firlatlen = firlatlen;
      ret = (nodeslen >= MIN_NODES && size <= class_size);
    }
  else
    ret = FALSE;

  return ret;
}

//
//
//
//    Helper function to calculate entropy from a probability
//
//    We speed this up with a lookup table for relatively small numbers
#define ENT_CACHE_MAXCOUNT 256
#define ENT_CACHE_MAXTOTAL 256
static double stats_2_entropy (long count, long total)
{
  static long init = 0;
  static double loglookups[ENT_CACHE_MAXCOUNT][ENT_CACHE_MAXTOTAL];
  double value;

  //   if no prior information, this is 1 bit exactly of data.
  if (total <= 0.0)
    return (1.00);

  if (count >= total)
    return (0.0);

  //  value =  ( - (crm114__logl (
  //	      (count + BIT_ENTROPIC_PROBABILITY_NERF)
  //	      / (total +  BIT_ENTROPIC_PROBABILITY_NERF)))
  //     / 0.69314718 );
  //  return (value);

  //      Do we need to initialize the cache to "no data", which we
  //      code for as a -1 as the entropy.
  if (init == 0)
    {
      long i, j;
      init = 1;
      for (i = 0; i < ENT_CACHE_MAXCOUNT; i++)
	for (j = 0; j < ENT_CACHE_MAXTOTAL; j++)
	  loglookups[i][j] = -1;
    }

  //    See if we've already cached this log probability value?
  if ( count < ENT_CACHE_MAXCOUNT && total < ENT_CACHE_MAXTOTAL)
    {
      value = loglookups[count][total];
      if (value > 0.0)
	return value;
    }

  //     Nope.  Nothing in the cache.  Calculate it and cache it.
  //
  //      "correct" entropy is factored by the unknown prior:
  // value =  ( - ( ( count + BIT_ENTROPIC_PROBABILITY_NERF)
  //	 / ( total + BIT_ENTROPIC_PROBABILITY_NERF))
  //     *  (crm114__logl (
  //	    (count + BIT_ENTROPIC_PROBABILITY_NERF)
  //	    / (total +  BIT_ENTROPIC_PROBABILITY_NERF)))
  //     / 0.69314718 );

  //      But here, we know the event has come to pass and so the
  //     prior is 1.0 (the event itself is a certainty at this point;
  //     we are now just counting bits needed to encode it ! ).
  value =  ( - crm114__logl (
			 (count + BIT_ENTROPIC_PROBABILITY_NERF)
			 / (total +  BIT_ENTROPIC_PROBABILITY_NERF)))
    / 0.69314718 ;


  //   if it fits, put it in the cache
  if ( count < ENT_CACHE_MAXCOUNT && total < ENT_CACHE_MAXTOTAL)
    loglookups[count][total] = value;

  //     and we're done.

  return (value);
}


//  Helper functions for dealing with the FIRlat
//    Given a FIR value, what slot would this reside in?
static long fir_2_slot (double fir, long firlatlen)
{
  long ifir;
  ifir = (fir * (firlatlen - 1.00));
  if (ifir < 0) ifir = 0;
  if (ifir >= firlatlen) ifir = firlatlen - 1;
  return (ifir);
}

static double slot_2_fir (long slot, long firlatlen)
{
  double outval;
  outval = (slot + 0.5) / (firlatlen);
  if (outval > 1.00) outval = 1.00;
  if (outval < 0.0) outval = 0.0;
  return (outval);
}

//    Dump just the significant entries in the FIRlat
static void firlat_significant (ENTROPY_FEATUREBUCKET *nodes,
				long *firlat, long firlatlen)
{
  long i;
  fprintf (stderr, "**** FIRLAT significant scan *****\n");
  fprintf (stderr, "root node low %d high %d\n",
	   nodes[0].fir_smaller, nodes[0].fir_larger);
  for (i = 0; i < firlatlen; i++)
    {
      if (firlat[i] > 0)
	{
	  fprintf (stderr,
		   "FIRLAT slot %ld pval %f node %ld fv %f down %d up %d\n",
		   i, slot_2_fir(i, firlatlen),
		   firlat[i], nodes[firlat[i]].fir_prior,
		   nodes[firlat[i]].fir_smaller,
		   nodes[firlat[i]].fir_larger);
	};
    };
}

static void firlat_sanity_scan (long *firlat, long firlatlen,
				ENTROPY_FEATUREBUCKET *nodes,
				long nodeslen)
{
  long i;
  long stepcounter;
  //   Scan the FIRLat for errors (optional)
  //
  for (i = 0; i < firlatlen; i++)
    {
      if (firlat[i] > nodeslen || firlat[i] < 0 )
	fprintf (stderr,
		 "Internal FIRLAT error: slot %ld claims OOB node %ld\n",
		 i, firlat[i]);
    };
  for (i = 0; i < nodeslen; i++)
    {
      if (   nodes[i].fir_smaller < -1
	     || nodes[i].fir_larger < -1
	     || nodes[i].fir_smaller > nodeslen
	     || nodes[i].fir_larger > nodeslen )
	fprintf (stderr,
		 "Internal FIRchain error at node %ld (%f) smaller: %d larger %d\n",
		 i, nodes[i].fir_prior,
		 nodes[i].fir_smaller, nodes[i].fir_larger);
    };

  //
  //   Now verify the FIRLAT chain integrity upward
  //
  stepcounter = 0;
  i = nodes[0].fir_larger;
  while (stepcounter < nodeslen + 1
	 && nodes[i].fir_larger > 0
	 && nodes[i].fir_larger < nodeslen)
    {
      stepcounter++;
      i = nodes[i].fir_larger;
    };
  if (stepcounter > nodeslen+1)
    fprintf (stderr, "ERROR: the FIR chain is figure-6ed upward\n");
  if (nodes[i].fir_larger != 0)
    fprintf (stderr, "ERROR: the FIR chain goes off to node %d\n",
	     nodes[i].fir_larger);

  //  and again, check chain integrity downward
  stepcounter = 0;
  i = nodes[0].fir_smaller;
  while (stepcounter < nodeslen + 1
	 && nodes[i].fir_smaller > 0
	 && nodes[i].fir_smaller < nodeslen)
    {
      stepcounter++;
      i = nodes[i].fir_smaller;
    };
  if (stepcounter > nodeslen+1)
    fprintf (stderr, "ERROR: the FIR chain is figure-6ed downward\n");
  if (nodes[i].fir_smaller != 0)
    fprintf (stderr, "ERROR: the FIR chain goes off to node %d\n",
	     nodes[i].fir_smaller);
}


//
//    Helper function to find the smallest node that's larger than
//    the called FIR.
//
//    Originally both this routine and find_closest returned exact
//    answers- but a bug caused the hash-table to find exact entry
//    points to overload buckets representing fairly common similar
//    states, and so we'd end up with exponentially increasing runtime
//    during the sequential search part of the run (measured at over
//    2000 steps in sequential search average, when the average number
//    of steps should have been 5 (five).
//
//    The intermediate fix is to only call this (exact) function when
//    you need to move a node - that is, during training.  Otherwise,
//    "close enough is close enough", and use the "close" call down
//    below.  But the _real_ bug has now been fixed, and the "close"
//    routine is now just an efficiency firewall.  It never should
//    have been; but hey... it's here.
//
static long firlat_find_smallest_larger
   ( ENTROPY_FEATUREBUCKET *nodes,
     long nodeslen,
     long *firlat,
     long firlatlen,
     double my_fir)
{
  long hit_node;
  long firlat_entry;
  long step_count;

  //  Start out with FIRlat option.
  if (my_fir < 0.00 || my_fir >= 1.00 || firlatlen < 10 || firlatlen > 200000)
    if (crm114__internal_trace)
      fprintf (stderr, "My FIR is outrageous (value: %e, firlatlen %ld)\n",
	       my_fir,
	       firlatlen);
  firlat_entry = fir_2_slot ( my_fir, firlatlen);
  if (crm114__internal_trace)
    fprintf (stderr, "FFSL: Searching for FIR: %f slot %ld \n",
	     my_fir, firlat_entry);

  if ((firlat_entry < 0) || (firlat_entry >= firlatlen))
    if (crm114__internal_trace)
      fprintf (stderr,
	       "FIRLAT is very hosed (myfir: %e, entry number %ld)!\n",
	       my_fir,
	       firlat_entry);
  if (crm114__internal_trace)
    fprintf
      (stderr,
       "FFSL search: FIR: %f, slot: %ld node: %ld, smaller: %d, larger: %d\n",
       (float) my_fir, firlat_entry, firlat[firlat_entry],
       nodes[firlat[firlat_entry]].fir_smaller,
       nodes[firlat[firlat_entry]].fir_larger );


  if (firlat[firlat_entry] > nodeslen
      || nodes[firlat[firlat_entry]].fir_smaller < 0
      || nodes[firlat[firlat_entry]].fir_larger < 0 )
    if (crm114__internal_trace)
      fprintf (stderr, "Internal FIR chain error at slot %ld node %ld (%f)\n",
	       firlat_entry, firlat[firlat_entry], my_fir);

  //   Move down in the firlat till we find a live node.
  while ( firlat_entry > 0 && firlat[firlat_entry] <= 0)
    {
      //    fprintf (stderr,
      //	   "firlat_entry: %ld points to node %ld so stepdown \n",
      //	   firlat_entry, firlat[firlat_entry]);
      firlat_entry--;
      if (crm114__internal_trace)
	fprintf (stderr, "_");

    }


  //    Do a little metering to see if we're getting firlat entries?
  //if (crm114__internal_trace)
  //if (firlat_entry == initial_firlat_entry)
  //  {
  //    fprintf (stderr, "`");
  //  }
  //else
  //  {
  //    fprintf (stderr, " %ld", firlat_entry - initial_firlat_entry);
  //  };

  //    Stupidity check - are we running down off the end of the FIRLAT?
  if (firlat_entry == 0)
    {
      //  Yes.  Resort to sequential search
      hit_node = nodes[0].fir_larger;
      if (crm114__internal_trace)
	fprintf (stderr,
		 "FFSL: Need to start from rootptr- next node is %ld ( %f) for fir %f \n",
		 hit_node, nodes[hit_node].fir_prior, my_fir);
    }
  else
    {
      //   This non-null entry is where we start to look.
      hit_node = firlat[firlat_entry];
      if (crm114__internal_trace)
	fprintf (stderr,
		 "FFSL: non-null ENTRY at slot %ld node: %ld prior: %f down %d up %d\n",
		 firlat_entry,
		 hit_node, nodes[hit_node].fir_prior,
		 nodes[hit_node].fir_smaller, nodes[hit_node].fir_larger);
    };


  //  Now we're on the closest "less than" firlat entry (which, if we
  //  had a firlat hit, may still be above our desired place.
  //   Part 1 - find a place where we are clearly smaller than
  //   the node above us.
  step_count = 0;
  while ( hit_node > 0 && my_fir > nodes[hit_node].fir_prior )
    {
      hit_node = nodes[hit_node].fir_larger;
      step_count++;
      if (crm114__internal_trace)
	fprintf (stderr, "+");
    };

  //   Part 2 - assure that (because of FIRlat issues) we are not "above"
  //   yet other nodes that we are smaller than.
  while ( hit_node > 0 && my_fir < nodes[hit_node].fir_prior )
    {
      hit_node = nodes[hit_node].fir_smaller;
      step_count++;
      if (crm114__internal_trace)
	fprintf (stderr, "-");
    };

  //    and again, try going up one last time.
  while ( hit_node > 0 && my_fir > nodes[hit_node].fir_prior )
    {
      hit_node = nodes[hit_node].fir_larger;
      step_count++;
      if (crm114__internal_trace)
	fprintf (stderr, "*");
    };

  //   hit_node is now the smallest node that's larger than us
  if (crm114__internal_trace)
    fprintf (stderr, "FFSL result node: %ld with FIR %f \n",
	     hit_node,
	     nodes[hit_node].fir_prior);

  if (crm114__internal_trace)
    fprintf (stderr, "Steps used: %ld\n", step_count);

  return (hit_node);
}

static long firlat_find_closest_node
  ( ENTROPY_FEATUREBUCKET *nodes,
    long nodeslen,
    long *firlat,
    long firlatlen,
    double currentfir)
{
  long larger;
  long smaller;
  long closest;
  double error_larger, error_smaller;
#define COMPLEX_FIND_CLOSEST
#ifdef COMPLEX_FIND_CLOSEST

  //       Complex find closest code.  The idea here is to do
  //       increasing radius searches to find the closest nearby
  //       strings in Hamming distance.  To generate this, we first
  //       search at +/- one LSB in the floating point mantissa, and
  //       then successively double it, which sweeps through the
  //       single-bit errors.  We can then repeat for double-bit
  //       errors, triple-bit errors, and so on... which leads to a
  //       huge number of possible next states.  For IEEE floating
  //       point, the limiting case is an underflowing double-bit
  //       error representation.  Of course, the value of this varies
  //       with the local FIR value, but for a FIR value of 1.0 the
  //       double that touches only the lowest-order mantissa of a
  //       double float is hex 0x3bf0000000000000 (approximately
  //       5.421011E-20).  For a FIR of 0.50000, the value is 1/2
  //       that, and so on.  However, this is really not
  //       computationally tractable right now.

#endif	// COMPLEX_FIND_CLOSEST

  //      return the index of the node that's closest in
  //      FIRLAT value to our current node.

  //          assume larger is closer;
  larger = firlat_find_smallest_larger
    (nodes, nodeslen, firlat, firlatlen, currentfir);
  error_larger = nodes[larger].fir_prior - currentfir;
  closest = larger;

  //          now make sure that smaller isn't better.
  smaller = nodes[larger].fir_smaller;
  error_smaller = currentfir - nodes[smaller].fir_prior;
  if (error_smaller < error_larger)
    closest = smaller;
  return (closest);
}

//      How we remove a node from the FIR chain and firlat
static void firlat_remove_node  ( ENTROPY_FEATUREBUCKET *nodes,
				  long nodeslen,
				  long *firlat,
				  long firlatlen,
				  long curnode)
{
  //   cut it out of the chain first
  long smaller_node, larger_node;
  long firlat_entry;
  double perfect_fir;

  //   set up larger and smaller nodes
  smaller_node = nodes[curnode].fir_smaller;
  larger_node = nodes[curnode].fir_larger;
  {
    if (crm114__internal_trace)
      {
	fprintf (stderr, "About to FIR chop node %ld (%ld %ld)\n",
		 curnode, smaller_node, larger_node);
      }
    if (smaller_node < 0 && larger_node < 0)
      {
	//  what do we do if the node ain't in the FIR chain - nuthin!
	//  we might be initiated on a shuffle or somesuch.
	//  if (crm114__internal_trace)
        //  fprintf (stderr, " BLARG!!!!  removing a non-FIRchained node!\n");
      }
    else
      {
	//  cut the node out of the FIR chain.
	nodes[smaller_node].fir_larger = larger_node;
	nodes[larger_node].fir_smaller = smaller_node;
      }
  }
  //  Now we're definitely out of the chain.  Mark us so....
  nodes[curnode].fir_smaller = -1;
  nodes[curnode].fir_larger = -1;

  if (crm114__internal_trace)
    fprintf
      ( stderr,
	"FIRchain delete %ld (%lf) between %ld (%lf) and %ld (%lf)\n",
	curnode,      nodes[curnode].fir_prior,
	smaller_node, nodes[curnode].fir_prior,
	larger_node,  nodes[curnode].fir_prior);

  //  Since we're not in the chain (and unfindable that way, we can
  //  now also be removed from the FIRLAT lookaside table.

  //    this is where we think we are - which might not be where we are.
  if (nodes[curnode].firlat_slot > -1)
    {
      firlat_entry = nodes[curnode].firlat_slot;
      if ( firlat[firlat_entry] == curnode)
	firlat[firlat_entry] = 0;
    }

  //     This is the slot where we once were.  Fix it up.
  //
  firlat_entry =  fir_2_slot ( nodes[curnode].fir_prior, firlatlen);
  if (firlat[firlat_entry] == curnode)
    {
      long correct_node;
      perfect_fir = slot_2_fir ( firlat_entry, firlatlen);
      correct_node = firlat_find_closest_node
	( nodes, nodeslen, firlat, firlatlen, perfect_fir);
      if (fir_2_slot(nodes[correct_node].fir_prior, firlatlen) == firlat_entry)
	{
	  firlat[firlat_entry] = correct_node;
	  if (crm114__internal_trace)
	    fprintf (stderr, "FIRtable insert node %ld at slot %ld\n",
		     correct_node, firlat_entry);
	};
    };

  if (crm114__internal_trace)
    firlat_significant (nodes, firlat, firlatlen);
}

//     insert a node into the FIR chain and firlat
static void firlat_insert_node
  ( ENTROPY_FEATUREBUCKET *nodes,
    long nodeslen,
    long *firlat,
    long firlatlen,
    long curnode)
{
  long smaller_node, larger_node;
  long current_occupant, firlat_slot;
  double perfect_fir_value, old_fir_error, new_fir_error;

  if ( nodes[curnode].fir_smaller > 0
       || nodes[curnode].fir_larger > 0)
    if (crm114__internal_trace)
      fprintf (stderr, "BLARG !!!  We're reinserting node %ld twice!\n",
	       curnode);

  if (crm114__internal_trace)
    firlat_significant (nodes, firlat, firlatlen);

  //   Get the next larger node
  larger_node = firlat_find_smallest_larger
    ( nodes, nodeslen, firlat, firlatlen, nodes[curnode].fir_prior);
  //     and the smaller node
  smaller_node = nodes[larger_node].fir_smaller;

  if (crm114__internal_trace)
    fprintf (stderr,
	     "FIRchain insert between node S: %ld (%f) and L: %ld (%f) \n",
	     smaller_node, nodes[smaller_node].fir_prior,
	     larger_node, nodes[larger_node].fir_prior);

  //    Plug ourselves into the fir chain
  nodes[larger_node].fir_smaller = curnode;
  nodes[curnode].fir_larger = larger_node;
  nodes[smaller_node].fir_larger = curnode;
  nodes[curnode].fir_smaller = smaller_node;
  if (crm114__internal_trace)
    fprintf
      ( stderr,
	"FIRchain done - inserted %ld (%lf) between %ld (%lf) and %ld (%lf)\n",
	curnode,      nodes[curnode].fir_prior,
	smaller_node, nodes[smaller_node].fir_prior,
	larger_node,  nodes[larger_node].fir_prior);

  //    and put ourselves into the FIRLat if appropriate.
  firlat_slot = fir_2_slot (nodes[curnode].fir_prior, firlatlen);
  current_occupant = firlat[firlat_slot ];

  if (current_occupant == 0)
    {
      //     No current occupant in the FIRLAT- put us in.
      firlat [firlat_slot] = curnode;
      nodes[curnode].firlat_slot = firlat_slot;
      if (crm114__internal_trace)
	fprintf (stderr,
		 "Inserted node %ld into empty slot at %ld (%f)\n",
		 curnode, firlat_slot, nodes[curnode].fir_prior);
    }
  else
    {
      //      There's a prior occupant- need to see who's more accurate.
      perfect_fir_value = slot_2_fir ( firlat_slot, firlatlen);
      old_fir_error = fabs (nodes[current_occupant].fir_prior
			    - perfect_fir_value);
      new_fir_error = fabs (nodes[curnode].fir_prior
			    - perfect_fir_value);
      if (new_fir_error < old_fir_error)
	{
	  firlat[firlat_slot] = curnode;
	  nodes[curnode].firlat_slot = firlat_slot;
	  nodes[current_occupant].firlat_slot = -1;
	  if (crm114__internal_trace)
	    fprintf (stderr,
		     "Insert over %ld at slot %ld (%f) with node %ld\n",
		     current_occupant, firlat_slot,
		     nodes[curnode].fir_prior, curnode);
	};
    };
  if (crm114__internal_trace)
    firlat_significant (nodes, firlat, firlatlen);

}


//
//      Routine to dump what's in the node set and lookaside table
//
static void dump_bit_entropy_data (
				   ENTROPY_FEATUREBUCKET *nodes,
				   long nodeslen,
				   long *firlat,
				   long firlatlen)
{
  int i;
  fprintf (stderr,
	   "  node    A0    c    A1    c    PFIR   FIR<   FIR> \n");
  for (i = 0; i < nodeslen && nodes[i].fir_prior > -1.0; i++)
    {
      fprintf (stderr,
	       "%5d %5d %5ld %5d %5ld %5.3f %5d %5d \n",
	       i,
	       nodes[i].abet[0].nextcell,
	       nodes[i].abet[0].count,
	       nodes[i].abet[1].nextcell,
	       nodes[i].abet[1].count,
	       nodes[i].fir_prior,
	       nodes[i].fir_smaller,
	       nodes[i].fir_larger);
    };
  fprintf (stderr, "FIRlat contents:");
  for (i = 0; i < firlatlen; i++)
    {
      if (firlat[i] > 0 )
	{
	  fprintf (stderr, "\n   %d     %ld  (perf: %f  actual: %f) ",
		   i, firlat[i],
		   ((i + 0.5) / firlatlen),
		   nodes[firlat[i]].fir_prior);
	}
    }
  fprintf (stderr, "\n");
}



//       Create a T tall, W wide shuffle network in node memory.
//       This is destructive; note that nodes get their backchannel
//       set correctly, but their FIRs are not changed, and they
//       are NOT inserted into the FIRlat.
//
//       Note also that this starts the shuffle block at the node
//       pointed to by "firstnode", and returns the index of the first
//       node NOT part of the shuffle block, which (handily) is the
//       start of the nominal freelist.
//
static long nodes_init_shufflenet (
				   ENTROPY_FEATUREBUCKET *nodes,
				   long height,
				   long width,
				   long firstnode)
{
  long w, h, a ;
  long node, target, f;
  // long trow, tcol;
  f = firstnode ;        // because firstnode is too long!
  for (w = 0; w < width; w++)      // zero to width-1
    for (h = 0; h < height; h++)    // zero to height-1
      {
	//      What is our "from" node?
	node = f + h + (w * height);
	//      Mark these nodes "not yet in the FIR chain".
	nodes[node].fir_smaller = -1;
	nodes[node].fir_larger = -1;
	//      set each member of the alphabet to the shuffle of the
	//      next column (modulo width, so the whole thing loops)
	for (a = 0; a < ENTROPY_ALPHABET_SIZE; a++)
	  {
	    //                     zero the count
	    nodes[node].abet[a].count = 0;
	    //tcol = (w + 1) % width;
	    //trow = ((h * ENTROPY_ALPHABET_SIZE) + a) % height;
	    //target = f
	    //  + tcol * height
	    //  + trow;
	    //                     where does this node.abet point?
	    target = f                        //  start of shuffle
	      + ((w+1) % width) * height      //  go to the next column
	      + (((h*ENTROPY_ALPHABET_SIZE) + a) % height) ; //  within col
	    //
	    //fprintf (stderr, "\nlinking from node %ld slot %ld to %ld",
	    //	     node, a, target);
	    //                     aim our current node.abet there.
	    nodes[node].abet[a].nextcell = target;
	    //   and initialize the prior
	    nodes[node].fir_prior = -1.0;

	  }
      }
  //  fprintf (stderr, "\n");
  //  dump_bit_entropy_data (nodes, nodeslen, NULL, 0);

  return (height * width + firstnode);
}


//
//    Helper function - do lookahead from a node in the existing graph and
//    indicate whether the proposed next few bits are "on path"
//    for this particular node in the graph.

static int lattice_lookahead_score (
			    ENTROPY_FEATUREBUCKET *nodes,
			    long proposed_node,   // the proposed next node
			    double node_fir,
			    long crosslink_mincount,
			    float crosslink_fir_thresh,
			    const char *text,  // current txt ptr
			    long textoffset,   // current textoffset
			    short bitnum       // and bitnum that got us here
				     )
{
  char thischar, nextchar;
  unsigned char n0, n1, n2, n3;          //  Our lookahead bits
  double pf0, pf1, pf2, pf3;         //  Our lookahead pathFIRs
  long retval;
  retval = 0;

  //      Set our lookahead characters up:
  thischar = text[textoffset];
  nextchar = text[textoffset + 1];

  //      Set up the lookahead bits next0, next1, next2, and next3
  //      (remember that next0 is the current bit)

  //       the current alph  (we're already here)
  n0 = ( thischar >> bitnum ) & ENTROPY_CHAR_BITMASK;

  //       the next alph
  n1 = ( thischar >> (bitnum - 1)) & ENTROPY_CHAR_BITMASK;
  if (bitnum == 0)
    n1 = ( nextchar >> 7) & ENTROPY_CHAR_BITMASK;

  //       the next-next alph
  n2 = ( thischar >> (bitnum - 2)) & ENTROPY_CHAR_BITMASK;
  if (bitnum == 0)
    n2 = ( nextchar >> 6) & ENTROPY_CHAR_BITMASK;
  if (bitnum == 1)
    n2 = ( nextchar >> 7) & ENTROPY_CHAR_BITMASK;

  //       the next-next-next alph
  n3 = ( thischar >> (bitnum - 2)) & ENTROPY_CHAR_BITMASK;
  if (bitnum == 0)
    n3 = ( nextchar >> 5) & ENTROPY_CHAR_BITMASK;
  if (bitnum == 1)
    n3 = ( nextchar >> 6) & ENTROPY_CHAR_BITMASK;
  if (bitnum == 2)
    n3 = ( nextchar >> 7) & ENTROPY_CHAR_BITMASK;

  //       Set up the path FIRs - these are the FIRs we'll have in
  //       the next few nodes.  We know what they will be from
  //       the nextalphs.
  pf0 = node_fir;

  //   pf1 is the path FIR that alph n1 gets us to.
  pf1 = (n1 * BIT_ENTROPIC_FIR_PRIOR_BIT_WEIGHT)
    + pf0 * (1.0 - BIT_ENTROPIC_FIR_PRIOR_BIT_WEIGHT);
  if (pf1 > 1.0) pf1 = 1.0;
  if (pf1 < 0.0) pf1 = 0.0;

  pf2 = (n2 * BIT_ENTROPIC_FIR_PRIOR_BIT_WEIGHT)
    + pf1 * (1.0 - BIT_ENTROPIC_FIR_PRIOR_BIT_WEIGHT);
  if (pf2 > 1.0) pf1 = 1.0;
  if (pf2 < 0.0) pf1 = 0.0;

  pf3 = (n3 * BIT_ENTROPIC_FIR_PRIOR_BIT_WEIGHT)
    + pf2 * (1.0 - BIT_ENTROPIC_FIR_PRIOR_BIT_WEIGHT);
  if (pf3 > 1.0) pf1 = 1.0;
  if (pf3 < 0.0) pf1 = 0.0;


  //       Now do a bit of lookahead against 'thisnode' to see how
  //       well it actually matches against these new values.
  //
  if (  proposed_node > 0 )
    { retval++ ;} else {return (retval); }
  if ( fabs (nodes[proposed_node].fir_prior - pf0) < crosslink_fir_thresh )
    {  retval++;} else {return (retval); };
  if ( nodes[proposed_node].abet[n1].count >= crosslink_mincount )
    {  retval++;} else {return (retval); };
  if ( nodes[proposed_node].abet[n1^0x1].count == 0 )
    {  retval++;} else {return (retval); };
  if ( nodes[proposed_node].abet[n1].nextcell > 0 )
    {  retval++;} else {return (retval); };

  //     Move to the next node in the pipe
  proposed_node = nodes[proposed_node].abet[n1].nextcell;
  if (  proposed_node > 0 )
    { retval++ ;} else {return (retval); }
  if ( fabs (nodes[proposed_node].fir_prior - pf1) < crosslink_fir_thresh )
    {  retval++;} else {return (retval); };
  if ( nodes[proposed_node].abet[n2].count >= crosslink_mincount )
    {  retval++;} else {return (retval); };
  if ( nodes[proposed_node].abet[n2^0x1].count == 0 )
    {  retval++;} else {return (retval); };
  if ( nodes[proposed_node].abet[n2].nextcell > 0 )
    {  retval++;} else {return (retval); };


  //      And once more, for the next node in the pipe
  proposed_node = nodes[proposed_node].abet[n2].nextcell;
  if (  proposed_node > 0 )
    { retval++ ;} else {return (retval); }
  if ( fabs (nodes[proposed_node].fir_prior - pf2) < crosslink_fir_thresh )
    {  retval++;} else {return (retval); };
  if ( nodes[proposed_node].abet[n3].count >= crosslink_mincount )
    {  retval++;} else {return (retval); };
  if ( nodes[proposed_node].abet[n3^0x1].count == 0 )
    {  retval++;} else {return (retval); };
  if ( nodes[proposed_node].abet[n3].nextcell > 0 )
    {  retval++;} else {return (retval); };

  //      And once more, for the next node in the pipe
  proposed_node = nodes[proposed_node].abet[n3].nextcell;
  if (  proposed_node > 0 )
    { retval++ ;} else {return (retval); }
  if ( fabs (nodes[proposed_node].fir_prior - pf3) < crosslink_fir_thresh )
    { retval++ ;} else {return (retval); }

  return (retval);
}


// Initialize a class's data.  Set up data structures.

void crm114__init_block_bit_entropy(CRM114_DATABLOCK *p_db, int whichclass)
{
  ENTROPY_FEATUREBUCKET *nodes;  // the node array (after mapping!)
  int nodeslen;			 // how many nodes do we have?
  long *firlat;			 // the FIR prior lookaside table
  ENTROPY_HEADER_STRUCT *header; // the what-is-where header.
  long *fmap;			 // class data
  int firlatlen;		 // how long is the FIR lookaside table?
  unsigned int i;

  fmap = (long *)&p_db->data[p_db->cb.block[whichclass].start_offset];
  header = (ENTROPY_HEADER_STRUCT *)fmap;

  if ( !bit_entropy_class_size_to_lengths(p_db->cb.block[whichclass].allocated_size, &nodeslen, &firlatlen))
    return;			// ???

  /////////////////////////////////////////////////////////////////
  //
  //     We have to write in the FIR lookaside table and the free node
  //     list.  Otherwise, the free list will recurse and GCing will
  //     not work.  Additionally, we need to initialize node 0 (the
  //     metanode) and node 1 (the start node); we'll also initialize
  //     nodes 2 and 3 to be the 0- and 1-successor of node 1.
  //

  //   Start with initializing the header.
  //
  header->firlatstart = ENTROPY_RESERVED_HEADER_LEN; //Start of the FIRlat
  //
  firlat = & fmap[header->firlatstart];
  //
  header->firlatlen = firlatlen;
  //
  //    the actual nodes start after the FIRlat
  header->nodestart = header->firlatstart + firlatlen;
  //
  //     and the actual address of the first node is:
  nodes = (ENTROPY_FEATUREBUCKET *) & (fmap[header->nodestart]);
  //
  //     and the number of nodes is...
  header->nodeslen = nodeslen;
  //
  header->totalbits = 0;

  //   initialize the FIR lookaside to contain the metanode (node 0) as
  //   a sentinel value, except for the two extremum nodes.
  for (i = 0; i < firlatlen; i++)
    firlat[i] = 0;

  //   initialize the metanode (node 0) which points to the
  //   start and end of the FIR chains, and to the free list.
  nodes[0].fir_smaller = 3;   // yes, the initial DB looks like an
  nodes[0].fir_larger = 2;    //  infinity sign, with node 0 "twisted"
  //   the start node is node 1, at node 0, alphabet 1 -> next, == 1
  nodes[0].abet[1].nextcell = 1;

  if ( !(p_db->cb.classifier_flags & CRM114_UNIQUE))
    {
      //   put in a shuffled initial state.
      long first_freenode;
      long width, height;
      //    width should be the square root of the size, rounded down
      //    to a multiple of 8 (assuming byte-wise orientation)
      width = ((int) (sqrt (nodeslen)) / 8 ) * 8;
      //     height should be as many as possible with complete rows
      height = nodeslen / width;
      if (crm114__user_trace)
	fprintf (stderr, "New toroid. width: %ld, height %ld\n",
		 width, height);
      // the 1 means "node 1 is first node!"
      first_freenode = nodes_init_shufflenet (nodes, height, width, 1);

      //    mark node 1 as the start of the actual Markov chain
      nodes[0].abet[1].nextcell = 1;
      //    chain the rest of the nodes into a freelist.
      for (i = first_freenode; i < nodeslen; i++)
	{
	  nodes[i].abet[0].nextcell = i+1 ;
	  nodes[i].fir_prior = -1.0;
	}
      //    mark the last node in the freelist as the end of the node
      //    freelist.
      nodes[0].abet[0].nextcell = first_freenode;
      nodes[nodeslen-1].abet[0].nextcell = 0;
    }
  else
    {//    Else very basic init - nodes 1, 2, and 3 in a vee, with
      //   nodes 2 and 3 with no sucessors whatsoever.
      //   initialize the START node (node 1) to point to node 2 on a 0
      nodes[1].abet[0].count = 0;
      nodes[1].abet[0].nextcell = 2;

      //   initialize the START node (node 1) to point to node 3 on a 1
      nodes[1].abet[1].count = 0;
      nodes[1].abet[1].nextcell = 3;

      //   initialize node 2 (that is, first bit is zero)
      //    initialize the output on a 0 to be "nowhere"
      nodes[2].abet[0].count = 0;
      nodes[2].abet[0].nextcell = 0;

      //   initialize the output on 1 to be "nowhere"
      nodes[2].abet[1].count = 0;
      nodes[2].abet[1].nextcell = 0;

      //   initialize node 3 (that is, first bit is 1)
      //    initialize the outoput on a 0 to be "nowhere"
      nodes[3].abet[0].count = 0;
      nodes[3].abet[0].nextcell = 0;

      //   initialize the output on 1 to be "nowhere"
      nodes[3].abet[1].count = 0;
      nodes[3].abet[1].nextcell = 0;

      //   initialize nodes 4 through n into a free list.  We don't care
      //   about any other field than the next node field while on the
      //   free list.
      for ( i = 4; i < nodeslen; i++)
	{
	  nodes[i].abet[0].nextcell = i+1 ;
	  nodes[i].fir_prior = -1.0;
	}
      nodes[0].abet[0].nextcell = 4;

      //    mark the last node in the freelist as the end of the node
      //    freelist.
      nodes[nodeslen-1].abet[0].nextcell = 0;
    };

  //   Either way, we have to initialize the start of the FIR system
  //    initialize the START node (node 1) into the FIR system
  nodes[1].fir_prior = 0.5;
  nodes[1].fir_smaller = 2;
  nodes[1].fir_larger = 3;
  //firlat[fir_2_slot (0.5, firlatlen)] = 1;
  //nodes[1].firlat_slot = fir_2_slot(0.5, firlatlen);

  //    initialize node 2 into the FIR system
  nodes[2].fir_prior = 0.0 ;
  nodes[2].fir_smaller = 0;
  nodes[2].fir_larger = 1;
  //firlat[fir_2_slot (0.0, firlatlen)] = 2;
  //nodes[2].firlat_slot = fir_2_slot(0.0, firlatlen);

  //    initialize node 3 into the FIR system
  nodes[3].fir_prior = 1.0;
  nodes[3].fir_smaller = 1;
  nodes[3].fir_larger = 0;
  //firlat[fir_2_slot (1.0, firlatlen)] = 3;
  //nodes[3].firlat_slot = fir_2_slot(1.0, firlatlen);

}


//
//    How to learn Bit Entropic style
//

CRM114_ERR crm114_learn_text_bit_entropy(CRM114_DATABLOCK **db,
					 int whichclass, const char text[],
					 long textlen)
{
  ENTROPY_FEATUREBUCKET *nodes;  //  the node array (after mapping!)
  int nodeslen = 0;              //  how many nodes do we have?
  size_t nodebytes;             //  how many bytes of nodes?
  long *firlat;               //  the FIR prior lookaside table
  ENTROPY_HEADER_STRUCT *header;     //  the what-is-where header.
  long *fmap;			      // class data
  int firlatlen = -1;             //  how long is the FIR lookaside table?
  size_t firlatbytes;           //  how many bytes of firlat
  double localfir;            //  the FIR value we've accumulated on this path
  long curnode;               //  the current node in our search

  //     Text to be learned stuff:
  long textoffset;
  long textmaxoffset;
  long bitnum;
  int sense;
  long crosslink;
  double crosslink_thresh;
  long crosslink_mincount;

  double bitweight;

  long unique_states;

  if (crm114__internal_trace)
    fprintf (stderr, "executing a bit-entropic LEARN\n");

  if (db == NULL || text == NULL)
    return CRM114_BADARG;
  if (whichclass < 0 || whichclass > (*db)->cb.how_many_classes - 1)
    return CRM114_BADARG;
  if ((*db)->cb.classifier_flags & CRM114_REFUTE)
    return CRM114_BADARG;

  //            set our cflags, if needed.  The defaults are
  //            "case" and "affirm", (both zero valued).
  //            and "crosslink" disabled.


  sense = 1;
  if ((*db)->cb.classifier_flags & CRM114_ERASE)
    {
      sense = -sense;
      if (crm114__user_trace)
	fprintf (stderr, " erasing bit-entropic learning\n");
    };

  crosslink = 0;
  if ((*db)->cb.classifier_flags & CRM114_CROSSLINK)
    {
      crosslink = 1;
      if (crm114__user_trace)
	fprintf (stderr, " enabling crosslinking.\n");
    };

  unique_states = 0;
  if ((*db)->cb.classifier_flags & CRM114_UNIQUE)
    {
      unique_states = 1;
      if (crm114__user_trace)
	fprintf (stderr, " enabling unique states.\n");
    };

  fmap = (long *)&(*db)->data[(*db)->cb.block[whichclass].start_offset];
  header = (ENTROPY_HEADER_STRUCT *)fmap;

  //    grab stuff out of the header:
  firlat = & (fmap [header->firlatstart]);
  firlatlen = header->firlatlen;
  firlatbytes = firlatlen * (sizeof (long));
  nodes = (ENTROPY_FEATUREBUCKET *) & (fmap[header->nodestart]);
  nodeslen = header->nodeslen;
  nodebytes = nodeslen * (sizeof (ENTROPY_FEATUREBUCKET));

  //     In this format, the first BIT_ENTROPIC_FIR_LOOKASIDE_FRACTION
  //     longwords contain the FIR prior lookaside table and the
  //     remainder contain actual nodes, and start immediately after
  //     the FIR lookaside table.
  //
  if (crm114__internal_trace)
    fprintf (stderr,
	     "Firlatlen = %d (%ld bytes), nodeslen = %d (%ld bytes) \n",
	     firlatlen, firlatbytes, nodeslen, nodebytes);

  ////////////////////////////////////////////////////////////////
  //     Crosslink Threshold is dependent on the number of nodes...
  //  crosslink_thresh = 10.0 / nodeslen;
  //  crosslink_thresh = 5.0 / nodeslen;
  //  crosslink_thresh = 1.0 / nodeslen;
  crosslink_thresh = 0.5 / nodeslen;
  //  crosslink_thresh = 0.25 / nodeslen;
  //  crosslink_thresh = 0.1 / nodeslen;
  //  crosslink_thresh = 0.05 / nodeslen;
  //  crosslink_thresh = 0.01 / nodeslen;
  //  crosslink_thresh = 0.003 / nodeslen;
  //  crosslink_thresh = 0.001 / nodeslen;

  ////////////////////////////////////////////////////////////
  //      Crosslink Mincount is the minimum number of good bits before we
  //      allow a crosslink
  crosslink_mincount = 2;

  //    Running 1 megaslot at 1E-7 thresh overflows TREC06 public.
  //    Running 2 megaslot at 1E-7 thresh uses up 76% of TREC06 public, but
  //     with lousy TER (36/10000, 24/1000) and 5140 total errors

  //     even better might be an _adaptive_ threshold, that looks at the
  //    available freelist length and modulates according to that.
  //    Boundary conditions: with empty file, threshold is very small.
  //    and with a full file, threshold is 10/nodeslen.
  //    (we use the parameterized form of a line here - the first term can
  //    be dropped, because it's always times zero.  But we leave it here
  //    in case we actually want a nonzero initial threshold.)

  //      If user specified an overriding value, use that.

  // CB is cleared to all zeroes, then filled in, so this double is
  // cleared to all 0 bits, which is +0 in IEEE 754 floating point.
  // So this is a valid test of whether this double has been set.
  // Yes, if your computer uses something other than IEEE 754 -- IBM
  // mainframes, some Crays -- you might be hosed.  Probably not,
  // though -- 0 is probably still 0.  It is in IBM's format.
  // (Thanks to Wikipedia).

  if ((*db)->cb.params.bit_entropy.crosslink_thresh > 0.0)
    crosslink_thresh = (*db)->cb.params.bit_entropy.crosslink_thresh;



  ////////////////////////////////////////////////////////////////
  //
  //        Setup is complete.  We now just churn through the
  //      incoming bitstream, incrementing and allocating new cells
  //      as needed.  Magic Merging and Cloning action may happen as well.

  //
  textoffset = 0;
  textmaxoffset = textlen;
  //     we'll number bits from 7 to 0, with 7 being most significant (and
  //     also, in most ASCII text, it's zero)
  bitnum = 8;

  //     The current node starts at the START node, which is the
  //     the abet[1] link of the metanode (node 0)
  curnode = nodes[0].abet[1].nextcell;

  bitweight = BIT_ENTROPIC_FIR_PRIOR_BIT_WEIGHT;

  //    localfir is the value of the FIR summary as we sit on
  //    the current node.  Thus, it contains the influence of the
  //    most recent step that got us here, but NOT the effect of the current
  //    alph.
  //
  //   Of course, the starting  is DEFINED as the FIR of the
  //   START node.
  localfir = nodes[curnode].fir_prior;

  if (crm114__internal_trace)
    fprintf (stderr, "Going to do %ld characters.\n",
	     (unsigned long) textmaxoffset - (unsigned long) textoffset);

  while (textoffset + 1 < textmaxoffset || bitnum > 0)
    {
      unsigned short thischar;
      unsigned short thisalph;
      double newnodefir;

      //  get the next alph member.
      bitnum = bitnum - ENTROPY_CHAR_SIZE;
      if (bitnum < 0)
	{
	  bitnum = 7;
	  textoffset ++;
	};
      thischar = text [ textoffset ];
      thisalph = ( thischar >> bitnum ) & ENTROPY_CHAR_BITMASK;

      if (crm114__internal_trace)
	firlat_sanity_scan (firlat, firlatlen, nodes, nodeslen);

      if (crm114__internal_trace)
	fprintf (stderr, "Working char '%c' (%ld untouched), bit %ld.\n",
		 thischar,
		 (unsigned long) textmaxoffset - (unsigned long) textoffset -1,
		 bitnum);

      if (crm114__internal_trace)
	fprintf (stderr,
		 "\nNow at node %ld, alph %d, (bit %ld) localFIR %f\n",
		 curnode, thisalph, bitnum, localfir);

      //   remove this node from the FIRlat and the fir chain

      if (crm114__internal_trace)
	fprintf (stderr, "Removing node %ld from FIRlat \n",
		 curnode);
      firlat_remove_node (nodes, nodeslen, firlat, firlatlen, curnode);


      ///////////////////////////////////////////////////////////////
      //
      //  calculate node's new FIR and where it will go into the FIRlat
      //
      {
	long totalcount, i;
	totalcount = 0;
	for (i = 0; i < ENTROPY_ALPHABET_SIZE; i++)
	  totalcount += nodes[curnode].abet[i].count;
	newnodefir =
	  ( localfir + (totalcount * nodes[curnode].fir_prior) )
	  / ( totalcount + 1.0 );
      };

      nodes[curnode].fir_prior = newnodefir;

      //////////////////////////////////////////////////////
      //
      //   Increment our exit path from curnode; remember that we
      //   might be "unlearning" so use "sense" rather than just incrementing
      //   Of course, this doesn't completely unlearn, as there's always the
      //   chance we allocated a new node, or did a merge or clone on
      //   the initial learn and there's no way to "undo" that for certain.
      //
      nodes[curnode].abet[thisalph].count += sense;

      //    and keep track of the total visits this file has seen.
      header->totalbits += sense;
      //    GROT GROT GROT
      //    on an UNLEARN, we might actually decrement total_count to
      //    zero, meaning that this node never actually got hit.  In
      //    which case, smoe gyrations are appropriate.  But for now, let's
      //    ignore the problem.


      ////////////////////////////////////////////////////////////
      //
      //   Now put the node back where it belongs.
      //

      if (crm114__internal_trace)
	{
	  fprintf (stderr,
		   "Current Status before replacement at node %ld\n", curnode);
	  dump_bit_entropy_data (nodes, nodeslen, firlat, firlatlen);
	};

      firlat_insert_node (nodes, nodeslen, firlat, firlatlen, curnode);

      if (crm114__internal_trace)
	{
	  fprintf (stderr,
		   "Current Status after replacement at node %ld\n", curnode);
	  dump_bit_entropy_data (nodes, nodeslen, firlat, firlatlen);
	};

      //////////////
      //   we've fixed up the FIR chain;
      //   we've fixed up the FIR lookaside table.
      //   It's now time to move on to the next node.
      //   If there is no next node, the fun begins and we need to
      //     allocate yet another node.

      /////////////////////////////////////////////////////
      //
      //   Calculate the new local FIR for this path (NOT for the
      //   node, but for the current path we will have taken at
      //   the next step in the node model.
      //
      localfir = ( thisalph * BIT_ENTROPIC_FIR_PRIOR_BIT_WEIGHT)
	+ ( localfir * ( 1.0 - BIT_ENTROPIC_FIR_PRIOR_BIT_WEIGHT ));

      if (localfir > 1.0000) localfir = 1.00;
      if (localfir < 0.0000) localfir = 0.00;


      //////////////////////////////////////////////////////////
      //    Time to move to the next node.
      //
      //   do we have a real cell as a successor already (i.e. have
      //   we been here before?)  Or is this the first time?
      //
      if (nodes[curnode].abet[thisalph].nextcell > 0)
	{
	  //  yes, there's a node already for this node's successor
	  //  and this particular alph element.
	  //  Make that the current node, and move along!

	  if (crm114__internal_trace)
	    fprintf (stderr, "from old cell %ld to old cell %d\n",
		     curnode, nodes[curnode].abet[thisalph].nextcell);
	  curnode = nodes[curnode].abet[thisalph].nextcell;
#ifdef BEN_GRAPHIC
	  fprintf (stderr, ".");
#endif	// BEN_GRAPHIC
	}
      else
	{    //  either crosslink or allocate new from here
	  //
	  //  no successor on this alphabet slot yet, so we need a new one.
	  //
	  //  If we have no free nodes, we have no choice but to crosslink.
	  //  If we have free nodes, and crosslinking isn't enabled, we
	  //      just allocate a new node.
	  //  If crosslinking is enabled, we see what the preferred
	  //      new FIR will be, and see what node will work for that.
	  //
	  //  Note that this is a lattice - try A, then B, then C.
	  //  so we have to do it in two phases.

	  long further_node, i, do_crosslink;
	  double crosslink_err;

	  //fprintf (stderr, "-");
	  //   Sentinel to know we need to calculate a crosslink
	  further_node = -10001;
	  crosslink_err = 0.123456789;

	  // fprintf (stderr,
	  //  "Ran off the end of entropy model at node %ld\n",
	  //	   curnode);

	  //    Assume no crosslink unless we find out otherwise.
	  do_crosslink = 0;

	  //    Is crosslink forced because we're out of nodes?
	  if (nodes[0].abet[0].nextcell == 0)
	    {
	      do_crosslink = 1;
	      // fprintf (stderr, "$");
     	      if (crm114__internal_trace)
		fprintf (stderr, "Empty freelist, forced crosslink\n");
	    };
	  //    do we possibly want to crosslink anyway?  At least,
	  //    we may need to calculate the best next node, which is
	  //    the one with the closest FIR to what our nextnode FIR is.
	  if ( crosslink || do_crosslink)
	    {
	      long oneup, onedown;
	      long nscore, upscore, downscore;
	      long crosslink_mincount = 1;

              further_node = firlat_find_closest_node
                ( nodes,
		  nodeslen,
                  firlat,
                  firlatlen,
                  localfir);

	      oneup = nodes[further_node].fir_larger;
	      onedown = nodes[further_node].fir_smaller;

	      nscore = lattice_lookahead_score
		(nodes, further_node, localfir, crosslink_mincount,
		 crosslink_thresh,
		 text, textoffset, bitnum);

	      upscore = lattice_lookahead_score
		(nodes, oneup, localfir, crosslink_mincount,
		 crosslink_thresh,
		 text, textoffset, bitnum);

	      downscore = lattice_lookahead_score
		(nodes, onedown, localfir, crosslink_mincount,
		 crosslink_thresh,
		 text, textoffset, bitnum);

	      //  find best score - right on, one down, one up?
	      if (upscore > nscore)
		{
		  further_node = oneup;
		  nscore = upscore;
		};
	      if (downscore > nscore)
		{
		  further_node = onedown;
		  nscore = downscore;
		};
	      crosslink_err = fabs (localfir - nodes[further_node].fir_prior);
	      if (
		  crosslink_err < crosslink_thresh
		  && nscore > 13
		   )
		{
		  do_crosslink = 1;
		  if (crm114__internal_trace)
		    fprintf (stderr,
			     "Crosslink possible - curnode  %ld, score: %ld, error: %f furnode %ld\n",
			     curnode, nscore, crosslink_err, further_node);
		}
	    }       // end attempted crosslink - result in further_node

#ifdef BEN_GRAPHIC
	  fprintf (stderr, "!v");
#endif	// BEN_GRAPHIC
	  if (crm114__internal_trace)
	    fprintf (stderr,
		     "Opportunistic down-crosslink accepted %lf\n",
		     crosslink_err);

	  // Note that at this point further_node is set either way,
	  //  so we don't need to recalculate it.

	  //   Are we crosslinking?
	  if (do_crosslink)
	    {
	      //   Enforce sanity on further_node before crosslink
	      if (further_node < 1)
		{
		  if (further_node < 1)
		    if (crm114__internal_trace)
		      {
			fprintf (stderr,
				 "Bogus crosslink %ld!  We will branch back to node 1.\n",
				 further_node);
			fprintf (stderr, "?");
		      }
		  further_node = 1;
		};
	      //   Put furnode in as the next node for this alph
	      nodes[curnode].abet[thisalph].nextcell = further_node;
	    }
	  else
	    {     // allocate a new node
	      //   Time for a new node.  We link it in with the
	      //   current alph's slot in curnode..
#ifdef BEN_GRAPHIC
	      fprintf (stderr, "@");
#endif	// BEN_GRAPHIC
	      further_node = nodes[0].abet[0].nextcell;
	      if (crm114__internal_trace)
		fprintf (stderr, "Allocating new cell %ld\n",
			 further_node);
	      nodes[curnode].abet[thisalph].nextcell = further_node;

	      //   cut this node out of the front of the free list.
	      nodes[0].abet[0].nextcell =
		nodes[further_node].abet[0].nextcell;

	      //   wipe out any old data that might be there.
	      for (i = 0; i < ENTROPY_ALPHABET_SIZE ; i++)
		{
		  nodes[further_node].abet[i].count = 0;
		  nodes[further_node].abet[i].nextcell = 0;
		};
	      //   initialize the new node with the current FIR prior
	      nodes[further_node].fir_prior = localfir;

	      //    hook the new node into the FIR chain
	      firlat_insert_node
		(nodes, nodeslen, firlat, firlatlen, further_node);
	    }   //   end allocate  new node code.

	  //  Either way, further-node is now set correctly
	  if (crm114__internal_trace)
	    fprintf (stderr,
		     "Now moving from node %ld to %ld (nfir %f, pfir %f)\n",
		     curnode, further_node,
		     nodes[further_node].fir_prior, localfir);
	  //  and move to the new node.
	  curnode = further_node;
	}
    }

  //    All done munging the markov chain.

  (*db)->cb.class[whichclass].documents++;
  (*db)->cb.class[whichclass].features = header->totalbits;

  return (CRM114_OK);
}


CRM114_ERR crm114_classify_text_bit_entropy(const CRM114_DATABLOCK *db,
					    const char text[], long textlen,
					    CRM114_MATCHRESULT *result)
{
  int i;

  long totalfeatures;   //  total features

  //    variables used when tracing the markov graph
  long curnode;
  double total_entropy[CRM114_MAX_CLASSES];

  //     Variables for calculating entropy
  double ptc[CRM114_MAX_CLASSES]; // current running probability of this class
  double renorm = 0.0;
  double entropy_renorm = 0.0;
  double entropy_sum = 0.0;

  ENTROPY_FEATUREBUCKET *nodestarts[CRM114_MAX_BLOCKS];
  ENTROPY_FEATUREBUCKET *nodes;
  ENTROPY_HEADER_STRUCT *headers[CRM114_MAX_BLOCKS]; //  pointers in the files.
  long *fmaps[CRM114_MAX_BLOCKS];
  long nodelens[CRM114_MAX_BLOCKS];
  long nodeslen;
  long *firlats[CRM114_MAX_BLOCKS];
  long firlatlens[CRM114_MAX_BLOCKS];
  long firjumps[CRM114_MAX_BLOCKS];

  int ncls;
  long textoffset;
  long textmaxoffset;

  if (crm114__internal_trace)
    fprintf (stderr, "executing a bit-entropic CLASSIFY\n");

  if (db == NULL || text == NULL || result == NULL)
    return CRM114_BADARG;

  //     Our evaluator is to simply traverse the Markov chain that
  //     starts at nodes[0].abet[0].nextcell, and add together all of
  //     the entropies - that is, the -log2(prob) of each node.  Of
  //     course, we have to nerf the probabilities away from 0, as
  //     that would yield a log2(0.0) which is infinite and thus not
  //     well-represented in IEEE floating point.
  //
  //     For our purposes, we will use:
  //           probability = this_count + BIT_ENTROPIC_PROBABILITY_NERF
  //                          / total_count
  //
  //     If we run off the "end" of the Markov chain, we use the FIRs
  //     to quickly find another node that's "really similar" to this
  //     node.  (this can only happen if the initialized Markov
  //     contained open ends; this doesn't happen when we initialize
  //     with a perfect shuffle graph which is toroidal and has no
  //     "end" to run off of.
  //

  ncls = db->cb.how_many_classes;

  // sanity checks...  Uncomment for super-strict CLASSIFY.
  //
  //	do we have at least 1 valid .css files?
  if (ncls == 0)
    return CRM114_BADARG;

#if 0
  // do we have at least one success class and one failure class?
  {
    int nsucc = 0;
    int nfail = 0;

    for (i = 0; i < ncls; i++)
      if (db->cb.class[i].success)
	nsucc++;
      else
	nfail++;
    if ( !(nsucc > 0 && nfail > 0))
      return CRM114_BADARG;
  }
#endif

  // end sanity checks

  //    a classify of no classes is a "success", if declared sane above
  if (ncls == 0)
    return (CRM114_OK);

  //      initialize our arrays for N classes
  for (i = 0; i < CRM114_MAX_CLASSES; i++)
    {
      ptc[i] = 0.5;      // priori probability
      //        zero out our entropy accumulators
      total_entropy[i] = 0.0;
      firjumps[i] = 0;
    };

  // set up headers and such
  for (i = 0; i < ncls; i++)
    {

      fmaps[i] = (long *)&db->data[db->cb.block[i].start_offset];
      headers[i] = (ENTROPY_HEADER_STRUCT *) fmaps[i];

      firlats[i]= (long *) & (fmaps[i] [headers[i]->firlatstart]);
      firlatlens[i] = headers[i]->firlatlen;
      nodestarts[i] = (ENTROPY_FEATUREBUCKET *)
	& (fmaps[i] [headers[i]->nodestart]);
      nodelens[i] = headers[i]->nodeslen;

      if (crm114__internal_trace)
	fprintf (stderr,
		 "Class #%d firlat %lx len %ld and nodes %lx\n",
		 i,
		 (unsigned long ) firlats[i],
		 firlatlens [i],
		 (unsigned long )nodestarts[i]);
    };


  //   now we can start following the chains and adding up entropy.

  //////////////////////////////////////////////////////////////
  //
  //        The actual graph-following starts here.  To maximize locality
  //        of reference, we do the graph-following first for one graph,
  //        then for another, then for another.  That way, we tend to stay
  //        in the CPU cache better.
  //
  //        Note to self - doing this may or may not speed up the other
  //        classifiers- but because they tend to use less memory it may
  //        make less of a difference.

  totalfeatures = 0;

  {
    int c;            //  c is going to be our class subscript
    unsigned short thischar;
    unsigned short thisalph;
    double localfir;
    long bitnum;
    long nextnode;

    for (c = 0; c < ncls; c++)
      {
	if (crm114__internal_trace)
	  fprintf (stderr, "Now running against stats file %d\n", c);
	//        initialize our per-graph-following stuff:
	totalfeatures = 0;
	nodes = nodestarts[c];
	nodeslen = nodelens[c];
	//        initialize our starting conditions.
	curnode = nodes[0].abet[1].nextcell;
	localfir = nodes[nodes[0].abet[1].nextcell].fir_prior;

	textoffset = 0;
	textmaxoffset = textlen;
	bitnum = 8;

	//    Now the big loop to follow the graph
	while (textoffset + 1 < textmaxoffset || bitnum > 0)
	  {
	    long nodetotcount, itc;
	    float add_entropy;
	    bitnum = bitnum - ENTROPY_CHAR_SIZE;
	    if (bitnum < 0)
	      {
		bitnum = 7;
		textoffset ++;
	      }
	    thischar = text [textoffset];
	    thisalph = ( thischar >> bitnum ) & ENTROPY_CHAR_BITMASK;
	    totalfeatures++;

	    if (crm114__internal_trace)
	      firlat_sanity_scan (firlats[c], firlatlens[c], nodes, nodeslen);


	    //   update our local FIR to what it will be when we're
	    //   at the next node.  (this is effective not at the
	    //   current node, but one step further into the future.)
	    localfir = thisalph * BIT_ENTROPIC_FIR_PRIOR_BIT_WEIGHT
	      + localfir * (1.0 - BIT_ENTROPIC_FIR_PRIOR_BIT_WEIGHT);
	    if (localfir < 0.0) localfir = 0.0;
	    if (localfir > 1.0) localfir = 1.0;

	    //   and go where it told us.  Note that we may want to
	    //   FIR jump if we've never been here before, or there is
	    //   no "next node", or there _is_ no next node.
	    //   we *have* been to before.
	    nodetotcount = 0;
	    for (itc = 0; itc < ENTROPY_ALPHABET_SIZE; itc++)
	      nodetotcount += nodes[curnode].abet[itc].count;
	    if (crm114__internal_trace)
	      fprintf (stderr, "%ld/%ld ",
		       nodes[curnode].abet[thisalph].count, nodetotcount);
	    if (
		( nodetotcount < 1)
		|| nodes[curnode].abet[thisalph].count < 1
		|| nodes[curnode].abet[thisalph].nextcell < 1
		)
	      {
		//    Either no corpus history, or
		//           this cell has never gone here before, or
		//           we're off the edge of the model completely
		//    In any case, we're better off FIR-jumping no
		//    matter what because we'll never be back "in corpus"
		double error;

		//  Note that the localfir is the fir *after* our current
		//  bit (that is, thisalph, which is at txtoffset:bitnum).
		nextnode = firlat_find_closest_node
		  (nodes, nodeslen, firlats[c], firlatlens[c], localfir);
		//
		//   Do a little search to find the best node to jump to
		//
		{
		  long oneup, onedown;
		  long nscore, upscore, downscore;
		  long crosslink_mincount = 0;
		  oneup = nodes[nextnode].fir_larger;
		  onedown = nodes[nextnode].fir_smaller;

		  nscore = lattice_lookahead_score
		    (nodes, nextnode, localfir, crosslink_mincount,
		     0.00001,
		     text, textoffset, bitnum);

		  upscore = lattice_lookahead_score
		    (nodes, oneup, localfir, crosslink_mincount,
		     0.00001,
		     text, textoffset, bitnum);

		  downscore = lattice_lookahead_score
		    (nodes, onedown, localfir, crosslink_mincount,
		     0.00001,
		     text, textoffset, bitnum);

		  if (upscore > nscore)
		    {
		      nextnode = oneup;
		      nscore = upscore;
		    };
		  if (downscore > nscore)
		    {
		      nextnode = onedown;
		      nscore = downscore;
		    };
		}

		error = localfir - nodes[nextnode].fir_prior;
		if (crm114__internal_trace)
		  fprintf (stderr,
			   "FIR jumping, error = %12lg, new node %ld \n",
			   error, nextnode);
		if (error < 0.00) error = - error;

		//    and add the error entropy estimate
		//    due to the FIR jump; since this is knowable
		//    due to it being "no choice", the entropy is very small.
		add_entropy =
		  // pow (2.0, (error / BIT_ENTROPIC_FIR_PRIOR_BIT_WEIGHT));
		  // stats_2_entropy (1, nodelens[c]) ;

		  //   the following one seems to work well.. entirely
		  //   unjustifiably, but it works.  ROC=0.128
		  // stats_2_entropy (1,10);
		  stats_2_entropy (1,1000);

		//   This one is horrible.  5% error rates...
		//   stats_2_entropy (nodes[nextnode].total_count,
		//  nodes[0].total_count);

		//stats_2_entropy (0, nodetotcount);
		//0.5;

		//   This one is justifiable, and is very close to
		//   the unjustifiable one above, but not quite as good.
		//  GROT GROT GROT
		//stats_2_entropy (1, nodetotcount + 1);

	        total_entropy[c] = total_entropy[c] + add_entropy;
		curnode = nextnode;
		firjumps[c]++;
	      }
	    else
	      //     else no FIRjump, things look good.
	      //     we're still "in the corpus" or we've not enabled
	      //     FIR jumping.
	      {
		add_entropy =
		  stats_2_entropy ( nodes[curnode].abet[thisalph].count,
				    nodetotcount );
		total_entropy[c] = total_entropy[c] + add_entropy;
		//       and move to the next node.
		curnode = nodes[curnode].abet[thisalph].nextcell;
		//  go back to the start cell if we're off the end and
		//  we've not enabled crosslinked FIRjumping
		if (curnode < 1)
		  {
		    if (crm114__internal_trace)
		      fprintf (stderr, "X");
		    curnode = nodes[0].abet[0].nextcell;
		  };
	      };
	    if (crm114__internal_trace)
	      fprintf (stderr, "%f \n", add_entropy);
	  }
      }

    //          Test Results on TREC public testset
    // No node-merge or node-clone
    //   Unique states
    //       TOE - 1000000 nodes -  6339/92189, ~356/144/64 min (filled@ ~10K)
    //   128 x 128 lattice:
    //     no FIR-hop
    //       TOE
    //   256 x 256 lattice:
    //     no FIR-hop
    //     FIR-hopping
    //       TOE shuffle - 3342/91996, time 199/71/15 minutes
    //   512 x 512 lattice:
    //     no FIR-hop
    //       TOE  - 3035/92189 ( ~3 % )  time: 233 / 74 / 25 min
    //       TEFT - 8609/75962 and 9508/92189, 272 minutes
    //   1024x1024 lattice:
    //     FIR-hop
    //       TOE  7914/92189 and ?/96/60 minutes
    //       Fixed 5109/92189 and 378/153/63 minutes (with some suspiciously
    //              long delays during a few messages)
    //   2048x2048 lattice:
    //     no FIR-hop
    //       TOE  3780 / 92189 318/87/60 min

    if (crm114__internal_trace)
      {
	for (i = 0; i < ncls; i++)
	  fprintf (stderr, "entropy for classifier %d is %f\n",
		   i, total_entropy[i]);
      };
  };

  ////////////////////////////////////////////////////
  //
  //   Now build the output report.
  //
  //   How we convert entropy into probability and pR:
  //
  //   Basically, we cheat a lot.  If you consider the definition of
  //   entropy, it's the log of probability.  The probability of a
  //   particular document being generated by a particular markov
  //   model is equal to the product of the probabilities of each
  //   particular transition.  The entropy is the sum of the log of
  //   the individual transition probabilities, and (remembering
  //   that multiplying in the real domain is equivalent to summing
  //   in the log domains) we see that the absolute probabililities
  //   (that is, the monkeys-on-typewriters probability) of each
  //   document are just 1 / (2 ^ entropy), and the relative
  //   probabilities can then be scaled out by summing and dividing
  //   in the real domain.
  //
  //   However, those probabilities are REALLY SMALL - well below
  //   the lower limit of IEEE floating point numbers, and so we can
  //   renormalize them to close to 1 by addition of an arbitrary
  //   constant (which is equivalent to multiplying the
  //   probabilities by an arbitrary constant), thus converting them
  //   to relative rather than absolute probabilities (oh- we should
  //   do a renormalization so they really do add to 1.000).
  //
  //   Once we have relative probabilities, we can then jump right
  //   back into the standard summing code to decide on success or
  //   failure.

  //   Step 1: find the minimum needed to add to start renormalization
  entropy_renorm = total_entropy[0];
  entropy_sum = 0.0;
  for (i = 0; i < ncls; i++)
    {
      if (crm114__internal_trace)
	fprintf (stderr, "entropy class %d is %f\n", i, total_entropy[i]);
      if (total_entropy[i] < entropy_renorm )
	{
	  entropy_renorm = total_entropy [i];
	}
      entropy_sum += total_entropy[i];
    };
  if (crm114__internal_trace)
    fprintf (stderr, "entropy_renorm = %f\n", entropy_renorm);

  //   Step 2: add entropy renorm constant, and convert to unnormalized P
  //   Note that this is 2 raised to very small powers, and often
  //   returns zero, so we have to nerf it with DBL_MIN.
  for (i = 0; i < ncls; i++)
    {
      //  Bad formula - technically accurate but pR maxes out
      //    at 304 for nearly all texts
      //ptc [i] = 1 /
      // ( pow (2.00, (total_entropy[i] - entropy_renorm)) + 1000 * DBL_MIN);
      //    better formula - probs very close to 0.5
      // ptc [i] = (entropy_sum - total_entropy[i] ) / entropy_sum;
      //
      //  try a much smaller exponential to bring things back into line:
      //   Note: the 1.1 below is _empirical_, chosen to normalize this
      //   classifier to a +/1 10 pR units for good thick threshold training.
      //   It has no other meaning beyond that.
      ptc [i] = 1 /
	(pow (1.1, (total_entropy[i] - entropy_renorm)) + (1000 * DBL_MIN)) ;
      if (crm114__internal_trace)
	fprintf (stderr, "class %d ptc %f\n", i, ptc[i]);
    };
  //   Step 3: renormalize the probabilities to sum to 1.000
  renorm = 0.0;
  for (i = 0; i < ncls; i++)
    renorm = renorm + ptc[i];
  for (i = 0; i < ncls; i++)
    {
      ptc[i] = (ptc[i] / renorm) + 1000 * DBL_MIN ;
      if (crm114__internal_trace)
	fprintf (stderr, "class %d RENORM ptc %f\n", i, ptc[i]);
    };

  //    Now we're in probability space in the ptc array.  Away we go!

  crm114__result_do_common(result, &db->cb, ptc);
  result->unk_features = totalfeatures;
  for (i = 0; i < ncls; i++)
    {
      result->class[i].u.bit_entropy.entropy = total_entropy[i];
      result->class[i].u.bit_entropy.jumps = firjumps[i];
      result->class[i].hits = totalfeatures - firjumps[i];
    };

  // all done
  return CRM114_OK;
}

// Read/write a DB's data blocks from/to a text file.

int crm114__bit_entropy_learned_write_text_fp(const CRM114_DATABLOCK *db,
					      FILE *fp)
{
  int b;		// block subscript (same as class subscript)
  int i, j;		// feature/sentinel subscript
  //int cksum;

  ENTROPY_HEADER_STRUCT *header;
  long *fmap;
  long *firlat;
  int firlatlen;
  ENTROPY_FEATUREBUCKET *nodes;
  int nodeslen;
  int totalbits;

  for (b = 0; b < db->cb.how_many_blocks; b++)
    {
      
      //  get the per-block startup data
      header = (ENTROPY_HEADER_STRUCT *) 
	(&db->data[ db->cb.block[b].start_offset ]);
      fmap = (long *) header;  // fmap is the long abomination
      firlat = (long *) & (fmap [header->firlatstart]);
      firlatlen = header->firlatlen;
      nodes = (ENTROPY_FEATUREBUCKET *) &(fmap[header->nodestart]);
      nodeslen = header -> nodeslen;
      totalbits = header->totalbits;
      //  write the block header "block N"
      fprintf (fp, TN_BLOCK " %d \n", b);
      
      //  write the header info
      fprintf (fp, TN_FIRLATLEN " %d \n", firlatlen);
      fprintf (fp, TN_NODESLEN " %d \n", nodeslen);
      fprintf (fp, TN_TOTALBITS " %d \n", totalbits);
      //  nb at this point, we know how big a block we need.

      //  write the firlat entries (cheapest to assume none are zero!)
      for (i = 0; i < firlatlen; i++)
	fprintf (fp, "%ld \n", firlat[i]);

      //  cksum = 0;
      //for (i = 0; i < firlatlen; i++)
      //	{
      //  if (firlat[i] != 0 && i != 0)
      //    cksum += i * firlat[i];
      //}
      //printf ("Write Firlat checksum: %d\n", cksum);


      //  write the nodes (none will be zero because either the
      //  unused chain or the toroid linkings will exist)
      //cksum = 0;
      for (i = 0; i < nodeslen; i++)
	{
	  fprintf (fp, TN_ENTROPY_NODE " %d " PRINTF_DOUBLE_PROB_FMT " %d %d %d\n",
		   i,
		   nodes[i].fir_prior,
		   nodes[i].fir_larger,
		   nodes[i].fir_smaller,
		   nodes[i].firlat_slot);
	  //  cksum += i * (nodes[i].fir_larger 
	  //		+ nodes[i].fir_smaller 
	  //		+ nodes[i].firlat_slot);
	  for (j = 0; j < ENTROPY_ALPHABET_SIZE; j++)
	    {
	      fprintf (fp, "  %d %ld %d \n", 
		       j,
		       nodes[i].abet[j].count,
		       nodes[i].abet[j].nextcell);
	      //  cksum += j * nodes[i].abet[j].nextcell;
	    };
	};
      //      printf ("Write nodes checksum %d\n", cksum);
      
      //  All done!
    }
  return TRUE;			// writes always work, right?
}

// Doesn't realloc() *db.  Shouldn't need to...
int crm114__bit_entropy_learned_read_text_fp(CRM114_DATABLOCK **db, FILE *fp)
{

  int b;		// block subscript (same as class subscript)
  int chkb;		// block subscript read from text file, for checking
  int i, j;		// subscripts
  //int cksum;
  int cr;               // count read

  ENTROPY_HEADER_STRUCT *ent_header;
  long *fmap;
  long *firlat;
  int firlatlen;
  ENTROPY_FEATUREBUCKET *nodes;
  int nodeslen;
  int totalbits;
  
  for (b = 0; b < (*db)->cb.how_many_blocks; b++)
    {
      //  get the header startup data (already put in place by
      //  the crm114__learned_read_text_fp)
      fmap = (long *) 
	(&((*db)->data[(*db)->cb.block[b].start_offset ]));
      ent_header = (ENTROPY_HEADER_STRUCT *) fmap;

      //  read the block header "block N"
      if ((j = fscanf(fp, " " TN_BLOCK " %d", &chkb)) != 1 || chkb != b)
	{
	  // printf ("Block Header mismatch!  read %d got %d expected %d\n", 
	  //  j, chkb, b);
	  return CRM114_BADARG;
	}
      else
	{
	  //printf ("Read header for block %d\n", b);
	};

      //  read the header info and set up the intra-block header
      //  firlat first...
      ent_header->firlatstart = ENTROPY_RESERVED_HEADER_LEN;
      firlat = (long *) & (fmap [ent_header->firlatstart]);
      fscanf (fp, " " TN_FIRLATLEN " %d", &firlatlen);
      ent_header->firlatlen = firlatlen;

      //  then the nodes part
      fscanf (fp, " " TN_NODESLEN " %d", &nodeslen);
      ent_header -> nodeslen = nodeslen;
      ent_header->nodestart = ent_header->firlatstart + firlatlen;
      nodes = (ENTROPY_FEATUREBUCKET *) &(fmap[ent_header->nodestart]);

      //  the total bits
      fscanf (fp, " " TN_TOTALBITS " %d", &totalbits);
      totalbits = ent_header->totalbits;
      
      //      printf ("Firlatlen: %d  Nodeslen: %d  Totalbits: %d\n",
      //	      firlatlen, nodeslen, totalbits);
      //
      //printf("Starting read of block %d (firlat %ld l %d nodes %ld l %d \n",
      //   b, (long)firlat, firlatlen, (long)nodes, nodeslen);


      //  read the firlat entries (cheapest to assume none are zero!)
      for (i = 0; i < firlatlen; i++)
	fscanf (fp, " %ld", &(firlat[i]) );
      
      //cksum = 0;
      //for (i = 0; i < firlatlen; i++)
      //	{
      //  if (firlat[i] != 0 && i != 0)
      //    cksum += i * firlat[i];
      //}
      // printf ("Read Firlat checksum: %d\n", cksum);

      //  read the nodes (none will be zero because either the
      //  unused node chain or the toroid linkings will exist)
      //cksum = 0;
      for (i = 0; i < nodeslen; i++)
	{
	  int chki, chkj;
	  cr = fscanf (fp, " " TN_ENTROPY_NODE " %d " SCANF_DOUBLE_PROB_FMT " %d %d %d",
		  &chki,
		  &(nodes[i].fir_prior),
		  &(nodes[i].fir_larger),
		  &(nodes[i].fir_smaller),
		  &(nodes[i].firlat_slot));
	  //if (cr != 5) printf ("DARN! expected 5 nums, got %d\n", cr);
	  //if(chki != i) printf ("OUCH!! expected node %d got %d\n", i, chki);
	  //cksum += i * (nodes[i].fir_larger 
	  //		+ nodes[i].fir_smaller 
	  //		+ nodes[i].firlat_slot);
	  //
	  for (j = 0; j < ENTROPY_ALPHABET_SIZE; j++)
	    {
	      cr = fscanf (fp, " %d %ld %d", 
		      &chkj,
		      &(nodes[i].abet[j].count),
		      &(nodes[i].abet[j].nextcell));
	      //if (cr != 3) printf ("DARN!  expected 3 nums, got %d\n", cr);
	      //if (chkj != j) printf ("OUCH!! expected aslot %d got %d\n", j, chkj);
	      //cksum += j * nodes[i].abet[j].nextcell;
	    };
	};
      //      printf ("Read nodes checksum %d\n", cksum);
    }
  //  All done!
  return TRUE;			// writes always work, right?
}

