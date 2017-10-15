//
//  terminator_classifier_pam.cc
//  terminator
//

#include "terminator_classifier_pam.h"

const double TerminatorClassifierPAM::DEFAULT_PAM_SHIFT = 1.25;
const double TerminatorClassifierPAM::DEFAULT_PAM_LAMBDA = 0.1;
const int TerminatorClassifierPAM::DEFAULT_PAM_MAX_ITERATIONS = 200;

TerminatorClassifierPAM::TerminatorClassifierPAM() {
  this->pam_shift_ = TerminatorClassifierPAM::DEFAULT_PAM_SHIFT;
  this->pam_lambda_ = TerminatorClassifierPAM::DEFAULT_PAM_LAMBDA;
  this->pam_max_iterations_ = TerminatorClassifierPAM::DEFAULT_PAM_MAX_ITERATIONS;
}

double TerminatorClassifierPAM::Predict(std::map<std::string, node>& weights) {
  double score = 0.0;
  std::map<std::string, node>::iterator iter;
  for (iter = weights.begin(); iter != weights.end(); ++iter) {
    score += (iter->second).pam;
  }
  score = logist(score / this->pam_shift_);
  return score;
}

void TerminatorClassifierPAM::Train(std::map<std::string, node>& weights,
                                    bool is_spam) {
  int label;
  if (is_spam)
    label = 1;
  else
    label = -1;
  double score = 0.0;
  std::map<std::string, node>::iterator iter;
  for (iter = weights.begin(); iter != weights.end(); ++iter) {
    score += (iter->second).pam;
  }
  // hinge loss
  int count = 0;
  while (label * score < 1.0 && count < this->pam_max_iterations_) {
    double tol = this->pam_lambda_ / weights.size();
    for (iter = weights.begin(); iter != weights.end(); ++iter) {
      (iter->second).pam += label * tol;
    }
    score = 0.0;
    for (iter = weights.begin(); iter != weights.end(); ++iter) {
      score += (iter->second).pam;
    }
    count++;
  }
}
