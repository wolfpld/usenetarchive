//
//  terminator_classifier_owv.cc
//  terminator
//

#include "terminator_classifier_owv.h"

const double TerminatorClassifierOWV::DEFAULT_OWV_STEP = 0.02;

TerminatorClassifierOWV::TerminatorClassifierOWV(double* weights_classifier) {
  this->owv_step_ = TerminatorClassifierOWV::DEFAULT_OWV_STEP;

  this->classifiers_[0] = new TerminatorClassifierBWinnow();
  this->classifiers_[1] = new TerminatorClassifierLR();
  this->classifiers_[2] = new TerminatorClassifierNB();
  this->classifiers_[3] = new TermiantorClassifierNSNB();
  this->classifiers_[4] = new TerminatorClassifierWinnow();
  this->classifiers_[5] = new TerminatorClassifierPA();
  this->classifiers_[6] = new TerminatorClassifierPAM();
  this->classifiers_[7] = new TerminatorClassifierHIT();

  unsigned i = 0;
  while (i++ < CLASSIFIER_NUMBER) this->weights_classifier_[i] = weights_classifier[i];
}

double TerminatorClassifierOWV::Predict(std::map<std::string, node>& weights) {
  double final_score = 0.0;
  double total_weights = 0.0;
  for (unsigned i = 0; i < CLASSIFIER_NUMBER; i++) {
    final_score += classifiers_[i]->Predict(weights) * weights_classifier_[i];
    total_weights += weights_classifier_[i];
  }
  return final_score / total_weights;
}

void TerminatorClassifierOWV::Train(std::map<std::string, node>& weights, bool is_spam) {
  double final_score = 0.0;
  double total_weights = 0.0;
  double scores[CLASSIFIER_NUMBER];
  for (unsigned i = 0; i < CLASSIFIER_NUMBER; i++) {
    scores[i] = classifiers_[i]->Predict(weights);
    final_score += scores[i] * weights_classifier_[i];
    total_weights += weights_classifier_[i];
  }
  final_score /= total_weights;
  if (is_spam) {
    if (final_score > TerminatorClassifierBase::CLASSIFIER_THRESHOLD) {
      for (unsigned i = 0; i < CLASSIFIER_NUMBER; i++) {
        if (scores[i] <= TerminatorClassifierBase::CLASSIFIER_THRESHOLD)
          weights_classifier_[i] -= owv_step_;
      }
    } else {
      for (unsigned i = 0; i < CLASSIFIER_NUMBER; i++) {
        if (scores[i] > TerminatorClassifierBase::CLASSIFIER_THRESHOLD)
          weights_classifier_[i] += owv_spam_tradeoff * owv_step_;
      }
    }
  } else {
    if (final_score > TerminatorClassifierBase::CLASSIFIER_THRESHOLD) {
      for (unsigned i = 0; i < CLASSIFIER_NUMBER; i++) {
        if (scores[i] <= TerminatorClassifierBase::CLASSIFIER_THRESHOLD)
          weights_classifier_[i] += owv_spam_tradeoff * owv_step_;
      }
    } else {
      for (unsigned i = 0; i < CLASSIFIER_NUMBER; i++) {
        if (scores[i] > TerminatorClassifierBase::CLASSIFIER_THRESHOLD)
          weights_classifier_[i] -= owv_step_;
      }
    }
  }

  for (unsigned i = 0; i < CLASSIFIER_NUMBER; i++) {
    classifiers_[i]->Train(weights, is_spam);
  }
}

