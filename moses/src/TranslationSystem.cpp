// $Id: $

/***********************************************************************
Moses - factored phrase-based language decoder
Copyright (C) 2010 University of Edinburgh

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

#include <stdexcept>
#include <iostream>

#include "StaticData.h"
#include "TranslationSystem.h"
#include "Util.h"

using namespace std;

namespace Moses {

    TranslationSystem::TranslationSystem(const std::string& config) {
        VERBOSE(2,"Creating translation system " << config << endl);
        vector<string> fields;
        Tokenize(fields,config);
        if (fields.size() % 2 != 1) {
            throw runtime_error("Incorrect number of fields in translation system config");
        }
        m_id = fields[0];
        for (size_t i = 1; i < fields.size(); i += 2) {
            const string& key = fields[i];
            vector<size_t> indexes;
            Tokenize<size_t>(fields[i+1],",");
            if (key == "L") {
                m_languageModelIds = indexes;
            } else if (key == "T") {
                m_phraseTableIds = indexes;
            } else if (key == "R") {
                m_reorderingTableIds = indexes;
            } else if (key == "G") {
                m_generationTableIds = indexes;
            } else {
                throw runtime_error("Unknown table id in translation systems config");
            }
        }
    }

};
