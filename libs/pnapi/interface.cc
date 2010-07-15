/*!
 * \file    interface.cc
 *
 * \brief   Interface class holding asynchronous and synchronous communication
 *
 * \author  Niels Lohmann <nlohmann@informatik.hu-berlin.de>,
 *          Christian Gierds <gierds@informatik.hu-berlin.de>,
 *          Martin Znamirowski <znamirow@informatik.hu-berlin.de>,
 *          Robert Waltemath <robert.waltemath@uni-rostock.de>,
 *          Stephan Mennicke <stephan.mennicke@uni-rostock.de>,
 *          last changes of: $Author: cas $
 *
 * \since   2005-10-18
 *
 * \date    $Date: 2010-07-05 13:50:04 +0200 (Mon, 05 Jul 2010) $
 *
 * \version $Revision: 5879 $
 */

#include "config.h"

#include "pnapi-assert.h"

#include "interface.h"
#include "petrinet.h"
#include "util.h"

#include <iostream>
#include <sstream>
#include <vector>

using std::cerr;
using std::endl;
using std::map;
using std::set;
using std::string;
using std::stringstream;
using std::vector;

namespace pnapi
{

/*!
 * \brief constructor
 */
Interface::Interface(PetriNet & net) :
  net_(net), nextID_(0)
{
}

/*!
 * \brief compose constructor
 * 
 * Given two interfaces, ports with equal names will be composed
 * and the result will be stored in this interface.
 * Ports without a partner will be copied.
 * Ports remaining empty after composition will be removed.
 * 
 * By composition the given references label2label, label2place
 * and commonLabels will be written.
 * 
 * \param net underlying petri net
 * \param interface1 first interface of composition
 * \param interface2 second interface of composition
 * \param label2label mapping from original labels to labels in resulting interface
 * \param label2place mapping from original labels to generated places
 * \param commonLabels common synchronous labels
 */
Interface::Interface(PetriNet & net, const Interface & interface1, const Interface & interface2,
                     std::map<Label *, Label *> & label2label, std::map<Label *, Place *> & label2place,
                     std::set<Label *> & commonLabels) :
  net_(net), nextID_(0)
{
  // seen ports of interface2
  set<Port *> seenPorts;
                       
  // copy or compose ports of interface1
  PNAPI_FOREACH(port, interface1.ports_)
  {
    Label * l1 = *(port->second->getAllLabels().begin());
    PNAPI_ASSERT_USER(l1 != NULL, string("port '") + port->first + "' is empty"); 
    
    Label * l2 = interface2.findLabel(l1->getName());
    
    if(l2 == NULL) // no matching port
    {
      // copy port
      Port * p = new Port(net_, port->first);
      ports_[port->first] = p;
      
      PNAPI_FOREACH(label, port->second->getAllLabels())
      {
        label2label[*label] = &(p->addLabel((*label)->getName(), (*label)->getType()));
      }
    }
    else
    {
      Port * p2 = &(l2->getPort());
      // compliance check with composition
      PNAPI_ASSERT_USER((port->second->getAllLabels().size() == p2->getAllLabels().size()),
                        string("ports '") + port->first + "' and '" + p2->getName() + "' share a label but do not fit",
                        exception::UserCausedError::UE_COMPOSE_ERROR);
                        
      PNAPI_FOREACH(l1, port->second->getAllLabels())
      {
        Label * l2 = p2->findLabel((*l1)->getName());
        PNAPI_ASSERT_USER(l2 != NULL,
                          string("ports '") + port->first + "' and '" + p2->getName() + "' only fit partially",
                          exception::UserCausedError::UE_COMPOSE_ERROR);
        PNAPI_ASSERT_USER((*l1)->getType() + l2->getType() == 4,
                          string("labels '") + l2->getName() + "' do not fit by type",
                          exception::UserCausedError::UE_COMPOSE_ERROR);
        
        if(l2->getType() == Label::SYNCHRONOUS)
        {
          commonLabels.insert(*l1);
          commonLabels.insert(l2);
        }
        else
        {
          // replace label by a place
          Place * p = &(net_.createPlace(l2->getName()));
          label2place[*l1] = p;
          label2place[l2] = p;
        }
      }
      seenPorts.insert(p2);
    }
  }
  
  // copy remaining ports of interface2
  PNAPI_FOREACH(port, interface2.ports_)
  {
    if(seenPorts.find(port->second) == seenPorts.end()) // no matching port
    {
      // copy port
      Port * p = new Port(net_, port->first);
      ports_[port->first] = p;
      
      PNAPI_FOREACH(label, port->second->getAllLabels())
      {
        label2label[*label] = &(p->addLabel((*label)->getName(), (*label)->getType()));
      }
    }
    // else this port already has been composed in loop above
  }
}

/*!
 * \brief destructor
 */
Interface::~Interface()
{
  clear();
}

/*!
 * \brief removes all ports and their labels
 * 
 * \note Port "" will be recreated.
 */
void Interface::clear()
{
  PNAPI_FOREACH(port, ports_)
  {
    delete (port->second);
  }
  ports_.clear();
  nextID_ = 0;
}

/*!
 * \brief adding a single port
 */
Port & Interface::addPort(const std::string & name)
{
  PNAPI_ASSERT_USER(ports_[name] == NULL, string("port '") + name + "' already exists");

  Port * p = new Port(net_, name);
  ports_[name] = p;
  return *p;
}

/*!
 * \brief adding a label
 */
Label & Interface::addLabel(const std::string & name, Label::Type type, const std::string & port)
{
  Label * result = NULL;
  switch(type)
  {
  case Label::INPUT: result = &addInputLabel(name, port); break;
  case Label::OUTPUT: result = &addOutputLabel(name, port); break;
  case Label::SYNCHRONOUS: result = &addSynchronousLabel(name, port); break;
  default: PNAPI_ASSERT(false);
  }
  
  return *result;
}

/*!
 * \brief adding an input label
 */
Label & Interface::addInputLabel(const std::string & label, Port & port)
{
  return port.addInputLabel(label);
}

/*!
 * \brief adding an input label
 */
Label & Interface::addInputLabel(const std::string & label, const std::string & port)
{
  if(port == "")
  {
    Port & p = addPort(generatePortName());
    return p.addInputLabel(label);
  }
  PNAPI_ASSERT_USER(ports_[port] != NULL, string("unknown port '") + port + "'");
  return ports_[port]->addInputLabel(label);
}

/*!
 * \brief adding an output label
 */
Label & Interface::addOutputLabel(const std::string & label, Port & port)
{
  return port.addOutputLabel(label);
}

/*!
 * \brief adding an output label
 */
Label & Interface::addOutputLabel(const std::string & label, const std::string & port)
{
  if(port == "")
  {
    Port & p = addPort(generatePortName());
    return p.addOutputLabel(label);
  }
  PNAPI_ASSERT_USER(ports_[port] != NULL, string("unknown port '") + port + "'");
  return ports_[port]->addOutputLabel(label);
}

/*!
 * \brief adding a synchronous label
 */
Label & Interface::addSynchronousLabel(const std::string & label, Port & port)
{
  return port.addSynchronousLabel(label);
}

/*!
 * \brief adding a synchronous label
 */
Label & Interface::addSynchronousLabel(const std::string & label, const std::string & port)
{
  if(port == "")
  {
    Port & p = addPort(generatePortName());
    return p.addSynchronousLabel(label);
  }
  PNAPI_ASSERT_USER(ports_[port] != NULL, string("unknown port '") + port + "'");
  return ports_[port]->addSynchronousLabel(label);
}

/*!
 * \brief swaps input and output labels
 */
void Interface::mirror()
{
  PNAPI_FOREACH(port, ports_)
  {
    port->second->mirror();
  }
}

/*!
 * \brief returning the port that belongs to the given name
 */
Port * Interface::getPort(const std::string & port) const
{
  map<string, Port *>::const_iterator it = ports_.find(port);
  return ((it == ports_.end()) ? NULL : (it->second));
}

/*!
 * \brief returning ports map
 */
const std::map<std::string, Port *> & Interface::getPorts() const
{
  return ports_;
}

/*!
 * \brief returning input labels
 */
const std::set<Label *> & Interface::getInputLabels(Port & port) const
{
  return port.getInputLabels();
}

/*!
 * \brief returning input labels
 * 
 * if no name (or an empty name) is given, all input labels will be returned.
 */
std::set<Label *> Interface::getInputLabels(const std::string & port) const
{
  set<Label *> result;
  if(port == "")
  {
    PNAPI_FOREACH(p, ports_)
    {
      result = util::setUnion(result, p->second->getInputLabels());
    }
  }
  else
  {
    map<string, Port *>::const_iterator p = ports_.find(port);
    if(p != ports_.end())
    {
      result = p->second->getInputLabels();
    }
  }
  
  return result;
}

/*!
 * \brief returning output labels
 */
const std::set<Label *> & Interface::getOutputLabels(Port & port) const
{
  return port.getOutputLabels();
}

/*!
 * \brief returning output labels
 * 
 * if no name (or an empty name) is given, all output labels will be returned.
 */
std::set<Label *> Interface::getOutputLabels(const std::string & port) const
{
  set<Label *> result;
  if (port == "")
  {
    PNAPI_FOREACH(p, ports_)
    {
      result = util::setUnion(result, p->second->getOutputLabels());
    }
  }
  else
  {
    map<string, Port *>::const_iterator p = ports_.find(port);
    if(p != ports_.end())
    {
      result = p->second->getOutputLabels();
    }
  }
  
  return result;
}

/*!
 * \brief returning synchronous labels
 */
const std::set<Label *> & Interface::getSynchronousLabels(Port & port) const
{
  return port.getSynchronousLabels();
}

/*!
 * \brief returning synchronous labels
 * 
 * if no name (or an empty name) is given, all synchronous labels will be returned.
 */
std::set<Label *> Interface::getSynchronousLabels(const std::string & port) const
{
  set<Label *> result;
  if (port == "")
  {
    PNAPI_FOREACH(p, ports_)
    {
      result = util::setUnion(result, p->second->getSynchronousLabels());
    }
  }
  else
  {
    map<string, Port *>::const_iterator p = ports_.find(port);
    if(p != ports_.end())
    {
      result = p->second->getSynchronousLabels();
    }
  }
  
  return result;
}

/*!
 * \brief returning asynchronous labels (i.e. union of input and output places)
 */
std::set<Label *> Interface::getAsynchronousLabels(Port & port) const
{
  return util::setUnion(getInputLabels(port), getOutputLabels(port));
}

/*!
 * \brief returning asynchronous labels (i.e. union of input and output places)
 */
std::set<Label *> Interface::getAsynchronousLabels(const std::string & port) const
{
  return util::setUnion(getInputLabels(port), getOutputLabels(port));
}

/*!
 * \brief find a certain label
 */
Label * Interface::findLabel(const std::string & label) const
{
  PNAPI_FOREACH(i, ports_)
  {
    Label * result = i->second->findLabel(label);
    if (result)
      return result;
  }
  return NULL;
}

/*!
 * \brief get the underlying petri net
 */
PetriNet & Interface::getPetriNet() const
{
  return net_;
}

/*!
 * \brief if there is no port or label defined
 */
bool Interface::isEmpty() const
{
  return ports_.empty();
}

/*!
 * \brief generate unused port name
 */
std::string Interface::generatePortName()
{
  string result;
  while(true)
  {
    std::stringstream ss;
    ss << "port" << (nextID_++);
    if(ports_.find(ss.str()) == ports_.end())
    {
      result = ss.str();
      break;
    }
  }
  return result;
}

} /* namespace pnapi */
