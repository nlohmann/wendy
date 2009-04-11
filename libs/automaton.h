// -*- C++ -*-

#ifndef PNAPI_AUTOMATON_H
#define PNAPI_AUTOMATON_H

#include <iostream>
#include <cassert>
#include <deque>
#include <list>
#include <set>
#include <vector>
#include <ostream>

#include "state.h"
#include "petrinet.h"

using std::deque;
using std::ostream;
using std::vector;

namespace pnapi
{


  // forward declarations
  class PetriNet;
  class Marking;
  template <class T> class AbstractAutomaton;
  class Automaton;
  class ServiceAutomaton;


  /// Operating Guidelines
  typedef AbstractAutomaton<StateOG> OG;


  /*!
   * \brief AbstractAutomaton
   * 
   * \note T must provide public method "isFinal()::bool".
   * 
   * \todo comment me!!!
   */
  template <class T>
  class AbstractAutomaton
  {
  public:
    /// standard constructor
    AbstractAutomaton();
    /// constructor used by ServiceAutomaton class
    AbstractAutomaton(PetriNet &net);
    /// standard destructor
    virtual ~AbstractAutomaton();

    /// creating a state
    void createState(const string name = "");
    /// finding a state
    const T * findState(const string name) const;
    /// creating an edge
    void createEdge(T &source, T &destination, const string label,
        pnapi::Node::Type type);

    /// returns the Petri net
    PetriNet getPetriNet() const { return net_; }

    /// Service automata output format (ig like) -- absolutely temporary!!
    void output_sa(std::ostream &os) const;

    /// stg output format -- absolutely temprary
    void output_stg(std::ostream &os) const;
    
    /// transform an automaton to a state machine petri net
    PetriNet toStateMachine() const;
    
    /// returns a set of states which are the initial states of the automaton
    set<const T *> initialStates() const;

  protected:
    /// set of states
    vector<T *> states_;
    /// set of edges
    vector<Edge<T> *> edges_;

    /// described PetriNet -- needed by ServiceAutomaton
    PetriNet net_;

    /// Service automata output format (ig like)
    //void output_sa(std::ostream &os) const;
    /// stg output format
    //void output_stg(std::ostream &os) const;

  };


  /*!
   * \brief   standard constructor
   */
  template <class T>
  AbstractAutomaton<T>::AbstractAutomaton() :
    net_(*new PetriNet())
  {
  }


  /*!
   * \brief   Constructor which instantiates a Petri net
   *
   * This constructor is used by the construction Petri net => Service
   * Automaton.
   */
  template <class T>
  AbstractAutomaton<T>::AbstractAutomaton(PetriNet &net) :
    net_(net)
  {
  }


  /*!
   * \brief   Standard destructor
   */
  template <class T>
  AbstractAutomaton<T>::~AbstractAutomaton()
  {
  }


  /*!
   * \brief Creates a state
   */
  template <class T>
  void AbstractAutomaton<T>::createState(const string name)
  {
    states_.push_back(new State(name));
  }


  /*!
   * \brief   Finds a state in the set of states
   *
   * Finds a state by name.
   */
  template <class T>
  const T * AbstractAutomaton<T>::findState(const string name) const
  {
    for (unsigned int i = 0; i < states_.size(); i++)
      if (states_[i]->getName() == name)
        return states_[i];

    return NULL;
  }


  /*!
   * \brief   Creates an automaton's edge from one state to 
   *          another with a label
   */
  template <class T>
  void AbstractAutomaton<T>::createEdge(T &source, T &destination,
      const string label, pnapi::Node::Type type)
  {
    edges_.push_back(new Edge<T>(source, destination, label, type));
  }


  /*!
   * \brief   Provides the SA output (IG like)
   *
   * Service Automaton format provides a single start node (initial state)
   * and many final nodes (final states).
   */
  template <class T>
  void AbstractAutomaton<T>::output_sa(std::ostream &os) const
  {
    bool first;
    std::set<string> seen;

    os << "INTERFACE\n";
    os << "  " << "INPUT ";
    first = true;
    seen.clear();
    for (unsigned int i = 0; i < edges_.size(); i++)
    {
      if ((*edges_[i]).getType() == pnapi::Node::INPUT &&
          seen.count((*edges_[i]).getLabel()) == 0)
      {
        if (first)
        {
          first = false;
          os << (*edges_[i]).getLabel();
          //os << "?" << (*edges_[i]).getLabel();
        }
        else
          os << ", " << (*edges_[i]).getLabel();
          //os << ", ?" << (*edges_[i]).getLabel();
      }
      seen.insert((*edges_[i]).getLabel());
    }
    os << ";\n";

    os << "  " << "OUTPUT ";
    first = true;
    seen.clear();
    for (unsigned int i = 0; i < edges_.size(); i++)
    {
      if ((*edges_[i]).getType() == pnapi::Node::OUTPUT &&
          seen.count((*edges_[i]).getLabel()) == 0)
      {
        if (first)
        {
          first = false;
          os << (*edges_[i]).getLabel();
        }
        else
          os << ", " << (*edges_[i]).getLabel();
      }
      seen.insert((*edges_[i]).getLabel());
    }
    os << ";\n\n";

    os << "NODES\n  ";
    first = true;
    for (unsigned int i = 0; i < states_.size(); i++)
      if (first)
      {
        first = false;
        os << (*states_[i]).getName();
      }
      else
        os << ", " << (*states_[i]).getName();
    os << ";\n\n";

    os << "INITIALNODE\n";
    os << "  " << (*states_[0]).getName();
    os << ";\n\n";

    os << "FINALNODES\n  ";
    first = true;
    for (unsigned int i = 0; i < states_.size(); i++)
    {
      if ((*states_[i]).isFinal())
      {
        if (first)
        {
          first = false;
          os << (*states_[i]).getName();
        }
        else
          os << ", " << (*states_[i]).getName();
      }
    }
    os << ";\n\n";

    os << "TRANSITIONS\n";
    for (unsigned int i = 0; i < edges_.size(); i++)
    {
      os << "  " << (*edges_[i]).getSource().getName() << " -> ";
      os << (*edges_[i]).getDestination().getName() << " : ";
      switch ((*edges_[i]).getType())
      {
      case pnapi::Node::INPUT:
        os << "?";
        break;
      case pnapi::Node::OUTPUT:
        os << "!";
      default: break;
      }
      os << (*edges_[i]).getLabel();
      if (i == edges_.size()-1)
        os << ";\n";
      else
        os << ",\n";
    }
  }


  /*!
   * \brief   Output method for STG format.
   */
  template <class T>
  void AbstractAutomaton<T>::output_stg(std::ostream &os) const
  {
    os << ".model Labeled_Transition_System\n";
    os << ".dummy ";
    std::set<string> seen;
    seen.clear();
    bool first = true;
    for (unsigned int i = 0; i < edges_.size(); i++)
    {
      std::string label = (*edges_[i]).getLabel();
      if (seen.count(label) > 0)
        continue;
      seen.insert(label);
      if (label == "tau")
      {
        if (first)
        {
          first = false;
          os << label;
        }
        else
          os << ", " << label;
      }
    }
    os << "\n";
    os << ".state graph\n";
    for (unsigned int i = 0; i < edges_.size(); i++)
    {
      os << edges_[i]->getSource().getName() << " ";
      os << edges_[i]->getLabel() << " ";
      os << edges_[i]->getDestination().getName() << "\n";
    }
    os << ".marking {" << states_[0]->getName() << "}\n";
    os << ".end\n";
  }

  /*!
   * \brief Transforms the automaton to a state machine petri net.
   * 
   * States become places, edges become transitions, initial states
   * will be initially marked and final states will be connected
   * disjunctive in the final condition. 
   * 
   * \note  it is assumed that the first state in the state vector is
   *        the initial state. 
   */
  template <class T>
  PetriNet AbstractAutomaton<T>::toStateMachine() const
  {
    PetriNet result_; // resulting net
    std::map<T*,Place*> state2place_; // places by states
    
    Condition final_;
    final_ = false; // final places
    
    /* no comment */
    
    if (states_.empty())
      return result_;
    
    // mark first state initially (see assumtion above)
    state2place_[states_[0]] = &(result_.createPlace("",Node::INTERNAL,1));
    if (states_[0]->isFinal())
      final_ = final_.formula() || (*(state2place_[states_[0]])) == 1;
    
    // generate places from states
    for(int i=1; i < states_.size(); ++i)
    {
      Place* p = &(result_.createPlace());
      state2place_[states_[i]] = p;
     
      /* 
       * if the state is final then the according place 
       * has to be in the final marking.
       */ 
      if(states_[i]->isFinal())
        final_ = final_.formula() || (*(state2place_[states_[i]])) == 1;
    }  
    
    // generate transitions from edges
    for(int i=0; i < edges_.size(); ++i)
    {
      Transition* t = &(result_.createTransition());
      
      Place* p = state2place_[&(edges_[i]->getSource())];
      result_.createArc(*p,*t);
      
      p = state2place_[&(edges_[i]->getDestination())];
      result_.createArc(*t,*p);
    }
    
    // generate final condition;
    /// \todo addd all other places empty
    result_.finalCondition() = final_.formula();
    
    return result_;
  }
  
  
  /*!
   *  \brief    returns a set of initial states
   *
   *  An initial state is assumed to be a state with empty preset.
   */
  template <class T>
  set<const T *> AbstractAutomaton<T>::initialStates() const
  {
    set<const T *> result;
    result.clear();
    
    for (int i = 0; i < states_.size(); i++)
      if (states_[i]->preset().empty())
        result.insert(states_[i]);
    
    return result;
  }
  

  /// Automaton class which will be able to read SA format
  class Automaton : public AbstractAutomaton<State>
  {
    friend ostream & operator<<(ostream &, const Automaton &);
  public:
    Automaton();
    virtual ~Automaton() {}
  };


  /// Service Automaton
  class ServiceAutomaton : public AbstractAutomaton<StateB>
  {
    //friend ostream & operator<<(ostream &, const ServiceAutomaton &);
  public:
    /// constructor with Petri net - initial marking taken from place tokens
    ServiceAutomaton(PetriNet &n);
    /// standard destructor
    virtual ~ServiceAutomaton();

    /// overloaded method createState() -- replaces the older method addState
    void createState(StateB &s);

  private:
    /// attributes & methods used for creating an automaton from Petri net
    static const int PNAPI_SA_HASHSIZE = 65535;
    /// hash table for recovering already seen states
    std::vector<std::set<StateB *> > hashTable_;
    /// edge labels which represent the interface of the automaton
    std::map<Transition *, string> edgeLabels_;
    /// each transitions has got a type according to the interface
    /// place connected to the transition
    std::map<Transition *, pnapi::Node::Type> edgeTypes_;

    /// Depth-First-Search
    void dfs(StateB &i);
  };


}

#endif /* SERVICEAUTOMATON_H */
