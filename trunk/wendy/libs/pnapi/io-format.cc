/*!
 * \file  io-format.cc
 */

#include "config.h"
#include <cassert>

#include <sstream>

#include "automaton.h"
#include "petrinet.h"
#include "state.h"
#include "myio.h"
#include "util.h"

using std::endl;
using std::map;
using std::ostream;
using std::set;
using std::string;
using std::stringstream;

using pnapi::io::util::delim;
using pnapi::io::util::filterMarkedPlaces;
using pnapi::io::util::filterInternalArcs;
using pnapi::io::util::filterInterfacePropositions;
using pnapi::io::util::Mode;
using pnapi::io::util::ModeData;

using namespace pnapi::formula;

namespace pnapi
{

namespace io
{
using pnapi::io::util::operator<<;


/* FORMAT IMPLEMENTATION: add a corresponding section */





/*************************************************************************
 ***** ONWD input
 *************************************************************************/

std::istream & onwd(std::istream & ios)
{
  util::FormatData::data(ios) = util::ONWD;
  return ios;
}

util::Manipulator<std::map<std::string, PetriNet *> >
nets(std::map<std::string, PetriNet *> & nets)
{
  return util::PetriNetManipulator(nets);
}



/*************************************************************************
 ***** STAT output
 *************************************************************************/

std::ostream & stat(std::ostream & ios)
{
  util::FormatData::data(ios) = util::STAT;
  return ios;
}


namespace __stat
{
std::ostream & output(std::ostream & os, const PetriNet & net)
{
  return os
  << "|P|= "     << net.places_.size()       << "  "
  << "|P_in|= "  << net.inputPlaces_.size()  << "  "
  << "|P_out|= " << net.outputPlaces_.size() << "  "
  << "|T|= "     << net.transitions_.size()  << "  "
  << "|F|= "     << net.arcs_.size();
}


std::ostream & output(std::ostream & os, const Automaton & sa)
{
  return os
  << "|Q|= " << sa.states_.size() << "  "
  << "|E|= " << sa.edges_.size();
}
}



/*************************************************************************
 ***** DOT output
 *************************************************************************/

std::ostream & dot(std::ostream & ios)
{
  util::FormatData::data(ios) = util::DOT;
  return ios;
}


namespace __dot
{

/*!
 * Creates a DOT output (see http://www.graphviz.org) of the Petri
 * net. It uses the digraph-statement and adds labels to transitions,
 * places and arcs if neccessary.
 */
std::ostream & output(std::ostream & os, const PetriNet & net)
{
  bool interface = true;
  string filename = net.getMetaInformation(os, io::INPUTFILE);
  set<string> labels = net.labels_;

  os  //< output everything to this stream

  << delim("\n")
  << util::mode(io::util::NORMAL)

  << "digraph N {" << endl
  << " graph [fontname=\"Helvetica\" nodesep=0.25 ranksep=\"0.25\""
  << " remincross=true label=\""
  << "Petri net"
  << (filename.empty() ? "" : " generated from " + filename) << "\"]"
  << endl
  << " node [fontname=\"Helvetica\" fixedsize width=\".3\""
  << " height=\".3\" label=\"\" style=filled fillcolor=white]" << endl
  << " edge [fontname=\"Helvetica\" color=white arrowhead=none"
  << " weight=\"20.0\"]" << endl
  << endl

  << " // places" << endl
  << " node [shape=circle]" << endl
  << net.internalPlaces_ << endl
  << (interface ? net.interfacePlaces_ : set<Place *>()) << endl
  << endl

  << " // labels" << endl
  << " node [shape=box]" << endl;
  for (set<string>::iterator it = labels.begin();
  it != labels.end(); ++it)
    os << " l" << *it << " [fillcolor=black width=.1]" << endl
    << " l" << *it << "_l [style=invis]" << endl
    << " l" << *it << "_l -> l" << *it << " [headlabel=\"" << *it
    << "\"]" << endl;
  return os
  << endl

  << " // transitions" << endl
  << " node [shape=box]" << endl
  << net.transitions_ << endl
  << endl

  << util::mode(io::util::INNER)
  << delim(" ")
  << " // inner cluster" << endl
  << " subgraph cluster1" << endl
  << " {" << endl
  << "  " << net.transitions_ << endl
  << "  " << net.internalPlaces_ << endl
  << "  label=\"\" style=" << (interface ? "\"dashed\"" : "invis")
  << endl << " }" << endl
  << endl

  << net.interfacePlacesByPort_
  << endl

  << delim("\n")
  << util::mode(io::util::ARC)
  << " // arcs" << endl
  << " edge [fontname=\"Helvetica\" arrowhead=normal"
  << " color=black]" << endl
  << net.arcs_
  << endl

  << "}"
  << endl;
}


std::ostream & output(std::ostream & os, const Place & p)
{
  // place attributes (used in NORMAL mode)
  stringstream attributes;
  if (p.getTokenCount() == 1)
    attributes << "fillcolor=black peripheries=2 height=\".2\" "
    << "width=\".2\" ";
  else if (p.getTokenCount() > 1)
    attributes << "label=\"" << p.getTokenCount() << "\" "
    << "fontcolor=black fontname=\"Helvetica\" ";

  switch (p.getType())
  {
  case Node::INPUT:
    attributes << "fillcolor=orange";
    break;
  case Node::OUTPUT:
    attributes << "fillcolor=yellow";
    break;
  default:
    if (p.wasInterface())
      attributes << "fillcolor=lightgoldenrod1";
    break;
  }

  // output the place as a node
  return output(os, p, attributes.str());
}


std::ostream & output(std::ostream & os, const Transition & t)
{
  // transition attributes (used in NORMAL mode)
  stringstream attributes;
  switch(t.getType())
  {
  case(Node::INPUT):  attributes << "fillcolor=orange"; break;
  case(Node::OUTPUT): attributes << "fillcolor=yellow"; break;
  case(Node::INOUT):  attributes
  << "fillcolor=gold label=< <TABLE BORDER=\"1\""
  << " CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\""
  << " HEIGHT=\"21\" WIDTH=\"21\" FIXEDSIZE=\"TRUE\"><TR>"
  << "<TD HEIGHT=\"11\" WIDTH=\"21\" FIXEDSIZE=\"TRUE\""
  << " BGCOLOR=\"ORANGE\">"
  << "</TD></TR><TR>"
  << "<TD HEIGHT=\"10\" WIDTH=\"21\" FIXEDSIZE=\"TRUE\""
  << " BGCOLOR=\"YELLOW\">"
  << "</TD></TR></TABLE> >";
  break;
  default: break;
  }

  // output the transition as a node
  output(os, t, attributes.str());

  // output labels
  set<string> labels = t.getSynchronizeLabels();
  Mode mode = ModeData::data(os);
  if (mode == io::util::NORMAL)
    for (set<string>::iterator it = labels.begin();
    it != labels.end(); ++it)
      os << endl << " l" << *it << " -> " << getNodeName(t)
      << " [style=dashed color=black]";

  return os;
}


std::ostream & output(std::ostream & os, const Arc & arc)
{
  bool interface = true;

  if (!interface && arc.getPlace().getType() != Place::INTERNAL)
    return os;
  os << " " << arc.getSourceNode() << " -> " << arc.getTargetNode()
  << "\t[";
  if (arc.getWeight() != 1)
    os << "label=\"" << arc.getWeight() << "\"";
  if (arc.getPlace().getType() == Place::INTERNAL)
    os << "weight=10000.0";
  return os << "]";
}


std::ostream & output(std::ostream & os, const Node & n, const std::string & attr)
{
  Mode mode = ModeData::data(os);
  string dotName   = getNodeName(n);
  string dotName_l = getNodeName(n, true);

  os << dotName;

  if (mode == io::util::NORMAL)
    os << "\t[" << attr << "]" << endl
    << " " << dotName_l << "\t[style=invis]" << endl
    << " " << dotName_l << " -> " << dotName
    << " [headlabel=\"" << n.getName() << "\"]";

  if (mode == io::util::INNER)
    os << " " << dotName_l;

  return os;
}


std::string getNodeName(const Node & n, bool withSuffix)
{
  static PetriNet * net = NULL;
  static map<string, string> names;

  if (net != &n.getPetriNet())
  {
    net = &n.getPetriNet();
    names.clear();

    int i = 0;
    set<Place *> places = net->getPlaces();
    for (set<Place *>::iterator it = places.begin();
    it != places.end(); ++it)
    {
      stringstream ss; ss << "p" << ++i;
      names[(*it)->getName()] = ss.str();
    }

    i = 0;
    set<Transition *> transitions = net->getTransitions();
    for (set<Transition *>::iterator it = transitions.begin();
    it != transitions.end(); ++it)
    {
      stringstream ss; ss << "t" << ++i;
      names[(*it)->getName()] = ss.str();
    }
  }

  return names[n.getName()] + string(withSuffix ? "_l" : "");
}


std::ostream & output(std::ostream & os,
    const std::pair<std::string, std::set<Place *> > & p)
    {
  string port = p.first;
  return os
  << " // cluster for port " << port << endl
  << " subgraph cluster_" << port << "\n {\n"
  << "  label=\"" << port << "\";" << endl
  << "  style=\"filled,rounded\"; bgcolor=grey95  labelloc=t;" << endl
  << "  " << delim("\n  ") << p.second
  << endl << " }" << endl << endl;
    }


void outputGroupPrefix(std::ostream & os, const std::string & port)
{
}


void outputGroupSuffix(std::ostream & os, const std::string & port)
{
}


std::ostream & output(std::ostream &os, const Automaton &sa)
{
  os
  << "Digraph ServiceAutomaton {"
  << "{" << std::endl
  << "q0 [style=invis];" << std::endl;
  for (int i = 0; i < (int) sa.states_.size(); i++)
    if (sa.states_[i]->isFinal())
      os << sa.states_[i]->name() << " [];" << std::endl;
    else
      os << sa.states_[i]->name() << ";" << std::endl;
  os
  << "}" << std::endl;
  os
  << "{" << std::endl
  << "q0 -> " << (*sa.initialStates().begin())->name() << ";"
  << std::endl;
  for (int i = 0; i < (int) sa.edges_.size(); i++)
    os << sa.edges_[i]->source().name() << " -> "
    << sa.edges_[i]->destination().name() << " [label=\""
    << sa.edges_[i]->label() <<"\"];" << std::endl;
  return os
  << "}" << std::endl
  << "}" << std::endl;
}


} /* namespace __dot */



/*************************************************************************
 ***** LOLA output
 *************************************************************************/

std::ios_base & lola(std::ios_base & ios)
{
  util::FormatData::data(ios) = util::LOLA;
  return ios;
}


std::ostream & formula(std::ostream & os)
{
  util::FormulaData::data(os).formula = true;
  return os;
}


namespace __lola
{

std::ostream & output(std::ostream & os, const PetriNet & net)
{
  string creator = net.getMetaInformation(os, CREATOR, PACKAGE_STRING);
  string inputfile = net.getMetaInformation(os, INPUTFILE);

  os //< output everything to this stream

  << "{ Petri net created by " << creator
  << (inputfile.empty() ? "" : " reading " + inputfile)
  << " }" << endl
  << endl

  << "PLACE" << mode(io::util::PLACE) << endl
  << "  " << delim(", ") << net.internalPlaces_ << ";" << endl
  << endl

  << "MARKING" << mode(io::util::PLACE_TOKEN) << endl
  << "  " << filterMarkedPlaces(net.internalPlaces_) << ";" << endl
  << endl << endl

  // transitions
  << delim("\n") << net.transitions_ << endl
  << endl;

  if (util::FormulaData::data(os).formula) os
  << "FORMULA" << endl
  << "  " << net.finalCondition_ << endl
  << endl;

  return os
  << "{ END OF FILE }" << endl;
}


std::ostream & output(std::ostream & os, const Place & p)
{
  os << p.getName();
  if (ModeData::data(os) == io::util::PLACE_TOKEN)
    os << ":" << p.getTokenCount();
  return os;
}


std::ostream & output(std::ostream & os, const Transition & t)
{
  return os
  << "TRANSITION " << t.getName() << endl
  << delim(", ")
  << "  CONSUME "
  << filterInternalArcs(t.getPresetArcs())  << ";" << endl
  << "  PRODUCE "
  << filterInternalArcs(t.getPostsetArcs()) << ";" << endl;
}


std::ostream & output(std::ostream & os, const Arc & arc)
{
  return os << arc.getPlace().getName() << ":" << arc.getWeight();
}


std::ostream & output(std::ostream & os, const formula::Negation & f)
{
  set<const Formula *> children =
    filterInterfacePropositions(f.children());
  if (children.empty())
    assert(false); // FIXME: don't know what to do in this case
  else
    return os << "NOT (" << **f.children().begin() << ")";
}


std::ostream & output(std::ostream & os, const formula::Conjunction & f)
{
  set<const Formula *> children =
    filterInterfacePropositions(f.children());
  if (children.empty())
    //return os << formula::FormulaTrue();
    assert(false); // FIXME: don't know what to do in this case
  else
    return os << "(" << delim(" AND ") << children << ")";
}


std::ostream & output(std::ostream & os, const formula::Disjunction & f)
{
  set<const Formula *> children =
    filterInterfacePropositions(f.children());
  if (children.empty())
    //return os << formula::FormulaFalse();
    assert(false); // FIXME: don't know what to do in this case
  else
    return os << "(" << delim(" OR ") << children << ")";
}


std::ostream & output(std::ostream & os, const formula::FormulaTrue &)
{
  return os << "TRUE";  // keyword not yet implemented in lola
}


std::ostream & output(std::ostream & os, const formula::FormulaFalse &)
{
  return os << "FALSE"; // keyword not yet implemented in lola
}


std::ostream & output(std::ostream & os, const formula::FormulaEqual & f)
{
  return os << f.place().getName() << " = " << f.tokens();
}


std::ostream & output(std::ostream & os, const formula::FormulaNotEqual & f)
{
  return os << f.place().getName() << " # " << f.tokens();
}


std::ostream & output(std::ostream & os, const formula::FormulaGreater & f)
{
  return os << f.place().getName() << " > " << f.tokens();
}


std::ostream & output(std::ostream & os, const formula::FormulaGreaterEqual & f)
{
  return os << f.place().getName() << " >= " << f.tokens();
}


std::ostream & output(std::ostream & os, const formula::FormulaLess & f)
{
  return os << f.place().getName() << " < " << f.tokens();
}


std::ostream & output(std::ostream & os, const formula::FormulaLessEqual & f)
{
  return os << f.place().getName() << " <= " << f.tokens();
}

} /* namespace __lola */



/*************************************************************************
 ***** PNML output
 *************************************************************************/

std::ios_base & pnml(std::ios_base & ios)
{
  util::FormatData::data(ios) = util::PNML;
  return ios;
}


namespace __pnml
{

std::ostream & outputInterface(std::ostream & os, const PetriNet & net) {
    os << "      <port id=\"portId1\">" << endl;

    PNAPI_FOREACH(set<Place*>, net.getInputPlaces(), p) {
        os << "        <input id=\"" << (*p)->getName() << "\" />" << endl;
    }

    PNAPI_FOREACH(set<Place*>, net.getOutputPlaces(), p) {
        os << "        <output id=\"" << (*p)->getName() << "\" />" << endl;
    }

    PNAPI_FOREACH(set<std::string>, net.getSynchronousLabels(), l) {
        os << "        <synchronous id=\"" << *l << "\" />" << endl;
    }

    os << "      </port>" << endl;
    return os;
}


std::ostream & output(std::ostream & os, const PetriNet & net)
{
  os //< output everything to this stream

  << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" << endl
  << "<!--" << endl
  << "  generator:   " << net.getMetaInformation(os, CREATOR, PACKAGE_STRING) << endl
  << "  input file:  " << net.getMetaInformation(os, INPUTFILE) << endl
  << "  invocation:  " << net.getMetaInformation(os, io::INVOCATION) << endl
  << "  net size:    " << stat << net << pnml << endl  
  << "-->" << endl
  << endl

  << "<pnml>" << endl
  
  << "  <module>" << endl

  << "    <ports>" << endl;
  outputInterface(os, net);
  
  os << "    </ports>" << endl

  << "    <net id=\"n1\" type=\"PTNet\">" << endl;

  if (!net.getMetaInformation(os, INPUTFILE).empty()) {
    os << "    <name>" << endl
       << "      <text>" << net.getMetaInformation(os, INPUTFILE) << "</text>" << endl
       << "    </name>" << endl;
  }

  os << mode(io::util::PLACE) << net.internalPlaces_

  << net.transitions_

  << mode(io::util::ARC) << filterInternalArcs(net.arcs_)

  << "    </net>" << endl

  << "    <finalmarkings>" << endl
  << "      <marking>" << endl
  << net.finalCondition_
  << "      </marking>" << endl  
  << "    </finalmarkings>" << endl

  << "  </module>" << endl
  
  << "</pnml>" << endl;

  return os << endl;
}


std::ostream & output(std::ostream & os, const Place & p)
{
  os << "      <place id=\"" << p.getName() << "\"";
  
  if (p.getTokenCount()) {
    os
    << ">" << endl
    << "        <initialMarking>" << endl
    << "          <text>" << p.getTokenCount() << "</text>" << endl
    << "        </initialMarking>" << endl
    << "      </place>" << endl;
  } else {
    os << " />" << endl;
  }

  return os;
}


std::ostream & output(std::ostream & os, const Transition & t)
{
  set<Place*> inputs = t.getPetriNet().getInputPlaces();
  set<Place*> outputs = t.getPetriNet().getOutputPlaces();

  string comm;
  PNAPI_FOREACH(set<Node*>, t.getPreset(), p) {
      if (inputs.find(static_cast<Place*>(*p)) != inputs.end()) {
          comm += "\n        <receive idref=\"" + (*p)->getName() + "\" />";
      }
  }
  PNAPI_FOREACH(set<Node*>, t.getPostset(), p) {
      if (outputs.find(static_cast<Place*>(*p)) != outputs.end()) {
          comm += "\n        <send idref=\"" + (*p)->getName() + "\" />";
      }
  }
  PNAPI_FOREACH(set<std::string>, t.getSynchronizeLabels(), l) {
      comm += "\n        <synchronize idref=\"" + *l + "\" />";
  }

  if (comm.empty()) {
      return os << "      <transition id=\"" << t.getName() << "\" />" << endl;
  } else {
      return os
      << "      <transition id=\"" << t.getName() << "\">"
      << comm << endl
      << "      </transition>" << endl;
  }
}


std::ostream & output(std::ostream & os, const Arc & arc)
{
  static unsigned int arcId = 0;
  os
  << "      <arc id=\"arcId" << ++arcId
  << "\" source=\"" << arc.getSourceNode().getName()
  << "\" target=\"" << arc.getTargetNode().getName()
  << "\"";

  if (arc.getWeight() > 1) {
    os << ">" << endl
       << "        <inscription>" << endl
       << "          <text>" << arc.getWeight() << "</text>" << endl
       << "        </inscription>" << endl
       << "      </arc>" << endl;
  } else {
    os << " />" << endl;
  }

  return os;
}


std::ostream & output(std::ostream & os, const formula::Negation & f)
{
  set<const Formula *> children = filterInterfacePropositions(f.children());
  if (children.empty())
    assert(false); // FIXME: don't know what to do in this case
  else
    return os << "NOT (" << **f.children().begin() << ")";
}


std::ostream & output(std::ostream & os, const formula::Conjunction & f)
{
  set<const Formula *> children = filterInterfacePropositions(f.children());
  if (children.empty())
    //return os << formula::FormulaTrue();
    assert(false); // FIXME: don't know what to do in this case
  else
    return os << children;
}


std::ostream & output(std::ostream & os, const formula::Disjunction & f)
{
  set<const Formula *> children = filterInterfacePropositions(f.children());
  if (children.empty())
    //return os << formula::FormulaFalse();
    assert(false); // FIXME: don't know what to do in this case
  else
    return os << delim("      </marking>\n      <marking>\n") << children;
}


std::ostream & output(std::ostream & os, const formula::FormulaTrue &)
{
    return os;// << "TRUE";  // keyword not yet implemented in lola
}


std::ostream & output(std::ostream & os, const formula::FormulaFalse &)
{
  return os;// << "FALSE"; // keyword not yet implemented in lola
}


std::ostream & output(std::ostream & os, const formula::FormulaEqual & f)
{
  return os << "        <place idref=\"" << f.place().getName() << "\">" << endl
  << "          <text>" << f.tokens() << "</text>\n       </place>" << endl;
}


std::ostream & output(std::ostream & os, const formula::FormulaNotEqual & f)
{
  return os << f.place().getName() << " # " << f.tokens();
}


std::ostream & output(std::ostream & os, const formula::FormulaGreater & f)
{
  return os << f.place().getName() << " > " << f.tokens();
}


std::ostream & output(std::ostream & os, const formula::FormulaGreaterEqual & f)
{
  return os << f.place().getName() << " >= " << f.tokens();
}


std::ostream & output(std::ostream & os, const formula::FormulaLess & f)
{
  return os << f.place().getName() << " < " << f.tokens();
}


std::ostream & output(std::ostream & os, const formula::FormulaLessEqual & f)
{
  return os << f.place().getName() << " <= " << f.tokens();
}

} /* namespace __lola */



/*************************************************************************
 ***** OWFN output
 *************************************************************************/

std::ios_base & owfn(std::ios_base & ios)
{
  util::FormatData::data(ios) = util::OWFN;
  return ios;
}


namespace __owfn
{

std::ostream & output(std::ostream & os, const PetriNet & net)
{
  set<string> labels = net.labels_;
  //util::collectSynchronizeLabels(net.synchronizedTransitions_);

  os  //< output everything to this stream

  << "{" << endl
  << "  generated by: "
  << net.getMetaInformation(os, io::CREATOR, PACKAGE_STRING)  << endl
  << "  input file:   "
  << net.getMetaInformation(os, io::INPUTFILE)                << endl
  << "  invocation:   "
  << net.getMetaInformation(os, io::INVOCATION)               << endl
  << "  net size:     "
  << stat << net << owfn                                << endl
  << "}" << endl
  << endl

  << util::mode(io::util::PLACE) << delim("; ")
  << "PLACE"      << endl;

  if(!net.getRoles().empty() && !net.isIgnoringRoles()){
     os 
     << delim(", ")
     << "  ROLES "      << net.getRoles()          << ";" << endl;	
  }

  os
  << "  INTERNAL" << endl
  << "    " << io::util::groupPlacesByCapacity(net.internalPlaces_)
  << ";" << endl << endl << delim(", ")
  << "  INPUT"    << endl
  << "    " << net.inputPlaces_
  << ";" << endl << endl
  << "  OUTPUT"   << endl
  << "    " << net.outputPlaces_
  << ";" << endl << endl;

  if (!labels.empty()) os
  << "  SYNCHRONOUS" << endl
  << "    " << labels
  << ";" << endl << endl;

  if (!net.interfacePlacesByPort_.empty())
    os << delim("; ")
    << "PORTS" << endl
    << "  " << net.interfacePlacesByPort_ << ";" << endl
    << endl;

  os
  << mode(io::util::PLACE_TOKEN) << delim(", ")
  << "INITIALMARKING" << endl
  << "  " << io::util::filterMarkedPlaces(net.internalPlaces_) << ";" << endl
  << endl

  << "FINALCONDITION" << endl;
  
  // if formel is total
  if(net.finalCondition().concerningPlaces().size() == net.getInternalPlaces().size()) 
  {
    Condition tmpCond;
    tmpCond = net.finalCondition_.formula();
    set<const Place*> emptyPlaces = tmpCond.formula().emptyPlaces();
    
    if(emptyPlaces.size() == net.getInternalPlaces().size())
    {
      os << "  ALL_PLACES_EMPTY;" << endl << endl << endl;
    }
    else
    {
      for(set<const Place*>::iterator p = emptyPlaces.begin();
           p != emptyPlaces.end(); ++p)
      {
        tmpCond.removePlace(**p);
      }
      
      os << "  (" << tmpCond << ") AND ALL_OTHER_PLACES_EMPTY;" << endl << endl << endl;
    }
  }
  else
  {
    os << "  " << net.finalCondition_ << ";" << endl << endl << endl;
  }

  os
  << delim("\n")
  << net.transitions_ << endl
  << endl

  << "{ END OF FILE '" << net.getMetaInformation(os, io::OUTPUTFILE) << "' }"
  << endl;

  return os;
}


std::ostream & output(std::ostream & os, const Place & p)
{
  os << p.getName();
  if (ModeData::data(os) == io::util::PLACE_TOKEN && p.getTokenCount() != 1)
    os << ": " << p.getTokenCount();
  return os;
}


std::ostream & output(std::ostream & os, const Transition & t)
{
  os
  << "TRANSITION " << t.getName() << endl;

  if (t.getCost() != 0)
    os << "  COST " << t.getCost() << ";" << endl;

  if (!t.getRoles().empty() && !t.getPetriNet().isIgnoringRoles()){
    os 
    << delim(", ")
    << "  ROLES "     << t.getRoles()             << ";" << endl;
  }

  os
  << delim(", ")
  << "  CONSUME "     << t.getPresetArcs()        << ";" << endl
  << "  PRODUCE "     << t.getPostsetArcs()       << ";" << endl;

  if (t.isSynchronized()) os
  << "  SYNCHRONIZE " << t.getSynchronizeLabels() << ";" << endl;
  return os;
}


std::ostream & output(std::ostream & os, const Arc & arc)
{
  os << arc.getPlace().getName();
  if (arc.getWeight() != 1)
    os << ":" << arc.getWeight();
  return os;
}


std::ostream & output(std::ostream & os,
    const std::pair<std::string, std::set<Place *> > & p)
    {
  return os << p.first << ": " << delim(", ") << p.second;
    }


std::ostream & output(std::ostream & os,
    const std::pair<unsigned int, std::set<Place *> > & p)
    {
  if (p.first > 0)
    os << "SAFE " << p.first << ": ";
  return os << delim(", ") << p.second;
    }


std::ostream & output(std::ostream & os, const formula::Negation & f)
{
  return os << "NOT (" << **f.children().begin() << ")";
}


std::ostream & output(std::ostream & os, const formula::Conjunction & f)
{
  if (f.children().empty())
    return os << formula::FormulaTrue();
  else
    return os << "(" << delim(" AND ") << f.children() << ")";
}


std::ostream & output(std::ostream & os, const formula::Disjunction & f)
{
  if (f.children().empty())
    return os << formula::FormulaFalse();
  else
    return os << "(" << delim(" OR ") << f.children() << ")";
}


std::ostream & output(std::ostream & os, const formula::FormulaTrue &)
{
  return os << "TRUE";
}


std::ostream & output(std::ostream & os, const formula::FormulaFalse &)
{
  return os << "FALSE";
}


std::ostream & output(std::ostream & os, const formula::FormulaEqual & f)
{
  return os << f.place().getName() << " = " << f.tokens();
}


std::ostream & output(std::ostream & os, const formula::FormulaNotEqual & f)
{
  return os << f.place().getName() << " != " << f.tokens();
}


std::ostream & output(std::ostream & os, const formula::FormulaGreater & f)
{
  return os << f.place().getName() << " > " << f.tokens();
}


std::ostream & output(std::ostream & os, const formula::FormulaGreaterEqual & f)
{
  return os << f.place().getName() << " >= " << f.tokens();
}


std::ostream & output(std::ostream & os, const formula::FormulaLess & f)
{
  return os << f.place().getName() << " < " << f.tokens();
}


std::ostream & output(std::ostream & os, const formula::FormulaLessEqual & f)
{
  return os << f.place().getName() << " <= " << f.tokens();
}

} /* namespace __owfn */


/*************************************************************************
 ***** SA output
 *************************************************************************/

std::ios_base & sa(std::ios_base &base)
{
  util::FormatData::data(base) = util::SA;
  return base;
}


std::istream & sa2sm(std::istream &is)
{
  util::FormatData::data(is) = util::SA2SM;
  return is;
}


namespace __sa
{
std::ostream & output(std::ostream &os, const Automaton &sa)
{
  os  << "INTERFACE" << endl
  << "  INPUT ";
  output(os, sa.input());
  os  << ";" << endl
  << "  OUTPUT ";
  output(os, sa.output());
  os  << ";" << endl
  << "  SYNCHRONOUS ";
  output(os, sa.getSynchronousLabels());
  os  << ";" << endl << endl
  << "NODES" << endl;
  output(os, sa.states_);

  return os;
}

std::ostream & output(std::ostream &os, const State &s)
{
  os << s.name();
  if (s.isInitial() || s.isFinal())
  {
    os << ": ";
    if (s.isInitial() && s.isFinal())
      os << "INITIAL, FINAL";
    else
      if (s.isInitial())
        os << "INITIAL";
      else
        os << "FINAL";
  }
  os << endl;
  output(os, s.postsetEdges());
  os << endl;

  return os;
}

std::ostream & output(std::ostream &os, const std::vector<State *> &vs)
{
  for (unsigned int i = 0; i < vs.size(); ++i)
    output(os, *vs[i]);

  return os;
}

std::ostream & output(std::ostream &os, const std::set<Edge *> &edges)
{
  for (std::set<Edge *>::iterator e = edges.begin(); e != edges.end(); e++)
    os << "  " << (*e)->label() << " -> " << (*e)->destination().name() << endl;

  return os;
}

std::ostream & output(std::ostream &os, const std::set<std::string> &ss)
{
  if (!ss.empty())
  {
    os << *ss.begin();
    for (std::set<std::string>::iterator s = ++ss.begin(); 
    s != ss.end(); ++s)
      os << ", " << *s;
  }

  return os;
}
} /* namespace __sa */

} /* namespace io */

} /* namespace pnapi */
