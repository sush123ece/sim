//
// Created by Dingming Wu on 10/31/17.
//

#ifndef SIM_RUNTIMELINKFAILURE_H
#define SIM_RUNTIMELINKFAILURE_H


#include "eventlist.h"
#include "topology.h"
#include "queue.h"
#include "../main.h"
#include "FlowConnection.h"

typedef vector<PacketSink*> route_t;

enum FailureState {GOOD, WAITING_REROUTING, BAD, WAITING_RECOVER};

class SingleDynamicFailureEvent: public EventSource {
public:
    SingleDynamicFailureEvent(EventList& eventlist, Topology* topo,
                              simtime_picosec startFrom, simtime_picosec failureTime,
                              int linkid, int nodeid);
    SingleDynamicFailureEvent(EventList& eventlist, Topology* topo);
    SingleDynamicFailureEvent(EventList& eventList, simtime_picosec startFrom, simtime_picosec failureTime, int linkid);
    void setStartEndTime(simtime_picosec startFrom, simtime_picosec endTime);
    void setFailedLinkid(int linkid);
    void setFailedNodeid(int nodeid);
    void installEvent();
    void setFailureRecoveryDelay(simtime_picosec setupReroutingDelay, simtime_picosec pathRestoreDelay);
    void setTopology(Topology*);
    void doNextEvent();
    set<Queue*>* _relevantQueues;
    simtime_picosec _startFrom;
    simtime_picosec _failureTime;
    Topology* _topo;
    FailureState _failureStatus = GOOD;
    bool UsingShareBackup = false;
    int _linkid, _dualLink, _nodeid, _dualNode;
    simtime_picosec _setupReroutingDelay, _pathRestoreDelay;
    set<TcpSrc*>* _activeConnections;
    set<TcpSrc*>* _sleepingConnections;
    void addActiveConnection(TcpSrc *fc);
    void removeActiveConnection(TcpSrc *fc);
    void addSleepingConnection(TcpSrc*fc);
    void removeSleepingConnection(TcpSrc*fc);
    bool isPathOverlapping(route_t*);
    vector<int>* lpBackupUsageTracker;
    vector<int>* upBackupUsageTracker;
    vector<int>* coreBackupUsageTracker;
    int* _group1;
    int* _group2;
    void setBackupUsageTracker(vector<int>* lp, vector<int>*up, vector<int>* core);
    int getNumImpactedCoflow(map<int,double>*coflowStats);


private:
    void rerouting();
    bool hasEnoughBackup();
    void initTracker();
    void circuitReconfig();
};


#endif //SIM_RUNTIMELINKFAILURE_H
