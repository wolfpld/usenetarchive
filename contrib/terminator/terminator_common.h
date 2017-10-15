//
//  terminator_common.h
//  terminator
//

#ifndef terminator_terminator_common_h
#define terminator_terminator_common_h

#include <string>
#include <map>
#include <cmath>

/**
 * base model of terminator
 * store information of each feature
 * */
struct Node {
  float logist;
  float bwinnow_upper;
  float bwinnow_lower;
  int nsnb_spam;
  int nsnb_ham;
  double nsnb_confidence;
  float pam;
  float pa;
  float winnow;
  int hit_spam;
  int hit_ham;
  float hit;
  int nb_spam;
  int nb_ham;
};

typedef struct Node node;
typedef struct Node * ptr_node;

inline double logist(double x) {
  return 1.0 / (1.0 + exp(-x));
}

inline double invlogist(double x) {
  return log(x / (1 - x));
}

#endif
