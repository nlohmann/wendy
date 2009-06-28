/*!
 * \file    petrinet.cc
 *
 * \brief   Petri net basics
 *
 * \author  Niels Lohmann <nlohmann@informatik.hu-berlin.de>,
 *          Christian Gierds <gierds@informatik.hu-berlin.de>,
 *          Martin Znamirowski <znamirow@informatik.hu-berlin.de>,
 *          Robert Waltemath <robert.waltemath@uni-rostock.de>,
 *          last changes of: $Author: cas $
 *
 * \since   2005-10-18
 *
 * \date    $Date: 2009-06-28 20:03:39 +0200 (So, 28. Jun 2009) $
 *
 * \version $Revision: 4372 $
 */

#include "config.h"
#include <cassert>

#ifndef NDEBUG
#include <iostream>
#include <fstream>
#include "io.h"
using std::cout;
using std::cerr;
using std::endl;
using pnapi::io::operator<<;
using pnapi::io::util::operator<<;
#endif

#include <sstream>

#include "link.h"
#include "util.h"
#include "petrinet.h"

using std::pair;
using std::multimap;
using std::vector;
using std::stringstream;
using std::ostringstream;
using std::deque;

namespace pnapi
{

  namespace util
  {

  /****************************************************************************
   *** Class ComponentObserver Function Defintions
   ***************************************************************************/

  /*!
   */
  ComponentObserver::ComponentObserver(PetriNet & net) :
    net_(net)
  {
    assert(&net.observer_ == this);
  }


  /*!
   */
  PetriNet & ComponentObserver::getPetriNet() const
  {
    return net_;
  }


  /*!
   */
  void ComponentObserver::updateNodeNameHistory(Node & node,
					  const std::deque<string> & oldHistory)
  {
    assert(net_.containsNode(node));

    finalizeNodeNameHistory(node, oldHistory);
    initializeNodeNameHistory(node);
  }


  /*!
   */
  void ComponentObserver::updatePlaceType(Place & place, Node::Type type)
  {
    assert(net_.containsNode(place));

    finalizePlaceType(place, type);
    initializePlaceType(place);
  }


  void ComponentObserver::updateArcCreated(Arc & arc)
  {
    assert(&arc.getPetriNet() == &net_);
    assert(net_.arcs_.find(&arc) == net_.arcs_.end());
    assert(net_.findArc(arc.getSourceNode(), arc.getTargetNode()) == NULL);

    // update pre- and postsets
    net_.arcs_.insert(&arc);
    arc.getTargetNode().preset_.insert(&arc.getSourceNode());
    arc.getTargetNode().presetArcs_.insert(&arc);
    arc.getSourceNode().postset_.insert(&arc.getTargetNode());
    arc.getSourceNode().postsetArcs_.insert(&arc);

    // update transition type
    arc.getTransition().updateType();
  }

  /*
   * \pre Arc has to be removed from PetriNet::arcs_ before calling this
   */
  void ComponentObserver::updateArcRemoved(Arc & arc)
  {
    assert(&arc.getPetriNet() == &net_);
    assert(net_.arcs_.find(&arc) == net_.arcs_.end());

    // update pre- and postsets
    arc.getTargetNode().preset_.erase(&arc.getSourceNode());
    arc.getTargetNode().presetArcs_.erase(&arc);
    arc.getSourceNode().postset_.erase(&arc.getTargetNode());
    arc.getSourceNode().postsetArcs_.erase(&arc);

    // update transition type
    arc.getTransition().updateType();
  }

  /*
   * \brief Only purpose of this function is erasing the original arc
   *        by mirroring so that ComponentObserver::updateArcRemoved
   *        can be called.
   */
  void ComponentObserver::updateArcMirror(Arc & arc)
  {
    // you could check place type and pre-/postset here
    // assert(...)

    net_.arcs_.erase(&arc);
  }


  void ComponentObserver::updatePlaces(Place & place)
  {
    updateNodes(place);
    net_.places_.insert(&place);
    initializePlaceType(place);
  }


  void ComponentObserver::updateTransitions(Transition & trans)
  {
    updateNodes(trans);
    net_.transitions_.insert(&trans);
    if (trans.isSynchronized())
      net_.synchronizedTransitions_.insert(&trans);
  }


  void ComponentObserver::updateNodes(Node & node)
  {
    assert(&node.getPetriNet() == &net_);
    assert(!net_.containsNode(node));
    assert(net_.nodesByName_.find(node.getName()) == net_.nodesByName_.end());

    net_.nodes_.insert(&node);
    initializeNodeNameHistory(node);
  }


  void ComponentObserver::updateNodesMerged(Node & node1, Node & node2)
  {
    //assert(node2.getNameHistory().empty());

    // delete node2 from net
    Place * place = dynamic_cast<Place *>(&node2);
    if (place != NULL)
      net_.deletePlace(*place);
    else
      net_.deleteTransition(*dynamic_cast<Transition *>(&node2));
  }


  void ComponentObserver::initializeNodeNameHistory(Node & node)
  {
    deque<string> history = node.getNameHistory();
    deque<string>::iterator it = history.begin();
    //for (deque<string>::iterator it = history.begin(); it != history.end();
    //     ++it)
    {
      assert((net_.nodesByName_.find(*it))->second == &node ||
	     net_.nodesByName_.find(*it) == net_.nodesByName_.end());
      net_.nodesByName_[*it] = &node;
    }
  }


  void ComponentObserver::finalizeNodeNameHistory(Node & node,
						  const deque<string> & history)
  {
    deque<string>::const_iterator it = history.begin();
    //for (deque<string>::const_iterator it = history.begin();
    //     it != history.end(); ++it)
    net_.nodesByName_.erase(*it);
  }


  void ComponentObserver::initializePlaceType(Place & place)
  {
    string port = place.getPort();

    switch (place.getType())
      {
      case Node::INTERNAL:
	assert(port.empty());
	net_.internalPlaces_.insert(&place);
	break;
      case Node::INPUT:
	net_.inputPlaces_.insert(&place);
	net_.interfacePlaces_.insert(&place);
	if (!port.empty())
	  net_.interfacePlacesByPort_
	    .insert(pair<string, Place *>(port, &place));
	break;
      case Node::OUTPUT:
	net_.outputPlaces_.insert(&place);
	net_.interfacePlaces_.insert(&place);
	if (!port.empty())
	  net_.interfacePlacesByPort_
	    .insert(pair<string, Place *>(port, &place));
	break;
      default: break;
      }
  }


  void ComponentObserver::finalizePlaceType(Place & place, Node::Type type)
  {
    pair<multimap<string, Place *>::iterator,
      multimap<string, Place *>::iterator> portRange =
      net_.interfacePlacesByPort_.equal_range(place.getPort());

    switch (type)
      {
      case Node::INTERNAL:
	net_.internalPlaces_.erase(&place);
	break;
      case Node::INPUT:
	net_.inputPlaces_.erase(&place);
	net_.interfacePlaces_.erase(&place);
	for (multimap<string, Place *>::iterator it = portRange.first;
	     it != portRange.second; ++it)
	  if (it->second == &place)
	    {
	      net_.interfacePlacesByPort_.erase(it);
	      break;
	    }
	break;
      case Node::OUTPUT:
	net_.outputPlaces_.erase(&place);
	net_.interfacePlaces_.erase(&place);
	for (multimap<string, Place *>::iterator it = portRange.first;
	     it != portRange.second; ++it)
	  if (it->second == &place)
	    {
	      net_.interfacePlacesByPort_.erase(it);
	      break;
	    }
	break;
      default: break;
      }
  }

  } /* namespace util */



  /****************************************************************************
   *** Class PetriNet Function Defintions
   ***************************************************************************/

  /*!
   * \note    The condition is standardly set to True.
   */
  PetriNet::PetriNet() :
    observer_(*this)
  {
  }


  /*!
   * The copy constructor with deep copy.
   *
   * \note    The condition is standardly set to True.
   */
  PetriNet::PetriNet(const PetriNet & net) :
    labels_(net.labels_),
    observer_(*this),
    condition_(net.condition_, copyStructure(net)),
    meta_(net.meta_)
  {
    setConstraintLabels(net.constraints_);
  }


  /*!
   * The destructor of the PetriNet class.
   */
  PetriNet::~PetriNet()
  {
    clear();

    assert(nodes_.empty());
    assert(nodesByName_.empty());
    assert(transitions_.empty());
    assert(places_.empty());
    assert(internalPlaces_.empty());
    assert(inputPlaces_.empty());
    assert(outputPlaces_.empty());
    assert(interfacePlaces_.empty());
    assert(interfacePlacesByPort_.empty());
    assert(arcs_.empty());
  }


  void PetriNet::clear()
  {
    labels_.clear();
    meta_.clear();
    constraints_.clear();
    condition_ = true;

    // delete all places
    set<Place *> places = places_;
    for (set<Place *>::iterator it = places.begin(); it != places.end(); ++it)
      deletePlace(**it);

    // delete all transitions
    set<Transition *> transitions = transitions_;
    for (set<Transition *>::iterator it = transitions.begin();
	 it != transitions.end(); ++it)
      deleteTransition(**it);
  }


  /*!
   * The "=" operator.
   */
  PetriNet & PetriNet::operator=(const PetriNet & net)
  {
    assert(this != &net);

    this->~PetriNet();
    return *new (this) PetriNet(net);
  }


  /*!
   */
  map<const Place *, const Place *>
  PetriNet::copyPlaces(const PetriNet & net, const string & prefix)
  {
    map<const Place *, const Place *> placeMapping;
    for (set<Place *>::iterator it = net.places_.begin();
	 it != net.places_.end(); ++it)
      {
	assert(!containsNode((*it)->getName()));
	placeMapping[*it] = new Place(*this, observer_, **it, prefix);
      }
    return placeMapping;
  }


  /*!
   * Adds all nodes and arcs of the second net.
   *
   * \pre   the node names are unique (disjoint name sets); use
   *        prefixNodeNames() on <em>both</em> nets to achieve this
   */
  map<const Place *, const Place *>
  PetriNet::copyStructure(const PetriNet & net, const string & prefix)
  {
    // add all transitions of the net
    for (set<Transition *>::iterator it = net.transitions_.begin();
	 it != net.transitions_.end(); ++it)
      {
	assert(!containsNode((*it)->getName()));
	new Transition(*this, observer_, **it, prefix);
      }

    // add all places
    const map<const Place *, const Place *> & placeMapping =
      copyPlaces(net, prefix);

    // create arcs according to the arcs in the net
    for (set<Arc *>::iterator it = net.arcs_.begin(); it != net.arcs_.end();
	 ++it)
      new Arc(*this, observer_, **it);

    return placeMapping;
  }


  /*!
   */
  Condition & PetriNet::finalCondition()
  {
    return condition_;
  }


  /*!
   */
  const Condition & PetriNet::finalCondition() const
  {
    return condition_;
  }


  /*!
   * Given a second Petri net #net, the internal structure is added and input
   * and output places are connected appropriatly (if an input and an output
   * place name of the two nets match).
   */
  void PetriNet::compose(const PetriNet & net, const string & prefix,
			 const string & netPrefix)
  {
    PetriNet result;
    map<const Place *, const Place *> placeMap;

    set<Place *> thisPlaces = getInternalPlaces();
    set<Place *> netPlaces = net.getInternalPlaces();
    for (set<Place *>::iterator p = thisPlaces.begin(); p != thisPlaces.end(); p++)
      placeMap[*p] = &result.createPlace(prefix+(*p)->getName(), Node::INTERNAL, (*p)->getTokenCount());
    for (set<Place *>::iterator p = netPlaces.begin(); p != netPlaces.end(); p++)
      placeMap[*p] = &result.createPlace(netPrefix+(*p)->getName(), Node::INTERNAL, (*p)->getTokenCount());
    set<Transition *> thisTransitions = getTransitions();
    set<Transition *> netTransitions = net.getTransitions();
    for (set<Transition *>::iterator t = thisTransitions.begin(); t != thisTransitions.end(); t++)
    {
      Transition &nt = result.createTransition(prefix+(*t)->getName());
      set<Arc *> preset = (*t)->getPresetArcs();
      set<Arc *> postset = (*t)->getPostsetArcs();
      for (set<Arc *>::iterator f = preset.begin(); f != preset.end(); f++)
      {
        Place *p = result.findPlace(prefix+(*f)->getPlace().getName());
        if (p != NULL)
          result.createArc(*p, nt, (*f)->getWeight());
      }
      for (set<Arc *>::iterator f = postset.begin(); f != postset.end(); f++)
      {
        Place *p = result.findPlace(prefix+(*f)->getPlace().getName());
        if (p != NULL)
          result.createArc(nt, *p, (*f)->getWeight());
      }
    }
    for (set<Transition *>::iterator t = netTransitions.begin(); t != netTransitions.end(); t++)
    {
      Transition &nt = result.createTransition(netPrefix+(*t)->getName());
      set<Arc *> preset = (*t)->getPresetArcs();
      set<Arc *> postset = (*t)->getPostsetArcs();
      for (set<Arc *>::iterator f = preset.begin(); f != preset.end(); f++)
      {
        Place *p = result.findPlace(netPrefix+(*f)->getPlace().getName());
        if (p != NULL)
          result.createArc(*p, nt, (*f)->getWeight());
      }
      for (set<Arc *>::iterator f = postset.begin(); f != postset.end(); f++)
      {
        Place *p = result.findPlace(netPrefix+(*f)->getPlace().getName());
        if (p != NULL)
          result.createArc(nt, *p, (*f)->getWeight());
      }
    }

    set<Place *> thisInput = getInputPlaces();
    set<Place *> thisOutput = getOutputPlaces();
    set<Place *> netInput = net.getInputPlaces();
    set<Place *> netOutput = net.getOutputPlaces();

    for (set<Place *>::iterator p = thisInput.begin(); p != thisInput.end(); p++)
    {
      Place *ip = NULL;
      Place *opponent = net.findPlace((*p)->getName());
      set<Arc *> postset = (*p)->getPostsetArcs();
      set<Arc *> preset;

      if (opponent == NULL)
      {
        ip = &result.createPlace(prefix+(*p)->getName(), Node::INPUT);
      }
      else
      {
        ip = &result.createPlace((*p)->getName());
        if (opponent->getType() != Node::OUTPUT)
          preset.clear();
        else
        {
          netOutput.erase(opponent);
          placeMap[opponent] = ip;
          preset = opponent->getPresetArcs();
        }
      }
      placeMap[*p] = ip;
      for (set<Arc *>::iterator f = postset.begin(); f != postset.end(); f++)
        result.createArc(*ip, *result.findTransition(prefix+(*f)->getTransition().getName()), (*f)->getWeight());
      for (set<Arc *>::iterator f = preset.begin(); f != preset.end(); f++)
        result.createArc(*result.findTransition(netPrefix+(*f)->getTransition().getName()), *ip, (*f)->getWeight());
    }

    for (set<Place *>::iterator p = thisOutput.begin(); p != thisOutput.end(); p++)
    {
      //cerr << "iterating output set: " << (*p)->getName() << endl;
      Place *ip = NULL;
      Place *opponent = net.findPlace((*p)->getName());
      set<Arc *> postset;
      set<Arc *> preset = (*p)->getPresetArcs();

      if (opponent == NULL)
      {
        ip = &result.createPlace(prefix+(*p)->getName(), Node::OUTPUT);
      }
      else
      {
        ip = &result.createPlace((*p)->getName());
        if (opponent->getType() != Node::INPUT)
          postset.clear();
        else
        {
          netInput.erase(opponent);
          placeMap[opponent] = ip;
          postset = opponent->getPostsetArcs();
        }
      }
      placeMap[*p] = ip;
      for (set<Arc *>::iterator f = postset.begin(); f != postset.end(); f++)
        result.createArc(*ip, *result.findTransition(netPrefix+(*f)->getTransition().getName()), (*f)->getWeight());
      for (set<Arc *>::iterator f = preset.begin(); f != preset.end(); f++)
        result.createArc(*result.findTransition(prefix+(*f)->getTransition().getName()), *ip, (*f)->getWeight());
    }

    for (set<Place *>::iterator p = netInput.begin(); p != netInput.end(); p++)
    {
      Place *np = &result.createPlace((*p)->getName(), Node::INPUT);
      placeMap[*p] = np;

      set<Arc *> postset = (*p)->getPostsetArcs();
      for (set<Arc *>::iterator f = postset.begin(); f != postset.end(); f++)
        result.createArc(*np, *result.findTransition(netPrefix+(*f)->getTransition().getName()), (*f)->getWeight());
    }

    for (set<Place *>::iterator p = netOutput.begin(); p != netOutput.end(); p++)
    {
      Place *np = &result.createPlace((*p)->getName(), Node::OUTPUT);
      placeMap[*p] = np;

      set<Arc *> preset = (*p)->getPresetArcs();
      for (set<Arc *>::iterator f = preset.begin(); f != preset.end(); f++)
        result.createArc(*result.findTransition(netPrefix+(*f)->getTransition().getName()), *np, (*f)->getWeight());
    }

    result.finalCondition().merge(finalCondition(), placeMap);
    result.finalCondition().merge(net.finalCondition(), placeMap);

    /*cerr << io::owfn << *this;
    cerr << io::owfn << net;
    cerr << io::owfn << result;*/

    *this = result;

    /*assert(prefix != netPrefix);

    map<string, PetriNet *> nets;
    nets[prefix] = this;
    nets[netPrefix] = const_cast<PetriNet *>(&net);

    *this = compose(nets);*/


    /* below the old implementation
    PetriNet tmpNet = net;  // copy the net to prefix it

    // find the interface places to be merged
    vector<pair<Place *, Place *> > mergePlaces;
    const set<Place *> & interface = tmpNet.getInterfacePlaces();
    for (set<Place *>::iterator it = interface.begin(); it != interface.end();
	 ++it)
      {
	Place * place = findPlace((*it)->getName());
	if (place != NULL && place->isComplementType((*it)->getType()))
	  mergePlaces.push_back(pair<Place *, Place *>(place, *it));
      }

    // prefix and combine the nets
    prefixNodeNames(prefix);
    tmpNet.prefixNodeNames(netPrefix);
    const map<const Place *, const Place *> & placeMapping =
      copyStructure(tmpNet);

    // translate references and merge places
    for (vector<pair<Place *, Place *> >::iterator it = mergePlaces.begin();
	 it != mergePlaces.end(); ++it)
      {
	if (&it->first->getPetriNet() != this)
	  it->first = findPlace(it->first->getName());
	else if (&it->second->getPetriNet() != this)
	  it->second = findPlace(it->second->getName());
	it->first->merge(*it->second);
	//deletePlace(*it->second);
      }

    // merge final conditions
    condition_.merge(net.condition_, placeMapping);
    */
  }


  /*!
   */
  PetriNet PetriNet::compose(const map<string, PetriNet *> & nets)
  {
    PetriNet result;

    // create instance map
    typedef map<string, vector<PetriNet> > Instances;
    Instances instances;
    for (map<string, PetriNet *>::const_iterator it = nets.begin();
	 it != nets.end(); ++it)
      instances[it->first].push_back(*it->second);

    // create wiring
    typedef map<Place *, LinkNode *> Wiring;
    Wiring wiring;
    for (Instances::iterator it1 = instances.begin();
	 it1 != instances.end(); ++it1)
      {
	string prefix = it1->first; assert(!prefix.empty());
	PetriNet & net1 = *it1->second.begin();

	for (Instances::iterator it2 = instances.begin();
	     it2 != instances.end(); ++it2)
	  {
	    PetriNet & net2 = *it2->second.begin();
	    wire(net1, net2, wiring);
	  }
      }

    // create result net
    return result.createFromWiring(instances, wiring);
  }


  /*!
   */
  void PetriNet::wire(const PetriNet & net1, const PetriNet & net2,
		      map<Place *, LinkNode *> & wiring)
  {
    set<Place *> interface = net1.getInterfacePlaces();
    for (set<Place *>::iterator it = interface.begin();
	 it != interface.end(); ++it)
      {
	Place * p1 = *it;
	Place * p2 = net2.findPlace(p1->getName());
	if (p2 != NULL && p2->isComplementType(p1->getType()))
	  {
	    LinkNode * node1 = wiring[p1];
	    if (node1 == NULL)
	      wiring[p1] = node1 = new LinkNode(*p1, LinkNode::ANY);
	    LinkNode * node2 = wiring[p2];
	    if (node2 == NULL)
	      wiring[p2] = node2 = new LinkNode(*p2, LinkNode::ANY);
	    node1->addLink(*node2);
	  }
      }
  }


  /*!
   */
  PetriNet &
  PetriNet::createFromWiring(map<string, vector<PetriNet> > & instances,
          const map<Place *, LinkNode *> & wiring)
  {
    vector<LinkNode *> wiredNodes;
    wiredNodes.reserve(wiring.size());

    // clean up old net
    clear();

    // add structure of nets
    for (map<string, vector<PetriNet> >::iterator it = instances.begin();
            it != instances.end(); ++it)
    {
      assert(!it->first.empty());

      for (unsigned int i = 0; i < it->second.size(); i++)
      {
        PetriNet & net = it->second[i];
        stringstream ss; ss << i + 1;
        map<const Place *, const Place *> placeMapping =
        copyStructure(net.prefixNodeNames(it->first + "[" + ss.str() +
                        "]."));
        condition_.merge(net.condition_, placeMapping);

        // translate references in wiring
        for (map<Place *, LinkNode *>::const_iterator it = wiring.begin();
                it != wiring.end(); ++it)
        if (placeMapping.find(it->first) != placeMapping.end())
        wiredNodes.push_back(&wiring.find(it->first)->second
                ->replacePlace(const_cast<Place &>(*placeMapping
                                .find(it->first)->second)));
      }
    }

    set<LinkNode *> expanded;
    vector<LinkNode *> result;

    // expand all nodes
    for (vector<LinkNode *>::iterator it = wiredNodes.begin();
            it != wiredNodes.end(); ++it)
    {
      result = (*it)->expand();
      expanded.insert(result.begin(), result.end());
      delete (*it); // clean up
    }

    // join one-to-one links
    LinkNode * node;
    set<LinkNode *>::iterator it;
    while (!expanded.empty())
    {
      it = expanded.begin();
      (*it)->joinPlaces();

      // clean up
      node = &(*it)->getPartner();
      delete *it;
      expanded.erase(it);
      delete node;
      expanded.erase(node);
    }

    return *this;
  }


  /*!
   * \param   source  the source Node
   * \param   target  the target Node
   * \param   weight  weight of the Arc
   * \return  the newly created Arc
   */
  Arc & PetriNet::createArc(Node & source, Node & target, unsigned int weight)
  {
    return *new Arc(*this, observer_, source, target, weight);
  }


  /*!
   * If an empty name is given, one is generated using getUniqueNodeName().
   *
   * \param   name  the (initial) name of the place
   * \param   type  communication type of the place (internal or interface)
   * \return  the newly created place
   */
  Place & PetriNet::createPlace(const string & name, Node::Type type,
				unsigned int tokens, unsigned int capacity,
				const string & port)
  {
    return *new Place(*this, observer_,
		      name.empty() ? getUniqueNodeName("p") : name, type,
		      tokens, capacity, port);
  }


  /*!
   * If an empty name is given, one is generated using getUniqueNodeName().
   */
  Transition & PetriNet::createTransition(const string & name,
					  const std::set<std::string> & labels)
  {
    return *new Transition(*this, observer_,
			   name.empty() ? getUniqueNodeName("t") : name,
			   labels);
  }


  /*!
   */
  bool PetriNet::containsNode(Node & node) const
  {
    return nodes_.find(&node) != nodes_.end();
  }


  /*!
   */
  bool PetriNet::containsNode(const string & name) const
  {
    return findNode(name) != NULL;
  }


  /*!
   */
  Node * PetriNet::findNode(const string & name) const
  {
    map<string, Node *>::const_iterator result = nodesByName_.find(name);

    if (result != nodesByName_.end())
      return result->second;

    return NULL;
  }


  /*!
   * \return  a pointer to the place or a NULL pointer if the place was not
   *          found.
   */
  Place * PetriNet::findPlace(const string & name) const
  {
    // if we had a map placesByName we could avoid the dynamic cast here
    return dynamic_cast<Place *>(findNode(name));
  }


  /*!
   * \return  a pointer to the transition or a NULL pointer if the transition
   *          was not found.
   */
  Transition * PetriNet::findTransition(const string & name) const
  {
    // if we had a map transitionsByName we could avoid the dynamic cast here
    return dynamic_cast<Transition *>(findNode(name));
  }


  /*!
   */
  Arc * PetriNet::findArc(const Node & source, const Node & target) const
  {
    for (set<Arc*>::iterator it = source.getPostsetArcs().begin();
          it != source.getPostsetArcs().end(); ++it)
    {
      if ( &((*it)->getTargetNode()) == &target )
        return *it;
    }

    return NULL;
  }


  /*!
   */
  const set<Node *> & PetriNet::getNodes() const
  {
    return nodes_;
  }


  /*!
   */
  const set<Place *> & PetriNet::getPlaces() const
  {
    return places_;
  }


  /*!
   */
  const set<Place *> & PetriNet::getInternalPlaces() const
  {
    return internalPlaces_;
  }


  /*!
   */
  const set<Place *> & PetriNet::getInputPlaces() const
  {
    return inputPlaces_;
  }


  /*!
   */
  const set<Place *> & PetriNet::getOutputPlaces() const
  {
    return outputPlaces_;
  }


  /*!
   */
  const set<Place *> & PetriNet::getInterfacePlaces() const
  {
    return interfacePlaces_;
  }


  /*!
   */
  set<Place *> PetriNet::getInterfacePlaces(const string & port) const
  {
    set<Place *> places;
    pair<multimap<string, Place *>::const_iterator,
      multimap<string, Place *>::const_iterator> portRange =
      interfacePlacesByPort_.equal_range(port);
    for (multimap<string, Place *>::const_iterator it = portRange.first;
	 it != portRange.second; ++it)
      places.insert(it->second);
    return places;
  }


  /*!
   */
  const set<Transition *> & PetriNet::getTransitions() const
  {
    return transitions_;
  }


  /*!
   */
  const set<Transition *> & PetriNet::getSynchronizedTransitions() const
  {
    return synchronizedTransitions_;
  }


  /*!
   */
  std::set<std::string> PetriNet::getSynchronousLabels() const
  {
    return labels_;
  }


  /*!
   */
  void PetriNet::setSynchronousLabels(const std::set<std::string> & ls)
  {
    labels_ = ls;
  }


  /*!
   * \param   base  base name
   * \return  a string to be used as a name for a new node
   */
  string PetriNet::getUniqueNodeName(const string & base) const
  {
    int i = 1;
    string name;

    // use a "mutable" cache to make this more efficient
    do { ostringstream str; str << base << i++; name = str.str(); }
    while (nodesByName_.find(name) != nodesByName_.end());

    return name;
  }


  /*!
   * \brief   checks whether the Petri net is free choice
   *
   *          A Petri net is free choice iff
   *            all two transitions have either disjoint or equal presets
   *
   * \return  true if the criterion is fulfilled and false if not
   */
  bool PetriNet::isFreeChoice() const
  {
    for (set<Transition *>::iterator t = transitions_.begin();
        t != transitions_.end(); t++)
      for (set<Transition *>::iterator tt = transitions_.begin();
          tt != transitions_.end(); tt++)
      {
        set<Node *> t_pre  = (*t)->getPreset();
        set<Node *> tt_pre = (*tt)->getPreset();
        if ((t_pre != tt_pre) &&
            !(util::setIntersection(t_pre, tt_pre).empty()))
          return false;
      }
    return true;
  }


  /*!
   * \brief   Checks if the net is normal
   *
   * A PetriNet N is called normal iff each transition t has only one
   * Place \f[p \in (P_{in} \cup P_{out}) : p \in ^*t \cup t^*\f]
   *
   * \return  true iff the net is normal
   */
  bool PetriNet::isNormal() const
  {
    for (set<Transition *>::const_iterator t = transitions_.begin();
        t != transitions_.end(); t++)
      if (!(*t)->isNormal())
        return false;

    return true;
  }


  /*!
   * \brief   normalizes the given Petri net
   *
   * A Petri net (resp. open net) is called normal if
   * each transition has only one adjacent interface
   * place.
   *
   * \return  map<Transition *, string> a mapping representing the edge labels
   *          of the later service automaton.
   */
  const std::map<Transition *, string> PetriNet::normalize()
  {
    if (!getSynchronizedTransitions().empty())
    {
      normalize_classical();
    }
    else
    {
      normalize_rules();
    }

    std::map<Transition *, string> edgeLabels; ///< returning map
    edgeLabels.clear();

    for (set<Transition *>::const_iterator t = transitions_.begin();
        t != transitions_.end(); t++)
    {
      switch ((*t)->getType())
      {
      case Node::INPUT:
      {
        set<Arc *> preset = (*t)->getPresetArcs();
        for (set<Arc *>::const_iterator f = preset.begin();
            f != preset.end(); f++)
        {
          if ((*f)->getPlace().getType() == Node::INPUT)
          {
            edgeLabels[*t] = (*f)->getPlace().getName();
            break;
          }
        }
        break;
      }
      case Node::OUTPUT:
      {
        set<Arc *> postset = (*t)->getPostsetArcs();
        for (set<Arc *>::const_iterator f = postset.begin();
            f != postset.end(); f++)
        {
          if ((*f)->getPlace().getType() == Node::OUTPUT)
          {
            edgeLabels[*t] = (*f)->getPlace().getName();
            break;
          }
        }
        break;
      }
      case Node::INTERNAL:
      {
        edgeLabels[*t] = "TAU";
        break;
      }
      default: ;//assert(false);
      }
    }

    return edgeLabels;
  }


  /*!
   * Makes the inner structure of the net, which means to delete all
   * interface places to simplify the firing rules when creating a
   * service automaton.
   */
  void PetriNet::makeInnerStructure()
  {
    std::set<Place *> interface;
    while (!(interface = getInterfacePlaces()).empty())
      deletePlace(**interface.begin());
  }


  /*!
   * described in "Behavioral Constraints for Services" (BPM 2007)
   * (http://dx.doi.org/10.1007/978-3-540-75183-0_20)
   *
   * \note  There is an error in definition 5: The arcs of the transitions
   *        with empty label are missing.
   */
  void PetriNet::produce(const PetriNet & net, const string & aPrefix,
			 const string & aNetPrefix) throw (io::InputError)
  {
    typedef set<Transition *> Transitions;
    typedef map<Transition *, Transitions> Labels;
    typedef map<const Place *, const Place *> PlaceMapping;

    assert(!aPrefix.empty());
    assert(!aNetPrefix.empty());
    assert(aPrefix != aNetPrefix);
    assert(net.inputPlaces_.empty());
    assert(net.outputPlaces_.empty());

    string prefix = aPrefix + ".";
    string netPrefix = aNetPrefix + ".";
    Labels labels = translateConstraintLabels(net);


    // add (internal) places
    prefixNodeNames(prefix);
    PlaceMapping placeMapping = copyPlaces(net, netPrefix);

    // merge final conditions
    condition_.merge(net.condition_, placeMapping);

    // handle transitions with empty label
    for (Transitions::iterator it = net.transitions_.begin();
	 it != net.transitions_.end(); ++it)
      {
	Transition & netTrans = **it; // t'
	Transitions label;
	if (labels.find(&netTrans) != labels.end())
	  label = labels.find(&netTrans)->second;
	if (label.empty())
	  {
	    Transition & trans = createTransition(netTrans, netPrefix);
	    createArcs(trans, netTrans, &placeMapping);
	  }
      }

    // create product transitions
    Transitions labelTransitions;
    for (Labels::const_iterator it1 = labels.begin();
	 it1 != labels.end(); ++it1)
      {
	assert(it1->first != NULL);
	Transition & netTrans = *it1->first;             // t'
	assert(net.containsNode(netTrans));
	Transitions ts = it1->second;
	for (Transitions::const_iterator it2 = ts.begin();
	     it2 != ts.end(); ++it2)
	  {
	    assert(*it2 != NULL);
	    Transition & trans = **it2;                  // t
	    assert(containsNode(trans));
	    labelTransitions.insert(&trans);
	    Transition & prodTrans = createTransition(); // (t, t')
	    createArcs(prodTrans, trans);
	    createArcs(prodTrans, netTrans, &placeMapping);
	  }
      }

    // remove label transitions
    for (Transitions::iterator it = labelTransitions.begin();
	 it != labelTransitions.end(); ++it)
      deleteTransition(**it);
  }


  void PetriNet::setConstraintLabels(const map<Transition *, set<string> > & labels)
  {
    constraints_.clear();
    for (map<Transition *, set<string> >::const_iterator it = labels.begin();
	 it != labels.end(); ++it)
      {
	Transition * t = findTransition(it->first->getName());
	assert(t != NULL);
	constraints_[t] = it->second;
      }
  }


  map<Transition *, set<Transition *> >
  PetriNet::translateConstraintLabels(const PetriNet & net)
    throw (io::InputError)
  {
    typedef set<string> Labels;
    typedef set<Transition *> Transitions;
    typedef map<Transition *, Labels> OriginalLabels;
    typedef map<Transition *, Transitions> ResultLabels;

    OriginalLabels labels = net.constraints_;
    ResultLabels result;
    for (OriginalLabels::iterator it1 = labels.begin();
	 it1 != labels.end(); ++it1)
      {
	assert(it1->first != NULL);
	Transition & t = *it1->first;
	for (Labels::iterator it2 = it1->second.begin();
	     it2 != it1->second.end(); ++it2)
	  {
	    Transition * labelTrans = findTransition(*it2);
	    if (labelTrans != NULL)
	      result[&t].insert(labelTrans);
	    else
	      {
		string filename =
		  net.meta_.find(io::INPUTFILE) != net.meta_.end()
		  ? net.meta_.find(io::INPUTFILE)->second : "";
		throw io::InputError(io::InputError::SEMANTIC_ERROR, filename,
				     0, *it2, "unknown transition");
	      }
	  }
      }
    return result;
  }


  /*!
   */
  Transition & PetriNet::createTransition(const Transition & t,
					  const std::string & prefix)
  {
    return *new Transition(*this, observer_, t, prefix);
  }


  /*!
   */
  void PetriNet::createArcs(Transition & trans, Transition & otherTrans,
			 const map<const Place *, const Place *> * placeMapping)
  {
    typedef set<Arc *> Arcs;

    // add preset arcs of t' to t
    Arcs preset = otherTrans.getPresetArcs();
    for (Arcs::iterator it = preset.begin(); it != preset.end(); ++it)
      {
	Arc & arc = **it;
	Place & place = placeMapping == NULL ? arc.getPlace() :
	  *const_cast<Place *>(placeMapping->find(&arc.getPlace())->second);
	new Arc(*this, observer_, arc, place, trans);
      }

    // add postset arcs of t' to t
    Arcs postset = otherTrans.getPostsetArcs();
    for (Arcs::iterator it = postset.begin(); it != postset.end(); ++it)
      {
	Arc & arc = **it;
	Place & place = placeMapping == NULL ? arc.getPlace() :
	  *const_cast<Place *>(placeMapping->find(&arc.getPlace())->second);
	new Arc(*this, observer_, arc, trans, place);
      }
  }


  /*!
   * \brief   checks whether the Petri net is a workflow net or not
   *
   *          A Petri net N is a workflow net iff
   *            (1) there is exactly one place with empty preset
   *            (2) there is exactly one place with empty postset
   *            (3) all other nodes are situated on a path between the places described in (1) and (2)
   *
   * \return  true iff (1), (2) and (3) are fulfilled
   * \return  false in any other case
   */
  bool PetriNet::isWorkflow()
  {
    Place *first = NULL;
    Place *last  = NULL;

    // finding places described in condition (1) & (2)
    for (set<Place *>::const_iterator p = places_.begin(); p != places_.end(); p++)
    {
      if ((*p)->getPreset().empty())
      {
        if (first == NULL)
          first = *p;
        else
          return false;
      }
      if ((*p)->getPostset().empty())
      {
        if (last == NULL)
          last = *p;
        else
          return false;
      }
    }
    if (first == NULL || last == NULL)
      return false;

    // insert new transition which consumes from last and produces on first to form a cycle
    Transition &tarjan = createTransition("tarjan");
    createArc(*last, tarjan);
    createArc(tarjan, *first);

    // each 2 nodes \f$x,y \in P \cup T\f$ are in a directed cycle
    // (strongly connected net using tarjan's algorithm)
    unsigned int i = 0; ///< count index
    map<Node *, int> index; ///< index property for nodes
    map<Node *, unsigned int> lowlink; ///< lowlink property for nodes
    set<Node *> stacked; ///< the stack indication for nodes
    stack<Node *> S; ///< stack used by Tarjan's algorithm

    // set all nodes' index values to ``undefined''
    for (set<Place *>::const_iterator p = places_.begin();
        p != places_.end(); p++)
      index[*p] = (-1);
    for (set<Transition *>::const_iterator t = transitions_.begin();
        t != transitions_.end(); t++)
      index[*t] = (-1);

    // getting the number of strongly connected components reachable from first
    unsigned int sscCount = util::dfsTarjan<Node *>(first, S,
        stacked, i, index, lowlink);

    deleteTransition(tarjan);

    // check set $P \cup T$
    set<Node *> nodeSet;
    for (set<Place *>::const_iterator p = places_.begin(); p != places_.end(); p++)
      nodeSet.insert(*p);
    for (set<Transition *>::const_iterator t = transitions_.begin(); t != transitions_.end(); t++)
      nodeSet.insert(*t);

    /*
     * true iff only one strongly connected component found from first and all nodes
     * of $\mathcal{N}$ are members of this component
     */
    if (sscCount == 1 && util::setDifference(nodeSet, stacked).empty())
      return true;
    else
      return false;
  }


  /*!
   */
  void PetriNet::mirror()
  {
    typedef set<Place *> Places;
    Places interface = getInterfacePlaces();

    for (Places::iterator it = interface.begin(); it != interface.end(); ++it)
      (*it)->mirror();
  }



  /****************************************************************************
   *** Private PetriNet Function Definitions
   ***************************************************************************/

  string PetriNet::getMetaInformation(std::ios_base & ios,
				      io::MetaInformation i,
				      const string & def) const
  {
    map<io::MetaInformation, string> & streamMeta =
      io::util::MetaData::data(ios);
    if (streamMeta.find(i) != streamMeta.end())
      return streamMeta.find(i)->second;
    if (meta_.find(i) != meta_.end())
      return meta_.find(i)->second;
    return def;
  }


  /*!
   * All Places and Transitions of the net are prefixed.
   *
   * \param   prefix  the prefix to be added
   */
  PetriNet & PetriNet::prefixNodeNames(const string & prefix)
  {
    for (set<Node *>::iterator it = nodes_.begin(); it != nodes_.end(); ++it)
      (*it)->prefixNameHistory(prefix);
    return *this;
  }


  void PetriNet::deletePlace(Place & place)
  {
    observer_.finalizePlaceType(place, place.getType());
    places_.erase(&place);
    deleteNode(place);
  }


  void PetriNet::deleteTransition(Transition & trans)
  {
    if (trans.isSynchronized())
      synchronizedTransitions_.erase(&trans);
    transitions_.erase(&trans);
    deleteNode(trans);
  }


  /*!
   * \note  Never call this method directly! Use deletePlace() or
   *        deleteTransition() instead!
   */
  void PetriNet::deleteNode(Node & node)
  {
    assert(containsNode(node));
    assert(dynamic_cast<Place *>(&node) == NULL ? true :
	   places_.find(dynamic_cast<Place *>(&node)) == places_.end());
    assert(dynamic_cast<Transition *>(&node) == NULL ? true :
	   transitions_.find(dynamic_cast<Transition *>(&node)) ==
	   transitions_.end());

    while (!node.getPreset().empty())
      deleteArc(*findArc(**node.getPreset().begin(), node));
    while (!node.getPostset().empty())
      deleteArc(*findArc(node, **node.getPostset().begin()));

    observer_.finalizeNodeNameHistory(node, node.getNameHistory());
    nodes_.erase(&node);

    delete &node;
  }


  /*!
   * \pre Net and Arc have the same observer
   */
  void PetriNet::deleteArc(Arc & arc)
  {
    assert(arcs_.find(&arc) != arcs_.end());

    arcs_.erase(&arc);

    observer_.updateArcRemoved(arc);
    
    delete &arc;
  }


  /*!
   */
  void PetriNet::normalize_classical()
  {
    if (isNormal())
      return;

    std::set<Place *> input = getInputPlaces();
    std::set<Place *> output = getOutputPlaces();

    for (std::set<Place *>::const_iterator p = input.begin();
        p != input.end(); p++)
    {
      std::string name = (*p)->getName();

      Place &intp = createPlace("normal_"+name);
      Transition &intt = createTransition("t_"+name);
      Place &comp = createPlace("comp_"+name);
      comp.mark();

      std::set<Arc *> postset = (*p)->getPostsetArcs();
      for (std::set<Arc *>::const_iterator f = postset.begin();
          f != postset.end(); f++)
      {
        createArc(intp, (*f)->getTransition());
        createArc((*f)->getTransition(), comp);
        deleteArc(**f);
      }
      createArc(**p, intt);
      createArc(intt, intp);
      createArc(comp, intt);

      condition_ = condition_.formula() && intp == 0 && comp == 1;
    }
    for (std::set<Place *>::const_iterator p = output.begin();
        p != output.end(); p++)
    {
      std::string name = (*p)->getName();

      Place &intp = createPlace("normal_"+name);
      Transition &intt = createTransition("t_"+name);

      std::set<Arc *> preset = (*p)->getPresetArcs();
      for (std::set<Arc *>::const_iterator f = preset.begin();
          f != preset.end(); f++)
      {
        createArc((*f)->getTransition(), intp);
        deleteArc(**f);
      }
      createArc(intt, **p);
      createArc(intp, intt);

      condition_ = condition_.formula() && intp == 0;
    }
  }


  /*!
   * This method detects sequences of transitions with interface
   * communication. Then, it tries to normalize the net through
   * the rules from [Aalst07].
   *
   * \todo     The rules from [Aalst07] will follow soon!
   */
  void PetriNet::normalize_rules()
  {
    normalize_classical();
  }

}
