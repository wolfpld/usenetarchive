//
//  terminator_classifier_nb.cc
//  terminator
//

#include "terminator_classifier_nb.h"

const double TerminatorClassifierNB::DEFAULT_NB_SHIFT = 3200;
const double TerminatorClassifierNB::DEFAULT_NB_SMOOTH = 1e-5;
const double TerminatorClassifierNB::DEFAULT_NB_THICKNESS = 0.25;
const int TerminatorClassifierNB::DEFAULT_NB_INCREASING = 15;
const int TerminatorClassifierNB::DEFAULT_NB_MAX_ITERATIONS = 20;

TerminatorClassifierNB::TerminatorClassifierNB() {
  this->nb_shift_ = TerminatorClassifierNB::DEFAULT_NB_SHIFT;
  this->nb_smooth_ = TerminatorClassifierNB::DEFAULT_NB_SMOOTH;
  this->nb_thickness_ = TerminatorClassifierNB::DEFAULT_NB_THICKNESS;
  this->nb_increasing_ = TerminatorClassifierNB::DEFAULT_NB_INCREASING;
  this->nb_max_iterations_ = TerminatorClassifierNB::DEFAULT_NB_MAX_ITERATIONS;
}

double TerminatorClassifierNB::Predict(std::map<std::string, node>& weights) {
  double score = 0.0;
  std::map<std::string, node>::iterator iter;
  int s, h;
  for (iter = weights.begin(); iter != weights.end(); ++iter) {
    s = (iter->second).nb_spam;
    h = (iter->second).nb_ham;
    if (s == 0 && h == 0)
      continue;
    score += log(
               (s + this->nb_smooth_) / (h + this->nb_smooth_) * (TerminatorClassifierBase::TotalHam + 2 * this->nb_smooth_)
               / (TerminatorClassifierBase::TotalSpam + 2 * this->nb_smooth_));
  }
  score += log((TerminatorClassifierBase::TotalSpam + this->nb_smooth_) / (TerminatorClassifierBase::TotalHam + this->nb_smooth_));
  score = logist(score / this->nb_shift_);
  return score;
}

void TerminatorClassifierNB::TrainCell(std::map<std::string, node>& weights,
                                       bool is_spam) {
  std::map<std::string, node>::iterator iter;
  if (is_spam) {
    for (iter = weights.begin(); iter != weights.end(); ++iter) {
      (iter->second).nb_spam += this->nb_increasing_;
    }
  } else {
    for (iter = weights.begin(); iter != weights.end(); ++iter) {
      (iter->second).nb_ham += this->nb_increasing_;
    }
  }
}

void TerminatorClassifierNB::Train(std::map<std::string, node>& weights,
                                   bool is_spam) {
  std::map<std::string, node>::iterator iter;
  double score = this->Predict(weights);
  int count = 0;
  while (is_spam && score < TerminatorClassifierBase::CLASSIFIER_THRESHOLD + this->nb_thickness_ && count < this->nb_max_iterations_) {
    this->TrainCell(weights, is_spam);
    score = this->Predict(weights);
    count++;
  }
  count = 0;
  while (!is_spam && score > TerminatorClassifierBase::CLASSIFIER_THRESHOLD - this->nb_thickness_ && count < this->nb_max_iterations_) {
    this->TrainCell(weights, is_spam);
    score = this->Predict(weights);
    count++;
  }
}
