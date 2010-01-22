// -*- C++ -*-

/*!
 * \file    pnapi.h
 *
 * \brief   Petri Net API main header
 *
 * \since   2008/12/09
 *
 * \date    $Date: 2010-01-22 00:23:39 +0100 (Fr, 22. Jan 2010) $
 *
 * \version $Revision: 5258 $
 */

#ifndef PNAPI_PNAPI_H
#define PNAPI_PNAPI_H

#include "myio.h"
#include "condition.h"
#include "petrinet.h"
#include "automaton.h"
#include "state.h"

// overloaded operators should be available globally
using pnapi::io::operator<<;
using pnapi::io::operator>>;
using pnapi::operator==;
using pnapi::operator>;
using pnapi::operator>=;
using pnapi::operator<;
using pnapi::operator<=;
using pnapi::operator!=;
using pnapi::operator!;
using pnapi::operator||;
using pnapi::operator&&;


/*!
 * \mainpage
 * 
 * The Petri Net API provides structures for working with Petri nets.
 *
 * For using it, simply include the header file pnapi.h in your code:
 * \code
 * #include "pnapi.h"
 * \endcode
 *
 * Everything you will use can be found in the following namespaces:
 * - #pnapi
 * - pnapi::formula
 * - pnapi::io
 *
 * The classes pnapi::PetriNet and pnapi::Automaton provide the main 
 * functionality and can serve as
 * an entry point for getting an overview of the API's functionality.
 * 
 * \section sec Some Examples
 * 
 * Creating a small Petrinet
 * \code
 * PetriNet net;
 * Place & p1 = net.createPlace();
 * p1.mark();
 * Place & p2 = net.createPlace();
 * Transition & t = net.createTransition();
 * net.createArc(p1,t);
 * net.createArc(t,p2);
 * \endcode
 * 
 * Assigning a final condition
 * \code
 * // net from the previous example is recycled here
 * net.finalCondition() = ((p1 == 0) && (p2 == 1));
 * Place & p3 = net.createPlace();
 * net.finalCondition() = (net.finalCondition().formula() && (p3 == 0));
 * \endcode
 * 
 * Reading from stream
 * \code
 * istream is;
 * is >> io::owfn >> net;
 * \endcode
 * 
 * Writing to a stream
 * \code
 * ostream os;
 * // LoLA without formulae
 * os << io::lola << net;
 * // LoLA with formulae
 * os << io::lola << io::formula << net;
 * \endcode
 * 
 * Reducing by applying some rules
 * \code
 * net.reduce(PetriNet::SET_STARKE | PetriNet::KEEP_NORMAL);
 * \endcode
 * 
 * Creating a service automaton
 * \code
 * Automaton sa(net);
 * \endcode
 * 
 * Query structural information
 * \code
 * net.isNormal();
 * net.isWorkflow();
 * \endcode
 */


/*!
 * \brief   Petri Net API
 *
 * Structures for working with Petri nets (PetriNet) and service automata (Automaton).
 */
namespace pnapi
{
}

#endif

