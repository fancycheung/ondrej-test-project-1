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

#ifndef moses_LanguageModelSkip_h
#define moses_LanguageModelSkip_h

#include <vector>
#include <algorithm>
#include "LanguageModelMultiFactor.h"
#include "LanguageModelSingleFactor.h"
#include "Phrase.h"
#include "FactorCollection.h"

namespace Moses
{

/* Hacked up LM which skips any factor with string '---'
* order of chunk hardcoded to 3 (m_realNGramOrder)
*/
class LanguageModelSkip : public LanguageModelSingleFactor
{
protected:
  size_t m_realNGramOrder;
  LanguageModelSingleFactor *m_lmImpl;

public:
  /** Constructor
  * \param lmImpl SRI, IRST, or Ken LM which this LM can use to load data
  */
  LanguageModelSkip(LanguageModelSingleFactor *lmImpl) {
    m_lmImpl = lmImpl;
  }
  ~LanguageModelSkip() {
    delete m_lmImpl;
  }

  bool Load(const std::string &filePath
            , FactorType factorType
            , size_t nGramOrder) {
    m_factorType 				= factorType;
    m_filePath 					= filePath;
    m_nGramOrder 				= nGramOrder;

    m_realNGramOrder 		= 3;

    FactorCollection &factorCollection = FactorCollection::Instance();

    m_sentenceStartArray[m_factorType] = factorCollection.AddFactor(Output, m_factorType, BOS_);
    m_sentenceEndArray[m_factorType] = factorCollection.AddFactor(Output, m_factorType, EOS_);

    return m_lmImpl->Load(filePath, m_factorType, nGramOrder);
  }

  const FFState *GetNullContextState() const {
    return m_lmImpl->GetNullContextState();
  }

  const FFState *GetBeginSentenceState() const {
    return m_lmImpl->GetBeginSentenceState();
  }

  FFState *NewState(const FFState *from = NULL) const {
    return m_lmImpl->NewState(from);
  }

  LMResult GetValueForgotState(const std::vector<const Word*> &contextFactor, FFState &outState) const {
    LMResult ret;
    if (contextFactor.size() == 0) {
      ret.score = 0.0;
      ret.unknown = false;
      return ret;
    }

    // only process context where last word is a word we want
    const Factor *factor = (*contextFactor.back())[m_factorType];
    std::string strWord = factor->GetString();
    if (strWord.find("---") == 0) {
      ret.score = 0.0;
      ret.unknown = false;
      return ret;
    }

    // add last word
    std::vector<const Word*> chunkContext;
    Word* chunkWord = new Word;
    chunkWord->SetFactor(m_factorType, factor);
    chunkContext.push_back(chunkWord);

    // create context in reverse 'cos we skip words we don't want
    for (int currPos = (int)contextFactor.size() - 2 ; currPos >= 0 && chunkContext.size() < m_realNGramOrder ; --currPos ) {
      const Word &word = *contextFactor[currPos];
      factor = word[m_factorType];
      std::string strWord = factor->GetString();
      bool skip = strWord.find("---") == 0;
      if (skip)
        continue;

      // add word to chunked context
      Word* chunkWord = new Word;
      chunkWord->SetFactor(m_factorType, factor);
      chunkContext.push_back(chunkWord);
    }

    // create context factor the right way round
    std::reverse(chunkContext.begin(), chunkContext.end());

    // calc score on chunked phrase
    ret = m_lmImpl->GetValueForgotState(chunkContext, outState);

    RemoveAllInColl(chunkContext);

    return ret;
  }
};

}

#endif

