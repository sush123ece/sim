//
// Created by Dingming Wu on 10/31/17.
//

#include "SingleDynamicFailureEvent.h"
#include <iostream>
#include "tcp.h"
#include <set>

using namespace std;

SingleDynamicFailureEvent::SingleDynamicFailureEvent(EventList &eventlist, Topology *topo,
                                                     simtime_picosec startFrom,
                                                    simtime_picosec failureTime,
                                                     int linkid, int nodeid)
        : EventSource(eventlist, "SingleDynamicFailureEvent"), _startFrom(startFrom),
          _failureTime(failureTime), _topo(topo), _linkid(linkid), _nodeid(nodeid){
    _activeConnections = new set<TcpSrc *>();
    _sleepingConnections = new set<TcpSrc*>();
    _relevantQueues = new set<Queue*>();
}

SingleDynamicFailureEvent::SingleDynamicFailureEvent(EventList &eventlist, Topology *topo)
        : EventSource(eventlist, "SingleDynamicFailureEvent"), _topo(topo) {
    _activeConnections = new set<TcpSrc *>();
    _sleepingConnections = new set<TcpSrc*>();
    _relevantQueues = new set<Queue*>();
}

SingleDynamicFailureEvent::SingleDynamicFailureEvent(EventList &eventList, simtime_picosec startFrom, simtime_picosec failureTime,
                                   int linkid):EventSource(eventList,"SingleDynamicFailureEvent"), _startFrom(startFrom),
                                               _failureTime(failureTime), _linkid(linkid) {
    _activeConnections = new set<TcpSrc*>();
    _relevantQueues = new set<Queue*>();
    _sleepingConnections = new set<TcpSrc*>();
}

void SingleDynamicFailureEvent::setBackupUsageTracker(vector<int> *lp, vector<int> *up, vector<int> *core) {
    lpBackupUsageTracker = lp;
    upBackupUsageTracker = up;
    coreBackupUsageTracker = core;
}

void SingleDynamicFailureEvent::setFailedLinkid(int linkid) {
    _linkid = linkid;
    _dualLink = (linkid+K/2*K/2)%(NHOST/RATIO);
}

void SingleDynamicFailureEvent::setFailedNodeid(int nodeid) {
    _nodeid = nodeid;
    _dualNode = (nodeid+K/2)%(NK);
}

void SingleDynamicFailureEvent::setTopology(Topology *topo) {
    _topo = topo;
}
void SingleDynamicFailureEvent::setStartEndTime(simtime_picosec startFrom, simtime_picosec failureTime) {
    _startFrom = startFrom;
    _failureTime = failureTime;
}

void SingleDynamicFailureEvent::installEvent() {
    if(_linkid>=0) {
        pair<Queue *, Queue *> queues = _topo->linkToQueues(_linkid);
        _relevantQueues->insert(queues.first);
        _relevantQueues->insert(queues.second);
        queues = _topo->linkToQueues(_dualLink);
        _relevantQueues->insert(queues.first);
        _relevantQueues->insert(queues.second);

    }
    if(_nodeid>=0){
        vector<int>* links = _topo->getLinksFromSwitch(_nodeid);
        for(int link:*links){
            pair<Queue *, Queue *> queues = _topo->linkToQueues(link);
            _relevantQueues->insert(queues.first);
            _relevantQueues->insert(queues.second);
        }
        links = _topo->getLinksFromSwitch(_dualNode);
        for(int link:*links){
            pair<Queue *, Queue *> queues = _topo->linkToQueues(link);
            _relevantQueues->insert(queues.first);
            _relevantQueues->insert(queues.second);
        }
    }
    if(_relevantQueues->size()>0)
        this->eventlist().sourceIsPending(*this, _startFrom);
}

void SingleDynamicFailureEvent::setFailureRecoveryDelay(simtime_picosec setupReroutingDelay, simtime_picosec pathRestoreDelay) {
    _setupReroutingDelay = setupReroutingDelay;
    _pathRestoreDelay = pathRestoreDelay;
}

void SingleDynamicFailureEvent::addActiveConnection(TcpSrc *tcpSrc) {
    _activeConnections->insert(tcpSrc);
}

void SingleDynamicFailureEvent::removeActiveConnection(TcpSrc *tcpSrc) {

    if(_activeConnections->count(tcpSrc)>0)
        _activeConnections->erase(tcpSrc);
}

void SingleDynamicFailureEvent::removeSleepingConnection(TcpSrc *tcp) {
    if(_sleepingConnections->count(tcp)>0)
        _sleepingConnections->erase(tcp);
}

void SingleDynamicFailureEvent::doNextEvent() {
    if(UsingShareBackup && hasEnoughBackup() )
        circuitReconfig();
    else
        rerouting();

}

void SingleDynamicFailureEvent::circuitReconfig() {
    if (_failureStatus == GOOD) {
        if(_linkid>=0){
            _topo->failLink(_linkid);
            _topo->failLink(_dualLink);
        }else{
            assert(_nodeid>=0);
            _topo->failSwitch(_nodeid);
            _topo->failSwitch(_dualNode);
        }
        this->eventlist().sourceIsPending(*this, eventlist().now() + _setupReroutingDelay);
        _failureStatus = WAITING_REROUTING;

    }else if(_failureStatus == WAITING_REROUTING){
        if(_linkid>=0) {
            _topo->recoverLink(_linkid);
            _topo->recoverLink(_dualLink);
        }
        else {
            _topo->recoverSwitch(_nodeid);
            _topo->recoverSwitch(_dualNode);
        }
        _failureStatus = BAD;
        this->eventlist().sourceIsPending(*this, eventlist().now() + _failureTime);

        set<TcpSrc*>* sleepingConnections = new set<TcpSrc*>(*_sleepingConnections);
        for(TcpSrc* tcp:*sleepingConnections){
            tcp->wakeupFlow(10); //try wakeup a flow 10ms later
        }

    }else if(_failureStatus == BAD){

        _failureStatus = GOOD;
    }
}

bool SingleDynamicFailureEvent::hasEnoughBackup() {
//        if(_linkid>=0)
//            return *_group1 >0 && *_group2 >0;
//        else
//            return *_group1>0;
    // for single failure we always have enough backups
    return true;
}

void SingleDynamicFailureEvent::rerouting(){
    if (_failureStatus == GOOD) {
        if(_linkid>=0) {
            _topo->failLink(_linkid);
            _topo->failLink(_dualLink);
        }
        else{
            assert(_nodeid>=0);
            _topo->failSwitch(_nodeid);
            _topo->failSwitch(_dualNode);
        }
        this->eventlist().sourceIsPending(*this, eventlist().now() + _setupReroutingDelay);
        _failureStatus = WAITING_REROUTING;

    } else if (_failureStatus == WAITING_REROUTING) {
        set<TcpSrc*>* copyOfConnections = new set<TcpSrc*>(*_activeConnections);
        for (TcpSrc* tcp: *copyOfConnections) {
            if((tcp->_flowStatus!=Active)){
                continue;
            }
            tcp->handleImpactedFlow();
            route_t*currentPath = new route_t(tcp->_route->begin(), tcp->_route->end()-1);
            pair<route_t *, route_t *> newDataPath = _topo->getReroutingPath(tcp->_src, tcp->_dest, currentPath);
            if (_topo->isPathValid(newDataPath.first) && _topo->isPathValid(newDataPath.second)) {
                cout << "Now:"<<eventlist().now()/1e9<<" route change for flow " << tcp->_src << "->"
                     << tcp->_dest <<" timeout:"<<tcp->_rto/1e9<<"ms"<<endl;

                cout << "[old path]---- ";
                _topo->printPath(cout, tcp->_route);
                tcp->handleFlowRerouting(newDataPath.first, newDataPath.second);
                cout << "[new path]---- ";
                _topo->printPath(cout, tcp->_route);

            } else {
                //cout << "Now:"<<eventlist().now()/1e9 <<" [Sleeping from Failure Event]"<< tcp->_src << "->" << tcp->_dest << endl;
                tcp->handleFlowSleeping();
            }
        }
        _failureStatus = BAD;
        this->eventlist().sourceIsPending(*this, eventlist().now() + _failureTime);

    } else if (_failureStatus == BAD) {
        this->eventlist().sourceIsPending(*this, eventlist().now() + _pathRestoreDelay);
        _failureStatus = WAITING_RECOVER;

    } else {

        if(_linkid>=0) {
            _topo->recoverLink(_linkid);
            _topo->recoverLink(_dualLink);
        }
        else{
            assert(_nodeid>=0);
            _topo->recoverSwitch(_nodeid);
            _topo->recoverLink(_dualNode);
        }
        pair<route_t *, route_t *> path;
        set<TcpSrc*>* copyOfConnections = new set<TcpSrc*>(*_activeConnections);
        for (TcpSrc *tcp: *copyOfConnections) {
            if((tcp->_flowStatus!=Active)){
                continue;
            }
            path = _topo->getStandardPath(tcp->_src, tcp->_dest);
            if (_topo->isPathValid(path.first) && _topo->isPathValid(path.second)) {
                cout << "Now:"<<eventlist().now()/1e9
                     << "route [restore/reroute] for flow " << tcp->_src << "->" << tcp->_dest << endl;
                cout << "[current path]---- ";
                _topo->printPath(cout, tcp->_route);
                tcp->handleFlowRerouting(path.first, path.second);
                cout << "[new path]----";
                _topo->printPath(cout, tcp->_route);
            }
        }
        set<TcpSrc*>* sleepingConnections = new set<TcpSrc*>(*_sleepingConnections);
        for(TcpSrc* tcp:*sleepingConnections){
           // cout <<eventlist().now()/1e9<<"[Wake up in Failure Event]"<<tcp->_src<<"->"<<tcp->_dest<<endl;
            tcp->wakeupFlow(10); //try wakeup a flow 10ms later
        }

        _failureStatus = GOOD;
    }
}



bool SingleDynamicFailureEvent::isPathOverlapping(route_t *path) {
    if(_relevantQueues->size()>0) {
        for (PacketSink *t: *path) {
            for(Queue* q:*_relevantQueues)
            if (t->_gid == q->_gid) {
                return true;
            }
        }
    }
    return false;
}

void SingleDynamicFailureEvent::addSleepingConnection(TcpSrc *fc) {
    _sleepingConnections->insert(fc);
}