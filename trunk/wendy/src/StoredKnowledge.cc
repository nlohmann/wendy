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


#include <config.h>
#include <set>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>

#include "StoredKnowledge.h"
#include "Cover.h"
#include "verbose.h"
#include "cmdline.h"
#include "LivelockOperatingGuideline.h"
#include "AnnotationLivelockOG.h"
#include "util.h"

using std::map;
using std::set;
using std::string;
using std::vector;

extern gengetopt_args_info args_info;
extern string invocation;


/******************
 * STATIC MEMBERS *
 ******************/

std::map<hash_t, std::vector<StoredKnowledge*> > StoredKnowledge::hashTree;
StoredKnowledge* StoredKnowledge::root = NULL;
StoredKnowledge* StoredKnowledge::empty = reinterpret_cast<StoredKnowledge*>(1);
std::set<StoredKnowledge*> StoredKnowledge::deletedNodes;
std::set<StoredKnowledge*> StoredKnowledge::seen;
std::vector<StoredKnowledge*> StoredKnowledge::tarjanStack;
unsigned int StoredKnowledge::bookmarkTSCC = 0;
bool StoredKnowledge::emptyNodeReachable = false;
StoredKnowledge::_stats StoredKnowledge::stats;
std::map<const StoredKnowledge*, std::pair<unsigned int, unsigned int> > StoredKnowledge::tarjanMapping;


/********************
 * STATIC FUNCTIONS *
 ********************/

/*!
 \note maxBucketSize must be initialized to 1
 */
StoredKnowledge::_stats::_stats()
    : hashCollisions(0), storedEdges(0), builtInsaneNodes(0),
      maxBucketSize(1), storedKnowledges(0), maxSCCSize(0),
      numberOfNonTrivialSCCs(0), numberOfTrivialSCCs(0) {}


/*!
 \param[in] K   a knowledge bubble (explicitly stored)
 \param[in] SK  a knowledge bubble (compactly stored)
 \param[in] l   a label
 */
void StoredKnowledge::processSuccessor(const Knowledge* K,
                                       StoredKnowledge* const SK,
                                       const Label_ID& l) {
    // create a new knowledge for the given label
    Knowledge* K_new = new Knowledge(K, l);

    // do not store and process the empty node explicitly
    if (K_new->size == 0) {
        if (K_new->is_sane) {
            SK->addSuccessor(l, empty);
        }
        delete K_new;
        return;
    }

    // only process knowledges within the message bounds
    if (K_new->is_sane or args_info.diagnose_given) {
        // create a compact version of the knowledge bubble
        StoredKnowledge* SK_new = new StoredKnowledge(K_new);

        // add it to the knowledge tree
        StoredKnowledge* SK_store = SK_new->store();

        // store an edge from the parent to this node
        SK->addSuccessor(l, SK_store);

        // evaluate the storage result
        if (SK_store == SK_new) {
            if (K_new->is_sane) {
                // the node was new and sane, so check its successors
                processNode(K_new, SK_store);
            }
        } else {
            // we did not find new knowledge
            delete SK_new;
        }

        // the successors of the new knowledge have been calculated, so adjust lowlink value of SK
        SK->adjustLowlinkValue(SK_store, SK_store == SK_new);
    } else {
        // the node was not sane -- count it
        ++stats.builtInsaneNodes;
    }

    delete K_new;
}


/*!
 \param[in] K   a knowledge bubble (explicitly stored)
 \param[in] SK  a knowledge bubble (compactly stored)

 \note possible optimization: don't create a copy for the last label but use
       the object itself
 */
void StoredKnowledge::processNode(const Knowledge* K, StoredKnowledge* const SK) {
    // traverse the labels of the interface and process K's successors
    for (Label_ID l = Label::first_receive; l <= Label::last_sync; ++l) {

        // reduction rule: send leads to insane node
        if (not args_info.ignoreUnreceivedMessages_flag and SENDING(l) and not K->considerSendingEvent(l)) {
            continue;
        }

        // reduction rule: sequentialize receiving events
        // if current receiving event is not to be considered, continue
        if (args_info.seqReceivingEvents_flag and RECEIVING(l) and not K->considerReceivingEvent(l)) {
            continue;
        }

        // reduction rule: receive before send
        if (args_info.receivingBeforeSending_flag and not RECEIVING(l) and K->receivingHelps()) {
            break;
        }

        // reduction rule: only consider waitstates
        if (args_info.waitstatesOnly_flag and not RECEIVING(l) and not K->resolvableWaitstate(l)) {
            continue;
        }

        // recursion
        processSuccessor(K, SK, l);

        // reduction rule: quit, once all waitstates are resolved
        if (args_info.quitAsSoonAsPossible_flag and SK->sat(true)) {
            break;
        }

        // reduction rule: stop considering another sending event, if the
        // latest sending event considered succeeded
        /// \todo what about synchronous events?
        if (args_info.succeedingSendingEvent_flag and SENDING(l) and SK->sat(true)) {
            if ((args_info.correctness_arg != correctness_arg_livelock) or SK->is_final_reachable) {
                l = Label::first_sync;
            }
        }
    }

    SK->evaluateKnowledge();
}


/*!
 create the predecessor relation of all knowledges contained in the given set
 and then evaluate each member of the given set of knowledges and propagate
 the property of being insane accordingly

 \pre  knowledgeSet contains all nodes (knowledges) of the current SCC
 \post knowledgeSet is empty
*/
void StoredKnowledge::analyzeSCCOfKnowledges(std::set<StoredKnowledge*>& knowledgeSet) {
    // a temporary data structure to store the predecessor relation
    std::map<StoredKnowledge*, std::set<StoredKnowledge*> > tempPredecessors;

    // remember if from at least one node of the current SCC a final node is reachable
    bool is_final_reachable = false;

    // if it is not a TSCC, we have to evaluate each member of the SCC
    // first, we generate the predecessor relation between the members
    FOREACH(iScc, knowledgeSet) {
        // for each successor which is part of the current SCC, register the predecessor
        for (Label_ID l = Label::first_receive; l <= Label::last_sync; ++l) {
            if ((**iScc).successors[l - 1] != NULL and(**iScc).successors[l - 1] != empty and
                    knowledgeSet.find((**iScc).successors[l - 1]) != knowledgeSet.end()) {

                tempPredecessors[(**iScc).successors[l - 1]].insert(*iScc);
            }
            // check if there exists a successor (within or outside of the current SCC) from which a
            // final node is reachable
            if ((**iScc).successors[l - 1] != NULL and(**iScc).successors[l - 1] != empty and
                    (**iScc).successors[l - 1]->is_final_reachable) {
                is_final_reachable = true;
            }
        }
    }

    // propagate that at least from one node of the current SCC a final node is reachable
    if (is_final_reachable) {
        FOREACH(iScc, knowledgeSet) {
            (**iScc).is_final_reachable = true;
        }
    }

    // evaluate each member
    while (not knowledgeSet.empty()) {
        StoredKnowledge* currentKnowledge = *knowledgeSet.begin();
        knowledgeSet.erase(knowledgeSet.begin());

        if (currentKnowledge->is_sane and not currentKnowledge->sat()) {
            currentKnowledge->is_sane = 0;

            // tell each predecessor of current knowledge that this knowledge switched its status to insane
            /// \todo looks as if a set union would do the job here
            knowledgeSet.insert(tempPredecessors[currentKnowledge].begin(), tempPredecessors[currentKnowledge].end());

        } else if (args_info.correctness_arg == correctness_arg_livelock and currentKnowledge->is_sane) {
            currentKnowledge->is_final_reachable = 1;
        }
    }
}


/*!
  move all transient markings to the end of the array and adjust size of the
  markings array

  \post all deadlock markings can be found between index 0 and sizeDeadlockMarkings
 */
void StoredKnowledge::rearrangeKnowledgeBubble() {
    // check the stored markings and remove all transient states
    innermarkingcount_t j = 0;

    while (j < sizeDeadlockMarkings) {
        assert(interface[j]);

        // find out whether marking is transient
        bool transient = false;

        // case 1: a final marking that is not a waitstate
        if (InnerMarking::inner_markings[inner[j]]->is_final and interface[j]->unmarked()) {
            // remember that this knowledge contains a final marking
            is_final = is_final_reachable = 1;

            // only if the final marking is not a waitstate, we're done
            if (not InnerMarking::inner_markings[inner[j]]->is_waitstate) {
                transient = true;
            }
        }

        // case 2: a resolved waitstate
        if (InnerMarking::inner_markings[inner[j]]->is_waitstate) {
            // check if DL is resolved by interface marking
            for (Label_ID l = Label::first_send; l <= Label::last_send; ++l) {
                if (interface[j]->marked(l) and
                        InnerMarking::receivers[l].find(inner[j]) != InnerMarking::receivers[l].end()) {
                    transient = true;
                }
            }
        } else {
            // case 3: a transient marking
            transient = true;
        }

        // "hide" transient markings behind the end of the array
        if (transient) {
            InnerMarking_ID temp_inner = inner[j];
            InterfaceMarking* temp_interface = interface[j];

            inner[j] = inner[ sizeDeadlockMarkings - 1 ];
            interface[j] = interface[ sizeDeadlockMarkings - 1 ];

            inner[ sizeDeadlockMarkings - 1 ] = temp_inner;
            interface[ sizeDeadlockMarkings - 1 ] = temp_interface;

            --sizeDeadlockMarkings;
        } else {
            ++j;
        }
    }
}


/******************************************
 * CONSTRUCTOR, DESTRUCTOR, AND FINALIZER *
 ******************************************/

/*!
 converts a Knowledge object into a StoredKnowledge object

 \note The arrays for the successors is allocated with calloc to avoid a for
       loop to set the entries to NULL.

 \param[in] K  the knowledge to copy from
*/
StoredKnowledge::StoredKnowledge(const Knowledge* K)
    : is_final(0), is_final_reachable(0), is_sane(K->is_sane),
      is_on_tarjan_stack(1), sizeDeadlockMarkings(K->size),
      sizeAllMarkings(K->size),
      // reserve the necessary memory for the internal and interface markings
      inner(new InnerMarking_ID[sizeAllMarkings]),
      interface(new InterfaceMarking*[sizeAllMarkings]),
      // reserve and zero the necessary memory for the successors (fixed)
      successors((StoredKnowledge**)calloc(Label::events, SIZEOF_VOIDP)) {
    assert(sizeAllMarkings > 0);

    // copy data structure to C-style arrays
    innermarkingcount_t count = 0;

    // traverse the bubble and copy the markings into the C arrays
    FOREACH(pos, K->bubble) {
        for (innermarkingcount_t i = 0; i < pos->second.size(); ++i, ++count) {
            // copy the inner marking and the interface marking
            inner[count] = pos->first;
            interface[count] = new InterfaceMarking(*(pos->second[i]));
        }
    }

    // we must not forget a marking
    assert(sizeAllMarkings == count);

    // check this knowledge for covering nodes
    if (args_info.cover_given) {
        Cover::checkKnowledge(this, K->bubble);
    }

    // move waitstates to front of bubble
    rearrangeKnowledgeBubble();
}


StoredKnowledge::~StoredKnowledge() {
    // delete the interface markings
    for (innermarkingcount_t i = 0; i < sizeAllMarkings; ++i) {
        delete interface[i];
    }
    delete[] interface;

    delete[] inner;
    free(successors);

    if (args_info.cover_given) {
        Cover::removeKnowledge(this);
    }

    tarjanMapping.erase(this);
}


void StoredKnowledge::finalize() {
    unsigned int count = 0;

    FOREACH(it, hashTree) {
        for (size_t i = 0; i < it->second.size(); ++i, ++count) {
            delete it->second[i];
        }
    }

    status("StoredKnowledge: deleted %d objects", count);
}


/********************
 * MEMBER FUNCTIONS *
 ********************/

/*!
 adjust lowlink value of stored knowledge object according to Tarjan algorithm

 \param SK           new knowledge whose successors have been calculated completely
 \param newKnowledge SK has been newly created in calling function process()
*/
void StoredKnowledge::adjustLowlinkValue(const StoredKnowledge* const SK, const bool newKnowledge) const {
    // successor node is not new
    if (not newKnowledge) {
        if (SK->is_on_tarjan_stack) {
            // but it is still on the stack, compare lowlink and dfs value
            tarjanMapping[this].second = MINIMUM(tarjanMapping[this].second, tarjanMapping[SK].first);
        }
    } else {
        // successor node is new, compare lowlink values
        tarjanMapping[this].second = MINIMUM(tarjanMapping[this].second, tarjanMapping[SK].second);
    }
}


/*!
 if current knowledge is representative of an SCC (of knowledges) then evaluate current SCC
*/
void StoredKnowledge::evaluateKnowledge() {
    assert(not tarjanStack.empty());

    // check, if the current knowledge is a representative of a SCC
    // if so, get all knowledges within the SCC
    if (tarjanMapping[this].first == tarjanMapping[this].second) {

        unsigned int numberOfSccElements = 0;

        // node which has just been popped from Tarjan stack
        StoredKnowledge* poppedSK;

        // set of knowledges
        // first used to store the whole SCC, then it is used as a set storing those
        // knowledges that have to be evaluated again
        std::set<StoredKnowledge*> knowledgeSet;

        // found a TSCC
        if (tarjanMapping[this].first > StoredKnowledge::bookmarkTSCC) {
            StoredKnowledge::bookmarkTSCC = tarjanMapping[this].first;
        }

        // get (T)SCC
        do {
            // get and delete last element from stack
            poppedSK = tarjanStack.back();
            tarjanStack.pop_back();

            // remember that we removed this knowledge from the Tarjan stack
            poppedSK->is_on_tarjan_stack = 0;

            // remember this knowledge to be part of the current (T)SCC
            knowledgeSet.insert(poppedSK);

            ++numberOfSccElements;
        } while (this != poppedSK);


        // check if (T)SCC is trivial
        if (numberOfSccElements == 1) {
            // check if every deadlock is resolved -> if not, this knowledge is definitely not sane
            // deadlock freedom or livelock freedom will be treated in sat()
            (**knowledgeSet.begin()).is_sane = (**knowledgeSet.begin()).sat();

            ++stats.numberOfTrivialSCCs;
        } else if (numberOfSccElements > 1) {
            analyzeSCCOfKnowledges(knowledgeSet);

            stats.maxSCCSize = MAXIMUM(stats.maxSCCSize, numberOfSccElements);
            ++stats.numberOfNonTrivialSCCs;
        }
    }
}


/*!
 \return a pointer to a knowledge stored in the hash tree -- it is either
         "this" if the knowledge was not found in the hash tree or a pointer
         to a previously stored knowledge. In the latter case, the calling
         function can detect the duplicate
 */
StoredKnowledge* StoredKnowledge::store() {
    // we do not want to store the empty node
    assert(sizeAllMarkings != 0);

    // get the element's hash value
    const hash_t myHash(hash());

    // check if we find a bucket with that hash value
    std::map<hash_t, std::vector<StoredKnowledge*> >::const_iterator el = hashTree.find(myHash);
    if (el != hashTree.end()) {
        // we found an element with the same hash -- is it a collision?
        for (size_t i = 0; i < el->second.size(); ++i) {
            assert(el->second[i]);

            // compare the sizes
            if (sizeAllMarkings == el->second[i]->sizeAllMarkings) {
                // if still true after the loop, we found previously stored knowledge
                bool found = true;

                // compare the inner and interface markings
                for (innermarkingcount_t j = 0; (j < sizeAllMarkings and found); ++j) {
                    if (inner[j] != el->second[i]->inner[j] or
                            *interface[j] != *el->second[i]->interface[j]) {
                        found = false;
                        break;
                    }
                }

                // check if we found a previously stored knowledge
                if (found) {
                    // we found a previously stored element with the same
                    // markings -> return a pointer to this element
                    return el->second[i];
                }
            }
        }

        // this object was not found in the bucket -- this is a collision
        ++stats.hashCollisions;

        // update maximal bucket size (we add 1 as we will store this object later)
        stats.maxBucketSize = MAXIMUM(stats.maxBucketSize, hashTree[myHash].size() + 1);
    }

    // we need to store this object
    hashTree[myHash].push_back(this);
    ++stats.storedKnowledges;

    // set Tarjan values (first == dfs; second == lowlink)
    tarjanMapping[this].first = tarjanMapping[this].second = stats.storedKnowledges;

    // put knowledge on the Tarjan stack
    tarjanStack.push_back(this);

    // we return a pointer to the this object since it was newly stored
    return this;
}


hash_t StoredKnowledge::hash() const {
    hash_t result(0);

    for (innermarkingcount_t i = 0; i < sizeAllMarkings; ++i) {
        result += ((1 << i) * (inner[i]) + interface[i]->hash());
    }

    return result;
}


void StoredKnowledge::addSuccessor(const Label_ID& label, StoredKnowledge* const knowledge) {
    // we will never store label 0 (tau) -- hence decrease the label
    successors[label - 1] = knowledge;

    // statistics output
    if (args_info.reportFrequency_arg and ++stats.storedEdges % args_info.reportFrequency_arg == 0) {
        message("%8d knowledges, %8d edges", stats.storedKnowledges, stats.storedEdges);
    }
}


/*!
 \return whether each deadlock in the knowledge is resolved

 \param checkOnTarjanStack in case of reduction rules "quit as soon as possible" and "succeeding sending event"
                           it has to be ensured that the successor node has been completely analyzed and thus
                           is off the Tarjan stack

 \pre the markings in the array (0 to sizeDeadlockMarkings-1) are deadlocks -- all transient
      states or unmarked final markings are removed from the marking array
*/
bool StoredKnowledge::sat(const bool checkOnTarjanStack) {

    bool is_on_tarjan_stack = false;

    // if we find a sending successor, this node is OK
    for (Label_ID l = Label::first_send; l <= Label::last_send; ++l) {
        if (successors[l - 1] != NULL and successors[l - 1] != empty and successors[l - 1]->is_sane) {

            if (checkOnTarjanStack and successors[l - 1]->is_on_tarjan_stack) {
                is_on_tarjan_stack = true;
                continue;
            }

            if (args_info.correctness_arg == correctness_arg_livelock and not successors[l - 1]->is_final_reachable) {
                continue;
            }

            is_final_reachable = successors[l - 1]->is_final_reachable;

            return true;
        }
    }

    bool resolved = false;

    // now each deadlock must have ?-successors
    for (innermarkingcount_t i = 0; i < sizeDeadlockMarkings; ++i) {
        resolved = false;

        // we found a deadlock -- check whether for at least one marked
        // output place exists a respective receiving edge
        for (Label_ID l = Label::first_receive; l <= Label::last_receive; ++l) {

            if (interface[i]->marked(l) and successors[l - 1] != NULL and successors[l - 1] != empty and
                    successors[l - 1]->is_sane) {

                if (checkOnTarjanStack and successors[l - 1]->is_on_tarjan_stack) {
                    is_on_tarjan_stack = true;
                    continue;
                }

                if (args_info.correctness_arg == correctness_arg_livelock and not successors[l - 1]->is_final_reachable) {
                    continue;
                }

                is_final_reachable = successors[l - 1]->is_final_reachable;

                resolved = true;
                break;
            }
        }

        // deadlock is resolved, so check the next deadlock
        if (resolved) {
            continue;
        }

        // check if a synchronous action can resolve this deadlock
        for (Label_ID l = Label::first_sync; l <= Label::last_sync; ++l) {
            if (InnerMarking::synchs[l].find(inner[i]) != InnerMarking::synchs[l].end() and
                    successors[l - 1] != NULL and successors[l - 1] != empty and successors[l - 1]->is_sane) {

                if (checkOnTarjanStack and successors[l - 1]->is_on_tarjan_stack) {
                    is_on_tarjan_stack = true;
                    continue;
                }

                if (args_info.correctness_arg == correctness_arg_livelock and not successors[l - 1]->is_final_reachable) {
                    continue;
                }

                is_final_reachable = successors[l - 1]->is_final_reachable;

                resolved = true;
                break;
            }
        }

        // the deadlock is neither resolved nor a final marking
        if (not resolved and not(InnerMarking::inner_markings[inner[i]]->is_final and interface[i]->unmarked())) {
            return false;
        }
    }

    // in case of livelock freedom
    // current knowledge is definitely not sane iff
    // (1) no deadlock has been resolved, which means that there are no deadlocks because otherwise we would have returned false already
    // (2) the current knowledge is not on the Tarjan stack anymore
    // (3) no final knowledge is reachable
    if (args_info.correctness_arg == correctness_arg_livelock and not resolved and not is_on_tarjan_stack and not is_final_reachable) {
        return false;
    }

    // if we reach this line, every deadlock was resolved
    return true;
}


/*!
 \post All nodes that are reachable from the initial node are added to the
       set "seen".
 */
void StoredKnowledge::traverse() {
    if (seen.insert(this).second) {
        for (Label_ID l = Label::first_receive; l <= Label::last_sync; ++l) {
            if (successors[l - 1] != NULL and successors[l - 1] != empty and
                    (successors[l - 1]->is_sane or args_info.diagnose_given)) {
                successors[l - 1]->traverse();
            }
        }
    }
}


/****************************************
 * OUTPUT FUNCTIONS (STATIC AND MEMBER) *
 ****************************************/

void StoredKnowledge::fileHeader(std::ostream& file) {
    file << "{\n  generator:    " << PACKAGE_STRING
         << " (" << CONFIG_BUILDSYSTEM ")"
         << "\n  invocation:   " << invocation << "\n  events:       "
         << static_cast<unsigned int>(Label::send_events) << " send, "
         << static_cast<unsigned int>(Label::receive_events) << " receive, "
         << static_cast<unsigned int>(Label::sync_events) << " synchronous"
         << "\n  statistics:   " << seen.size() << " nodes";

    if (args_info.correctness_arg == correctness_arg_livelock and args_info.og_given) {
        file << ", " << LivelockOperatingGuideline::stats.numberOfSCSs << " SCSs";
    }

    file << "\n}\n\n";
}

/*!
  \param[in,out] file  the output stream to write the OG to

  \note Fiona identifies node numbers by integers. To avoid numbering of
        nodes, the pointers are casted to integers. Though ugly, it still is
        a valid numbering.
  \todo Create a filter that maps the pointer addresses to human-readable numbers.
 */
void StoredKnowledge::output_og(std::ostream& file) {
    fileHeader(file);
    file << "INTERFACE\n";

    if (Label::receive_events > 0) {
        file << "  INPUT\n    ";
        bool first = true;
        for (Label_ID l = Label::first_receive; l <= Label::last_receive; ++l) {
            if (not first) {
                file << ", ";
            }
            first = false;
            file << Label::id2name[l];
        }
        file << ";\n";
    }

    if (Label::send_events > 0) {
        file << "  OUTPUT\n    ";
        bool first = true;
        for (Label_ID l = Label::first_send; l <= Label::last_send; ++l) {
            if (not first) {
                file << ", ";
            }
            first = false;
            file << Label::id2name[l];
        }
        file << ";\n";
    }

    if (Label::sync_events > 0) {
        file << "  SYNCHRONOUS\n    ";
        bool first = true;
        for (Label_ID l = Label::first_sync; l <= Label::last_sync; ++l) {
            if (not first) {
                file << ", ";
            }
            first = false;
            file << Label::id2name[l];
        }
        file << ";\n";
    }

    // livelock operating guideline has been calculated
    if (args_info.correctness_arg == correctness_arg_livelock and args_info.og_given) {

        std::map<const StoredKnowledge*, unsigned int> nodeMapping;

        // get the annotations of the livelock operating guideline
        LivelockOperatingGuideline::output(false, file, nodeMapping);
    }

    file << "\nNODES\n";

    // the root
    root->print(file);

    // all other nodes
    FOREACH(it, seen) {
        if (*it != root) {
            (**it).print(file);
        }
    }

    // print empty node unless we print an automaton
    if (not args_info.sa_given and emptyNodeReachable) {
        // the empty node
        file << "  0 : true\n";

        // empty node loops
        for (Label_ID l = Label::first_receive; l <= Label::last_sync; ++l) {
            file << "    " << Label::id2name[l]  << " -> 0\n";
        }
    }
}


/*!
  \bug Final states should not have outgoing tau or sending events.
 */
void StoredKnowledge::print(std::ostream& file) const {
    file << "  " << reinterpret_cast<size_t>(this);

    if (args_info.sa_given) {
        if (this == root and is_final) {
            file << " : INITIAL, FINAL";
        } else {
            if (this == root) {
                file << " : INITIAL";
            }
            if (is_final) {
                file << " : FINAL";
            }
        }
    } else if (not(args_info.correctness_arg == correctness_arg_livelock and args_info.og_given)) {
        file << " : " << formula();
    }

    file << "\n";

    for (Label_ID l = Label::first_receive; l <= Label::last_sync; ++l) {
        if (successors[l - 1] != NULL and
                successors[l - 1] != empty and
                (seen.find(successors[l - 1]) != seen.end())) {
            file << "    " << Label::id2name[l] << " -> "
                 << reinterpret_cast<size_t>(successors[l - 1])
                 << "\n";
        } else {
            if (successors[l - 1] == empty and not args_info.sa_given) {
                emptyNodeReachable = true;
                file << "    " << Label::id2name[l] << " -> 0\n";
            }
        }
    }
}


/*!
 \return a string representation of the formula

 \param dot whether this formula should be drawn in dot mode

 \note This function is also used for an operating guidelines output for
       Fiona.
*/
std::string StoredKnowledge::formula(bool dot) const {
    set<string> sendDisjunction;

    // represents the formula (set of disjunctions which each are a set again)
    set<set<string> > conjunctionOfDisjunctions;

    unsigned int countLiterals = 0;

    // collect outgoing !-edges
    for (Label_ID l = Label::first_send; l <= Label::last_send; ++l) {
        if (successors[l - 1] != NULL and successors[l - 1]->is_sane) {

            sendDisjunction.insert(Label::id2name[l]);
        }
    }

    // traverse through the deadlock markings and
    // collect outgoing ?-edges and #-edges for the deadlocks
    bool dl_found = false;
    for (innermarkingcount_t i = 0; i < sizeDeadlockMarkings; ++i) {
        dl_found = true;

        // add sending events to current disjunction
        set<string> disjunctionSendingReceivingSynchronous(sendDisjunction);

        // receiving event
        for (Label_ID l = Label::first_receive; l <= Label::last_receive; ++l) {
            // receiving event resolves deadlock
            if (interface[i]->marked(l) and
                    successors[l - 1] != NULL and successors[l - 1] != empty and
                    successors[l - 1]->is_sane) {

                disjunctionSendingReceivingSynchronous.insert(Label::id2name[l]);
            }
        }

        // synchronous communication
        for (Label_ID l = Label::first_sync; l <= Label::last_sync; ++l) {
            // synchronous communication resolves deadlock
            if (InnerMarking::synchs[l].find(inner[i]) != InnerMarking::synchs[l].end() and
                    successors[l - 1] != NULL and successors[l - 1] != empty and
                    successors[l - 1]->is_sane) {

                disjunctionSendingReceivingSynchronous.insert(Label::id2name[l]);
            }
        }

        // deadlock is final
        if (interface[i]->unmarked() and InnerMarking::inner_markings[inner[i]]->is_final) {
            disjunctionSendingReceivingSynchronous.insert("final");
        }

        // add clause
        conjunctionOfDisjunctions.insert(disjunctionSendingReceivingSynchronous);

        if (args_info.correctness_arg == correctness_arg_livelock and args_info.og_given) {
            countLiterals += disjunctionSendingReceivingSynchronous.size();
        }
    }

    // all markings are transient
    if (not dl_found) {
        return "true";
    }

    // create the formula
    string formula;

    if (not conjunctionOfDisjunctions.empty()) {
        // traverse the conjunctions to access the disjunctions
        FOREACH(it, conjunctionOfDisjunctions) {
            if (it != conjunctionOfDisjunctions.begin()) {
                formula += (dot ? " &and; " : " * ");
            }
            if (it->size() > 1) {
                formula += "(";
            }

            // get clause which contains !, ? or # events
            FOREACH(it2, *it) {
                if (it2 != it->begin()) {
                    formula += (dot ? " &or; " : " + ");
                }
                formula += *it2;
            }
            if (it->size() > 1) {
                formula += ")";
            }
        }
    }

    // required for diagnose mode in which a non-final node might have no
    // successors
    if (formula.empty()) {
        formula = "false";

        ++countLiterals;
    }

    if (args_info.correctness_arg == correctness_arg_livelock and args_info.og_given) {

        AnnotationElement::stats.cumulativeNumberOfClauses += sizeDeadlockMarkings;

        if (sizeDeadlockMarkings > AnnotationElement::stats.maximalNumberOfClauses) {
            AnnotationElement::stats.maximalNumberOfClauses = sizeDeadlockMarkings;
        }

        LivelockOperatingGuideline::stats.numberOfTSCCInSCSs += sizeDeadlockMarkings;
        Clause::stats.cumulativeSizeAllClauses += countLiterals;

        if (countLiterals > Clause::stats.maximalSizeOfClause) {
            Clause::stats.maximalSizeOfClause = countLiterals;
        }
    }

    return formula;
}


/*!
  \param[in,out] file  the output stream to write the dot representation to

  \note  The empty node has the number 0 and is only drawn if the command line
         parameter "--showEmptyNode" was given. For each node and receiving
         label, we add an edge to the empty node if this edge is not present
         before.
*/
void StoredKnowledge::output_dot(std::ostream& file) {

    // store a (human-readable) number for each node found in case the parameter showInternalNodeNames has not been set
    std::map<const StoredKnowledge*, unsigned int> nodeMapping;

    file << "digraph G {\n";

    // livelock operating guideline has been calculated
    if (args_info.correctness_arg == correctness_arg_livelock and args_info.og_given) {
        // generate dot output showing the annotation of the livelock operating guideline
        LivelockOperatingGuideline::output(true, file, nodeMapping);
    }

    file  << " node [fontname=\"Helvetica\" fontsize=10]\n"
          << " edge [fontname=\"Helvetica\" fontsize=10]\n";

    // create invisible node (in order to mark the initial state)
    file << "INIT [label=\"\" height=\"0.01\" width=\"0.01\" style=\"invis\"]\n";
    file << "INIT -> \"" << root << "\" [minlen=\"0.5\"]" << "\n";

    // draw the nodes
    FOREACH(it, hashTree) {
        for (size_t i = 0; i < it->second.size(); ++i) {
            if ((it->second[i]->is_sane or args_info.diagnose_given) and
                    (seen.find(it->second[i]) != seen.end())) {

                file << "\"" << it->second[i] << "\" [label=\"";

                // livelock operating guideline has been calculated
                if (args_info.correctness_arg == correctness_arg_livelock and args_info.og_given) {
                    if (not args_info.showInternalNodeNames_flag) {
                        if (nodeMapping.find(it->second[i]) == nodeMapping.end()) {
                            nodeMapping[it->second[i]] = nodeMapping.size();
                        }
                        // formula is not shown, but node number is shown
                        file << nodeMapping[it->second[i]] << "\\n";
                    } else {
                        // formula is not shown, but node number is shown
                        file << reinterpret_cast<size_t>(it->second[i]) << "\\n";
                    }
                } else if (not args_info.sa_given) {
                    // show only formula
                    file << it->second[i]->formula(true) << "\\n";
                }

                if (args_info.diagnose_given and not it->second[i]->is_sane) {
                    file << "is not sane\\n";
                }

                if (args_info.showWaitstates_flag) {
                    for (innermarkingcount_t j = 0; j < it->second[i]->sizeDeadlockMarkings; ++j) {
                        file << "m" << static_cast<size_t>(it->second[i]->inner[j]) << " ";
                        file << *(it->second[i]->interface[j]) << " (w)\\n";
                    }
                }

                if (args_info.showTransients_flag) {
                    for (innermarkingcount_t j = it->second[i]->sizeDeadlockMarkings; j < it->second[i]->sizeAllMarkings; ++j) {
                        file << "m" << static_cast<size_t>(it->second[i]->inner[j]) << " ";
                        file << *(it->second[i]->interface[j]) << " (t)\\n";
                    }
                }

                file << "\"]\n";

                // draw the edges
                for (Label_ID l = Label::first_receive; l <= Label::last_sync; ++l) {
                    if (it->second[i]->successors[l - 1] != NULL and
                            (seen.find(it->second[i]->successors[l - 1]) != seen.end()) and
                            (args_info.showEmptyNode_flag or it->second[i]->successors[l - 1] != empty)) {
                        file << "\"" << it->second[i] << "\" -> \""
                             << it->second[i]->successors[l - 1]
                             << "\" [label=\"" << PREFIX(l)
                             << Label::id2name[l] << "\"]\n";
                    }

                    // draw edges to the empty node if requested
                    if (args_info.showEmptyNode_flag and
                            it->second[i]->successors[l - 1] == empty) {
                        emptyNodeReachable = true;
                        file << "\"" << it->second[i] << "\" -> 0"
                             << " [label=\"" << PREFIX(l)
                             << Label::id2name[l] << "\"]\n";
                    }
                }
            }
        }
    }

    // draw the empty node if it is requested and reachable
    if (args_info.showEmptyNode_flag and emptyNodeReachable) {
        file << "0 [label=\"true\"]\n";

        for (Label_ID l = Label::first_receive; l <= Label::last_sync; ++l) {
            file << "0 -> 0 [label=\"" << PREFIX(l) << Label::id2name[l] << "\"]\n";
        }
    }

    file << "}\n";
}


void StoredKnowledge::output_migration(std::ostream& o) {
    map<InnerMarking_ID, map<StoredKnowledge*, set<InterfaceMarking*> > > migrationInfo;

    FOREACH(it, seen) {
        // traverse the bubble
        for (innermarkingcount_t i = 0; i < (**it).sizeAllMarkings; ++i) {
            InnerMarking_ID inner = (**it).inner[i];
            InterfaceMarking* interface = (**it).interface[i];
            StoredKnowledge* knowledge = *it;

            migrationInfo[inner][knowledge].insert(interface);
        }
    }

    // print information on the interface
    o << "INTERFACE [";
    for (Label_ID l = Label::first_receive; l <= Label::last_sync; ++l) {
        if (l > Label::first_receive) {
            o << ",";
        }
        o << Label::id2name[l];
    }
    o << "]\n\n";

    // iterate the inner markings
    FOREACH(it1, migrationInfo) {
        // iterate the interface markings
        FOREACH(it2, it1->second) {
            // iterate the knowledge bubbles
            FOREACH(it3, it2->second) {
                o << (size_t) it1->first << " " << (size_t) it2->first << " " << **it3 << "\n";
            }
        }
    }
}


void StoredKnowledge::output_results(Results& r) {
    switch (args_info.correctness_arg) {
        case(correctness_arg_deadlock):
            r.add("controllability.correctness", "deadlock freedom");
            break;
        case(correctness_arg_livelock):
            r.add("controllability.correctness", "weak termination");
            break;
    }

    r.add("controllability.result", static_cast<bool>(root->is_sane));
    r.add("controllability.message_bound", args_info.messagebound_arg);

    r.add("statistics.nodes", stats.storedKnowledges);
    r.add("statistics.nodes_sane", static_cast<unsigned int>(seen.size()));
    r.add("statistics.nodes_insane", stats.builtInsaneNodes);
    r.add("statistics.edges", stats.storedEdges);
    r.add("statistics.hash_buckets_capacity", (1 << (sizeof(hash_t) * 8)));
    r.add("statistics.hash_buckets_used", static_cast<unsigned int>(hashTree.size()));
    r.add("statistics.hash_buckets_maximal_size", static_cast<unsigned int>(stats.maxBucketSize));
    r.add("statistics.queue_maximal_length", static_cast<unsigned int>(Queue::maximal_length));
    r.add("statistics.queue_reserved_length", static_cast<unsigned int>(Queue::initial_length));
    r.add("statistics.queue_maximal_queues", static_cast<unsigned int>(Queue::maximal_objects));
    r.add("statistics.scc_trivial", stats.numberOfTrivialSCCs);
    r.add("statistics.scc_nontrivial", stats.numberOfNonTrivialSCCs);
    r.add("statistics.scc_maximal_size", stats.maxSCCSize);

    // SA/OG output does not make sense together with --diagnose
    if (args_info.diagnose_given) {
        return;
    }

    // create OG or SA output
    if (args_info.og_given or args_info.sa_given) {
        std::stringstream s;

        s << "  channels_input = (";
        bool first = true;
        for (Label_ID l = Label::first_receive; l <= Label::last_receive; ++l) {
            if (not first) {
                s << ", ";
            }
            first = false;
            s << "\"" << Label::id2name[l] << "\"";
        }
        s << ");\n";

        s << "  channels_output = (";
        first = true;
        for (Label_ID l = Label::first_send; l <= Label::last_send; ++l) {
            if (not first) {
                s << ", ";
            }
            first = false;
            s << "\"" << Label::id2name[l] << "\"";
        }
        s << ");\n";

        s << "  channels_synchronous = (";
        first = true;
        for (Label_ID l = Label::first_sync; l <= Label::last_sync; ++l) {
            if (not first) {
                s << ", ";
            }
            first = false;
            s << "\"" << Label::id2name[l] << "\"";
        }
        s << ");\n";


        std::vector<std::size_t> finalStates;
        std::stringstream temp;

        FOREACH(it, hashTree) {
            for (size_t i = 0; i < it->second.size(); ++i) {
                if (it->second[i]->is_sane) {

                    if (it != hashTree.begin() or i != 0) {
                        temp << ",\n";
                    }

                    temp << "    { id = " << reinterpret_cast<size_t>(it->second[i]) << ";\n";
                    if (it->second[i]->is_final) {
                        finalStates.push_back(reinterpret_cast<size_t>(it->second[i]));
                    }

                    if (not args_info.sa_given) {
                        temp << "      formula = \"" << it->second[i]->formula(true) << "\";\n";
                    }

                    bool firstSuccessor = true;
                    temp << "      successors = (";
                    // draw the edges
                    for (Label_ID l = Label::first_receive; l <= Label::last_sync; ++l) {
                        if (it->second[i]->successors[l - 1] != NULL and
                                (seen.find(it->second[i]->successors[l - 1]) != seen.end()) and
                                (args_info.showEmptyNode_flag or it->second[i]->successors[l - 1] != empty)) {

                            if (not firstSuccessor) {
                                temp << ", ";
                            }
                            temp << "(\"" << Label::id2name[l] << "\", "
                                 << reinterpret_cast<size_t>(it->second[i]->successors[l - 1]) << ")";
                            firstSuccessor = false;
                        }

                        // draw edges to the empty node if requested
                        if (args_info.showEmptyNode_flag and
                                it->second[i]->successors[l - 1] == empty) {
                            emptyNodeReachable = true;
                            temp << "(\"" << Label::id2name[l] << "\", " << 0 << ")";
                        }
                    }
                    temp << "); }";
                }
            }
        }

        s << "  initial_states = (" << reinterpret_cast<size_t>(root) << ");\n";
        if (not args_info.og_given) {
            s << "  final_states = (";
            FOREACH(state, finalStates) {
                if (state != finalStates.begin()) {
                    s << ", ";
                }
                s << *state;
            }
            s << ");\n";
        }

        s << "  states = (\n";
        s << temp.str() << "\n";
        s << "  );\n";

        // add stream to results file
        if (args_info.og_given) {
            r.add("og", s);
        } else {
            r.add("sa", s);
        }
    }
}
