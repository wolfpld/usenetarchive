//
//  terminator_classifier_bwinnow.cc
//  terminator
//

#include "terminator_classifier_bwinnow.h"

const double TerminatorClassifierBWinnow::DEFAULT_BWINNOW_ALPHA = 1.11;
const double TerminatorClassifierBWinnow::DEFAULT_BWINNOW_BETA = 0.89;
const double TerminatorClassifierBWinnow::DEFAULT_BWINNOW_SHIFT = 1;
const double TerminatorClassifierBWinnow::DEFAULT_BWINNOW_THRESHOLD = 1.0;
const double TerminatorClassifierBWinnow::DEFAULT_BWINNOW_THICKNESS = 0.1;
const double TerminatorClassifierBWinnow::DEFAULT_BWINNOW_MAX_ITERATIONS = 200;

TerminatorClassifierBWinnow::TerminatorClassifierBWinnow() {
  this->bwinnow_alpha_ = TerminatorClassifierBWinnow::DEFAULT_BWINNOW_ALPHA;
  this->bwinnow_beta_ = TerminatorClassifierBWinnow::DEFAULT_BWINNOW_BETA;
  this->bwinnow_shift_ = TerminatorClassifierBWinnow::DEFAULT_BWINNOW_SHIFT;
  this->bwinnow_threshold_ = TerminatorClassifierBWinnow::DEFAULT_BWINNOW_THRESHOLD;
  this->bwinnow_thickness_ = TerminatorClassifierBWinnow::DEFAULT_BWINNOW_THICKNESS;
  this->bwinnow_max_iterations_ = TerminatorClassifierBWinnow::DEFAULT_BWINNOW_MAX_ITERATIONS;
}

double TerminatorClassifierBWinnow::Predict(std::map<std::string, node>& weights) {
  double bwinnow_score = 0.0;
  std::map<std::string, node>::iterator iter =
    weights.begin();
  while (iter != weights.end()) {
    bwinnow_score += (iter->second).bwinnow_upper
                     - (iter->second).bwinnow_lower;
    ++iter;
  }
  bwinnow_score /= weights.size();
  bwinnow_score -= this->bwinnow_threshold_;
  bwinnow_score = logist(bwinnow_score / this->bwinnow_shift_);
  return bwinnow_score;
}


void TerminatorClassifierBWinnow::Train(std::map<std::string, node>& weights,
                                        bool is_spam) {
  double bwinnow_score = this->Predict(weights);
  std::map<std::string, node>::iterator iter;
  int count = 0;

  while (is_spam && bwinnow_score <= TerminatorClassifierBase::CLASSIFIER_THRESHOLD + this->bwinnow_thickness_
         && count < this->bwinnow_max_iterations_) {
    for (iter = weights.begin(); iter != weights.end(); ++iter) {
      (iter->second).bwinnow_upper *= this->bwinnow_alpha_;
      (iter->second).bwinnow_lower *= this->bwinnow_beta_;
    }
    bwinnow_score = this->Predict(weights);
    count++;
  }
  count = 0;
  while (!is_spam && bwinnow_score >= TerminatorClassifierBase::CLASSIFIER_THRESHOLD - this->bwinnow_thickness_
         && count < this->bwinnow_max_iterations_) {
    for (iter = weights.begin(); iter != weights.end(); ++iter) {
      (iter->second).bwinnow_upper *= this->bwinnow_beta_;
      (iter->second).bwinnow_lower *= this->bwinnow_alpha_;
    }
    bwinnow_score = this->Predict(weights);
    count++;
  }
}
