#include <cassert>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>

#include "automaton.h"
#include "component.h"
#include "io.h"
#include "marking.h"
#include "petrinet.h"
#include "state.h"

namespace pnapi
{


  /*!
   * The result of the constructor is an emty automaton.
   */
  Automaton::Automaton() :
    counter_(0)
  {
    /* do nothing */
  }


  /*!
   * Transformation: PetriNet => Automaton.
   * First, the Petri net is normalized to avoid collisions.
   * Second, it is necessary to save the transition's type (see state.h class
   * Edge). Third, the marking is gained from the net to create the initial
   * state. At last the DEPTH-FIRST-SEARCH is started and results in a fully
   * built automaton from a Petri Net.
   */
  Automaton::Automaton(PetriNet &net) :
    counter_(0)
  {
    net_ = new PetriNet(net);
    edgeLabels_ = new std::map<Transition *, std::string>();
    edgeTypes_ = new std::map<Transition *, Node::Type>();
    weights_ = new std::map<const Place *, unsigned int>();
    hashTable_ = new std::vector<std::set<State *> >(HASH_SIZE);

    (*edgeLabels_) = net_->normalize();
    for (std::set<Transition *>::iterator t = net_->getTransitions().begin();
        t != net_->getTransitions().end(); t++)
      (*edgeTypes_)[*t] = (**t).getType();

    net_->makeInnerStructure();

    srand(time(NULL));
    //unsigned int size = net_->getPlaces().size();
    for (std::set<Place *>::iterator p = net_->getPlaces().begin();
        p != net_->getPlaces().end(); p++)
    {
      (*weights_)[*p] = rand() % HASH_SIZE;
    }

    State &start = createState(*new Marking(*net_));
    start.initial();
    dfs(start);
  }


  /*!
   * The standard copy constructor copies the everything except for
   * properties which marked as optional in the according header file.
   */
  Automaton::Automaton(const Automaton &a) :
    states_(a.states_), edges_(a.edges_), counter_(a.counter_+1)
  {
    if (net_ == NULL)
      net_ = NULL;
    else
      net_ = new PetriNet(*net_);
    edgeTypes_ = NULL;
  }


  /*!
   * The standard destructor deletes the optional objects created
   * while the transformation PetriNet => Automaton
   */
  Automaton::~Automaton()
  {
    if (net_ != NULL)
      delete net_;
    if (edgeLabels_ != NULL)
      delete edgeLabels_;
    if (edgeTypes_ != NULL)
      delete edgeTypes_;
    if (hashTable_ != NULL)
      delete hashTable_;

    //std::cerr << "|S| = " << states_.size() << std::endl;
  }


  /*!
   * A state will be added to the set of states. The name given can be
   * empty, so a standard name will be set.
   *
   * \param     const std::string name
   * \return    State &s .. the newly created state
   */
  State & Automaton::createState()
  {
    State *s = new State(&counter_);
    states_.push_back(s);
    return *s;
  }


  /*!
   * A state will be added to the set of states. This state is based on
   * a given marking, which is needed to calculate the state's hash value.
   *
   * \param     Marking &m
   * \param     const std::string name
   *
   * \return    State &s .. the newly created state
   */
  State & Automaton::createState(Marking &m)
  {
    State *s = new State(m, weights_, &counter_);
    states_.push_back(s);
    return *s;
  }


  /*!
   */
  Edge & Automaton::createEdge(State &s1, State &s2)
  {
    Edge *e = new Edge(s1, s2);
    edges_.push_back(e);
    return *e;
  }


  /*!
   */
  Edge & Automaton::createEdge(State &s1, State &s2,
    const std::string label)
  {
    Edge *e = new Edge(s1, s2, label);
    edges_.push_back(e);
    return *e;
  }


  /*!
   */
  Edge & Automaton::createEdge(State &s1, State &s2,
    const std::string label, Node::Type type)
  {
    Edge *e = new Edge(s1, s2, label, type);
    edges_.push_back(e);
    return *e;
  }


  /*!
   * \brief Transforms the automaton to a state machine Petri net.
   *
   * States become places, edges become transitions, initial states
   * will be initially marked and final states will be connected
   * disjunctive in the final condition.
   *
   */
  PetriNet & Automaton::stateMachine() const
  {
    PetriNet *result_ = new PetriNet(); // resulting net
    std::map<State*,Place*> state2place_; // places by states

    Condition final_;
    final_ = false; // final places

    /* no comment */

    if (states_.empty())
      return *result_;

    // mark first state initially (see assumption above)
    state2place_[states_[0]] = &(result_->createPlace("",Node::INTERNAL,1));
    if (states_[0]->isFinal())
      final_ = final_.formula() || (*(state2place_[states_[0]])) == 1;

    // generate places from states
    for(unsigned int i=1; i < states_.size(); ++i)
    {
      Place* p = &(result_->createPlace());
      state2place_[states_[i]] = p;

      /*
       * if the state is final then the according place
       * has to be in the final marking.
       */
      if(states_[i]->isFinal())
        final_ = final_.formula() || (*(state2place_[states_[i]])) == 1;
    }

    // generate transitions from edges
    for(unsigned int i=0; i < edges_.size(); ++i)
    {
      Transition* t = &(result_->createTransition());

      Place* p = state2place_[&(edges_[i]->source())];
      result_->createArc(*p,*t);

      p = state2place_[&(edges_[i]->destination())];
      result_->createArc(*t,*p);
    }

    // generate final condition;
    /// \todo add all other places empty
    result_->finalCondition() = final_.formula();

    return *result_;
  }


  /*!
   * The initial states are those states which have no preset.
   * In basic service automata there is only one state which
   * is called the initial state.
   *
   * \return    std::set<State *> result - set of initial states
   */
  const std::set<State *> Automaton::initialStates() const
  {
    std::set<State *> result;
    result.clear();
    for (unsigned int i = 0; i < states_.size(); i++)
      if (states_[i]->isInitial())
        result.insert(states_[i]);

    return result;
  }


  /*!
   * The final states are those which are flagged as final. You can trigger
   * this flag by calling State::final() on a state object.
   *
   * \return    std::set<State *> result - set of final states
   */
  const std::set<State *> Automaton::finalStates() const
  {
    std::set<State *> result;
    result.clear();
    for (unsigned int i = 0; i < states_.size(); i++)
      if (states_[i]->isFinal())
        result.insert(states_[i]);

    return result;
  }


  /*!
   */
  std::set<std::string> Automaton::input() const
  {
    std::set<std::string> result;
    result.clear();
    for (std::map<Transition *, Node::Type>::iterator
        p = (*edgeTypes_).begin(); p != (*edgeTypes_).end(); p++)
      if (p->second == Node::INPUT)
        result.insert((*edgeLabels_)[p->first]);

    return result;
  }


  /*!
   */
  std::set<std::string> Automaton::output() const
  {
    std::set<std::string> result;
    result.clear();
    for (std::map<Transition *, Node::Type>::iterator
        p = (*edgeTypes_).begin(); p != (*edgeTypes_).end(); p++)
      if (p->second == Node::OUTPUT)
        result.insert((*edgeLabels_)[p->first]);

    return result;
  }


  /*!
   * Special depth first search method for service automaton creation
   * from Petri net. It's a recursive method which takes a State (named
   * start) and then tries to find its successors.
   *
   * \param     State &start
   */
  void Automaton::dfs(State &start)
  {
    (*hashTable_)[start.hashValue()].insert(&start);
    // assuming that each state has a marking
    Marking m = *start.marking();

    // final state
    if (net_->finalCondition().isSatisfied(m))
    {
      start.final();
      return;
    }

    bool doubled;
    // iterate over all transitions to check if they can fire
    for (std::set<Transition *>::const_iterator
        t = net_->getTransitions().begin(); t != net_->getTransitions().end();
        t++)
    {
      if (!m.activates(**t))
        continue;

      State &j = createState(m.successor(**t));
      if (start == j)
      {
        deleteState(&j);
        continue;
      }

      // collision detection
      doubled = false;
      for (std::set<State *>::const_iterator s =
          (*hashTable_)[j.hashValue()].begin();
          s != (*hashTable_)[j.hashValue()].end(); s++)
      {
        if (**s == j)
        {
          doubled = true;
          createEdge(start, **s, (*edgeLabels_)[*t], (*edgeTypes_)[*t]);
          break;
        }
      }

      if (doubled)
      {
        deleteState(&j);
        continue;
      }

      createEdge(start, j, (*edgeLabels_)[*t], (*edgeTypes_)[*t]);

      dfs(j);
    }
  }


  /*!
   *
   */
  void Automaton::deleteState(State *s)
  {
    if (s->preset().empty() && s->postset().empty())
    {
      if (states_[states_.size()-1] == s)
        states_.pop_back();
      else
        for (unsigned int i = 0; i < states_.size()-2; i++)
          if (states_[i] == s)
          {
            states_[i] = states_[states_.size()-1];
            states_.pop_back();
          }
    }
  }


  /*!
   * \brief     creates a STG file of the graph
   * \param     edgeLabels a reference to a vector of strings containing the old
   *            label names from this graph
   * \return    the filename of the created STG file
   */
  std::string Automaton::printToSTG(std::vector<std::string> &edgeLabels) const
  {
    // build STG file name
    std::string STGFileName = "AutomatonToPetrinet.stg";

    // create and fill stringstream for buffering graph information
    std::map<State *, bool> visitedNodes; // visited nodes
    State *rootNode = states_[0]; // root node
    std::ostringstream STGStringStream; // used as buffer for graph information

    STGStringStream << ".state graph" << "\n";
    printToSTGRecursively(rootNode, STGStringStream, visitedNodes, edgeLabels);
    STGStringStream << ".marking {p" << rootNode->name() << "}" << "\n";
    STGStringStream << ".end";


    // create STG file, print header, transition information and then
    // add buffered graph information
    std::fstream STGFileStream(STGFileName.c_str(), std::ios_base::out |
        std::ios_base::trunc | std::ios_base::binary);
    if (!STGFileStream.good())
    {
      STGFileStream.close();
      exit(1);
    }
    STGFileStream << ".model Labeled_Transition_System" << "\n";
    STGFileStream << ".dummy";
    for (int i = 0; i < (int)edgeLabels.size(); i++)
    {
        STGFileStream << " t" << i;
    }
    std::string STGGraphString = STGStringStream.str();
    STGFileStream << "\n" << STGGraphString << std::endl;
    STGFileStream.close();

    return STGFileName;
  }


  /*!
   * \brief     depth-first-search through the graph printing each node and
   *            edge to the output stream
   *
   * \param     v current node in the iteration process
   * \param     os output stream
   * \param     visitedNodes[] array of bool storing the nodes that we have
   *            looked at so far
   */
  void Automaton::printToSTGRecursively(State *v,
      std::ostringstream &os, std::map<State *, bool> &visitedNodes,
      std::vector<std::string> &edgeLabels) const
  {
    assert(v != NULL);
    visitedNodes[v] = true;             // mark current node as visited

    // TODO: possibly buggy because for every final node is "FINAL" added?
    if (v->isFinal())
    {
      // each label is mapped to his position in edgeLabes
      std::string currentLabel = "FINAL";
      currentLabel += v->name();
      int foundPosition = (int)edgeLabels.size();
      edgeLabels.push_back(currentLabel);
      os << "p" << v->name() << " t" << foundPosition << " p00" << std::endl;
    }

    // go through all edges
    for (unsigned int i = 0; i < edges_.size(); i++)
    {
      Edge *element = edges_[i];
      if (&element->source() != v)
        continue;
      State *vNext = &element->destination();

      // build label vector:
      // each label is mapped to his position in edgeLabes
      std::string currentLabel = element->label();
      int foundPosition = -1;
      for (int i = 0; i < (int)edgeLabels.size(); i++)
      {
          if (currentLabel == edgeLabels.at(i))
          {
              foundPosition = i;
              break;
          }
      }
      if (foundPosition == -1)
      {
          foundPosition = (int)edgeLabels.size();
          edgeLabels.push_back(currentLabel);
      }
      assert(foundPosition >= 0);
      assert(currentLabel == edgeLabels.at(foundPosition));

      // print current transition to stream
      os  << "p" << v->name() << " t" << foundPosition << " p"
          << vNext->name() << std::endl;

      // recursion
      if ( vNext != v && visitedNodes.find(vNext) == visitedNodes.end())
      {
          printToSTGRecursively(vNext, os, visitedNodes, edgeLabels);
      }
    }
  }


} /* END OF NAMESPACE pnapi */
