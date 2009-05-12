#ifndef NDEBUG
#include <iostream>
using std::cout;
using std::endl;
#endif

#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>

#include "parser.h"
#include "io.h"
#include "formula.h"

using std::ifstream;
using std::stringstream;
using std::string;
using std::map;
using std::set;
using std::pair;
using std::vector;

using namespace pnapi::formula;

using pnapi::io::operator>>;

namespace pnapi
{

  namespace parser
  {

    std::istream * stream;

    char * token;

    int line;

    void error(const string & msg)
    {
      throw io::InputError(io::InputError::SYNTAX_ERROR,
		       io::util::MetaData::data(*parser::stream)[io::INPUTFILE],
			   parser::line, parser::token, msg);
    }


    namespace owfn
    {

      typedef const set<const Place *> * Places;
      typedef pair<Formula *, const set<const Place *> *> WildcardFormula;

      Node * node;

      Node::Node() :
	BaseNode(), type(NO_DATA)
      {
      }

      Node::Node(Node * node) :
	BaseNode(node), type(NO_DATA)
      {
      }

      Node::Node(Node * node1, Node * node2) :
	BaseNode(node1, node2), type(NO_DATA)
      {
      }

      Node::Node(Node * node1, Node * node2, Node * node3) :
	BaseNode(node1, node2, node3), type(NO_DATA)
      {
      }

      Node::Node(Node * node1, Node * node2, Node * node3, Node * node4) :
	BaseNode(node1, node2, node3, node4), type(NO_DATA)
      {
      }

      Node::Node(int val) :
	type(DATA_NUMBER), number(val)
      {
      }

      Node::Node(string * str) :
	type(DATA_IDENTIFIER), identifier(*str)
      {
	delete str;
      }

      Node::Node(Type type) :
	type(type)
      {
      }

      Node::Node(Type type, Node * node) :
	type(type)
      {
	assert(node != NULL);

	if (node->type == DATA_NUMBER || node->type == DATA_IDENTIFIER)
	  mergeData(node);
	else if (node->type == NO_DATA)
	  mergeChildren(node);
	else
	  addChild(node);
      }

      Node::Node(Type type, Node * node, int number) :
	type(type), number(number)
      {
	mergeData(node);
      }

      Node::Node(Type type, Node * node1, Node * node2) :
	type(type)
      {
	if (type == FORMULA_OR || type == FORMULA_AND)
	  {
	    addChild(node1);
	    addChild(node2);
	  }
	else
	  {
	    mergeData(node1);
	    mergeChildren(node2);
	  }
      }

      Node::Node(Type type, Node * data, Node * node1, Node * node2) :
	BaseNode(node1, node2), type(type)
      {
	mergeData(data);
      }

      Node::Node(Type type, Node * data, Node * node1, Node * node2, 
		 Node * node3) :
	BaseNode(node1, node2, node3), type(type)
      {
	mergeData(data);
      }

      Node::Node(Type type, Node * data, Node * node1, Node * node2, 
		 Node * node3, Node * node4) :
	BaseNode(node1, node2, node3, node4), type(type)
      {
	mergeData(data);
      }

      Node & Node::operator=(const Node & node)
      {
	number = node.number;
	identifier = node.identifier;
	return *this;
      }

      void Node::mergeData(Node * node)
      {
	assert(node != NULL);
	assert(node->type == DATA_NUMBER || node->type == DATA_IDENTIFIER);

	if (type == CAPACITY)
	  *this = *node;
	else
	  if (node->type != DATA_NUMBER)
	    identifier = node->identifier;
	  else
	    { stringstream ss; ss << node->number; identifier = ss.str(); }

	delete node;
      }

      void Node::mergeChildren(Node * node)
      {
	assert(node != NULL);
	assert(node->type == NO_DATA);

	children_ = node->children_;
	node->children_.clear();
	delete node;
      }

      Parser::Parser() :
	parser::Parser<Node>(node, owfn::parse)
      {
      }

      Visitor::Visitor() :
	isSynchronize_(true), finalMarking_(net_)
      {
      }

      const PetriNet & Visitor::getPetriNet() const
      {
	return net_;
      }

      const std::map<Transition *, std::set<std::string> > & 
      Visitor::getConstraintLabels() const
      {
	return constraintMap_;
      }

      const std::set<std::string> & Visitor::getSynchronousLabels() const
      {
	return synchronousLabels_;
      }

      void Visitor::beforeChildren(const Node & node)
      {
	switch (node.type)
	  {
	  case INPUT:    placeType_ = Place::INPUT;    break;
	  case OUTPUT:   placeType_ = Place::OUTPUT;   break;
	  case INTERNAL: placeType_ = Place::INTERNAL; break;

	  case CAPACITY: capacity_  = node.number;     break;
	  case PORT:     port_      = node.identifier; break;

	  case PLACE:
	    node.check(places_.find(node.identifier) == places_.end(),
		       node.identifier, "node name already used");
	    places_[node.identifier].type = placeType_;
	    places_[node.identifier].capacity = capacity_;
	    if (!port_.empty())
	      {
		node.check(placeType_ != Place::INTERNAL,
			   node.identifier, "interface place expected");
		node.check(places_[node.identifier].port.empty(),
			   node.identifier, "place already assigned to port '" +
			   places_[node.identifier].port + "'");
		places_[node.identifier].port = port_;
	      }
	    break;
	  case PORT_PLACE:
	    {
	      map<string, PlaceAttributes>::iterator p =
		places_.find(node.identifier);
	      node.check(p != places_.end(),
			 node.identifier, "unknown place");
	      node.check(p->second.type != Place::INTERNAL,
			 node.identifier, "interface place expected");
	      node.check(p->second.port.empty(),
			 node.identifier, "place already assigned to port '" +
			 p->second.port + "'");
	      p->second.port = port_;
	      break;
	    }

	  case LABEL:
	    if (isSynchronize_)
	      synchronizeLabels_.insert(node.identifier);
	    else
	      constrainLabels_.insert(node.identifier);
	    break;

	  case INITIALMARKING:
	    isInitial_ = true;
	    break;
	  case FINALMARKING:
	    isInitial_ = false;
	    finalMarking_ = Marking(net_, true);
	    break;
	  case MARK:
	    node.check(places_.find(node.identifier) != places_.end(),
		       node.identifier, "unknown place");
	    if (isInitial_)
	      places_[node.identifier].marking = node.number;
	    else
	      finalMarking_[*net_.findPlace(node.identifier)] = node.number;
	    break;

	  case PRESET:    isPreset_ = true;       break;
	  case POSTSET:   isPreset_ = false;      break;
	  case CONSTRAIN: isSynchronize_ = false; break;

	  case ARC:
	    {
	      Place * place = net_.findPlace(node.identifier);
	      node.check(place != NULL, node.identifier, "unknown place");
	      if (isPreset_)
		{
		  node.check(preset_.find(node.identifier) == preset_.end(),
			     node.identifier, "place already used in preset");
		  node.check(place->getType() != Place::OUTPUT, node.identifier,
			     "output place not allowed in preset");
		  preset_[node.identifier] = node.number;
		}
	      else
		{
		  node.check(postset_.find(node.identifier) == postset_.end(),
			     node.identifier, "place already used in postset");
		  node.check(place->getType() != Place::INPUT, node.identifier,
			     "input place not allowed in postset");
		  postset_[node.identifier] = node.number;
		}
	    }
	    break;

	  case FORMULA_TRUE:
	    formulas_.push(WildcardFormula(new FormulaTrue, NULL));
	    break;
	  case FORMULA_FALSE:
	    formulas_.push(WildcardFormula(new FormulaFalse, NULL));
	    break;

	  case FORMULA_EQ:
	  case FORMULA_NE:
	  case FORMULA_LT:
	  case FORMULA_GT:
	  case FORMULA_GE:
	  case FORMULA_LE:
	    {
	      Place * place;
	      unsigned int nTokens;
	      Formula * formula;
	      place = net_.findPlace(node.identifier);
	      nTokens = node.number;
	      switch (node.type)
		{
		case FORMULA_EQ:
		  formula = new FormulaEqual(*place, nTokens); break;
		case FORMULA_NE:
		  formula = new Negation(FormulaEqual(*place, nTokens));
		  break;
		case FORMULA_LT:
		  formula = new FormulaLess(*place, nTokens); break;
		case FORMULA_GT:
		  formula = new FormulaGreater(*place, nTokens); break;
		case FORMULA_GE:
		  formula = new FormulaGreaterEqual(*place, nTokens); break;
		case FORMULA_LE:
		  formula = new FormulaLessEqual(*place, nTokens); break;
		default: assert(false);
		}
	      formulas_.push(WildcardFormula(formula, NULL));
	      break;
	    }

	  default: /* empty */ ;
	  }
      }

      void Visitor::afterChildren(const Node & node)
      {
	const set<const Place *> * wcPlaces = NULL;

	switch (node.type)
	  {
	  case SYNCHRONOUS:
	    synchronousLabels_ = synchronizeLabels_;
	    synchronizeLabels_.clear();
	    break;

	  case INITIALMARKING:
	    // create places
	    for (map<string, PlaceAttributes>::iterator it = places_.begin();
		 it != places_.end(); ++it)
	      net_.createPlace(it->first, it->second.type, it->second.marking,
			       it->second.capacity, it->second.port);
	    // add synchronous labels
	    for (set<string>::iterator it = synchronizeLabels_.begin(); 
		 it != synchronizeLabels_.end(); ++it)
	      ; // do we really need the labels globally?
	    synchronizeLabels_.clear();
	    break;
	  case FINALMARKING:
	    net_.finalCondition().addMarking(finalMarking_);
	    break;

	  case TRANSITION:
	    {
	      node.check(!net_.containsNode(node.identifier), node.identifier,
			 "node name already used");
	      for (set<string>::iterator it = synchronizeLabels_.begin();
		   it != synchronizeLabels_.end(); ++it)
		node.check(synchronousLabels_.find(*it) != 
			   synchronousLabels_.end(), *it, "undeclared label");
	      Transition & trans = net_.createTransition(node.identifier, 
							 synchronizeLabels_);
	      for (map<string, unsigned int>::iterator it = preset_.begin();
		   it != preset_.end(); ++ it)
		net_.createArc(*net_.findPlace(it->first), trans, it->second);
	      for (map<string, unsigned int>::iterator it = postset_.begin();
		   it != postset_.end(); ++ it)
		net_.createArc(trans, *net_.findPlace(it->first), it->second);
	      constraintMap_[&trans] = constrainLabels_;
	      preset_.clear();
	      postset_.clear();
	      synchronizeLabels_.clear();
	      constrainLabels_.clear();
	      isSynchronize_ = true;
	      break;
	    }

	  case FORMULA_NOT:
	    {
	      assert(formulas_.size() > 0);
	      WildcardFormula wcf = formulas_.top(); formulas_.pop();
	      Formula * iwcf = integrateWildcard(wcf);
	      Formula * f = new Negation(*iwcf); delete iwcf;
	      formulas_.push(WildcardFormula(f, NULL));
	      delete wcf.first;
	      break;
	    }
	  case FORMULA_AND:
	  case FORMULA_OR:
	    {
	      assert(formulas_.size() > 1);
	      WildcardFormula op2 = formulas_.top(); formulas_.pop();
	      WildcardFormula op1 = formulas_.top(); formulas_.pop();
	      Formula * f;
	      if (node.type == FORMULA_AND)
		{
		  node.check(op1.second == NULL, "",
			     "wildcard must be last AND operand");
		  f = new Conjunction(*op1.first, *op2.first);
		  formulas_.push(WildcardFormula(f, op2.second));
		}
	      else
		{
		  Formula * f1 = integrateWildcard(op1);
		  Formula * f2 = integrateWildcard(op2);
		  f = new Disjunction(*f1, *f2); delete f1; delete f2;
		  formulas_.push(WildcardFormula(f, NULL));
		}
	      delete op1.first;
	      delete op2.first;
	      break;
	    }

	  case FORMULA_APE:
	    formulas_.push(WildcardFormula(new Conjunction(), 
					   (Places)&net_.getPlaces()));
	    break;

	  case FORMULA_AAOPE:
	    wcPlaces = wcPlaces ? wcPlaces : (Places)&net_.getPlaces();
	    // fallthrough intended
	  case FORMULA_AAOIPE:
	    wcPlaces = wcPlaces ? wcPlaces : (Places)&net_.getInternalPlaces();
	    // fallthrough intended
	  case FORMULA_AAOEPE:
	    wcPlaces = wcPlaces ? wcPlaces : (Places)&net_.getInterfacePlaces();
	    // fallthrough intended
	    {
	      assert(formulas_.size() > 0);
	      node.check(formulas_.top().second == NULL, "",
			 "wildcard must be last AND operand");
	      Formula * f = new Conjunction(*formulas_.top().first);
	      delete formulas_.top().first; formulas_.pop();
	      formulas_.push(WildcardFormula(f, wcPlaces));
	    }
	    break;

	  case CONDITION:
	    {
	      assert(formulas_.size() == 1);
	      Formula * f = integrateWildcard(formulas_.top());
	      net_.finalCondition() = *f; delete f;
	      delete formulas_.top().first;
	      break;
	    }

	  default: /* empty */ ;
	  }
      }

      Formula * Visitor::integrateWildcard(WildcardFormula wcf)
      {
	Conjunction * c = dynamic_cast<Conjunction *>(wcf.first);
	if (c == NULL || wcf.second == NULL)
	  return wcf.first->clone();
	else
	  return new Conjunction(*c, *wcf.second);
      }

    } /* namespace owfn */



    namespace lola
    {
      
      Node * node;

      Parser::Parser() :
	parser::Parser<Node>(node, lola::parse)
      {
      }

    } /* namespace lola */



    namespace petrify
    {

      Node * node;

      Node::Node(Type type) : type(type) {}

      Node::Node(Type type, Node *child1, Node *child2, Node *child3, Node *child4) :
        type(type)
      {
        if(child1!=NULL)
        {
          addChild(child1);
        }

        Node *n1 = new Node(CTRL1);
        addChild(n1);

        if(child2!=NULL)
        {
          addChild(child2);
        }
        if(child3!=NULL)
        {
          addChild(child3);
        }

        Node *n2 = new Node(CTRL2);
        addChild(n2);

        if(child4!=NULL)
        {
          addChild(child4);
        }
      }

      Node::Node(Type type, string data, Node *child) :
        type(type), data(data)
      {
        if(child != NULL)
        {
          addChild(child);
        }
      }

      Node::Node(Type type, string data, Node *child1, Node *child2) :
        type(type), data(data)
      {
        if(child1 != NULL)
        {
          addChild(child1);
        }
        if(child2 != NULL)
        {
          addChild(child2);
        }
      }

      Visitor::Visitor()
      {
        in_marking_list=false;
        in_arc_list=false;
      }

      Parser::Parser() :
        parser::Parser<Node>(node, petrify::parse)
      {
      }

      void Visitor::beforeChildren(const Node & node)
      {
        // erzeuge result_ aus Knoten hier ...
        switch(node.type)
        {
        case Node::TLIST:
          {
            if(in_arc_list)
            {
              tempNodeSet.insert(node.data);
              result_->transitions.insert(node.data);
            } else
            {
              result_->interface.insert(node.data);
            }
            break;
          }
        case Node::PLIST:
          {
            result_->places.insert(node.data);
            if (in_marking_list)
              result_->initialMarked.insert(node.data);
            else
              tempNodeSet.insert(node.data);
            break;
          }
        case Node::PT:
        case Node::TP:
          {
            tempNodeStack.push(tempNodeSet);
            tempNodeSet.clear();
            break;
          }
        case Node::CTRL1:
          {
            in_arc_list = true;
            break;
          }
        case Node::CTRL2:
          {
            in_marking_list = true;
            break;
          }
        default: break;

        }
      }

      void Visitor::afterChildren(const Node & node)
      {
        // ... und hier
        switch(node.type)
        {
        case Node::PT:
        case Node::TP:
        {
          result_->arcs[node.data] = tempNodeSet;
          tempNodeSet.clear();
          if(tempNodeStack.size() > 0)
          {
            tempNodeSet = tempNodeStack.top();
            tempNodeStack.pop();
          }
        }
        default: break;
        }
      }

    }


    namespace onwd
    {

      Node * node;


      Parser::Parser() :
	parser::Parser<Node>(node, onwd::parse)
      {
      }


      /* simple child adding */

      Node::Node() :
	BaseNode(), type(STRUCT), number(0)
      {
      }

      Node::Node(Node * node) :
	BaseNode(node), type(STRUCT), number(0)
      {
      }

      Node::Node(Node * node1, Node * node2) :
	BaseNode(node1, node2), type(STRUCT), number(0)
      {
      }

      Node::Node(Node * node1, Node * node2, Node * node3) :
	BaseNode(node1, node2, node3), type(STRUCT), number(0)
      {
      }


      /* data node construction */

      Node::Node(Type type, string * str1, int number) :
	type(type), string1(*str1), number(number)
      {
	delete str1;
      }

      Node::Node(Type type, string * str1, string * str2) :
	type(type), string1(*str1), string2(*str2), number(0)
      {
	delete str1;
	delete str2;
      }

      Node::Node(Type type, string * str1, string * str2, int number) :
	type(type), string1(*str1), string2(*str2), number(number)
      {
	delete str1;
	delete str2;
      }


      /* typed node construction */

      Node::Node(Type type) :
	BaseNode(), type(type), number(0)
      {
      }

      Node::Node(Type type, Node * node) :
	BaseNode(node), type(type), number(0)
      {
      }

      Node::Node(Type type, Node * node1, Node * node2) :
	BaseNode(node1, node2), type(type), number(0)
      {
      }


      void Node::mergeData(Node * node)
      {
	assert(node != NULL);
	assert(node->type == DATA);

	// FIXME
	assert(false);

	delete node;
      }

      void Node::mergeChildren(Node * node)
      {
	assert(node != NULL);
	assert(node->type == STRUCT);

	children_ = node->children_;
	node->children_.clear();
	delete node;
      }


      /* visiting nodes */

      Visitor::Visitor(map<string, PetriNet *> & nets) :
	nets_(nets)
      {
      }

      void Visitor::beforeChildren(const Node & node)
      {
	switch (node.type)
	  {
	  case INSTANCE:
	    {
	      string name = node.string1;
	      PetriNet net;
	      if (nets_.find(name) != nets_.end())
		net = *nets_[name];
	      for (int i = 0; i < node.number; i++)
		instances_[name].push_back(net);
	      break;
	    }

	  case PLACE:
	    {
	      string netName = node.string1;
	      string placeName = node.string2;
	      vector<PlaceDescription> places;
	      for (vector<PetriNet>::iterator it = instances_[netName].begin();
		   it != instances_[netName].end(); ++it)
		places.push_back(PlaceDescription(netName, *it, placeName));
	      places_.push_back(places);
	      break;
	    }

	  default: /* empty */ ;
	  }
      }

      void Visitor::afterChildren(const Node & node)
      {
	switch (node.type)
	  {
	  case INSTANCES:
	    for (map<string, PetriNet *>::iterator it = nets_.begin(); 
		 it != nets_.end(); ++it)
	      node.check(instances_.find(it->first) != instances_.end(), 
			 it->first, "Petri net not declared as instance");
	    break;

	  case ANY_WIRING:
	  case ALL_WIRING:
	    {
	      assert(places_.size() == 2);
	      vector<PlaceDescription> p1s = 
		places_.front(); places_.pop_front();
	      vector<PlaceDescription> p2s = 
		places_.front(); places_.pop_front();
	      for (vector<PlaceDescription>::iterator p1i = 
		     p1s.begin(); p1i != p1s.end(); ++p1i)
		for (vector<PlaceDescription>::iterator p2i = 
		       p2s.begin(); p2i != p2s.end(); ++p2i)
		  {
		    pair<Place *, bool> p1 = 
		      getPlace(node, *p1i, Place::OUTPUT);
		    pair<Place *, bool> p2 = 
		      getPlace(node, *p2i, Place::INPUT);
		    LinkNode * n1 = 
		      getLinkNode(*p1.first, getLinkNodeMode(node.type), 
				  !p1.second);
		    LinkNode * n2 = 
		      getLinkNode(*p2.first, getLinkNodeMode(node.type),
				  !p2.second);
		    n1->addLink(*n2);
		    wiring_[p1.first] = n1;
		    wiring_[p2.first] = n2;
		  }
	      break;
	    }

	  default: /* empty */ ;
	  }
      }

      pair<Place *, bool> Visitor::getPlace(const Node & node, 
					    PlaceDescription & pd, 
					    Place::Type type)
      {
	bool isNetGiven = nets_.find(pd.netName) != nets_.end();
	Place * p = pd.instance->findPlace(pd.placeName);
	bool createPlace = !isNetGiven && p == NULL;
	if (createPlace)
	  {
	    p = &pd.instance->createPlace(pd.placeName, type);
	    pd.instance->finalCondition().addProposition(*p == 0);
	  }
	node.check(p != NULL && p->getType() == type, 
		   pd.netName + "." + pd.placeName, 
		   (string)(type == Place::INPUT ? "input" : "output") + 
		   (string)" place expected");
	return pair<Place *, bool>(p, createPlace);
      }

      LinkNode * Visitor::getLinkNode(Place & p, LinkNode::Mode mode, 
				      bool internalize)
      {
	map<Place *, LinkNode *>::iterator it = wiring_.find(&p);
	if (it != wiring_.end())
	  return (*it).second;
	else
	  return new LinkNode(p, mode, internalize);
      }

      LinkNode::Mode Visitor::getLinkNodeMode(Type type)
      {
	assert(type == ANY_WIRING || type == ALL_WIRING);

	if (type == ANY_WIRING)
	  return LinkNode::ANY;
	else
	  return LinkNode::ALL;
      }

      map<string, vector<PetriNet> > & Visitor::instances()
      {
	return instances_;
      }

      const map<Place *, LinkNode *> & Visitor::wiring()
      {
	return wiring_;
      }


    } /* namespace onwd */

  } /* namespace parser */

} /* namespace pnapi */
