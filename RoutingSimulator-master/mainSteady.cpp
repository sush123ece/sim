//
// Created by Dingming Wu on 11/29/17.
//
#include <sstream>
#include <strstream>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <list>
#include <vector>
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
#include "AspenTree.h"
#include "main.h"


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
            (*cct)[cid] = INT32_MAX;
        else {
            double duration = it.second->_duration_ms;
            if (cct->count(cid) == 0 || cct->at(cid) < duration)
                (*cct)[cid] = duration;
        }
    }
    for (int dc:*deadCoflows){
        if(cct->count(dc) == 0){
            (*cct)[dc] = INT32_MAX;
        }
    }
    return cct;
}

string getCCTFileName(int topology, int routing, int pos, int linkNum, int switchNum, string trace, int trial){
    stringstream file;
    file<<"top"<<topology<<"rt"<<routing<<"_"<<GLOBAL_LOAD_BALANCING
        <<"pos"<<pos<<"l"<<linkNum<<"s"<<switchNum<<"_"<<trace<<"_"<<trial<<".cct.csv";
    return file.str();
}

string getFCTFileName(int topology, int routing, int pos, int linkNum, int switchNum, string trace, int trial){
    stringstream file;
    file<<"top"<<topology<<"rt"<<routing<<"_"<<GLOBAL_LOAD_BALANCING
        <<"pos"<<pos<<"l"<<linkNum<<"s"<<switchNum<<"_"<<trace<<"_"<<trial<<".fct";
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
    std::srand (time(0));
    eventlist.setEndtime(timeFromSec(3000.01));

    int failedLinks[]={};
    int failedSwitches[]{};
    double totalTrafficBytes = 0;
    double lastArrivalTime =0;
    int totalFlows = 0;

    set<int>* impactedCoflow = new set<int>();
    set<int>* impactedFlow = new set<int>();
    set<int>* deadCoflow = new set<int>();
    set<int>* deadFlow = new set<int>();
    set<int>* secondImpactedFlow = new set<int>();
    set<int>* secondImpactedCoflow = new set<int>();
    set<int>* totalImpactedFlow = new set<int>();
    set<int>* totalImpactedCoflow = new set<int>();

    //initializing vec_sw

    // int _numSwitches = NK + NK + NK / 2;
    int row = NK + NK + NK / 2;
    //vector <vector<int>> vec_sw(row);
    int col;

    // size of next hop for each switch
  //  int colom1;
    colom1 = (int *) malloc(row*sizeof(int));
    for (int i = 0; i < row; i++) {
        if (i == 0)
        {
            colom1[i] = 1;
            continue;
        }

        if (i == 1)
        {
            colom1[i] = 1;
            continue;
        }

        if (i == 2)
        {
            colom1[i] = 1;
            continue;
        }

        if (i == K * K / 2)
        {
            colom1[i] = 2;
            continue;
        }

        else
            colom1[i] = 0;
    }

    cout << "switch 0 next hop " << colom1[0] << endl;

/*
    for (int i = 0; i < row; i++) {


        // size of column

        col = colom1[i];

        // declare the i-th row to size of column
      //  vec_sw[i] = vector<int>[col];
      vec_sw[i] = (int*)malloc(col*sizeof(int));

        if (i == 0)
            vec_sw[i] = {0}; // for ToR switch 0, net hop is pipe 0
        if (i == K * K / 2)
            vec_sw[i] = {K/2,K}; // for Aggr switch K^2/2, next hops are pipe K/2 and K


    } */

   // cout << "number of next hop of switch id 0 is " << vec_sw[0].size() << endl;
   // int sush = vec_sw[K*K/2][0];

    // end

    // initializing vec_pipe


    // int _numLinks = K * K * K / 2 + NHOST; //[WDM] should include host links
    //  int _numSwitches = NK + NK + NK / 2;
    row = K * K * K / 2 + NHOST;
  //  vector <vector<int>> vec_pipe(row);

    // size of next hop for each switch
  //  int *colom2;
    colom2 = (int *) malloc(row*sizeof(int));
    for (int i = 0; i < row; i++) {

        if (i == 0 || i == K/2 || i == K)
            colom2[i] = 1;
        else
            colom2[i] = 0;

    }


  /*  for (int i = 0; i < row; i++) {
        col = colom2[i];

        // declare the i-th row to size of column
        vec_pipe[i] = vector<int>(col);

        if (i == 0)
            vec_pipe[i] = {K^K/2}; // for ToR switch 0, net hop is pipe 0
        if (i == K/2)
            vec_pipe[i] = {1}; // for Aggr switch K^2/2, next hops are pipe K/2 and K
        if (i == K)
            vec_pipe[i] = {2};


    } */
    cout << "table created" << endl;


    //end vec_pipe

    double simStartingTime_ms = -1;
    int isDdlFlow = 0;
    int routing = 0; //[WDM] 0 stands for ecmp; 1 stands for two-level routing
    int topology = 0; // 0 stands for fattree; 1 stands for sharebackup; 2 stands for f10
    int failureLocation = 0;// 0 edge, 1 aggregation, 2 core

    int failedLinkNum = 0;
    int failedSwitchNum = 0;
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

        if (argc > i && !strcmp(argv[i], "-linkNum")) {
            failedLinkNum = atoi(argv[i + 1]);
            i += 2;
        }

        if (argc > i && !strcmp(argv[i], "-switchNum")) {
            failedSwitchNum = atoi(argv[i + 1]);
            i += 2;
        }

        if (argc > i && !strcmp(argv[i], "-failurePos")) {
             failureLocation= atoi(argv[i + 1]);
            i += 2;
        }

        if (argc > i && !strcmp(argv[i], "-isddlflow")) {
            isDdlFlow = atoi(argv[i + 1]);
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

   // vector<vector<int>> vec_sw;
   // vector<vector<int>> vec_pipe;




    cout<<"Topology: "<<topology<<" routing: "<<routing<<" failurePos: "
        <<failureLocation<<" trafficLevel: "<<trafficLevel<<" trial: "<<trial<<" K: "<<K<<endl;

    string traceName = traf_file_name.substr(traf_file_name.rfind("/")+1);

    string cctLogFilename
            =getCCTFileName(topology, routing, failureLocation, failedLinkNum,failedSwitchNum,traceName, trial);

    string fctLogFilename
            = getFCTFileName(topology, routing, failureLocation, failedLinkNum,failedSwitchNum,traceName, trial);

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

    MultipleSteadyFailures* multipleSteadyLinkFailures= nullptr;
    if (topology ==0) {
        top = new FatTreeTopology(&eventlist);

    }else if(topology==1){
        top = new FatTreeTopology(&eventlist);
    }
    else if(topology==2) {
        top = new F10Topology(&eventlist);

    }else{
        top = new AspenTree(&eventlist);

    }
    multipleSteadyLinkFailures = new MultipleSteadyFailures(&eventlist,top);

    if(topology==1){
        multipleSteadyLinkFailures->_useShareBackup=true;
    }

    multipleSteadyLinkFailures->setRandomLinkFailures(failedLinkNum, failureLocation);
    multipleSteadyLinkFailures->setRandomSwitchFailure(failedSwitchNum, failureLocation);
    for(int i:failedLinks){
        multipleSteadyLinkFailures->setSingleLinkFailure(i);
    }
    for(int i:failedSwitches){
        multipleSteadyLinkFailures->setSingleSwitchFailure(i);
    }
    multipleSteadyLinkFailures->installFailures();
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
    srand (0);
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
            if(isDdlFlow>0) {
                totalTrafficBytes += atof(sub.c_str())*1000*2;
                volume_bytes = (uint64_t) (atof(sub.c_str()) / mappers.size() * 1000*2);
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
                tcpSrc->installTcp(top, nullptr, routing);
                tcpRtxScanner.registerTcp(*tcpSrc);
                flowId++;

            }
        }
    }


    cout<<"TotalTrafficKB:"<<totalTrafficBytes/1000<<" TotalTime:"<<lastArrivalTime
        <<" load:"<<Topology::getLoad(lastArrivalTime, totalTrafficBytes/1000)<<endl;

    tcpRtxScanner.StartFrom(timeFromMs(simStartingTime_ms));


    // Multicast objects creation

    // cbr src and dest object creation
    int num_dest = 2;
    int Mulsrc = 0; // src host possibly
    // CBR source and sink
    CbrSrc *cbrSrc;
   // CbrSink *cbrSink;

    linkspeed_bps rate = 10;
    simtime_picosec active= 800000;
   // simtime_picosec end_active = 800000;
    simtime_picosec mulStart = 0;
    simtime_picosec idle= 0;
    cbrSrc = new CbrSrc(Mulsrc,eventlist,rate,active,idle);
    // need a starting point

    for (int i=0;i<1;i++)
    {
        cbrSrc->installCbr(mulStart);
    }



    // GO!
    while (eventlist.doNextEvent()) {

    }

    // logging

    std::ofstream cctLogFile(cctLogFilename.c_str());
    std::ofstream fctLogFile(fctLogFilename.c_str());
    assert(cctLogFile.is_open());
    assert(fctLogFile.is_open());

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

    multipleSteadyLinkFailures->printFailures();

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
