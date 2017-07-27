//
//  terminator_classifier_nsnb.cc
//  terminator
//

#include "terminator_classifier_nsnb.h"

const double TermiantorClassifierNSNB::DEFAULT_NSNB_SHIFT = 3200;
const double TermiantorClassifierNSNB::DEFAULT_NSNB_SMOOTH = 1e-5;
const double TermiantorClassifierNSNB::DEFAULT_NSNB_THICKNESS = 0.25;
const double TermiantorClassifierNSNB::DEFAULT_NSNB_LEARNING_RATE = 0.65;
const double TermiantorClassifierNSNB::DEFAULT_NSNB_MAX_ITERATIONS = 250;

TermiantorClassifierNSNB::TermiantorClassifierNSNB() {
  this->nsnb_shift_ = TermiantorClassifierNSNB::DEFAULT_NSNB_SHIFT;
  this->nsnb_smooth_ = TermiantorClassifierNSNB::DEFAULT_NSNB_SMOOTH;
  this->nsnb_thickness_ = TermiantorClassifierNSNB::DEFAULT_NSNB_THICKNESS;
  this->nsnb_learning_rate_ = TermiantorClassifierNSNB::DEFAULT_NSNB_LEARNING_RATE;
  this->nsnb_max_iterations_ = TermiantorClassifierNSNB::DEFAULT_NSNB_MAX_ITERATIONS;
}

double TermiantorClassifierNSNB::Predict(std::map<std::string, node>& weights) {
  double score = 0.0;
  int s, h;
  std::map<std::string, node>::iterator iter;
  for (iter = weights.begin(); iter != weights.end(); ++iter) {
    s = (iter->second).nsnb_spam;
    h = (iter->second).nsnb_ham;
    if (s == 0 && h == 0)
      continue;
    score += log(
               (s + this->nsnb_smooth_) / (h + this->nsnb_smooth_)
               * (TerminatorClassifierBase::TotalHam + 2 * this->nsnb_smooth_)
               / (TerminatorClassifierBase::TotalSpam + 2 * this->nsnb_smooth_)
               * (iter->second).nsnb_confidence);
  }
  score += log((TerminatorClassifierBase::TotalSpam + this->nsnb_smooth_) / (TerminatorClassifierBase::TotalHam + this->nsnb_smooth_));
  score = logist(score / this->nsnb_shift_);
  return score;
}

void TermiantorClassifierNSNB::TrainCell(std::map<std::string, node>& weights, bool is_spam) {
  std::map<std::string, node>::iterator iter;
  if (is_spam) {
    TerminatorClassifierBase::TotalSpam += 1;
    for (iter = weights.begin(); iter != weights.end(); ++iter) {
      (iter->second).nsnb_spam += 1;
    }
  } else {
    TerminatorClassifierBase::TotalHam += 1;
    for (iter = weights.begin(); iter != weights.end(); ++iter) {
      (iter->second).nsnb_ham += 1;
    }
  }
}
void TermiantorClassifierNSNB::Train(std::map<std::string, node>& weights,
                                     bool is_spam) {
  std::map<std::string, node>::iterator iter;
  double score = this->Predict(weights);
  if (is_spam) {
    TerminatorClassifierBase::TotalSpam += 1;
  } else {
    TerminatorClassifierBase::TotalHam += 1;
  }
  int count = 0;
  while (is_spam && score < TerminatorClassifierBase::CLASSIFIER_THRESHOLD + this->nsnb_thickness_ && count < this->nsnb_max_iterations_) {
    for (iter = weights.begin(); iter != weights.end(); ++iter) {
      (iter->second).nsnb_confidence /= this->nsnb_learning_rate_;
    }
    TrainCell(weights, is_spam);
    TerminatorClassifierBase::TotalSpam -= 1;
    score = this->Predict(weights);
    count++;
  }
  count = 0;
  while (!is_spam && score > TerminatorClassifierBase::CLASSIFIER_THRESHOLD - this->nsnb_thickness_ && count < this->nsnb_max_iterations_) {
    for (iter = weights.begin(); iter != weights.end(); ++iter) {
      (iter->second).nsnb_confidence *= this->nsnb_learning_rate_;
    }
    TrainCell(weights, is_spam);
    TerminatorClassifierBase::TotalHam -= 1;
    score = this->Predict(weights);
    count++;
  }
}
