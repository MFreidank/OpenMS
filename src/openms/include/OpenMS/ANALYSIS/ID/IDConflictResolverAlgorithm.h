// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2017.
//
// This software is released under a three-clause BSD license:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of any author or any participating institution
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
// For a full list of authors, refer to the file AUTHORS.
// --------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
// INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// --------------------------------------------------------------------------
// $Maintainer: Hendrik Weisser $
// $Authors: Hendrik Weisser, Lucia Espona $
// --------------------------------------------------------------------------

#ifndef OPENMS_ANALYSIS_ID_IDCONFLICTRESOLVERALGORITHM
#define OPENMS_ANALYSIS_ID_IDCONFLICTRESOLVERALGORITHM

#include <OpenMS/APPLICATIONS/TOPPBase.h>

#include <OpenMS/FORMAT/ConsensusXMLFile.h>
#include <OpenMS/FORMAT/FeatureXMLFile.h>
#include <OpenMS/FORMAT/FileHandler.h>
#include <OpenMS/METADATA/PeptideIdentification.h>

#include <algorithm>

using namespace OpenMS;
using namespace std;

//-------------------------------------------------------------
// Doxygen docu
//-------------------------------------------------------------

/**
    @page TOPP_IDConflictResolver IDConflictResolver

    @brief Resolves ambiguous annotations of features with peptide identifications.

    The peptide identifications are filtered so that only one identification
    with a single hit (with the best score) is associated to each feature. (If
    two IDs have the same best score, either one of them may be selected.)


*/

namespace OpenMS
{

class OPENMS_DLLAPI IDConflictResolverAlgorithm
{
public:
  /** @brief Resolves ambiguous annotations of features with peptide identifications.
    The the filtered identifications are added to the vector of unassigned peptides
    and also reduced to a single best hit.
  **/
  static void resolve(FeatureMap & features);

  /** @brief Resolves ambiguous annotations of consensus features with peptide identifications.
    The the filtered identifications are added to the vector of unassigned peptides
    and also reduced to a single best hit.
  **/
  static void resolve(ConsensusMap & features);

  /** @brief In a single (feature/consensus) map, peptides with the same sequence and charge states may appear. 
    Only the feature with the highest intensity will pass this filter. 
    (Features without any identification pass this filter automatically)
  **/
  template<class T>
  static void makeUnique(T & map)
  {
    // A std::map tracking the set of unique features.
    // Uniqueness criterion/key is a pair <charge, sequence> for each feature.
    typedef std::map<std::pair<Int, AASequence>, typename T::value_type*> FeatureSet;
    FeatureSet feature_set;

    // Create a std::map `feature_set` mapping pairs <charge, sequence> to a pointer to
    // the feature with the highest intensity for this sequence.
    for (typename T::value_type& element : map)
    {
      std::vector<PeptideIdentification>& pep_ids = element.getPeptideIdentifications();

      if (!pep_ids.empty())
      {
        if (pep_ids.size() != 1) 
        {
          throw OpenMS::Exception::IllegalArgument(__FILE__, __LINE__, __FUNCTION__, "Feature does contain multiple identifications.");
        }

        // Assumption: first hit returned by `getHits()` has always highest search engine score.
        // TODO: Is this assumption reasonable or do we need to sort the hits first?
        std::vector<PeptideHit>& hits = pep_ids.front().getHits();

        if (!hits.empty()) 
        {
          PeptideHit& highest_score_hit = hits.front();
          
          // Pair <charge, sequence> of charge of the new feature and the sequence of its highest scoring peptide hit.
          std::pair<Int, AASequence> pair = std::make_pair(element.getCharge(), highest_score_hit.getSequence());

          // If a <charge, sequence> pair is not yet in the FeatureSet or new feature `feature_in_set`
          // has higher intensity than its counterpart `feature_set[<charge, sequence>]`
          // store a pointer to `feature_in_set` in `feature_set`.
          typename FeatureSet::iterator feature_in_set = feature_set.find(pair);
          if (feature_in_set != feature_set.end()) 
          {
            // Identical (charge, sequence) key found. Remove annotations from either the old or new feature.
            
            if (feature_in_set->second->getIntensity() < element.getIntensity()) 
            {
              // Remove annotations from the old low-intensity feature.
              std::vector<PeptideIdentification> pep_ids_empty;
              feature_in_set->second->setPeptideIdentifications(pep_ids_empty);
              
              // Replace feature in the set.
              feature_in_set->second = &(element);
            }
            else
            {
              // Remove annotations from the new low-intensity feature.
              std::vector<PeptideIdentification> pep_ids_empty;
              element.setPeptideIdentifications(pep_ids_empty);
            }
          }
          else 
          {
            // Feature is not yet in our set -- add it.
            feature_set[pair] = &(element);
          }
        }
      }
    }
  }

protected:

  template<class T>
  static void resolveConflict_(T & map)
  {
    // annotate as not part of the resolution
    for (PeptideIdentification & p : map.getUnassignedPeptideIdentifications())
    {
      p.setMetaValue("feature_id", "not mapped"); // not mapped to a feature
    }

    for (auto & c : map)
    {
      c.setMetaValue("feature_id", String(c.getUniqueId()));
      resolveConflict_(c.getPeptideIdentifications(), 
        map.getUnassignedPeptideIdentifications(),
        c.getUniqueId());
    }
  }

  // compare peptide IDs by score of best hit (hits must be sorted first!)
  // (note to self: the "static" is necessary to avoid cryptic "no matching
  // function" errors from gcc when the comparator is used below)
  static bool compareIDsSmallerScores_(const PeptideIdentification & left,
                          const PeptideIdentification & right);

  static void resolveConflict_(
    vector<PeptideIdentification> & peptides, 
    vector<PeptideIdentification> & removed,
    UInt64 uid);
};

}

#endif

