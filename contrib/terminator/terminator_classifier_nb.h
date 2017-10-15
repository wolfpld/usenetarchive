//
//  terminator_classifier_nb.h
//  terminator
//

#ifndef terminator_terminator_classifier_nb_h
#define terminator_terminator_classifier_nb_h

#include "terminator_classifier_base.h"

class TerminatorClassifierNB: public TerminatorClassifierBase {
 private:
  static const double DEFAULT_NB_SHIFT;
  static const double DEFAULT_NB_SMOOTH;
  static const double DEFAULT_NB_THICKNESS;
  static const int DEFAULT_NB_INCREASING;
  static const int DEFAULT_NB_MAX_ITERATIONS;

  double nb_shift_;
  double nb_smooth_;
  double nb_thickness_;
  int nb_increasing_;
  int nb_max_iterations_;

  void TrainCell(std::map<std::string, node>& weights, bool is_spam);

 public:
  TerminatorClassifierNB();
  virtual double Predict(std::map<std::string, node>& weights);
  virtual void Train(std::map<std::string, node>& weights, bool is_spam);

};


#endif
