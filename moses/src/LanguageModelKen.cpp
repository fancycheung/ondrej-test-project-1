// $Id$

/***********************************************************************
Moses - factored phrase-based language decoder
Copyright (C) 2006 University of Edinburgh

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
***********************************************************************/

#include <cassert>
#include <cstring>
#include <iostream>
#include <stdlib.h>
#include "lm/binary_format.hh"
#include "lm/enumerate_vocab.hh"
#include "lm/model.hh"

#include "LanguageModelKen.h"
#include "FFState.h"
#include "TypeDef.h"
#include "Util.h"
#include "FactorCollection.h"
#include "Phrase.h"
#include "InputFileStream.h"
#include "StaticData.h"
#include "ChartHypothesis.h"

using namespace std;

namespace Moses
{

LanguageModelKenBase::~LanguageModelKenBase() {}

namespace
{

  class LanguageModelChartStateKenLM : public FFState
  {
  private:
    lm::ngram::ChartState m_state;
    const ChartHypothesis *m_hypo;

  public:
    explicit LanguageModelChartStateKenLM(const ChartHypothesis &hypo)
        :m_hypo(&hypo)
    {}

    const ChartHypothesis* GetHypothesis() const { return m_hypo; }

    const lm::ngram::ChartState &GetChartState() const { return m_state; }
    lm::ngram::ChartState &GetChartState() { return m_state; }

    int Compare(const FFState& o) const
    {
      const LanguageModelChartStateKenLM &other = static_cast<const LanguageModelChartStateKenLM&>(o);
      int ret = m_state.Compare(other.m_state);
      return ret;
    }
  };


class MappingBuilder : public lm::ngram::EnumerateVocab
{
public:
  MappingBuilder(FactorCollection &factorCollection, std::vector<lm::WordIndex> &mapping)
    : m_factorCollection(factorCollection), m_mapping(mapping) {}

  void Add(lm::WordIndex index, const StringPiece &str) {
    std::size_t factorId = m_factorCollection.AddFactor(str.data())->GetId();
    if (m_mapping.size() <= factorId) {
      // 0 is <unk> :-)
      m_mapping.resize(factorId + 1);
    }
    m_mapping[factorId] = index;
  }

private:
  FactorCollection &m_factorCollection;
  std::vector<lm::WordIndex> &m_mapping;
};

struct KenLMState : public FFState {
  lm::ngram::State state;
  int Compare(const FFState &o) const {
    const KenLMState &other = static_cast<const KenLMState &>(o);
    if (state.length < other.state.length) return -1;
    if (state.length > other.state.length) return 1;
    return std::memcmp(state.words, other.state.words, sizeof(lm::WordIndex) * state.length);
  }
};

/*
 * An implementation of single factor LM using Ken's code.
 */
template <class Model> class LanguageModelKen : public LanguageModelKenBase
{
private:
  Model *m_ngram;
  std::vector<lm::WordIndex> m_lmIdLookup;
  bool m_lazy;
  KenLMState m_nullContextState;
  KenLMState m_beginSentenceState;

  void TranslateIDs(const std::vector<const Word*> &contextFactor, lm::WordIndex *indices) const;

public:
  LanguageModelKen(bool lazy);
  ~LanguageModelKen();

  bool Load(const std::string &filePath
            , FactorType factorType
            , size_t nGramOrder);

  LMResult GetValueGivenState(const std::vector<const Word*> &contextFactor, FFState &state) const {
    return GetKenFullScoreGivenState(contextFactor, state);
  }
  LMKenResult GetKenFullScoreGivenState(const std::vector<const Word*> &contextFactor, FFState &state) const;

  LMResult GetValueForgotState(const std::vector<const Word*> &contextFactor, FFState &outState) const {
    return GetKenFullScoreForgotState(contextFactor, outState);
  }
  LMKenResult GetKenFullScoreForgotState(const std::vector<const Word*> &contextFactor, FFState &outState) const;

  void GetState(const std::vector<const Word*> &contextFactor, FFState &outState) const;

  const FFState *GetNullContextState() const;
  const FFState *GetBeginSentenceState() const;
  FFState *NewState(const FFState *from = NULL) const;

  lm::WordIndex GetLmID(const std::string &str) const;

  void CleanUpAfterSentenceProcessing() {}
  void InitializeBeforeSentenceProcessing() {}

  FFState *EvaluateChart(const ChartHypothesis& cur_hypo,
                         int featureID,
                         ScoreComponentCollection *accumulator,
                         const LanguageModel *feature) const;
};

template <class Model>
FFState *LanguageModelKen<Model>::EvaluateChart(
    const ChartHypothesis& hypo,
    int featureID,
    ScoreComponentCollection *accumulator,
    const LanguageModel *feature) const
{
  LanguageModelChartStateKenLM *newState = new LanguageModelChartStateKenLM(hypo);
  lm::ngram::RuleScore<Model> ruleScore(*m_ngram, newState->GetChartState());
  const AlignmentInfo::NonTermIndexMap &nonTermIndexMap = hypo.GetCurrTargetPhrase().GetAlignmentInfo().GetNonTermIndexMap();

  const size_t size = hypo.GetCurrTargetPhrase().GetSize();
  size_t phrasePos = 0;
  // Special cases for first word.  
  if (size) {
    const Word &word = hypo.GetCurrTargetPhrase().GetWord(0);
    if (word == GetSentenceStartArray()) {
      // Begin of sentence
      ruleScore.BeginSentence();
      phrasePos++;
    } else if (word.IsNonTerminal()) {
      // Non-terminal is first so we can copy instead of rescoring.  
      const ChartHypothesis *prevHypo = hypo.GetPrevHypo(nonTermIndexMap[phrasePos]);
      const lm::ngram::ChartState &prevState = static_cast<const LanguageModelChartStateKenLM*>(prevHypo->GetFFState(featureID))->GetChartState();
      ruleScore.BeginNonTerminal(prevState, prevHypo->GetScoreBreakdown().GetScoresForProducer(feature)[0]);
      phrasePos++;
    }
  }

  for (; phrasePos < size; phrasePos++) {
    const Word &word = hypo.GetCurrTargetPhrase().GetWord(phrasePos);
    if (word.IsNonTerminal()) {
      const ChartHypothesis *prevHypo = hypo.GetPrevHypo(nonTermIndexMap[phrasePos]);
      const lm::ngram::ChartState &prevState = static_cast<const LanguageModelChartStateKenLM*>(prevHypo->GetFFState(featureID))->GetChartState();
      ruleScore.NonTerminal(prevState, prevHypo->GetScoreBreakdown().GetScoresForProducer(feature)[0]);
    } else {
      std::size_t factor = word.GetFactor(GetFactorType())->GetId();
      lm::WordIndex new_word = (factor >= m_lmIdLookup.size() ? 0 : m_lmIdLookup[factor]);
      ruleScore.Terminal(new_word);
    }
  }

  accumulator->Assign(feature, ruleScore.Finish());
  return newState;
}

template <class Model> void LanguageModelKen<Model>::TranslateIDs(const std::vector<const Word*> &contextFactor, lm::WordIndex *indices) const
{
  FactorType factorType = GetFactorType();
  // set up context
  for (size_t i = 0 ; i < contextFactor.size(); i++) {
    std::size_t factor = contextFactor[i]->GetFactor(factorType)->GetId();
    lm::WordIndex new_word = (factor >= m_lmIdLookup.size() ? 0 : m_lmIdLookup[factor]);
    indices[contextFactor.size() - 1 - i] = new_word;
  }
}

template <class Model> LanguageModelKen<Model>::LanguageModelKen(bool lazy)
  :m_ngram(NULL), m_lazy(lazy)
{
}

template <class Model> LanguageModelKen<Model>::~LanguageModelKen()
{
  delete m_ngram;
}

template <class Model> bool LanguageModelKen<Model>::Load(const std::string &filePath,
    FactorType factorType,
    size_t /*nGramOrder*/)
{
  m_factorType = factorType;
  m_filePath   = filePath;

  FactorCollection &factorCollection = FactorCollection::Instance();
  m_sentenceStart = factorCollection.AddFactor(BOS_);
  m_sentenceStartArray[m_factorType] = m_sentenceStart;
  m_sentenceEnd = factorCollection.AddFactor(EOS_);
  m_sentenceEndArray[m_factorType] = m_sentenceEnd;

  MappingBuilder builder(factorCollection, m_lmIdLookup);
  lm::ngram::Config config;

  IFVERBOSE(1) {
    config.messages = &std::cerr;
  }
  else {
    config.messages = NULL;
  }

  config.enumerate_vocab = &builder;
  config.load_method = m_lazy ? util::LAZY : util::POPULATE_OR_READ;

  try {
    m_ngram = new Model(filePath.c_str(), config);
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
    abort();
  }
  m_nGramOrder  = m_ngram->Order();

  m_nullContextState.state = m_ngram->NullContextState();
  m_beginSentenceState.state = m_ngram->BeginSentenceState();
  return true;
}

template <class Model> LMKenResult LanguageModelKen<Model>::GetKenFullScoreGivenState(const std::vector<const Word*> &contextFactor, FFState &state) const
{
  LMKenResult result;
  if (contextFactor.empty()) {
    result.score = 0.0;
    result.unknown = false;
    result.ngram_length = 0;
    return result;
  }
  lm::ngram::State &realState = static_cast<KenLMState&>(state).state;
  std::size_t factor = contextFactor.back()->GetFactor(GetFactorType())->GetId();
  lm::WordIndex new_word = (factor >= m_lmIdLookup.size() ? 0 : m_lmIdLookup[factor]);
  lm::ngram::State copied(realState);
  lm::FullScoreReturn ret(m_ngram->FullScore(copied, new_word, realState));

  result.score = TransformLMScore(ret.prob);
  result.unknown = (new_word == 0);
  result.ngram_length = ret.ngram_length;
  return result;
}

template <class Model> LMKenResult LanguageModelKen<Model>::GetKenFullScoreForgotState(const vector<const Word*> &contextFactor, FFState &outState) const
{
  LMKenResult result;
  if (contextFactor.empty()) {
    static_cast<KenLMState&>(outState).state = m_ngram->NullContextState();
    result.score = 0.0;
    result.unknown = false;
    result.ngram_length = 0;
    return result;
  }

  lm::WordIndex indices[contextFactor.size()];
  TranslateIDs(contextFactor, indices);

  lm::FullScoreReturn ret(m_ngram->FullScoreForgotState(indices + 1, indices + contextFactor.size(), indices[0], static_cast<KenLMState&>(outState).state));

  result.score = TransformLMScore(ret.prob);
  result.unknown = (indices[0] == 0);
  result.ngram_length = ret.ngram_length;
  return result;
}

template <class Model> void LanguageModelKen<Model>::GetState(const std::vector<const Word*> &contextFactor, FFState &outState) const
{
  if (contextFactor.empty()) {
    static_cast<KenLMState&>(outState).state = m_ngram->NullContextState();
    return;
  }
  lm::WordIndex indices[contextFactor.size()];
  TranslateIDs(contextFactor, indices);
  m_ngram->GetState(indices, indices + contextFactor.size(), static_cast<KenLMState&>(outState).state);
}

template <class Model> const FFState *LanguageModelKen<Model>::GetNullContextState() const
{
  return &m_nullContextState;
}

template <class Model> const FFState *LanguageModelKen<Model>::GetBeginSentenceState() const
{
  return &m_beginSentenceState;
}

template <class Model> FFState *LanguageModelKen<Model>::NewState(const FFState *from) const
{
  KenLMState *ret = new KenLMState;
  if (from) {
    ret->state = static_cast<const KenLMState&>(*from).state;
  }
  return ret;
}

template <class Model> lm::WordIndex LanguageModelKen<Model>::GetLmID(const std::string &str) const
{
  return m_ngram->GetVocabulary().Index(str);
}

} // namespace

LanguageModelSingleFactor *ConstructKenLM(const std::string &file, bool lazy)
{
  lm::ngram::ModelType model_type;
  if (lm::ngram::RecognizeBinary(file.c_str(), model_type)) {
    switch(model_type) {
    case lm::ngram::HASH_PROBING:
      return new LanguageModelKen<lm::ngram::ProbingModel>(lazy);
    case lm::ngram::TRIE_SORTED:
      return new LanguageModelKen<lm::ngram::TrieModel>(lazy);
    case lm::ngram::QUANT_TRIE_SORTED:
      return new LanguageModelKen<lm::ngram::QuantTrieModel>(lazy);
    case lm::ngram::ARRAY_TRIE_SORTED:
      return new LanguageModelKen<lm::ngram::ArrayTrieModel>(lazy);
    case lm::ngram::QUANT_ARRAY_TRIE_SORTED:
      return new LanguageModelKen<lm::ngram::QuantArrayTrieModel>(lazy);
    default:
      std::cerr << "Unrecognized kenlm model type " << model_type << std::endl;
      abort();
    }
  } else {
    return new LanguageModelKen<lm::ngram::ProbingModel>(lazy);
  }
}

}

