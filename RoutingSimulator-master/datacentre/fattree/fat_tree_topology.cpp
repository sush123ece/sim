#include "fat_tree_topology.h"
#include <iostream>


string ntoa(double n);

string itoa(uint64_t n);

extern int N;

FatTreeTopology::FatTreeTopology(EventList *ev) {
    _eventlist = ev;
    _numLinks = K * K * K / 2 + NHOST; //[WDM] should include host links
    _numSwitches = NK + NK + NK / 2;
    _failedSwitches = new bool[_numSwitches];
    _failedLinks = new bool[_numLinks];
    for (int i = 0; i < _numLinks; i++) {
        _failedLinks[i] = false;
    }

    for (int i = 0; i < _numSwitches; i++) {
        _failedSwitches[i] = false;
    }
    _net_paths = new vector<route_t *> **[NHOST];

    for (int i = 0; i < NHOST; i++) {
        _net_paths[i] = new vector<route_t *> *[NHOST];
        for (int j = 0; j < NHOST; j++)
            _net_paths[i][j] = NULL;
    }

    init_network();
}

void FatTreeTopology::init_network() {

    QueueLoggerSampling *queueLogger = NULL;

    for (int i = 0; i < NSRV; i++) {
        HostTXQueues[i] = new RandomQueue(speedFromPktps(HOST_NIC),
                                          memFromPkt(FEEDER_BUFFER + RANDOM_BUFFER),
                                          *_eventlist, NULL, memFromPkt(RANDOM_BUFFER),
                                          "TxQueue:" + ntoa(i), -1);
        HostTXQueues[i]->_isHostQueue = true;

        HostRecvQueues[i] = new RandomQueue(speedFromPktps(HOST_NIC),
                                            memFromPkt(FEEDER_BUFFER + RANDOM_BUFFER),
                                            *_eventlist, NULL, memFromPkt(RANDOM_BUFFER),
                                            "RxQueue:" + ntoa(i), -1);
        HostRecvQueues[i]->_isHostQueue = true;
    }

    for (int j = 0; j < NC; j++)
        for (int k = 0; k < NK; k++) {
            queues_nc_nup[j][k] = NULL;
            pipes_nc_nup[j][k] = NULL;
            queues_nup_nc[k][j] = NULL;
            pipes_nup_nc[k][j] = NULL;
        }

    for (int j = 0; j < NK; j++)
        for (int k = 0; k < NK; k++) {
            queues_nup_nlp[j][k] = NULL;
            pipes_nup_nlp[j][k] = NULL;
            queues_nlp_nup[k][j] = NULL;
            pipes_nlp_nup[k][j] = NULL;
        }

    for (int j = 0; j < NK; j++)
        for (int k = 0; k < NSRV; k++) {
            pipes_nlp_ns[j][k] = NULL;
            queues_ns_nlp[k][j] = NULL;
            pipes_ns_nlp[k][j] = NULL;
        }

    // lower layer pod switch to server
    for (int j = 0; j < NK; j++) {

        for (int l = 0; l < K * RATIO / 2; l++) {
            //[WDM] k is both the global hostID and linkID
            int k = j * K * RATIO / 2 + l;

            //[WDM]handle host queues separately
            pipes_nlp_ns[j][k] = new Pipe(timeFromUs(RTT), *_eventlist,
                                          "Pipe-lp-host-" + ntoa(j) + "-" + ntoa(k));

            // Uplink
            queues_ns_nlp[k][j] = new RandomQueue(speedFromPktps(HOST_NIC),
                                                  memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER),
                                                  *_eventlist, queueLogger, memFromPkt(RANDOM_BUFFER),
                                                  "Queue-host-lp-" + ntoa(k) + "-" + ntoa(j), j);

            pipes_ns_nlp[k][j] = new Pipe(timeFromUs(RTT), *_eventlist,
                                          "Pipe-host-lp-" + ntoa(k) + "-" + ntoa(j));

            pipes_nlp_ns[j][k]->setDualPipe(pipes_ns_nlp[k][j]);
            queues_ns_nlp[k][j]->setDualQueue(HostRecvQueues[k]);

        }
    }

    //Lower layer in pod to upper layer in pod!
    for (int j = 0; j < NK; j++) {
        int podid = 2 * j / K;
        //Connect the lower layer switch to the upper layer switches in the same pod
        for (int k = MIN_POD_ID(podid); k <= MAX_POD_ID(podid); k++) {
            // Downlink

            queues_nup_nlp[k][j] = new RandomQueue(speedFromPktps(HOST_NIC / CORE_TO_HOST),
                                                   memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER),
                                                   *_eventlist, queueLogger,
                                                   memFromPkt(RANDOM_BUFFER),
                                                   "Queue-up-lp-" + ntoa(k) + "-" + ntoa(j), j);

            pipes_nup_nlp[k][j] = new Pipe(timeFromUs(RTT), *_eventlist,
                                           "Pipe-up-lp-" + ntoa(k) + "-" + ntoa(j));

            // Uplink
            queues_nlp_nup[j][k] = new RandomQueue(speedFromPktps(HOST_NIC / CORE_TO_HOST),
                                                   memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER),
                                                   *_eventlist, queueLogger,
                                                   memFromPkt(RANDOM_BUFFER),
                                                   "Queue-lp-up-" + ntoa(j) + "-" + ntoa(k), k + NK);

            pipes_nlp_nup[j][k] = new Pipe(timeFromUs(RTT), *_eventlist,
                                           "Pipe-lp-up-" + ntoa(j) + "-" + ntoa(k));

            pipes_nup_nlp[k][j]->setDualPipe(pipes_nlp_nup[j][k]);
            queues_nup_nlp[k][j]->setDualQueue(queues_nlp_nup[j][k]);
        }
    }


    // Upper layer in pod to core!
    for (int j = 0; j < NK; j++) {
        int podpos = j % (K / 2);
        for (int l = 0; l < K / 2; l++) {
            int k = podpos * K / 2 + l;

            queues_nup_nc[j][k] = new RandomQueue(speedFromPktps(HOST_NIC / CORE_TO_HOST),
                                                  memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER),
                                                  *_eventlist, queueLogger,
                                                  memFromPkt(RANDOM_BUFFER),
                                                  "Queue-up-co-" + ntoa(j) + "-" + ntoa(k), k + 2 * NK);

            pipes_nup_nc[j][k] = new Pipe(timeFromUs(RTT), *_eventlist,
                                          "Pipe-up-co-" + ntoa(j) + "-" + ntoa(k));


            queues_nc_nup[k][j] = new RandomQueue(speedFromPktps(HOST_NIC / CORE_TO_HOST),
                                                  memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER),
                                                  *_eventlist, queueLogger,
                                                  memFromPkt(RANDOM_BUFFER),
                                                  "Queue-co-up-" + ntoa(k) + "-" + ntoa(j), j + NK);


            pipes_nc_nup[k][j] = new Pipe(timeFromUs(RTT), *_eventlist,
                                          "Pipe-co-up-" + ntoa(k) + "-" + ntoa(j));

            queues_nup_nc[j][k]->setDualQueue(queues_nc_nup[k][j]);

            pipes_nc_nup[k][j]->setDualPipe(pipes_nup_nc[j][k]);

        }
    }

    //initializing vec_sw
   /*
    // int _numSwitches = NK + NK + NK / 2;
    int row = NK + NK + NK / 2;
    vector <vector<int>> vec_sw(row);
    int col;

    // size of next hop for each switch
    int *colom1;
    for (int i = 0; i < row; i++) {
        if (i == 0)
            colom1[i] = 1;
        if (i == K * K / 2)
            colom1[i] = 2;
        else
            colom1[i] = 0;
    }


    for (int i = 0; i < row; i++) {


        // size of column

        col = colom1[i];

        // declare the i-th row to size of column
        vec_sw[i] = vector<int>(col);

        if (i == 0)
            vec_sw[i] = {0}; // for ToR switch 0, net hop is pipe 0
        if (i == K * K / 2)
            vec_sw[i] = {K / 2, K}; // for Aggr switch K^2/2, next hops are pipe K/2 and K


    }

    // end

    // initializing vec_pipe


    // int _numLinks = K * K * K / 2 + NHOST; //[WDM] should include host links
    //  int _numSwitches = NK + NK + NK / 2;
    row = K * K * K / 2 + NHOST;
    vector <vector<int>> vec_pipe(row);

    // size of next hop for each switch
    int *colom2;
    for (int i = 0; i < row; i++) {

        if (i == 0 || i == K/2 || i == K)
            colom2[i] = 1;
        else
            colom2[i] = 0;

    }


    for (int i = 0; i < row; i++) {
        col = colom2[i];

        // declare the i-th row to size of column
        vec_pipe[i] = vector<int>(col);

        if (i == 0)
            vec_pipe[i] = {K^K/2}; // for ToR switch 0, net hop is pipe 0
        if (i == K/2)
            vec_pipe[i] = {1}; // for Aggr switch K^2/2, next hops are pipe K/2 and K
        if (i == K)
            vec_pipe[i] = {2};


    }

  */
    //end vec_pipe


}

vector<route_t *> *FatTreeTopology::get_paths_ecmp(int src, int dest) {
    vector<route_t *> *paths = new vector<route_t *>();
    route_t *routeout = NULL;
    if (HOST_POD_SWITCH(src) == HOST_POD_SWITCH(dest)) {
        routeout = new route_t();
        routeout->push_back(HostTXQueues[src]);

        routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)]);

        routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]);

        routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest]);

        routeout->push_back(HostRecvQueues[dest]);

        paths->push_back(routeout);
        return paths;

    } else if (HOST_POD(src) == HOST_POD(dest)) {
        //don't go up the hierarchy, stay in the pod only.
        int pod = HOST_POD(src);
        //there are K/2 paths between the source and the destination
        for (int upper = MIN_POD_ID(pod); upper <= MAX_POD_ID(pod); upper++) {
            //upper is nup

            routeout = new route_t();
            routeout->push_back(HostTXQueues[src]);

            routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)]);

            routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]);

            routeout->push_back(pipes_nlp_nup[HOST_POD_SWITCH(src)][upper]);

            routeout->push_back(queues_nlp_nup[HOST_POD_SWITCH(src)][upper]);

            routeout->push_back(pipes_nup_nlp[upper][HOST_POD_SWITCH(dest)]);

            routeout->push_back(queues_nup_nlp[upper][HOST_POD_SWITCH(dest)]);

            routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest]);

            routeout->push_back(HostRecvQueues[dest]);

            paths->push_back(routeout);

        }
        return paths;
    } else {
        int pod = HOST_POD(src);
        for (int upper = MIN_POD_ID(pod); upper <= MAX_POD_ID(pod); upper++) {

            for (int core = (upper % (K / 2)) * K / 2; core < ((upper % (K / 2)) + 1) * K / 2; core++) {

                //upper is nup

                routeout = new route_t();
                routeout->push_back(HostTXQueues[src]);

                routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)]);

                routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]);

                routeout->push_back(pipes_nlp_nup[HOST_POD_SWITCH(src)][upper]);

                routeout->push_back(queues_nlp_nup[HOST_POD_SWITCH(src)][upper]);

                routeout->push_back(pipes_nup_nc[upper][core]);

                routeout->push_back(queues_nup_nc[upper][core]);

                //now take the only link down to the destination server!

                int upper2 = HOST_POD(dest) * K / 2 + 2 * core / K;

                routeout->push_back(pipes_nc_nup[core][upper2]);

                routeout->push_back(queues_nc_nup[core][upper2]);

                routeout->push_back(pipes_nup_nlp[upper2][HOST_POD_SWITCH(dest)]);

                routeout->push_back(queues_nup_nlp[upper2][HOST_POD_SWITCH(dest)]);

                routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest]);

                routeout->push_back(HostRecvQueues[dest]);

                paths->push_back(routeout);
            }
        }
        return paths;
    }
}

//Implemented by WDM
route_t *FatTreeTopology::get_path_2levelrt(int src, int dest) {

    route_t *routeout = new route_t();
    RandomQueue *txqueue = HostTXQueues[src];
    routeout->push_back(txqueue);

    //under the same TOR
    if (HOST_POD_SWITCH(src) == HOST_POD_SWITCH(dest)) {

        routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)]);

        routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]);
        routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest]);

        //under the same Pod
    } else if (HOST_POD(src) == HOST_POD(dest)) {
        int pod = HOST_POD(src);
        int srcHostID = src % (K / 2);
        int aggSwitchID = MIN_POD_ID(pod) + srcHostID;

        routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)]);

        routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]);

        routeout->push_back(pipes_nlp_nup[HOST_POD_SWITCH(src)][aggSwitchID]);


        routeout->push_back(queues_nlp_nup[HOST_POD_SWITCH(src)][aggSwitchID]);

        routeout->push_back(pipes_nup_nlp[aggSwitchID][HOST_POD_SWITCH(dest)]);

        routeout->push_back(queues_nup_nlp[aggSwitchID][HOST_POD_SWITCH(dest)]);


        routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest]);


    } else {

        int pod = HOST_POD(src);
        int srcHostID = src % (K / 2); //could be used as outgoing offset
        int destHostID = dest % (K / 2);
        int aggSwitchID1 = MIN_POD_ID(pod) + srcHostID;
        int core = (aggSwitchID1 % (K / 2)) * (K / 2) +
                   (destHostID) % (K / 2); // [disabled]src_pod_id as an offset to diffuse incast traffic

        int aggSwitchID2 = HOST_POD(dest) * K / 2 + 2 * core / K;

        routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)]);

        routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]);

        routeout->push_back(pipes_nlp_nup[HOST_POD_SWITCH(src)][aggSwitchID1]);

        routeout->push_back(queues_nlp_nup[HOST_POD_SWITCH(src)][aggSwitchID1]);

        routeout->push_back(pipes_nup_nc[aggSwitchID1][core]);

        routeout->push_back(queues_nup_nc[aggSwitchID1][core]);

        routeout->push_back(pipes_nc_nup[core][aggSwitchID2]);

        routeout->push_back(queues_nc_nup[core][aggSwitchID2]);

        routeout->push_back(pipes_nup_nlp[aggSwitchID2][HOST_POD_SWITCH(dest)]);

        routeout->push_back(queues_nup_nlp[aggSwitchID2][HOST_POD_SWITCH(dest)]);

        routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest]);

    }
    RandomQueue *rxQueue = HostRecvQueues[dest];
    routeout->push_back(rxQueue);
    return routeout;
}


//route_t *FatTreeTopology::getMulticastPath(int src, int *dest) {

/*
 Node *FatTreeTopology::getMulticastPath() {

    //route_t *routeout = new route_t();
    //RandomQueue *txqueue = HostTXQueues[src];

    Node *root = newNode(0);
    (root->child).push_back(newNode(0));
    (root->child[0]->child).push_back(newNode((k*K)/2));
    (root->child[0]->child[0]->child).push_back(newNode(k/2));
    (root->child[0]->child[0]->child).push_back(newNode(k));
    (root->child[0]->child[0]->child[0]->child).push_back(newNode(1));
    (root->child[0]->child[0]->child[1]->child).push_back(newNode(2));

    //routeout->push_back(txqueue);

    //routeout->

    /*for (int i = 0; i < 2 ; i++) {

    }*/

/*
//under the same TOR
if (HOST_POD_SWITCH(src) == HOST_POD_SWITCH(dest[0]) && HOST_POD_SWITCH(src) == HOST_POD_SWITCH(dest[1]))
{

    routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)]);
    routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]);
    routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest[0])][dest[0]]);
    routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest[1])][dest[1]]);

    //under the same Pod
} else if (HOST_POD(src) == HOST_POD(dest[0]) && HOST_POD(src) == HOST_POD(dest[1]) ) {
    int pod = HOST_POD(src);
    int srcHostID = src % (K / 2);
    int aggSwitchID = MIN_POD_ID(pod) + srcHostID;

    routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)]);

    routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]);

    routeout->push_back(pipes_nlp_nup[HOST_POD_SWITCH(src)][aggSwitchID]);


    routeout->push_back(queues_nlp_nup[HOST_POD_SWITCH(src)][aggSwitchID]);

    routeout->push_back(pipes_nup_nlp[aggSwitchID][HOST_POD_SWITCH(dest)]);

    routeout->push_back(queues_nup_nlp[aggSwitchID][HOST_POD_SWITCH(dest)]);

    routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest]);


} else {

    int pod = HOST_POD(src);
    int srcHostID = src % (K / 2); //could be used as outgoing offset
    int destHostID = dest % (K / 2);
    int aggSwitchID1 = MIN_POD_ID(pod) + srcHostID;
    int core = (aggSwitchID1 % (K / 2)) * (K / 2) +
               (destHostID) % (K / 2); // [disabled]src_pod_id as an offset to diffuse incast traffic

    int aggSwitchID2 = HOST_POD(dest) * K / 2 + 2 * core / K;

    routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)]);

    routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]);

    routeout->push_back(pipes_nlp_nup[HOST_POD_SWITCH(src)][aggSwitchID1]);

    routeout->push_back(queues_nlp_nup[HOST_POD_SWITCH(src)][aggSwitchID1]);

    routeout->push_back(pipes_nup_nc[aggSwitchID1][core]);

    routeout->push_back(queues_nup_nc[aggSwitchID1][core]);

    routeout->push_back(pipes_nc_nup[core][aggSwitchID2]);

    routeout->push_back(queues_nc_nup[core][aggSwitchID2]);

    routeout->push_back(pipes_nup_nlp[aggSwitchID2][HOST_POD_SWITCH(dest)]);

    routeout->push_back(queues_nup_nlp[aggSwitchID2][HOST_POD_SWITCH(dest)]);

    routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest]);

}
RandomQueue *rxQueue = HostRecvQueues[dest];
routeout->push_back(rxQueue); */
//  return root;
// }


void FatTreeTopology::count_queue(RandomQueue *queue) {
    if (_link_usage.find(queue) == _link_usage.end()) {
        _link_usage[queue] = 0;
    }

    _link_usage[queue] = _link_usage[queue] + 1;
}

int FatTreeTopology::find_lp_switch(RandomQueue *queue) {
    //first check ns_nlp
    for (int i = 0; i < NSRV; i++)
        for (int j = 0; j < NK; j++)
            if (queues_ns_nlp[i][j] == queue)
                return j;

    //only count nup to nlp
    count_queue(queue);

    for (int i = 0; i < NK; i++)
        for (int j = 0; j < NK; j++)
            if (queues_nup_nlp[i][j] == queue)
                return j;

    return -1;
}

int FatTreeTopology::find_up_switch(RandomQueue *queue) {
    count_queue(queue);
    //first check nc_nup
    for (int i = 0; i < NC; i++)
        for (int j = 0; j < NK; j++)
            if (queues_nc_nup[i][j] == queue)
                return j;

    //check nlp_nup
    for (int i = 0; i < NK; i++)
        for (int j = 0; j < NK; j++)
            if (queues_nlp_nup[i][j] == queue)
                return j;

    return -1;
}

int FatTreeTopology::find_core_switch(RandomQueue *queue) {
    count_queue(queue);
    //first check nup_nc
    for (int i = 0; i < NK; i++)
        for (int j = 0; j < NC; j++)
            if (queues_nup_nc[i][j] == queue)
                return j;

    return -1;
}

int FatTreeTopology::find_destination(RandomQueue *queue) {
    for (int j = 0; j < NSRV; j++)
        if (HostTXQueues[j] == queue || HostRecvQueues[j] == queue)
            return j;

    return -1;
}

void FatTreeTopology::print_path(std::ofstream &paths, int src, route_t *route) {
    paths << "SRC_" << src << " ";

    if (route->size() / 2 == 2) {
        paths << "LS_" << find_lp_switch((RandomQueue *) route->at(1)) << " ";
        paths << "DST_" << find_destination((RandomQueue *) route->at(3)) << " ";
    } else if (route->size() / 2 == 4) {
        paths << "LS_" << find_lp_switch((RandomQueue *) route->at(1)) << " ";
        paths << "US_" << find_up_switch((RandomQueue *) route->at(3)) << " ";
        paths << "LS_" << find_lp_switch((RandomQueue *) route->at(5)) << " ";
        paths << "DST_" << find_destination((RandomQueue *) route->at(7)) << " ";
    } else if (route->size() / 2 == 6) {
        paths << "LS_" << find_lp_switch((RandomQueue *) route->at(1)) << " ";
        paths << "US_" << find_up_switch((RandomQueue *) route->at(3)) << " ";
        paths << "CS_" << find_core_switch((RandomQueue *) route->at(5)) << " ";
        paths << "US_" << find_up_switch((RandomQueue *) route->at(7)) << " ";
        paths << "LS_" << find_lp_switch((RandomQueue *) route->at(9)) << " ";
        paths << "DST_" << find_destination((RandomQueue *) route->at(11)) << " ";
    } else {
        paths << "Wrong hop count " << ntoa(route->size() / 2);
    }

    paths << endl;
}

int FatTreeTopology::getServerUnderTor(int tor, int givenTorNum) {
    int totalTorNum = NK;
    int mappedTor = (int) ((double) tor / givenTorNum * totalTorNum);

    int result = rand_host_sw(mappedTor);

    return result;
}

int FatTreeTopology::rand_host_sw(int sw) {
    int hostPerEdge = K * RATIO / 2;
    int server_sw = rand() % hostPerEdge;
    int server = sw * hostPerEdge + server_sw;
    return server;
}

int FatTreeTopology::nodePair_to_link(int node1, int node2) {
    int result;
    if (node1 > node2) {
        swap(node1, node2);
    }
    if (node1 < NK) {
        int aggr_in_pod = (node2 - NK) % (K / 2);
        result = (K * node1 / 2 + aggr_in_pod);
    } else {
        int core_in_group = (node2 - 2 * NK) % (K / 2);
        result = (K * NK / 2 + (node1 - NK) * K / 2 + core_in_group);
    }
    return result;
}

bool FatTreeTopology::isPathValid(route_t *rt) {
    if (rt == NULL||rt->size()<2)
        return false;
    for (int i = 0; i < rt->size(); i += 2) {
        if (rt->at(i)->_disabled) {
            return false;
        }
    }
    return true;
}

pair<Queue *, Queue *> FatTreeTopology::linkToQueues(int linkid) {
    pair<Queue *, Queue *> ret(nullptr, nullptr);
    int lower, higher;
    if (linkid < NHOST) {
        lower = linkid;
        higher = linkid / (K * RATIO / 2);
        ret.first = queues_ns_nlp[lower][higher];
        ret.second = HostRecvQueues[lower];
        return ret;
    } else if (linkid >= NHOST && linkid < NHOST + NK * K / 2) {
        linkid -= NHOST;
        lower = linkid / (K / 2);
        higher = MIN_POD_ID(lower / (K / 2)) + linkid % (K / 2);
        ret.first = queues_nlp_nup[lower][higher];
        ret.second = queues_nup_nlp[higher][lower];

    } else {
        assert(linkid >= NHOST + NK * K / 2 && linkid < NHOST + NK * K);
        linkid -= (NHOST + NK * K / 2);
        lower = linkid / (K / 2);
        higher = (lower % (K / 2)) * (K / 2) + linkid % (K / 2);
        ret.first = queues_nup_nc[lower][higher];
        ret.second = queues_nc_nup[higher][lower];
    }
    if (ret.first == nullptr || ret.second == nullptr) {
        cout << "invalid linkid for failure:" << linkid << endl;
        exit(-1);
    }
    return ret;
}

void FatTreeTopology::failSwitch(int sid) {
    vector<int> *links = getLinksFromSwitch(sid);
    for (int link:*links) {
        failLink(link);
    }
}

void FatTreeTopology::recoverSwitch(int sid) {
    vector<int> *links = getLinksFromSwitch(sid);
    for (int link:*links) {
        recoverLink(link);
    }
}

vector<int> *FatTreeTopology::getLinksFromSwitch(int sid) {
    vector<int> *ret = new vector<int>();
    if (_failedSwitches[sid])
        return ret;
    if (sid < NK) {
        //upfacing links
        for (int i = 0; i < (K / 2); i++) {
            int linkId = NHOST + sid * (K / 2) + i;
            ret->push_back(linkId);
        }
        //down-facing links
        for (int i = 0; i < (K / 2) * RATIO; i++) {
            int linkId = sid * (K / 2) * RATIO + i;
            ret->push_back(linkId);
        }
    } else if (sid >= NK && sid < 2 * NK) {
        //upfacing links
        for (int i = 0; i < K / 2; i++) {
            int linkId = NHOST + sid * K / 2 + i;
            ret->push_back(linkId);
        }
        //down-facing links
        int linkOffset = sid % (K / 2);
        int podId = (sid - NK) / (K / 2);
        for (int lp = MIN_POD_ID(podId); lp <= MAX_POD_ID(podId); lp++) {
            int linkId = lp * K / 2 + linkOffset + NHOST;
            ret->push_back(linkId);
        }

    } else {
        int coreOffset = (sid - 2 * NK) / (K / 2);
        int linkOffset = (sid - 2 * NK) % (K / 2);
        for (int pod = 0; pod < K; pod++) {
            int aggId = pod * K / 2 + coreOffset + NK;
            int linkId = aggId * K / 2 + linkOffset + NHOST;
            ret->push_back(linkId);
        }
    }
    return ret;
}


void FatTreeTopology::failLink(int linkid) {
    if (_failedLinks[linkid]) {
        return;
    }
    pair<Queue *, Queue *> queues = linkToQueues(linkid);
    Queue *upLinkQueue = queues.first;
    Queue *downLinkQueue = queues.second;
    assert(!upLinkQueue->_disabled && !downLinkQueue->_disabled);

    _failedLinks[linkid] = true;
    upLinkQueue->_disabled = true;
    downLinkQueue->_disabled = true;
}

void FatTreeTopology::recoverLink(int linkid) {
    if (!_failedLinks[linkid]) {
        return;
    }
    pair<Queue *, Queue *> queues = linkToQueues(linkid);
    Queue *upLinkQueue = queues.first;
    Queue *downLinkQueue = queues.second;
    assert(upLinkQueue->_disabled && downLinkQueue->_disabled);

    _failedLinks[linkid] = false;
    upLinkQueue->_disabled = false;
    downLinkQueue->_disabled = false;
}

pair<route_t *, route_t *> FatTreeTopology::getStandardPath(int src, int dest) {
    //[WDM] note that for two-level routing, ack packets may go through a different path than the data packets
    // here we assume that ACK always goes exactly the same path of data packets, just in opposite direction
    route_t *path = get_path_2levelrt(src, dest);
    route_t *ackPath = getReversePath(src, dest, path);
    return make_pair(path, ackPath);
}

pair<route_t *, route_t *> FatTreeTopology::getEcmpPath(int src, int dest) {
    if (_net_paths[src][dest] == NULL) {
        _net_paths[src][dest] = get_paths_ecmp(src, dest);
    }
    vector<route_t *> *paths = _net_paths[src][dest];
    unsigned rnd = rand() % paths->size();
    route_t *path = paths->at(rnd);
    route_t *ackPath = getReversePath(src, dest, path);
    return make_pair(path, ackPath);
}


pair<route_t *, route_t *> FatTreeTopology::getReroutingPath(int src, int dest, route_t *currentPath) {
    if(GLOBAL_LOAD_BALANCING>0)
        return getLeastLoadedPath(src, dest);
    else
        return getOneWorkingPath(src,dest);
}


pair<route_t *, route_t *> FatTreeTopology::getLeastLoadedPath(int src, int dest) {
    if (_net_paths[src][dest] == NULL) {
        _net_paths[src][dest] = get_paths_ecmp(src, dest);
    }
    vector<route_t *> *paths = _net_paths[src][dest];
    int minLoad = -1;
    route_t* bestPath = nullptr;
    for (route_t *path: *paths) {
        int load = Topology::getPathLoad(path);
        if ((minLoad == -1 || load < minLoad) && isPathValid(path)) {
            bestPath = path;
            minLoad = load;
        }
    }
    route_t *ackPath = getReversePath(src, dest, bestPath);
    return make_pair(bestPath, ackPath);
}

pair<route_t*, route_t*> FatTreeTopology::getOneWorkingPath(int src, int dest) {
    if (_net_paths[src][dest] == NULL) {
        _net_paths[src][dest] = get_paths_ecmp(src, dest);
    }
    route_t* retPath = nullptr;
    vector<route_t *> *paths = _net_paths[src][dest];
    for (route_t * path: *paths) {
        if ( isPathValid(path)) {
            retPath = path;
            break;
        }
    }
    route_t *ackPath = getReversePath(src, dest, retPath);
    return make_pair(retPath, ackPath);
}



route_t *FatTreeTopology::getReversePath(int src, int dest, route_t *dataPath) {
    if(!isPathValid(dataPath))
        return nullptr;

    route_t *ackPath = new route_t();
    assert(dataPath->size() >= 2);
    for (int i = dataPath->size() - 2; i >= 0; i -= 2) {
        PacketSink *dualQueue = dataPath->at(i + 1)->getDual();
        PacketSink *dualPipe = dataPath->at(i)->getDual();
        ackPath->push_back(dualPipe);
        ackPath->push_back(dualQueue);
    }
    ackPath->insert(ackPath->begin(), HostTXQueues[dest]);
    return ackPath;
}

