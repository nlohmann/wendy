/*!
 * \file  petrinet-pn.cc
 */


/******************************************************************************
 * Headers
 *****************************************************************************/

#include "config.h"

#include "pnapi-assert.h"

#include "automaton.h"
#include "Output.h"
#include "parser-pn-wrapper.h"
#include "util.h"

#include <fstream>
#include <sstream>
#include <utility>

using std::cerr;
using std::endl;
using std::istringstream;
using std::ifstream;
using std::map;
using std::pair;
using std::make_pair;
using std::set;
using std::string;
using std::vector;


namespace pnapi
{

/*!
 * The constructor transforming an automaton to a Petri net.
 * 
 * \param sa automaton to be converted
 * \param ac automaton converter to be used
 * \param capacity to be passed to Genet when using genet
 */
PetriNet::PetriNet(const Automaton & sa, AutomatonConverter ac, uint8_t capacity) :
  observer_(*this), warnings_(0), reductionCache_(NULL),
  genetCapacity_(capacity), automatonConverter_(ac), interface_(*this)
{
  if(automatonConverter_ == STATEMACHINE)
  {
    (*this) = sa.toStateMachine();
    return;
  }
  
  util::Output out;

  std::vector<std::string> edgeLabels;

  sa.printToTransitionGraph(edgeLabels, out, automatonConverter_);

  std::set<std::string> input = sa.getInputLabels();
  std::set<std::string> output = sa.getOutputLabels();
  std::set<std::string> synchronous = sa.getSynchronousLabels();

  createFromSTG(edgeLabels, out.name(), input, output, synchronous);
}




/*!
 * \brief     creates a TG file of the graph
 * 
 * \param     edgeLabels a reference to a vector of strings containing the old
 *            label names from this graph
 * \param     out a reference to the output class handleing the temporary file
 * \param     converter the converter from Automatons to PetriNets
 * 
 * \pre       edgeLabels is empty
 * \post      edgeLabels stores a mapping from event number to event label
 */
void Automaton::printToTransitionGraph(std::vector<std::string> & edgeLabels, 
                                       util::Output & out,
                                       PetriNet::AutomatonConverter converter) const
{
  switch(converter)
  {
  case PetriNet::PETRIFY :
  case PetriNet::GENET : break;
  default : PNAPI_ASSERT(false); // do not call with other converters
  }

  // create and fill stringstream for buffering graph information
  std::ostringstream TGStringStream; // used as buffer for graph information

  TGStringStream << ".state graph\n";

  // copy edges
  for(unsigned int i = 0; i < edges_.size(); ++i)
  {
    Edge * edge = edges_[i];

    // build label vector:
    // each label is mapped to his position in edgeLabes
    int foundPosition = -1;
    for(int j = 0; j < (int)edgeLabels.size(); ++j)
    {
      if (edge->getLabel() == edgeLabels[j])
      {
        foundPosition = j;
        break;
      }
    }
    if (foundPosition == -1)
    {
      foundPosition = (int)edgeLabels.size();
      edgeLabels.push_back(edge->getLabel());
    }

    // print current transition to stream
    TGStringStream  << "p" << edge->getSource().getName() << " t" << foundPosition 
    << " p" << edge->getDestination().getName() << "\n";
  }

  // mark final states
  for(unsigned int i = 0; i < states_.size(); ++i)
  {
    if (states_[i]->isFinal())
    {
      // each label is mapped to his position in edgeLabes
      std::string currentLabel = "FINAL";
      currentLabel += states_[i]->getName();
      int foundPosition = (int)edgeLabels.size();
      edgeLabels.push_back(currentLabel);
      TGStringStream << "p" << states_[i]->getName() << " t" << foundPosition << " p00\n";
    }
  }

  TGStringStream << ".marking {p" << (*getInitialStates().begin())->getName() << "}\n";
  TGStringStream << ".end" << std::flush;


  // create STG file, print header, transition information and then
  // add buffered graph information
  if(converter == PetriNet::PETRIFY)
  {
    out.stream() << ".model Labeled_Transition_System\n.dummy";
  }
  else
  {
    out.stream() << ".outputs";
  }

  for (int i = 0; i < (int)edgeLabels.size(); ++i)
  {
    out.stream() << " t" << i;
  }
  
  std::string TGGraphString = TGStringStream.str();
  out.stream() << "\n" << TGGraphString << std::endl;
}


/*!
 * \brief helper function for STG2oWFN
 * 
 * \param edge mapped label, e.g. t1 (created by printGraphToSTG) or t1/1 (created by petrify)
 * \param edgeLabels a vector of strings containing the former labels (created by printGraphToSTG)
 * 
 * \return a string containing the remapped label, e.g. if !Euro was mapped to t0, then t0 is remapped to !Euro
 *         labels created by petrify need special handling, e.g. t0/1 is remapped to !Euro/1
 * 
 * \post  delimeter ('/' or '_') will be replaced by ':')
 */
std::string PetriNet::remap(const std::string & edge, std::vector<std::string> & edgeLabels)
{
  char delimeter = ((automatonConverter_ == PETRIFY) ? '/' : '_');

  string affix = ((edge.find(delimeter) != string::npos) 
                  ? (string("/") + edge.substr(edge.find(delimeter) + 1)) 
                  : ""); // read affix
               
  string indexString = edge.substr(1, edge.size() - affix.size() - 1);  // read index

  istringstream indexStream; // read index as integer
  int index;
  indexStream.str(indexString);
  indexStream >> index;

  return edgeLabels[index] + affix;
}



/*!
 * \brief   creates a petri net from an STG file
 *
 * \param   edgeLabels mapping from edges' name (an unsigned integer) to their label
 * \param   fileName location of the STG file
 * \param   inputLabels set of labels, that name input places
 * \param   outputLabels set of labels, that name output places
 * \param   synchronousLabels set of labels, that name synchronization labels
 *
 * \note    - this function has been outsourced from fiona and recently only needed by fiona
 *          - petrify is needed to call this function
 * 
 * \todo maybe just copy label names instead of reading dummy transitions
 *
 */
void PetriNet::createFromSTG(std::vector<std::string> & edgeLabels,
                             const std::string & fileName,
                             std::set<std::string> & inputLabels,
                             std::set<std::string> & outputLabels,
                             std::set<std::string> & synchronousLabels)
{
  // preparing system call of petrify/genet
  string pnFileName = fileName + ".pn"; // add .pn to the output file
  string systemcall;
  if(automatonConverter_ == PETRIFY)
  {
    systemcall = pathToPetrify_ + " " + fileName
               + " -dead -ip -nolog -o " + pnFileName;
  }
  else
  {
    std::stringstream ss;
    ss << (int)genetCapacity_;
    systemcall = pathToGenet_ + " -k " + ss.str() + " " + fileName + " > " + pnFileName;
  }

  //int result = system(systemcall.c_str());
  int doNotCare_JustFixWarning = system(systemcall.c_str());
  doNotCare_JustFixWarning = doNotCare_JustFixWarning; // get rid of compilerwarning

  /// does not work for Genet, there seems to be a bug in Cudd
  //PNAPI_ASSERT(result == 0);

  // parse generated file
  ifstream ifs(pnFileName.c_str(), ifstream::in);
  PNAPI_ASSERT(ifs.good());

  parser::pn::Parser myParser;
  myParser.parse(ifs);
  ifs.close();

  // create places
  PNAPI_FOREACH(p, myParser.places_)
  {
    createPlace(*p, myParser.initialMarked_[*p]);
  }

  // create interface labels out of dummy transitions
  PNAPI_FOREACH(t, myParser.interface_)
  {
    string remapped = remap(*t, edgeLabels);

    if(remapped.substr(0,5) != "FINAL")
    {
      PNAPI_ASSERT(remapped.find('/') == remapped.npos); // petrify should not rename/create dummy transitions

      if(inputLabels.count(remapped) > 0)
      {
        Label * inLabel = interface_.findLabel(remapped);
        if (inLabel == NULL)
        {
          interface_.addInputLabel(remapped);
        }
      }
      else if(outputLabels.count(remapped) > 0)
      {
        Label * outLabel = interface_.findLabel(remapped);
        if(outLabel == NULL)
        {
          interface_.addOutputLabel(remapped);
        }
      }
      else if(synchronousLabels.count(remapped) > 0)
      {
        Label * synLabel = interface_.findLabel(remapped);
        if(synLabel == NULL)
        {
          interface_.addSynchronousLabel(remapped);
        }
      }
      // else must be TAU
    }
  }

  // create transitions
  PNAPI_FOREACH(t, myParser.transitions_)
  {
    string remapped = remap(*t, edgeLabels);

    if(remapped.substr(0, 5) != "FINAL")
    {
      // create transition if necessary
      Transition * transition = findTransition("t" + *t);
      if(transition == NULL)
      {
        transition = &createTransition("t" + *t);
        
        // link transition to corresponding label
        string labelName = remapped.substr( 0, remapped.find('/') );      // remove possible /

        Label * label = interface_.findLabel(labelName);

        if(inputLabels.count(labelName) > 0)
        {
          if(label == NULL) 
          {
            label = &interface_.addInputLabel(labelName);
          }
          transition->addLabel(*label);
        }
        else if(outputLabels.count(labelName) > 0)
        {
          if(label == NULL)
          {
            label = &interface_.addOutputLabel(labelName);
          }
          transition->addLabel(*label);
        }
        else if(synchronousLabels.count(labelName) > 0)
        {
          if(label == NULL)
          {
            label = &interface_.addSynchronousLabel(labelName);
          }
          transition->addLabel(*label);
        }
        // else it was a TAU transition
      }

      // create arcs t->p
      map<string, unsigned int> & tmpPlaceNameMap = myParser.arcs_[*t];
      PNAPI_FOREACH(p, tmpPlaceNameMap)
      {
        pnapi::Arc * arc = findArc(*transition, *findNode(p->first));
        if ( arc )
        {
            arc->setWeight(arc->getWeight() + p->second);
        }
        else
        {
            createArc(*transition, *findNode(p->first), p->second);
        }
      }
    }
  }

    // create arcs p->t

  // Create a map of string sets for final condition creation.
  map<string, set< pair < string, unsigned int> > > finalCondMap;
  PNAPI_FOREACH(p, myParser.places_)
  {
    PNAPI_FOREACH(t, myParser.arcs_[*p])
    {
      string transitionName = t->first; //remap(t->first, edgeLabels);

      if (remap(t->first, edgeLabels).substr(0,5) != "FINAL")
      {
          createArc(*findPlace(*p), *findTransition("t" + transitionName), t->second);
      }
      else
      {
        // This place is the result of a final node
        finalCondMap[transitionName].insert(make_pair(*p,t->second));
      }
    }
  }

  // fill interface places not occurring in the automaton
  PNAPI_FOREACH(input, inputLabels)
  {
    if(interface_.findLabel(*input) == NULL)
    {
      interface_.addInputLabel(*input);
    }
  }
  PNAPI_FOREACH(output, outputLabels)
  {
    if(interface_.findLabel(*output) == NULL)
    {
      interface_.addOutputLabel(*output);
    }
  }
  PNAPI_FOREACH(syn, synchronousLabels)
  {
    if(interface_.findLabel(*syn) == NULL)
    {
      interface_.addSynchronousLabel(*syn);
    }
  }

  // initialize final condition
  finalCondition_ = false;

  // For each transition found to be a final transition...
  PNAPI_FOREACH(transIt, finalCondMap)
  {
    // Create a set for the places having this transition in their post set.
    set< pair< Place *, unsigned int> > nextTrans;

    // For each place in the preset...
    PNAPI_FOREACH(placesIt, transIt->second)
    {
      // Insert this place in the preset.
      nextTrans.insert(make_pair(findPlace(placesIt->first), placesIt->second));
    }

    pnapi::formula::Conjunction * fd = NULL;
    pnapi::formula::FormulaEqual * store = NULL;
    // create clause for the final condition
    PNAPI_FOREACH(p, nextTrans)
    {
      if(fd == NULL)
      {
        if (store == NULL)
        {
          store = new pnapi::formula::FormulaEqual(*(p->first), p->second);
        }
        else
        {
          pnapi::formula::FormulaEqual * tmpF = new pnapi::formula::FormulaEqual(*(p->first), p->second);
          fd = new pnapi::formula::Conjunction(*store, *tmpF);
          delete tmpF;
          delete store;
          store = NULL;
        }
      }
      else
      {
        pnapi::formula::FormulaEqual * tmpF1 = new pnapi::formula::FormulaEqual(*(p->first), p->second);
        pnapi::formula::Conjunction * tmpF2 = new pnapi::formula::Conjunction(*fd, *tmpF1);
        delete fd;
        delete tmpF1;
        fd = tmpF2;
      }
    }
    
    if(fd != NULL)
    {
      finalCondition_ = (finalCondition_.getFormula() || (*fd));
      delete fd;
    }
    else
    {
      if(store != NULL)
      {
        finalCondition_ = (finalCondition_.getFormula() || (*store));
        delete store;
      }
    }
  }
  
  // make all other places empty
  finalCondition_.allOtherPlacesEmpty(*this);

  // cleaning up
  remove(pnFileName.c_str());
}

} /* namespace pnapi */
