//
//  terminator_classifier_hit.h
//  terminator
//

#ifndef terminator_terminator_classifier_hit_h
#define terminator_terminator_classifier_hit_h

#include "terminator_classifier_base.h"

class TerminatorClassifierHIT: public TerminatorClassifierBase {

 private:

  static const double DEFAULT_HIT_RATE;
  static const double DEFAULT_HIT_SHIFT;
  static const double DEFAULT_HIT_THICKNESS;
  static const double DEFAULT_HIT_SMOOTH;
  static const int DEFAULT_HIT_MAX_ITERATIONS;

  double hit_rate_;
  double hit_shift_;
  double hit_thickness_;
  double hit_smooth_;
  int hit_max_iterations_;

 public:

  TerminatorClassifierHIT();
  virtual void Train(std::map<std::string, node>& weights, bool is_spam);
  virtual double Predict(std::map<std::string, node>& weights);

};


#endif
