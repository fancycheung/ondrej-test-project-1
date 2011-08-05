#ifndef moses_FeatureFunction_h
#define moses_FeatureFunction_h

#include <vector>

#include "ScoreProducer.h"

namespace Moses {

class TargetPhrase;
class Hypothesis;
class FFState;
class InputType;
class ScoreComponentCollection;
class TranslationOption;

class FeatureFunction: public ScoreProducer {

public:
  FeatureFunction(const std::string& description) :
    ScoreProducer(description) {}
  virtual bool IsStateless() const = 0;	
  virtual ~FeatureFunction();

};

class StatelessFeatureFunction: public FeatureFunction {

public:
  StatelessFeatureFunction(const std::string& description) :
    FeatureFunction(description) {}
  //! Evaluate for stateless feature functions. Implement this.
  virtual void Evaluate(
    const TargetPhrase& cur_hypo,
    ScoreComponentCollection* accumulator) const;

  // If true, this value is expected to be included in the
  // ScoreBreakdown in the TranslationOption once it has been
  // constructed.
  // Default: true
  virtual bool ComputeValueInTranslationOption() const;

  bool IsStateless() const;
};

class StatefulFeatureFunction: public FeatureFunction {

public:
  StatefulFeatureFunction(const std::string& description) :
    FeatureFunction(description) {}

  /**
   * \brief This interface should be implemented.
   * Notes: When evaluating the value of this feature function, you should avoid
   * calling hypo.GetPrevHypo().  If you need something from the "previous"
   * hypothesis, you should store it in an FFState object which will be passed
   * in as prev_state.  If you don't do this, you will get in trouble.
   */
  virtual FFState* Evaluate(
    const Hypothesis& cur_hypo,
    const FFState* prev_state,
    ScoreComponentCollection* accumulator) const = 0;
  
  //! return the state associated with the empty hypothesis for a given sentence
  virtual const FFState* EmptyHypothesisState(const InputType &input) const = 0;

  bool IsStateless() const;
};

/**
  * Mixin used to help name features like language models which may have several
  * instances, LM_0, LM_1, etc
  **/
template<typename T>
struct FeatureNameCounter {
  FeatureNameCounter() {
    ++s_created;
  }

  static std::string Name(const std::string& stem) {
    std::ostringstream oss;
    oss << stem;
    oss << "_";
    oss << s_created;
    return oss.str();
  }

  static size_t s_created;
};

template <typename T> size_t FeatureNameCounter<T>::s_created(0);


/**
  * Stateful feature function that just requires a TranslationOption, rather
  * than a Hypothesis. This is the way all feature functions should be, but
  * for historical reasons the LM uses the Hypothesis.
  **/
class OptionStatefulFeatureFunction : public StatefulFeatureFunction {

  public:
    OptionStatefulFeatureFunction(const std::string& description) :
      StatefulFeatureFunction(description) {}

    virtual FFState* Evaluate(
      const Hypothesis& cur_hypo,
      const FFState* prev_state,
      ScoreComponentCollection* accumulator) const;

    virtual FFState* Evaluate(
      const TranslationOption& cur_option,
      const FFState* prev_state,
      ScoreComponentCollection* accumulator) const = 0;
};

}

#endif
