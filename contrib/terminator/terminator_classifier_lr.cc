//
//  terminator_classifier_lr.cc
//  terminator
//

#include "terminator_classifier_lr.h"

const double TerminatorClassifierLR::DEFAULT_LOGISTIC_LEARNING_RATE = 0.01;
const double TerminatorClassifierLR::DEFAULT_LOGISTIC_SHIFT = 10;
const double TerminatorClassifierLR::DEFAULT_LOGISTIC_THICKNESS = 0.20;
const double TerminatorClassifierLR::DEFAULT_LOGISTIC_MAX_ITERATIONS = 200;

TerminatorClassifierLR::TerminatorClassifierLR() {
  this->logistic_learning_rate_ = TerminatorClassifierLR::DEFAULT_LOGISTIC_LEARNING_RATE;
  this->logistic_max_iterations_ = TerminatorClassifierLR::DEFAULT_LOGISTIC_MAX_ITERATIONS;
  this->logistic_shift_ = TerminatorClassifierLR::DEFAULT_LOGISTIC_SHIFT;
  this->logistic_thickness_ = TerminatorClassifierLR::DEFAULT_LOGISTIC_THICKNESS;
}


double TerminatorClassifierLR::Predict(std::map<std::string, node>& weights) {
  double logist_score = 0.0;
  std::map<std::string, node>::iterator iter = weights.begin();
  while (iter != weights.end()) {
    logist_score += (iter->second).logist;
    ++iter;
  }
  logist_score = logist(logist_score / this->logistic_shift_);
  return logist_score;
}

void TerminatorClassifierLR::Train(std::map<std::string, node>& weights, bool is_spam) {
  double logist_score = this->Predict(weights);
  std::map<std::string, node>::iterator iter;
  int count = 0;

  while (is_spam && logist_score <= TerminatorClassifierBase::CLASSIFIER_THRESHOLD + this->logistic_thickness_
         && count < this->logistic_max_iterations_) {
    for (iter = weights.begin(); iter != weights.end(); ++iter) {
      (iter->second).logist += (1.0 - logist_score)
                               * this->logistic_learning_rate_;
    }
    logist_score = this->Predict(weights);
    count++;
  }
  count = 0;
  while (!is_spam && logist_score >= TerminatorClassifierBase::CLASSIFIER_THRESHOLD - this->logistic_thickness_
         && count < this->logistic_max_iterations_) {
    for (iter = weights.begin(); iter != weights.end(); ++iter) {
      (iter->second).logist -= logist_score * this->logistic_learning_rate_;
    }
    logist_score = this->Predict(weights);
    count++;
  }
}
