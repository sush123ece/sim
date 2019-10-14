//
// Created by Dingming Wu on 11/29/17.
//
#include <sstream>
#include <strstream>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <list>
#include <math.h>
#include <set>
#include <MultipleSteadyFailures.h>
#include "SingleDynamicFailureEvent.h"
#include "network.h"
#include "randomqueue.h"
#include "pipe.h"
#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "clock.h"
#include "mtcp.h"
#include "tcp.h"
#include "tcp_transfer.h"
#include "cbr.h"
#include "topology.h"
#include "config.h"
#include "topology.h"
#include "fat_tree_topology.h"
#include "F10.h"
#include "main.h"
#include "AspenTree.h"

// TOPOLOGY TO USE
#if CHOSEN_TOPO == FAT


#elif CHOSEN_TOPO == RRG
#include "rand_regular_topology.h"
#endif


#define USE_FIRST_FIT 0
#define FIRST_FIT_INTERVAL 100


// Simulation params
#define PRINT_PATHS 0
#define PERIODIC 0

int N = NSW;


map<int, double>* getCoflowStats(map<int,FlowConnection*>* flowStats, set<int>* deadCoflows){
    map<int, double>* cct = new map<int, double>();
    for(pair<int,FlowConnection*> it: *flowStats){
        int cid = it.second->_coflowId;
        if(deadCoflows->count(cid)>0)
            (*cct)[cid] = -1;
        else {
            double duration = it.second->_duration_ms;
            if (cct->count(cid) == 0 || cct->at(cid) < duration)
                (*cct)[cid] = duration;
        }
    }
    for (int dc:*deadCoflows){
        if(cct->count(dc) == 0){
            (*cct)[dc] = -1;
        }
    }
    return cct;
}

string getCCTFileName(int topology, int routing, int failedLinkId, int nodeId, string trace, int trial){
    stringstream file;
    file<<"top"<<topology<<"rt"<<routing<<"_"<<GLOBAL_LOAD_BALANCING
        <<"linkId"<<failedLinkId<<"nodeId"<<nodeId<<"_"<<trace<<"_"<<trial<<".cct.csv";
    return file.str();
}

string getFCTFileName(int topology, int routing, int failedLinkId, int nodeId, string trace, int trial){
    stringstream file;
    file<<"top"<<topology<<"rt"<<routing<<"_"<<GLOBAL_LOAD_BALANCING
        <<"linkId"<<failedLinkId<<"nodeId"<<nodeId<<"_"<<trace<<"_"<<trial<<".fct.csv";
    return file.str();
}

EventList eventlist;

void fileNotFoundError(string fn){
    cout << "cannot find trace file:" << fn << endl;
    char buffer[256];
    char *answer = getcwd(buffer, sizeof(buffer));
    string s_cwd;
    if (answer) {
        s_cwd = answer;
        cout << "current directory:" << s_cwd << endl;
        exit(1);
    }
}


int main(int argc, char **argv) {
    clock_t begin = clock();
    std::srand (0);

    int totalFlows = 0;
    double failureStartingTimeMs = 0.3;
    double failureDurationSec = 100;

    double totalTrafficBytes = 0;
    double lastArrivalTime =0;

    set<int>* impactedCoflow = new set<int>();
    set<int>* impactedFlow = new set<int>();
    set<int>* deadCoflow = new set<int>();
    set<int>* deadFlow = new set<int>();
    set<int>* secondImpactedFlow = new set<int>();
    set<int>* secondImpactedCoflow = new set<int>();
    set<int>* totalImpactedFlow = new set<int>();
    set<int>* totalImpactedCoflow = new set<int>();

    double simStartingTime_ms = -1;

    int routing = 0; //[WDM] 0 stands for ecmp; 1 stands for two-level routing
    int topology = 0; // 0 stands for fattree; 1 stands for sharebackup; 2 stands for f10
    int isDdlflow = 0;
    int failedLinkId = -1, failedNodeId = -1;
    int trafficLevel = 0; //0 stands for server level; 1 stands for rack level
    int trial = 0;
    string traf_file_name;
    if (argc > 1) {
        int i = 1;
        if (argc > i && !strcmp(argv[i], "-topo")) {
            topology = atoi(argv[i + 1]);
            i += 2;
        }
        if (argc > i && !strcmp(argv[i], "-routing")) {
            routing = atoi(argv[i + 1]);
            i += 2;
        }

        if (argc > i && !strcmp(argv[i], "-linkId")) {
            failedLinkId = atoi(argv[i + 1]);
            i += 2;
        }


        if (argc > i && !strcmp(argv[i], "-nodeId")) {
            failedNodeId = atoi(argv[i + 1]);
            i += 2;
        }

        if (argc > i && !strcmp(argv[i], "-isddlflow")) {
            isDdlflow = atoi(argv[i + 1]);
            i += 2;
        }

        if (argc > i && !strcmp(argv[i], "-trafficLevel")) {
            trafficLevel = atoi(argv[i + 1]);
            i += 2;
        }

        if (argc > i && !strcmp(argv[i], "-trial")) {
            trial= atoi(argv[i + 1]);
            i += 2;
        }

        traf_file_name = argv[i];
    } else {
        cout << "wrong arguments!" << endl;
        exit(1);
    }

    cout<<"Topology:"<<topology<<" routing:"<<routing<<" linkId:"
        <<failedLinkId<< " nodeId:"<<failedNodeId<<" isDeadline:"<<isDdlflow<<" trafficLevel: "<<trafficLevel<<" trial: "<<trial<<" K: "<<K<<endl;

    string traceName = traf_file_name.substr(traf_file_name.rfind("/")+1);

    string cctLogFilename
            =getCCTFileName(topology, routing, failedLinkId, failedNodeId, traceName, trial);

    string fctLogFilename
            = getFCTFileName(topology, routing, failedLinkId, failedNodeId, traceName, trial);

#if PRINT_PATHS
    filename << "logs.paths";
    std::ofstream pathFile(filename.str().c_str());
    if (!pathFile) {
        cout << "Can't open for writing paths file!" << endl;
        exit(1);
    }
#endif

    TcpSrc *tcpSrc;
    TcpSink *tcpSnk;

    TcpRtxTimerScanner tcpRtxScanner(timeFromMs(TCP_TIMEOUT_SCANNER_PERIOD), eventlist); //irregular tcp retransmit timeout
    Topology *top = NULL;

    SingleDynamicFailureEvent *linkFailureEvent;
    if (topology ==0 ) {
        top = new FatTreeTopology(&eventlist);
        linkFailureEvent = new SingleDynamicFailureEvent(eventlist, top,
                                                         timeFromMs(failureStartingTimeMs),
                                                         timeFromSec(failureDurationSec),
                                                         failedLinkId, failedNodeId);
        linkFailureEvent->setFailureRecoveryDelay(timeFromMs(GLOBAL_REROUTE_DELAY),
                                                  timeFromMs(GLOBAL_REROUTE_DELAY));
    }
    else if (topology == 1) {
        top = new FatTreeTopology(&eventlist);
        linkFailureEvent = new SingleDynamicFailureEvent(eventlist, top,
                                                         timeFromMs(failureStartingTimeMs),
                                                         timeFromSec(failureDurationSec),
                                                         failedLinkId, failedNodeId);
        linkFailureEvent->UsingShareBackup = true;
        vector<int> *lpBackupUsageTracker = new vector<int>(K,BACKUPS_PER_GROUP);
        vector<int> *upBackupUsageTracker = new vector<int>(K,BACKUPS_PER_GROUP);
        vector<int> *coreBackupUsageTracker = new vector<int>(K/2, BACKUPS_PER_GROUP);
        linkFailureEvent->setFailureRecoveryDelay(timeFromMs(CIRCUIT_SWITCHING_DELAY), 0);
        linkFailureEvent->setBackupUsageTracker(lpBackupUsageTracker, upBackupUsageTracker,
                                                coreBackupUsageTracker);
    }
    else if(topology == 2){
        top = new F10Topology(&eventlist);
        linkFailureEvent = new SingleDynamicFailureEvent(eventlist, top,
                                                         timeFromMs(failureStartingTimeMs),
                                                         timeFromSec(failureDurationSec),
                                                         failedLinkId, failedNodeId);
        linkFailureEvent->setFailureRecoveryDelay(timeFromMs(LOCAL_REROUTE_DELAY), timeFromMs(LOCAL_REROUTE_DELAY));
    }else{
        top = new AspenTree(&eventlist);
        linkFailureEvent = new SingleDynamicFailureEvent(eventlist, top,
                                                         timeFromMs(failureStartingTimeMs),
                                                         timeFromSec(failureDurationSec),
                                                         failedLinkId, failedNodeId);
        linkFailureEvent->setFailureRecoveryDelay(timeFromMs(ASPEN_TREE_DELAY), timeFromMs(ASPEN_TREE_DELAY));
    }

    linkFailureEvent->installEvent();

    int flowId = 0;
    string line;
    ifstream traf_file(traf_file_name.c_str());
    if (!traf_file.is_open()) {
        fileNotFoundError(traf_file_name.c_str());
    }
    getline(traf_file, line);
    int pos = line.find(" ");
    string str = line.substr(0, pos);
    int rack_n = atoi(str.c_str());
    int coflowNum = atoi(line.substr(pos + 1).c_str());

    vector<uint64_t *> flows_sent;
    vector<uint64_t *> flows_recv;
    vector<bool*> flows_finish;
    map<int,FlowConnection*>* finishedFlowStats = new map<int, FlowConnection*>();

    int src, dest;
    srand(0);
    while (getline(traf_file, line)){
        int i = 0;
        int pos = line.find(" ");
        int coflowId = atoi(line.substr(0, pos).c_str());
        i = ++pos;
        pos = line.find(" ", pos);
        string str = line.substr(i, pos - i);
        double arrivalTime_ms = atof(str.c_str());
        if (arrivalTime_ms < simStartingTime_ms || simStartingTime_ms < 0) {
            simStartingTime_ms = arrivalTime_ms;
        }
        i = ++pos;
        pos = line.find(" ", pos);
        str = line.substr(i, pos - i);
        int mapper_n = atoi(str.c_str());
        vector<int> mappers;
        for (int k = 0; k < mapper_n; k++) {
            i = ++pos;
            pos = line.find(" ", pos);
            str = line.substr(i, pos - i);
            if (trafficLevel == 0) {
                mappers.push_back(atoi(str.c_str()));
            } else {
                mappers.push_back(top->getServerUnderTor(atoi(str.c_str()), rack_n));
            }
        }
        i = ++pos;
        pos = line.find(" ", pos);
        str = line.substr(i, pos - i);
        int reducer_n = atoi(str.c_str());

        totalFlows += mapper_n*reducer_n;

        for (int k = 0; k < reducer_n; k++) {
            i = ++pos;
            pos = line.find(" ", pos);
            if (pos == string::npos)
                str = line.substr(i, line.length());
            else
                str = line.substr(i, pos - i);
            int posi = str.find(":");
            string sub = str.substr(0, posi);
            int reducer = atoi(sub.c_str());
            sub = str.substr(posi + 1, str.length());
            uint64_t volume_bytes;
            if(isDdlflow>0) {
                totalTrafficBytes += atof(sub.c_str())*1000;
                volume_bytes = (uint64_t) (atof(sub.c_str()) / mappers.size() * 1000);
            }else{
                totalTrafficBytes += atof(sub.c_str())*1000*1000;
                volume_bytes = (uint64_t) (atof(sub.c_str()) / mappers.size() * 1000000);
            }
            if (trafficLevel == 0) {
                dest = reducer;
            } else {
                dest = top->getServerUnderTor(reducer, rack_n);
            }
            for (int t = 0; t < mappers.size(); t++) {
                src = mappers[t];

                tcpSnk = new TcpSink(&eventlist);
                tcpSrc = new TcpSrc(tcpSnk, src, dest, eventlist, volume_bytes, arrivalTime_ms, flowId, coflowId);
                tcpSnk->set_super_id(flowId);

                lastArrivalTime = arrivalTime_ms>lastArrivalTime?arrivalTime_ms:lastArrivalTime;

                tcpSrc->setFlowCounter(impactedFlow,impactedCoflow,secondImpactedFlow,secondImpactedCoflow,deadFlow,deadCoflow,finishedFlowStats);
                tcpSrc->installTcp(top,linkFailureEvent,routing);
                tcpRtxScanner.registerTcp(*tcpSrc);
                flowId++;
            }
        }
    }

    cout<<"TotalTrafficKB:"<<totalTrafficBytes/1000<<" TotalTime:"<<lastArrivalTime
        <<" load:"<<Topology::getLoad(lastArrivalTime, totalTrafficBytes/1000)<<endl;
    cout<<"TotalFlows:"<<totalFlows<<endl;

    tcpRtxScanner.StartFrom(timeFromMs(simStartingTime_ms));
    // GO!
    while (eventlist.doNextEvent()) {

    }
    std::ofstream cctLogFile(cctLogFilename.c_str());
    std::ofstream fctLogFile(fctLogFilename.c_str());

    for (pair<int,FlowConnection*>it: *finishedFlowStats) {
        fctLogFile<<it.first<<","<<it.second->_src<<","<<it.second->_dest<<","
                  <<it.second->_duration_ms<<","<<it.second->_flowSize_Bytes <<endl;
    }

    for( int superId: *deadFlow){
        fctLogFile<<superId<<" "<<INT32_MAX<<endl;
    }

    map<int, double>* cct = getCoflowStats(finishedFlowStats,deadCoflow);
    for(pair<int,double> it:*cct){
        cctLogFile<<it.first<<","<<it.second<<endl;
    }
//
    fctLogFile.flush();
    fctLogFile.close();
    cctLogFile.flush();
    cctLogFile.close();

    //tally total impact
    totalImpactedFlow->insert(impactedFlow->begin(),impactedFlow->end());
    totalImpactedFlow->insert(secondImpactedFlow->begin(),secondImpactedFlow->end());
    totalImpactedFlow->insert(deadFlow->begin(),deadFlow->end());

    totalImpactedCoflow->insert(impactedCoflow->begin(),impactedCoflow->end());
    totalImpactedCoflow->insert(secondImpactedCoflow->begin(),secondImpactedCoflow->end());
    totalImpactedCoflow->insert(deadCoflow->begin(),deadCoflow->end());

    cout<<"FinishedFlows: " << finishedFlowStats->size() << " AllFlows: " << totalFlows << endl;
    cout<<"FinishedCoflows: "<<cct->size()<<" AllCoflows: "<<coflowNum<<endl;
    //cout<<"AverageFlowThroughput: "<<frateAccm/totalFlows<<" AverageCCT: "<<cctSum/cct->size()<<endl;
    cout<<"ImpactedFlows: "<<impactedFlow->size()<<" ImpactedCoflows: "<<impactedCoflow->size();
    cout<<" TotalImpactedFlows: "<<totalImpactedFlow->size()
        <<" TotalImpactedCoFlows: "<<totalImpactedCoflow->size();
    cout<<" DeadFlows: "<<deadFlow->size()<<" DeadCoflows: "<<deadCoflow->size()<<endl;
    double elapsed_secs = double(clock() - begin) / CLOCKS_PER_SEC;
    cout<<"Elapsed:" << elapsed_secs << "s" << endl;
}
