/*!
 * \file  myio.cc
 */

#include "config.h"
#include <cassert>

#include "parser.h"
#include "automaton.h"
#include "myio.h"

using std::map;
using std::set;
using std::string;
using std::pair;

using pnapi::formula::Formula;
using pnapi::formula::Proposition;

namespace pnapi
{

namespace io
{

/*************************************************************************
 ***** Basic I/O Implementation
 *************************************************************************/

/* FORMAT IMPLEMENTATION: add case label below */


/*!
 * Stream the PetriNet object to a given output stream, using the
 * stream format (see pnapi::io).
 */
std::ostream & operator<<(std::ostream & os, const PetriNet & net)
{
  switch (util::FormatData::data(os))
  {
  case util::DOT:  return __dot::output(os, net);
  case util::LOLA: return __lola::output(os, net);
  case util::OWFN: return __owfn::output(os, net);
  case util::STAT: return __stat::output(os, net);
  case util::PNML: return __pnml::output(os, net);

  default: assert(false);
  }
}


/*!
 * Reads a Petri net from a stream (in most cases backed by a file). The
 * format
 * of the stream data is not determined automatically. You have to set it
 * explicitly using a stream manipulator from pnapi::io.
 */
std::istream & operator>>(std::istream & is, PetriNet & net)
throw (InputError)
{
  switch (util::FormatData::data(is))
  {
  case util::OWFN:
  {
    parser::owfn::Parser parser;
    net = parser.parse(is);

    net.meta_ = util::MetaData::data(is);
    break;
  }

  case util::PNML:
  {
    parser::pnml::Parser parser;
    net = parser.parse(is);

    net.meta_ = util::MetaData::data(is);
    break;
  }

  case util::LOLA:
  {
    parser::lola::Parser parser;
    net = parser.parse(is);

    net.meta_ = util::MetaData::data(is);
    break;
  }

  case util::ONWD:
  {
    map<string, PetriNet *> nets = util::PetriNetData::data(is);
    parser::onwd::Parser parser;
    parser::onwd::Visitor visitor(nets);
    parser.parse(is).visit(visitor);
    net.createFromWiring(visitor.instances(), visitor.wiring());
    net.meta_ = util::MetaData::data(is);
    break;
  }

  case util::SA2SM:
  {
    parser::sa::Parser parser;
    net = parser.parseSA2SM(is);

    break;
  }
  default:
    assert(false);  // unsupported input format
  }

  return is;
}


/*!
 * Streams the Automaton object to a given output stream using the stream
 * format (see pnapi::io).
 */
std::ostream & operator<<(std::ostream &os, const Automaton &sa)
{
  switch (util::FormatData::data(os))
  {
  case      util::DOT: __dot::output(os, sa); break;
  case      util::SA:  __sa::output(os, sa);   break;
  case      util::STAT: __stat::output(os, sa); break;
  default:  assert(false); // unsupported output format
  }

  return os;
}


/*!
 * Reads an Automaton file and creates an object from it.
 */
std::istream & operator>>(std::istream &is, Automaton &sa)
{
  switch (util::FormatData::data(is))
  {
  case util::SA:
  {
    parser::sa::Parser parser;
    sa = parser.parse(is);
    break;
  }
  default: assert(false); /* unsupported format */
  }

  return is;
}



/*************************************************************************
 ***** Internal I/O Implementation
 *************************************************************************/

/* FORMAT IMPLEMENTATION: add case labels for entities you use */

namespace util
{

std::ostream & operator<<(std::ostream & os, const pnapi::Arc & arc)
{
  switch (FormatData::data(os))
  {
  case DOT:  return __dot::output(os, arc);
  case LOLA: return __lola::output(os, arc);
  case OWFN: return __owfn::output(os, arc);
  case PNML: return __pnml::output(os, arc);

  default: assert(false);
  }
}


std::ostream & operator<<(std::ostream & os, const pnapi::Place & p)
{
  switch (FormatData::data(os))
  {
  case DOT:  return __dot::output(os, p);
  case LOLA: return __lola::output(os, p);
  case OWFN: return __owfn::output(os, p);
  case PNML: return __pnml::output(os, p);

  default: assert(false);
  }
}


std::ostream & operator<<(std::ostream & os, const pnapi::Transition & t)
{
  switch (FormatData::data(os))
  {
  case DOT:  return __dot::output(os, t);
  case LOLA: return __lola::output(os, t);
  case OWFN: return __owfn::output(os, t);
  case PNML: return __pnml::output(os, t);

  default: assert(false);
  }
  return os;
}


std::ostream & operator<<(std::ostream & os, const formula::Negation & f)
{
  switch (FormatData::data(os))
  {
  case LOLA: return __lola::output(os, f);
  case OWFN: return __owfn::output(os, f);
  case PNML: return __pnml::output(os, f);

  default: assert(false);
  }
  return os;
}


std::ostream & operator<<(std::ostream & os, const formula::Conjunction & f)
{
  switch (FormatData::data(os))
  {
  case LOLA: return __lola::output(os, f);
  case OWFN: return __owfn::output(os, f);
  case PNML: return __pnml::output(os, f);

  default: assert(false);
  }
  return os;
}


std::ostream & operator<<(std::ostream & os, const formula::Disjunction & f)
{
  switch (FormatData::data(os))
  {
  case LOLA: return __lola::output(os, f);
  case OWFN: return __owfn::output(os, f);
  case PNML: return __pnml::output(os, f);

  default: assert(false);
  }
  return os;
}


std::ostream & operator<<(std::ostream & os, const formula::FormulaTrue & f)
{
  switch (FormatData::data(os))
  {
  case LOLA: return __lola::output(os, f);
  case OWFN: return __owfn::output(os, f);
  case PNML: return __pnml::output(os, f);

  default: assert(false);
  }
  return os;
}


std::ostream & operator<<(std::ostream & os, const formula::FormulaFalse & f)
{
  switch (FormatData::data(os))
  {
  case LOLA: return __lola::output(os, f);
  case OWFN: return __owfn::output(os, f);
  case PNML: return __pnml::output(os, f);

  default: assert(false);
  }
  return os;
}


std::ostream & operator<<(std::ostream & os, const formula::FormulaEqual & f)
{
  switch (FormatData::data(os))
  {
  case LOLA: return __lola::output(os, f);
  case OWFN: return __owfn::output(os, f);
  case PNML: return __pnml::output(os, f);

  default: assert(false);
  }
  return os;
}


std::ostream & operator<<(std::ostream & os, const formula::FormulaNotEqual & f)
{
  switch (FormatData::data(os))
  {
  case LOLA: return __lola::output(os, f);
  case OWFN: return __owfn::output(os, f);
  case PNML: return __pnml::output(os, f);

  default: assert(false);
  }
  return os;
}


std::ostream & operator<<(std::ostream & os, const formula::FormulaGreater & f)
{
  switch (FormatData::data(os))
  {
  case LOLA: return __lola::output(os, f);
  case OWFN: return __owfn::output(os, f);
  case PNML: return __pnml::output(os, f);

  default: assert(false);
  }
  return os;
}


std::ostream & operator<<(std::ostream & os, const formula::FormulaGreaterEqual & f)
{
  switch (FormatData::data(os))
  {
  case LOLA: return __lola::output(os, f);
  case OWFN: return __owfn::output(os, f);
  case PNML: return __pnml::output(os, f);

  default: assert(false);
  }
  return os;
}


std::ostream & operator<<(std::ostream & os, const formula::FormulaLess & f)
{
  switch (FormatData::data(os))
  {
  case LOLA: return __lola::output(os, f);
  case OWFN: return __owfn::output(os, f);
  case PNML: return __pnml::output(os, f);

  default: assert(false);
  }
  return os;
}


std::ostream & operator<<(std::ostream & os, const formula::FormulaLessEqual & f)
{
  switch (FormatData::data(os))
  {
  case LOLA: return __lola::output(os, f);
  case OWFN: return __owfn::output(os, f);
  case PNML: return __pnml::output(os, f);

  default: assert(false);
  }
  return os;
}


std::ostream & operator<<(std::ostream & os, const std::pair<std::string, std::set<Place *> > & p)
{
  switch (FormatData::data(os))
  {
  case OWFN: return __owfn::output(os, p);
  case DOT:  return __dot::output(os, p);

  default: assert(false);
  }
}


std::ostream & operator<<(std::ostream & os,
    const std::pair<unsigned int, std::set<Place *> > & p)
{
    switch (FormatData::data(os))
    {
        case OWFN: return __owfn::output(os, p);

        default: assert(false);
    }
}


} /* namespace util */




// What follows is generic code, that hopefully rarely needs to be touched.


/*************************************************************************
 ***** Generic I/O Implementation
 *************************************************************************/

 util::Manipulator<std::pair<MetaInformation, std::string> >
 meta(MetaInformation i, const std::string & s)
 {
   return util::MetaManipulator(pair<MetaInformation, string>(i, s));
 }


 InputError::InputError(Type type, const std::string & filename, int line,
     const std::string & token, const std::string & msg) :
       type(type), message(msg), token(token), line(line), filename(filename)
       {
       }


 std::ostream & operator<<(std::ostream & os, const io::InputError & e)
 {
   os << e.filename;
   if (e.line > 0)
     os << ":" << e.line;
   os << ": error";
   if (!e.token.empty())
     switch (e.type)
     {
     case io::InputError::SYNTAX_ERROR:
       os << " near '" << e.token << "'";
       break;
     case io::InputError::SEMANTIC_ERROR:
       os << ": '" << e.token << "'";
       break;
     }
   return os << ": " << e.message;
 }


 std::istream & operator>>(std::istream & is, const util::Manipulator<
     std::pair<MetaInformation, std::string> > & m)
     {
   util::MetaData::data(is)[m.data.first] = m.data.second;
   return is;
     }


 std::ostream & operator<<(std::ostream & os, const util::Manipulator<
     std::pair<MetaInformation, std::string> > & m)
     {
   util::MetaData::data(os)[m.data.first] = m.data.second;
   return os;
     }



 /*************************************************************************
  ***** Internal Generic I/O Implementation
  *************************************************************************/

 namespace util
 {

 Manipulator<Mode> mode(Mode m)
 {
   return Manipulator<Mode>(m);
 }


 Manipulator<Delim> delim(const std::string & s)
 {
   Delim d; d.delim = s;
   return Manipulator<Delim>(d);
 }


 std::ostream & outputContainerElement(std::ostream & os,
     const std::string & s)
     {
   return (os << s);
     }


 bool compareContainerElements(std::string s1, std::string s2)
 {
   return s1 < s2;
 }


 bool compareContainerElements(const Node * n1, const Node * n2)
 {
   return compareContainerElements(n1->getName(), n2->getName());
 }


 bool compareContainerElements(Place * p1, Place * p2)
 {
   return compareContainerElements(static_cast<Node *>(p1), static_cast<Node *>(p2));
 }


 bool compareContainerElements(const Place * p1, const Place * p2)
 {
   return compareContainerElements(static_cast<Node *>(const_cast<Place *>(p1)), 
                                   static_cast<Node *>(const_cast<Place *>(p2)));
 }


 bool compareContainerElements(Transition * t1, Transition * t2)
 {
   return compareContainerElements(static_cast<Node *>(t1), static_cast<Node *>(t2));
 }


 bool compareContainerElements(Arc * f1, Arc * f2)
 {
   return compareContainerElements(&f1->getPlace(), &f2->getPlace());
 }


 bool compareContainerElements(const formula::Formula *,
     const formula::Formula *)
 {
   return false;
 }


 std::set<Place *> filterMarkedPlaces(const std::set<Place *> & places)
 {
   set<Place *> filtered;
   for (set<Place *>::iterator it = places.begin(); it != places.end();
   ++it)
     if ((*it)->getTokenCount() > 0)
       filtered.insert(*it);
   return filtered;
 }


 std::set<Arc *> filterInternalArcs(const std::set<Arc *> & arcs)
 {
   set<Arc *> filtered;
   for (set<Arc *>::iterator it = arcs.begin(); it != arcs.end(); ++it)
     if ((*it)->getPlace().getType() == Place::INTERNAL)
       filtered.insert(*it);
   return filtered;
 }


 std::set<const formula::Formula *>
 filterInterfacePropositions(const std::set<const formula::Formula *> & formulas)
 {
   set<const formula::Formula *> result;
   for (set<const formula::Formula *>::iterator it = formulas.begin();
   it != formulas.end(); ++it)
   {
     const Proposition * p = dynamic_cast<const Proposition *>(*it);
     if (!(p != NULL && p->place().getType() != Place::INTERNAL))
       result.insert(*it);
   }
   return result;
 }


 std::multimap<unsigned int, Place *>
 groupPlacesByCapacity(const std::set<Place *> & places)
 {
   std::multimap<unsigned int, Place *> grouped;
   for (set<Place *>::iterator it = places.begin(); it != places.end();
   ++it)
     grouped.insert(pair<unsigned int, Place *>((*it)->getCapacity(),*it));
   return grouped;
 }


 std::set<std::string> collectSynchronizeLabels(const std::set<Transition *> & ts)
 {
   set<string> labels;
   for (set<Transition *>::iterator it = ts.begin(); it != ts.end(); ++it)
     labels.insert((*it)->getSynchronizeLabels().begin(),
         (*it)->getSynchronizeLabels().end());
   return labels;
 }


 std::ostream & operator<<(std::ostream & os, const pnapi::Node & node)
 {
   const Place * p = dynamic_cast<const Place *>(&node);
   if (p != NULL)
     return os << *p;
   else
     return os << *dynamic_cast<const Transition *>(&node);
 }


 std::ostream & operator<<(std::ostream & os, const pnapi::Condition & c)
 {
   return os << c.formula();
 }


 std::ostream & operator<<(std::ostream & os, const formula::Formula & f)
 {
   return f.output(os);
 }


 } /* namespace util */

} /* namespace io */

} /* namespace pnapi */
