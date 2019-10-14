#include "queue.h"
#include <iostream>
#include <math.h>
#include <set>
#include "../main.h"
#include <vector>
#include "pipe.h"
#include <ostream>
#include "../datacentre/fattree/fat_tree_topology.h"
#include "cbrpacket.h"
#include "cbr.h"

//std::vector<std::vector<int>> vec_sw(NK + NK + NK / 2);
//std::vector<std::vector<int>> vec_pipe(K * K * K / 2 + NHOST);

int *colom1;
int *colom2;


 Pipe *pipes_nc_nup[NC][NK];
 Pipe *pipes_nup_nlp[NK][NK];
 Pipe *pipes_nlp_ns[NK][NSRV];
RandomQueue *queues_nc_nup[NC][NK];//[WDM] queue is on the second end of the link
RandomQueue *queues_nup_nlp[NK][NK];
// RandomQueue *queues_nlp_ns[NK][NSRV]; //handle host queues separately

Pipe *pipes_nup_nc[NK][NC];
Pipe *pipes_nlp_nup[NK][NK];
Pipe *pipes_ns_nlp[NSRV][NK];
RandomQueue *queues_nup_nc[NK][NC];
RandomQueue *queues_nlp_nup[NK][NK];
RandomQueue *queues_ns_nlp[NSRV][NK];
RandomQueue *HostRecvQueues[NSRV];
RandomQueue *HostTXQueues[NSRV];




string ntoa(double n);

Queue::Queue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist, QueueLogger *logger)
        : EventSource(eventlist, "queue"),
          _maxsize(maxsize), _logger(logger), _bitrate(bitrate) {
    _queuesize = 0;
    _ps_per_byte = (simtime_picosec) ((pow(10.0, 12.0) * 8) / _bitrate);
}

Queue::Queue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist, QueueLogger *logger, string gid, int sid)
        : EventSource(eventlist, "queue"),
          _maxsize(maxsize), _logger(logger), _bitrate(bitrate) {
    _queuesize = 0;
    _ps_per_byte = (simtime_picosec) ((pow(10.0, 12.0) * 8) / _bitrate);
    _gid = gid;
    _switchId = sid;
}

void Queue::beginService() {
    assert(!_enqueued.empty());
    int delay = 0;
    if(!_isHostQueue){
        delay = drainTime(_enqueued.back());
    }
    eventlist().sourceIsPendingRel(*this, delay);
}

void Queue::completeService() {
    if (_enqueued.empty()) {
        std::cout << "empty queue:" << this->str() << std::endl;
        exit(1);
    }
    Packet *pkt = _enqueued.back();
    _enqueued.pop_back();

    _queuesize -= pkt->size();

    if(this->_gid=="Queue-up-lp-3-3"){
       // cout<<"[Packet Depart]:"<<pkt->_src<<"->"<<pkt->_dest<<" at "<<eventlist().now()/1e6<<"us"<<endl;
    }
    pkt->flow().logTraffic(*pkt, *this, TrafficLogger::PKT_DEPART);
    if (_logger)
        _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);

    // replicate packet before here (new function of switch)

    cout << "came in switch complete service" << endl;
    if(pkt->_mulFlag) // if multicast packet
    {
        // _eventlist.setEndtime(timeFromSec(3000.01));
        cout << "identify multicast packet, pkt id" << pkt->_id << endl;
        cout << "switchid is " << _switchId << endl;
        cout << "no of next hop" << colom1[_switchId] << endl;

     //   cout << "number of next hop of switch id 0" << vec_sw[_switchId].size() << endl;
     //   if (vec_sw[_switchId].size() == 1) // if one hop

        if(_switchId == -1)
            pkt->sendOn();
        if (colom1[_switchId] == 1) // if one hop
        {
            cout << "in one hop case" << endl;
            if (_switchId == 0) // giving explicit route (ToR)
            {

               // pkt->_route = new route_t();
                (pkt->_route)->push_back(pipes_nlp_nup[HOST_POD_SWITCH(0)][K * K / 2]);
                (pkt->_route)->push_back(queues_nlp_nup[HOST_POD_SWITCH(0)][K * K / 2]);
                pkt->sendOn();
                cout << "came here switch 0 " << (queues_nlp_nup[HOST_POD_SWITCH(0)][K * K / 2])->_switchId << endl;
            }

            if (_switchId == 1) {
               // pkt->_route = new route_t();

                 CbrSink* cbrsnk = new CbrSink();

                (pkt->_route)->push_back(pipes_nlp_ns[HOST_POD_SWITCH(K / 2)][K / 2]);
                (pkt->_route)->push_back(HostRecvQueues[K / 2]);
               // cbrsnk->receivePacket(*pkt);
                (pkt->_route)->push_back(cbrsnk);

                cout << "came here switch 1 " << pkt->_nexthop << endl;
                pkt->sendOn();
            }

            if (_switchId == 2) {
             //   pkt->_route = new route_t();
                CbrSink* cbrsnk = new CbrSink();
                (pkt->_route)->push_back(pipes_nlp_ns[HOST_POD_SWITCH(K)][K]);
                (pkt->_route)->push_back(HostRecvQueues[K]);
                (pkt->_route)->push_back(cbrsnk);
                cout << "came here switch 2 " << pkt->_nexthop << endl;
                pkt->sendOn();

            }

            //  cout << vec_sw[_switchId][0] << endl;


        }

        if(colom1[_switchId]>1) {

            // aggregate switch
            if (_switchId == K * K / 2) // giving explicit route
            {
                cout << " in aggre switch id " << _switchId << endl;
                //duplicate packet
              //  CbrPacket *npkt[colom1[_switchId]];
                //  CbrPacket* npkt1 = (CbrPacket*) malloc (sizeof(Packet));


                cout << "duplicate packet" << endl;

                int destination[colom1[_switchId]] = {K / 2, K}; // dest host
             //   cout << destination[0] << " " << (queues_nup_nlp[0][HOST_POD_SWITCH(destination[1])])->_switchId << endl;


                for (int i = 0; i < colom1[_switchId]; i++) {

                    cout << "inside duplication loop" << endl;
                    // npkt[i]=pkt;
                 //   npkt[i] = (CbrPacket *) malloc(sizeof(Packet));
                   // CbrPacket* npkt = new CbrPacket((pkt->_flow), (pkt->_route), (pkt->_id +1), pkt->_size);
                  //  Packet* npkt = (Packet*) malloc(sizeof(Packet));
                      CbrPacket* npkt = new CbrPacket();
                    route_t *routeout = new route_t();
                  //  PacketFlow* newflow = new PacketFlow(NULL);
                   cout << "pkt route size " << (pkt->_route)->size() << endl;

                  //  CbrPacket* npkt = (CbrPacket*) malloc (sizeof(Packet));
                    npkt->_src = pkt->_src;
                    npkt->_dest = pkt->_dest;
                    npkt->_size = pkt->_size;
                    npkt->_mulFlag = pkt->_mulFlag;
                 //   npkt->_route = pkt->_route;
                    routeout->push_back(pipes_nup_nlp[0][HOST_POD_SWITCH(destination[i])]);
                    routeout->push_back(queues_nup_nlp[0][HOST_POD_SWITCH(destination[i])]);
                    npkt->_route = routeout;

                    npkt->_nexthop = 0;
                    npkt->_id = (pkt->_id)*5 + i;
                    npkt->_flow = pkt->_flow;
                //  npkt->_flow = newflow;




                    int sush = 3;
                  //  cout << "sush: " << sush << endl;
                    cout << "new pkt" << npkt->_src << " " << npkt->_nexthop << endl;
                    cout << (npkt->_route) << " " << (pkt->_route) << " " << (queues_nup_nlp[0][HOST_POD_SWITCH(destination[i])])->_switchId << endl;

                    npkt->sendOn();


                }

            }

        }

       // else
        //    pkt->sendOn();
    }


     //   if(vec_sw[_switchId].size()>1) // if multiple hops



            /*    for(int i=0;i<colom1[_switchId];i++)
                {
                    //duplicate the packet
                   // Packet *npkt = pkt;

                    // add next hop
                  //  (npkt->_route)->push_back(pipes_nup_nlp[0][HOST_POD_SWITCH(dest[i])]);
                  //  (npkt->_route)->push_back(queues_nup_nlp[0][HOST_POD_SWITCH(dest[i])]);

                    //with array of packets
                    //duplicate
                    npkt[i]= pkt;
                    //add next hop
                    (npkt[i]->_route)->push_back(pipes_nup_nlp[_switchId][HOST_POD_SWITCH(dest[i])]);
                    (npkt[i]->_route)->push_back(queues_nup_nlp[_switchId][HOST_POD_SWITCH(dest[i])]);



                    npkt[i]->sendOn();
                }

            }
        } */

    /*    if(vec_sw[_switchId].size()>1)
        {
            for (int i=0;i<vec_sw[_switchId].size();i++)
            {

                // duplicate
                Packet temp;
                temp = *pkt;
                Packet* npkt;
                npkt = &temp;


                int pipeId = vec_sw[_switchId][i]; // next pipe getting from switch table
                // creating pipe object with src sw and dest sw (getting from pipe table) pattern
                Pipe nextpipe = new Pipe(timeFromUs(RTT), *_eventlist,
                                         "Pipe-up-lp-" + ntoa(_switchId) + "-" + ntoa(vec_pipe[pipeId][0]));

                (npkt->_route)->push_back(*nextpipe);

                //creating next switch object

                RandomQueue nextswitch = new RandomQueue(speedFromPktps(HOST_NIC / CORE_TO_HOST),
                                                         memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER),
                                                         *_eventlist, queueLogger,
                                                         memFromPkt(RANDOM_BUFFER),
                                                         "Queue-lp-up-" + ntoa(j) + "-" + ntoa(k), k + NK);

                (npkt->_route)->push_back(nextswitch);

                npkt->sendOn();



            }

        }

    }

    cout << "vec sw can be accessed:" << vec_sw[_switchId][0] << endl; */
    if(!pkt->_mulFlag)
    {
        pkt->sendOn();
    }

    if (!_enqueued.empty())
        beginService();
}

void Queue::doNextEvent() {
    completeService();
}


void Queue::receivePacket(Packet &pkt) {
    if (_disabled || _queuesize + pkt.size() > _maxsize) {
        if (_logger)
            _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
        pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_DROP);
        pkt.free();
        cout<<"[Packet Drop]"<<pkt.flow().str()<<" at "<<this->str()<<endl;
        return;
    }

    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_ARRIVE);
    bool queueWasEmpty = _enqueued.empty();
    _enqueued.push_front(&pkt);
    _queuesize += pkt.size();

    if (_logger)
        _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);
    if (queueWasEmpty) {
        assert(_enqueued.size() == 1);
        beginService();
    }
}
