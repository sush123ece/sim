#ifndef TOPOLOGY
#define TOPOLOGY

#include <sstream>
#include "network.h"
#include "queue.h"
#include "pipe.h"
#include "../main.h"

typedef vector<PacketSink *> route_t;

class Topology {
public:

    virtual void failLink(int linkid) = 0;

    virtual int getServerUnderTor(int tor, int torNum) = 0;

    virtual void recoverLink(int linkid) = 0;

    virtual void failSwitch(int sid) =0;

    virtual void recoverSwitch(int sid) =0;

    virtual pair<route_t *, route_t *> getReroutingPath(int src, int dest, route_t *currrentPath) = 0;

    virtual pair<route_t *, route_t *> getEcmpPath(int src, int dest) = 0;

    virtual bool isPathValid(route_t *path) =0;

    virtual pair<route_t *, route_t *> getStandardPath(int src, int dest) = 0;

    virtual pair<Queue *, Queue *> linkToQueues(int linkid) = 0;

    virtual vector<int> *get_neighbours(int src) = 0;

    virtual vector<int>* getLinksFromSwitch(int sid) = 0;

    static void addFlowToPath(int superId, int coflowId, route_t *path) {
        //assume a path starts with a transmitting queue followed by a pipe
        assert(path->size() >= 2);
        //starting from a pipe
        for (unsigned i = 1; i < path->size() - 2; i += 2) {
            Pipe *pipe = (Pipe *) path->at(i);
            assert(pipe->_flowTracker->count(superId) == 0);
            pipe->_flowTracker->insert(superId);
            pipe->_coflowTracker->insert(coflowId);
        }
    }

    static void removeFlowFromPath(int superId, int coflowId, route_t *path) {
        //assume a path starts with a transmitting queue followed by a pipe
        if(path==NULL || path->size() < 2){
            return;
        }
        //starting from a pipe
        for (unsigned i = 1; i < path->size() - 2; i += 2) {
            Pipe *pipe = (Pipe *) path->at(i);
            pipe->_flowTracker->erase(superId);
            if(pipe->_coflowTracker->count(coflowId)>0)
                pipe->_coflowTracker->erase(pipe->_coflowTracker->find(coflowId));

        }
    }

    static int getPathLoad(route_t *path) {
        //assume a path starts with a transmitting queue followed by a pipe
        assert(path->size() >= 2);
        //starting from a pipe
        int load = -1;
        for (unsigned i = 1; i < path->size() - 2; i += 2) {
            Pipe *pipe = (Pipe *) path->at(i);
            if (pipe->_flowTracker->size() > load)
                load = pipe->_flowTracker->size();
        }
        return load;
    }

    static void printPath(std::ostream &out, route_t *rt) {
        for (unsigned int i = 0; i < rt->size(); i += 2) {
            Queue *q = (Queue *) rt->at(i);
            if (q != NULL)
                out << q->_gid << " ";
            else
                out << "NULL ";
        }
        out << endl;
    }


    static int myrandom(int i) { return rand() % i; }

    string ntoa(double n) {
        stringstream s;
        s << n;
        return s.str();
    }

    string itoa(uint64_t n) {
        stringstream s;
        s << n;
        return s.str();
    }

    static double getLoad(double lastArrivalTime, double TrafficVolumeKB){
        double totalLinks = K/2*K/2*K*2;
        double timeSec = TrafficVolumeKB*1000*8*3.0/(1.0*1e9*totalLinks);
        return timeSec*1000.0/lastArrivalTime;
    }
};

#endif
