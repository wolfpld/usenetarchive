//
//  terminator_classifier_nsnb.h
//  terminator
//

#ifndef terminator_terminator_classifier_nsnb_h
#define terminator_terminator_classifier_nsnb_h

#include "terminator_classifier_base.h"

class TermiantorClassifierNSNB: public TerminatorClassifierBase {
 private:
  static const double DEFAULT_NSNB_SHIFT;
  static const double DEFAULT_NSNB_SMOOTH;
  static const double DEFAULT_NSNB_THICKNESS;
  static const double DEFAULT_NSNB_LEARNING_RATE;
  static const double DEFAULT_NSNB_MAX_ITERATIONS;

  double nsnb_shift_;
  double nsnb_smooth_;
  double nsnb_thickness_;
  double nsnb_learning_rate_;
  double nsnb_max_iterations_;

  void TrainCell(std::map<std::string, node>& weights, bool is_spam);

 public:
  TermiantorClassifierNSNB();
  virtual double Predict(std::map<std::string, node>& weights);
  virtual void Train(std::map<std::string, node>& weights, bool is_spam);
};


#endif
