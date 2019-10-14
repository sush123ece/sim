#ifndef FAT_TREE
#define FAT_TREE

#include "../../main.h"
#include "randomqueue.h"
#include "pipe.h"
#include "queue.h"
#include "loggers.h"
#include "network.h"
#include "topology.h"
#include "logfile.h"
#include "eventlist.h"
#include <ostream>
#include <iostream>
#include <vector>

#define FAIL_RATE 0.0

extern Pipe *pipes_nc_nup[NC][NK];
extern Pipe *pipes_nup_nlp[NK][NK];
extern Pipe *pipes_nlp_ns[NK][NSRV];
extern RandomQueue *queues_nc_nup[NC][NK];//[WDM] queue is on the second end of the link
extern RandomQueue *queues_nup_nlp[NK][NK];
// RandomQueue *queues_nlp_ns[NK][NSRV]; //handle host queues separately

extern Pipe *pipes_nup_nc[NK][NC];
extern Pipe *pipes_nlp_nup[NK][NK];
extern Pipe *pipes_ns_nlp[NSRV][NK];
extern RandomQueue *queues_nup_nc[NK][NC];
extern RandomQueue *queues_nlp_nup[NK][NK];
extern RandomQueue *queues_ns_nlp[NSRV][NK];
extern RandomQueue *HostRecvQueues[NSRV];
extern RandomQueue *HostTXQueues[NSRV];

//extern std::vector<std::vector<int>> vec_sw;
//extern std::vector<std::vector<int>> vec_pipe;

class FatTreeTopology : public Topology {
public:


    EventList *_eventlist;

    bool *_failedLinks;
    bool *_failedSwitches;
    int _numLinks;
    int _numSwitches;
    FatTreeTopology(EventList *ev);

    virtual void init_network();

    void count_queue(RandomQueue *);

    void print_path(std::ofstream &paths, int src, route_t *route);

    // Sushovan (next hop table for switch and pipe
    vector<vector<int>> make_next_hop_table_sw();
    vector<vector<int>> make_next_hop_table_pipe();

    // [WDM]


    int rand_host_sw(int sw);

    int nodePair_to_link(int a, int b);

    virtual int getServerUnderTor(int tor, int torNum);

    virtual void failLink(int linkid);

    virtual void failSwitch(int sid);

    virtual void recoverLink(int linkid);

    virtual void recoverSwitch(int sid);

    virtual pair<route_t *, route_t *> getReroutingPath(int src, int dest, route_t* currentPath= nullptr);

    virtual pair<route_t *, route_t *> getStandardPath(int src, int dest);

    virtual pair<route_t*, route_t*> getEcmpPath(int src, int dest);

    virtual vector<int> *get_neighbours(int src) { return NULL; };

    virtual pair<Queue *, Queue *> linkToQueues(int linkid);

    virtual bool isPathValid(route_t *path);

    map<RandomQueue *, int> _link_usage;
    virtual pair<route_t*, route_t*> getLeastLoadedPath(int src, int dest);
    virtual pair<route_t*, route_t*> getOneWorkingPath(int src, int dest);
    vector<route_t *> ***_net_paths;

    int find_lp_switch(RandomQueue *queue);

    int find_up_switch(RandomQueue *queue);

    int find_core_switch(RandomQueue *queue);

    int find_destination(RandomQueue *queue);

    vector<route_t *> *get_paths_ecmp(int src, int dest);

    route_t *get_path_2levelrt(int src, int dest);

    route_t* getReversePath(int src, int dest, route_t*dataPath);

    virtual vector<int>* getLinksFromSwitch(int sid);
};

#endif
