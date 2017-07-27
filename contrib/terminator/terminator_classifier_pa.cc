//
//  terminator_classifier_pa.cc
//  terminator
//

#include "terminator_classifier_pa.h"

const double TerminatorClassifierPA::DEFAULT_PA_SHIFT = 1.0;

TerminatorClassifierPA::TerminatorClassifierPA() {
  this->pa_shift_ = TerminatorClassifierPA::DEFAULT_PA_SHIFT;
}

double TerminatorClassifierPA::Predict(std::map<std::string, node>& weights) {
  double score = 0.0;
  std::map<std::string, node>::iterator iter;
  for (iter = weights.begin(); iter != weights.end(); ++iter) {
    score += (iter->second).pa;
  }
  score = logist(score / this->pa_shift_);
  return score;
}

void TerminatorClassifierPA::Train(std::map<std::string, node>& weights,
                                   bool is_spam) {
  int label;
  if (is_spam)
    label = 1;
  else
    label = -1;
  double score = 0.0;
  std::map<std::string, node>::iterator iter;
  for (iter = weights.begin(); iter != weights.end(); ++iter) {
    score += (iter->second).pa;
  }
  // hinge loss
  double loss = 0 > (1.0 - label * score) ? 0 : (1.0 - label * score);
  double tol = loss / weights.size();
  for (iter = weights.begin(); iter != weights.end(); ++iter) {
    (iter->second).pa += label * tol;
  }
}
