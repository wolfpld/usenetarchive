//
//  terminator_classifier_hit.cc
//  terminator
//

#include "terminator_classifier_hit.h"

const double TerminatorClassifierHIT::DEFAULT_HIT_RATE = 0.01;
const double TerminatorClassifierHIT::DEFAULT_HIT_SHIFT = 60;
const double TerminatorClassifierHIT::DEFAULT_HIT_THICKNESS = 0.27;
const double TerminatorClassifierHIT::DEFAULT_HIT_SMOOTH = 1e-5;
const int TerminatorClassifierHIT::DEFAULT_HIT_MAX_ITERATIONS = 250;

TerminatorClassifierHIT::TerminatorClassifierHIT() {
  this->hit_rate_ = TerminatorClassifierHIT::DEFAULT_HIT_RATE;
  this->hit_shift_ = TerminatorClassifierHIT::DEFAULT_HIT_SHIFT;
  this->hit_thickness_ = TerminatorClassifierHIT::DEFAULT_HIT_THICKNESS;
  this->hit_smooth_ = TerminatorClassifierHIT::DEFAULT_HIT_SMOOTH;
  this->hit_max_iterations_ = TerminatorClassifierHIT::DEFAULT_HIT_MAX_ITERATIONS;
}

double TerminatorClassifierHIT::Predict(std::map<std::string, node>& weights) {
  double score = 0.0;
  std::map<std::string, node>::iterator iter;
  for (iter = weights.begin(); iter != weights.end(); ++iter) {
    score += (iter->second).hit;
  }
  score = logist(score / this->hit_shift_);
  return score;
}

void TerminatorClassifierHIT::Train(std::map<std::string, node>& weights,
                                    bool is_spam) {
  std::map<std::string, node>::iterator iter;
  if (is_spam) {
    for (iter = weights.begin(); iter != weights.end(); ++iter) {
      (iter->second).hit_spam += 1;
    }
  } else {
    for (iter = weights.begin(); iter != weights.end(); ++iter) {
      (iter->second).hit_ham += 1;
    }
  }
  double score = 0.0;
  double ratio, p;
  score = this->Predict(weights);
  int count = 0;
  while (is_spam && score < TerminatorClassifierBase::CLASSIFIER_THRESHOLD + this->hit_thickness_ && count < this->hit_max_iterations_) {
    for (iter = weights.begin(); iter != weights.end(); ++iter) {
      p = ((iter->second).hit_spam + this->hit_smooth_)
          / ((iter->second).hit_ham + (iter->second).hit_spam
             + 2.0 * this->hit_smooth_);
      ratio = fabs(2 * p - 1.0);
      (iter->second).hit += (1.0 - score) * this->hit_rate_;
      (iter->second).hit *= ratio;
    }
    score = this->Predict(weights);
    count += 1;
  }
  count = 0;
  while (!is_spam && score > TerminatorClassifierBase::CLASSIFIER_THRESHOLD - this->hit_thickness_ && count < this->hit_max_iterations_) {
    for (iter = weights.begin(); iter != weights.end(); ++iter) {
      p = ((iter->second).hit_spam + this->hit_smooth_)
          / ((iter->second).hit_ham + (iter->second).hit_spam
             + 2.0 * this->hit_smooth_);
      ratio = fabs(2 * p - 1.0);
      (iter->second).hit -= score * this->hit_rate_;
      (iter->second).hit *= ratio;
    }
    score = this->Predict(weights);
    count += 1;
  }
}
