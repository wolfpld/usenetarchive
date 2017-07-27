//
//  terminator_classifier_winnow.cc
//  terminator
//

#include "terminator_classifier_winnow.h"

const double TerminatorClassifierWinnow::DEFAULT_WINNOW_THRESHOLD = 1.0;
const double TerminatorClassifierWinnow::DEFAULT_WINNOW_SHIFT = 1;
const double TerminatorClassifierWinnow::DEFAULT_WINNOW_THICKNESS = 0.1;
const double TerminatorClassifierWinnow::DEFAULT_WINNOW_ALPHA = 1.23;
const double TerminatorClassifierWinnow::DEFAULT_WINNOW_BETA = 0.83;
const double TerminatorClassifierWinnow::DEFAULT_WINNOW_MAX_ITERATIONS = 20;

TerminatorClassifierWinnow::TerminatorClassifierWinnow() {
  this->winnow_threshold_ = TerminatorClassifierWinnow::DEFAULT_WINNOW_THRESHOLD;
  this->winnow_shift_ = TerminatorClassifierWinnow::DEFAULT_WINNOW_SHIFT;
  this->winnow_thickness_ = TerminatorClassifierWinnow::DEFAULT_WINNOW_THICKNESS;
  this->winnow_alpha_ = TerminatorClassifierWinnow::DEFAULT_WINNOW_ALPHA;
  this->winnow_beta_ = TerminatorClassifierWinnow::DEFAULT_WINNOW_BETA;
  this->winnow_max_iterations_ = TerminatorClassifierWinnow::DEFAULT_WINNOW_MAX_ITERATIONS;
}

double TerminatorClassifierWinnow::Predict(std::map<std::string, node>& weights) {
  double score = 0.0;
  std::map<std::string, node>::iterator iter;
  for (iter = weights.begin(); iter != weights.end(); ++iter) {
    score += (iter->second).winnow;
  }
  score /= weights.size();
  score -= this->winnow_threshold_;
  score = logist(score / this->winnow_shift_);
  return score;
}

void TerminatorClassifierWinnow::Train(std::map<std::string, node>& tmp_weights,
                                       bool is_spam) {
  std::map<std::string, node>::iterator iter;
  double score = this->Predict(tmp_weights);

  int count = 0;
  if (is_spam && score < TerminatorClassifierBase::CLASSIFIER_THRESHOLD + this->winnow_thickness_
      && count < this->winnow_max_iterations_) {
    for (iter = tmp_weights.begin(); iter != tmp_weights.end(); ++iter) {
      (iter->second).winnow *= this->winnow_alpha_;
    }
    count++;
  }

  count = 0;
  if (!is_spam && score > TerminatorClassifierBase::CLASSIFIER_THRESHOLD - this->winnow_thickness_
      && count < this->winnow_max_iterations_) {
    for (iter = tmp_weights.begin(); iter != tmp_weights.end(); ++iter) {
      (iter->second).winnow *= this->winnow_beta_;
    }
    count++;
  }
}
