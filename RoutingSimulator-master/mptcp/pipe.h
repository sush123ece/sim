#ifndef PIPE_H
#define PIPE_H

/*
 * A pipe is a dumb device which simply delays all incoming packets
 */

#include <list>
#include <set>
#include <utility>
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"



class Pipe : public EventSource, public PacketSink {
public:
    Pipe(simtime_picosec delay, EventList &eventlist);

    Pipe(simtime_picosec delay, EventList &eventlist, string gid);

    void receivePacket(Packet &pkt); // inherited from PacketSink
    void doNextEvent(); // inherited from EventSource
    simtime_picosec delay() { return _delay; }
    Pipe* _dualPipe;
    inline void setDualPipe(Pipe* dual){
        _dualPipe = dual;
        dual->_dualPipe = this;
    }
    virtual PacketSink* getDual(){return _dualPipe;}
    set<int>* _flowTracker;
    multiset<int>* _coflowTracker;
private:
    simtime_picosec _delay;
    typedef pair<simtime_picosec, Packet *> pktrecord_t;
    list<pktrecord_t> _inflight; // the packets in flight (or being serialized)
};


#endif
