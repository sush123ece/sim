#ifndef EVENTLIST_H
#define EVENTLIST_H

#include <map>
#include "config.h"
#include "loggertypes.h"

class EventList;

class EventSource : public Logged {
public:
    EventSource(EventList &eventlist, const string &name) : Logged(name), _eventlist(eventlist) {};

    virtual ~EventSource() {};

    virtual void doNextEvent() = 0;

    inline EventList &eventlist() const { return _eventlist; }

protected:
    EventList &_eventlist;
};

class EventList {
public:
    EventList();

    void setEndtime(simtime_picosec endtime); // end simulation at endtime (rather than forever)
    bool doNextEvent(); // returns true if it did anything, false if there's nothing to do
    void sourceIsPending(EventSource &src, simtime_picosec when);

    void sourceIsPendingRel(EventSource &src, simtime_picosec timefromnow) {
        sourceIsPending(src, now() + timefromnow);
    }

    inline simtime_picosec now() const { return _lasteventtime; }

    uint64_t bufferOverflowPacketDrops = 0;
    uint64_t linkFailurePacketDrops = 0;
    uint64_t randomPacketLoss = 0;
    uint64_t globalTimeOuts = 0;
    uint64_t ackLinkFailureLoss = 0;
    uint64_t getTotalPacketLoss();
private:
    simtime_picosec _endtime;
    simtime_picosec _lasteventtime;
    typedef multimap<simtime_picosec, EventSource *> pendingsources_t;
    pendingsources_t _pendingsources;
};

#endif
