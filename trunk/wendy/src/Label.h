#ifndef _LABEL_H
#define _LABEL_H

#include <map>
#include <string>
#include "types.h"


/*! \def SILENT(L)
    \brief shorthand notation to find out whether a label is tau */
#define SILENT(L) (L == 0)

/*! \def SENDING(L)
    \brief shorthand notation to find out whether a label belongs to a send action */
#define SENDING(L) (L >= Label::first_send && L <= Label::last_send)

/*! \def RECEIVING(L)
    \brief shorthand notation to find out whether a label belongs to a receive action */
#define RECEIVING(L) (L >= Label::first_receive && L <= Label::last_receive)

/*! \def SYNC(L)
    \brief shorthand notation to find out whether a label belongs to a synchronous action */
#define SYNC(L) (L >= Label::first_sync && L <= Label::last_sync)


/*!
 \brief communication labels
 
 Communication labels are described from the point of view of the environment;
 that is, not from the point of view of the net. If the net sends a message,
 the respective communication label for us would be to receive.
 
 - silent action (tau) has label 0
 - asynchronous receive events have labels first_receive (=1) to last_receive
 - asynchronous send events have labels first_send to last_send
 - synchronous events have labels first_sync to last_sync
 */
class Label {
    public: /* static functions */
    
        /// initialize the interface labels and prepare the necessary mappings
        static void initialize();

    public: /* static attributes */
    
        /// label of first receive (?) event
        static Label_ID first_receive;
        /// label of last receive (?) event
        static Label_ID last_receive;

        /// label of first send (!) event
        static Label_ID first_send;
        /// label of last send (!) event
        static Label_ID last_send;

        /// label of first synchronous (!?) event
        static Label_ID first_sync;
        /// label of last synchronous (!?) event
        static Label_ID last_sync;

        /// the number of asynchronous (? and !) events
        static Label_ID async_events;
        /// the number of synchronous (?!) events
        static Label_ID sync_events;
        /// the number of all events
        static Label_ID events;

        /// given a communication label, returns a string representation
        static std::map<Label_ID, std::string> id2name;
        /// given a transition name, the communication label
        static std::map<std::string, Label_ID> name2id;        
};

#endif
