#ifndef QUEUE_H
#define QUEUE_H

/*
 * A simple FIFO queue
 */

#include <list>
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"
#include <vector>
#include "pipe.h"
#include "../main.h"

//#inlcude "../main.h"
// #include "../datacentre/fattree/fat_tree_topology.h"

//extern std::vector<std::vector<int>> vec_sw;
//extern std::vector<std::vector<int>> vec_pipe;

class Queue : public EventSource, public PacketSink {
public:
    Queue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist, QueueLogger *logger);
    Queue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist,  QueueLogger *logger, string gid, int sid);

    virtual void receivePacket(Packet &pkt);

    void doNextEvent();
    bool _isHostQueue = false;
    // should really be private, but loggers want to see
    mem_b _maxsize;
    mem_b _queuesize;
    Queue* _dualQueue;
    inline simtime_picosec drainTime(Packet *pkt) {
        simtime_picosec temp =(simtime_picosec) (pkt->size()) * _ps_per_byte;
        return temp;
    }

    inline mem_b serviceCapacity(simtime_picosec t) { return (mem_b) (timeAsSec(t) * (double) _bitrate); }
    inline void setDualQueue(Queue* dual){_dualQueue = dual; dual->_dualQueue=this;}
    virtual PacketSink* getDual(){return _dualQueue;}
protected:
    // Housekeeping
    QueueLogger *_logger;

    // Mechanism
    void beginService(); // start serving the item at the head of the queue
    void completeService(); // wrap up serving the item at the head of the queue
    linkspeed_bps _bitrate;
    simtime_picosec _ps_per_byte;  // service time, in picoseconds per byte
    list<Packet *> _enqueued;
};

#endif
