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

#include "StoredKnowledge.h"
#include "InnerMarking.h"
#include "InterfaceMarking.h"
#include "Clause.h"
#include "LivelockOperatingGuideline.h"


/*!
  Livelock Operating Guideline only

  a composite marking reflects a given stored knowledge, a certain inner marking and the respective interface

  \todo Make this a member of StoredKnowledge?
  \todo make dfs, lowlink to size_t
*/
class CompositeMarking {

    public: /* member functions */

        /// constructor
        CompositeMarking(const StoredKnowledge* _storedKnowledge,
                         const InnerMarking_ID _innerMarking_ID,
                         InterfaceMarking* _interface);

        /// destructor
        ~CompositeMarking();

        /// returns a string representing the annotation of the composite marking
        void getMyFormula(Clause* booleanClause, bool& emptyClause);

        /// compare two composite markings
        bool operator== (const CompositeMarking& other) const ;

        /// stream output operator
        friend std::ostream& operator<< (std::ostream&, const CompositeMarking&);

    public: /* member attributes */

        /// depth search number used for Tarjan algorithm
        unsigned int dfs;

        /// lowlink number used for Tarjan algorithm
        unsigned int lowlink;

        /// pointer to the stored knowledges associated with this composite marking
        const StoredKnowledge* storedKnowledge;

        /// pointer to the interface
        InterfaceMarking* interface;

        /// id of inner marking
        const InnerMarking_ID innerMarking_ID;
};


/*!
  Livelock Operating Guideline only

  takes care of composite markings, basically a name space to outsource specific attributes and functions

  \todo consider moving me to CompositeMarking as static stuff
*/
class CompositeMarkingsHandler {
    public: /* static functions */

        /// checks if the given marking has been visited already
        static CompositeMarking* isVisited(const StoredKnowledge* _storedKnowledge,
                                           const InnerMarking_ID _innerMarking_ID,
                                           InterfaceMarking* _interface,
                                           bool &foundMarking);

        /// stores given marking in the set of visited markings
        static void visitMarking(CompositeMarking* marking);

        /// initializes all values
        static void initialize(const std::set<StoredKnowledge* > & setOfStoredKnowledges);

        /// cleans up all members and composite markings stored
        static void finalize();

        /// adds the given clause to the set of annotations
        static void addClause(Clause* booleanClause);

    public: /* static attributes */

        /// an array storing all composite markings we have visited so far
        static CompositeMarking** visitedCompositeMarkings;

        /// stack of composite markings used for Tarjan algorithm
        static std::vector<CompositeMarking* > tarjanStack;

        /// number of elements stored so far
        static unsigned int numberElements;

        /// maximal number of elements that will be stored (sum of all inner markings of the current set of knowledges)
        /// \todo really needed?
        static unsigned int maxSize;

        /// last TSCC seen (Tarjan)
        static unsigned int minBookmark;

        /// represents the formula (set of disjunctions which each are a set again)
        static std::vector<Clause* > conjunctionOfDisjunctionsBoolean;
};
