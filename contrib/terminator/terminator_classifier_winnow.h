//
//  terminator_classifier_winnow.h
//  terminator
//

#ifndef terminator_terminator_classifier_winnow_h
#define terminator_terminator_classifier_winnow_h

#include "terminator_classifier_base.h"

class TerminatorClassifierWinnow: public TerminatorClassifierBase {
 private:

  static const double DEFAULT_WINNOW_THRESHOLD;
  static const double DEFAULT_WINNOW_SHIFT;
  static const double DEFAULT_WINNOW_THICKNESS;
  static const double DEFAULT_WINNOW_ALPHA;
  static const double DEFAULT_WINNOW_BETA;
  static const double DEFAULT_WINNOW_MAX_ITERATIONS;

  double winnow_threshold_;
  double winnow_shift_;
  double winnow_thickness_;
  double winnow_alpha_;
  double winnow_beta_;
  double winnow_max_iterations_;

 public:

  TerminatorClassifierWinnow();
  virtual double Predict(std::map<std::string, node>& weights);
  virtual void Train(std::map<std::string, node>& weights, bool is_spam);

};


#endif
