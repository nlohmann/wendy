/*****************************************************************************\
 Wendy -- Synthesizing Partners for Services

 Copyright (c) 2009 Niels Lohmann, Christian Sura, and Daniela Weinberg

 Wendy is free software: you can redistribute it and/or modify it under the
 terms of the GNU Affero General Public License as published by the Free
 Software Foundation, either version 3 of the License, or (at your option)
 any later version.

 Wendy is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for
 more details.

 You should have received a copy of the GNU Affero General Public License
 along with Wendy.  If not, see <http://www.gnu.org/licenses/>.
\*****************************************************************************/

#pragma once

#include <set>
#include <string>
#include "StoredKnowledge.h"
#include "Clause.h"

/*!
  Livelock Operating Guideline only

  base class for annotating a set of knowledges

*/
class AnnotationElement {

        friend class AnnotationLivelockOG;
        friend class LivelockOperatingGuideline;

    public: /* static attributes */

        /// struct combining the statistics on the class AnnotationElement
        static struct _stats {
            /// constructor
            _stats();

            ///
            unsigned int cumulativeNumberOfClauses;

            /// maximal number of clauses per annotation element
            unsigned int maximalNumberOfClauses;

            /// number of true clauses
            unsigned int numberOfTrueAnnotations;
        } stats;

    public: /* member functions */

        AnnotationElement(const std::set<StoredKnowledge* > & _setOfKnowledges);

        virtual ~AnnotationElement();

        /// returns a string containing the annotation of this element
        virtual void myAnnotationToStream(const bool& dot, std::ostream& file, std::map<const StoredKnowledge*, unsigned int>& nodeMapping) const = 0;

    protected: /* member attributes */
        /// pointer to the set of knowledges
        StoredKnowledge** setOfKnowledges;

        /// succeeding element
        AnnotationElement* successor;

};

/*!
  Livelock Operating Guideline only

  annotation representing literal true only

*/
class AnnotationElementTrue: public AnnotationElement {

        friend class AnnotationLivelockOG;
        friend class LivelockOperatingGuideline;

    public: /* member functions */

        /// constructor
        AnnotationElementTrue(const std::set<StoredKnowledge* > & _setOfKnowledges);

        /// destructor
        ~AnnotationElementTrue();

        /// returns a string containing the annotation of this element
        void myAnnotationToStream(const bool& dot, std::ostream& file, std::map<const StoredKnowledge*, unsigned int>& nodeMapping) const;

};

/*!
  Livelock Operating Guideline only

  a set of knowledges is annotated with a set of transition literals (knowledge + label) or final states

*/
class AnnotationElementTransitionLabel: public AnnotationElement {

    public: /* member functions */

        /// constructor
        AnnotationElementTransitionLabel(const std::set<StoredKnowledge* > & _setOfKnowledges,
                                         const std::vector<Clause* > & _annotationBoolean);

        /// destructor
        ~AnnotationElementTransitionLabel();

        /// returns a string containing the annotation of this element
        void myAnnotationToStream(const bool& dot, std::ostream& file, std::map<const StoredKnowledge*, unsigned int>& nodeMapping) const;

    private: /* member attributes */

        /// vector of label ids representing the annotation
        Clause** annotationBool;

};

/*!
  Livelock Operating Guideline only

  a queue containing the annotation of the livelock operating guideline,
  hence the annotation of each strongly connected set of knowledges of the LL-OG
*/
class AnnotationLivelockOG {

    public: /* member functions */

        /// constructor
        AnnotationLivelockOG();

        ///destructor
        ~AnnotationLivelockOG();

        /// inserts an annotation to the queue
        void push(const std::set<StoredKnowledge* > & setOfKnowledges,
                  const std::vector<Clause* > & annotationBoolean);

        /// returns the first element of the queue
        AnnotationElement* pop();

        /// sets current pointer to the root element again
        void initIterator();

    private: /* member attributes */

        /// pointer to the first element of the queue
        AnnotationElement* rootElement;

        /// pointer to the last element of the queue
        AnnotationElement* lastElement;

        /// current pointer
        AnnotationElement* currentPointer;
};
