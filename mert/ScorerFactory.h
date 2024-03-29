#ifndef __SCORER_FACTORY_H
#define __SCORER_FACTORY_H

#include <algorithm>
#include <cmath>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "Types.h"
#include "Scorer.h"
#include "BleuScorer.h"
#include "PerScorer.h"
#include "TerScorer.h"
#include "CderScorer.h"

using namespace std;

class ScorerFactory
{

public:
  vector<string> getTypes() {
    vector<string> types;
    types.push_back(string("BLEU"));
    types.push_back(string("PER"));
    types.push_back(string("TER"));
    types.push_back(string("CDER"));
    return types;
  }

  Scorer* getScorer(const string& type, const string& config = "") {
    if (type == "BLEU") {
      return (BleuScorer*) new BleuScorer(config);
    } else if (type == "PER") {
      return (PerScorer*) new PerScorer(config);
    } else if (type == "TER") {
      return (TerScorer*) new TerScorer(config);
    } else if (type == "CDER") {
      return (CderScorer*) new CderScorer(config);
    } else {
      throw runtime_error("Unknown scorer type: " + type);
    }
  }
};

#endif //__SCORER_FACTORY_H
