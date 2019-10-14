//
// Created by Dingming Wu on 11/28/17.
//

#ifndef SIM_MULTIPLESTEADYLINKFAILURES_H
#define SIM_MULTIPLESTEADYLINKFAILURES_H


#include "eventlist.h"
#include "topology.h"
#include <algorithm>    // std::random_shuffle
#include <ctime>        // std::time
#include <cstdlib>      // std::rand, std::srand
#include "../main.h"

class MultipleSteadyFailures{
public:
    MultipleSteadyFailures(EventList*ev,Topology*topo);
    EventList*_ev;
    set<int>* _givenFailedLinks;
    set<int>* _givenFailedSwitches;
    vector<int>* _allLinks;
    vector<int>* _allSwitches;
    vector<int>* _inNetworkLinks;
    vector<int>* _inNetworkSwitches;
    vector<int>* _edgeLinks;
    vector<int>* _edgeSwitches;
    vector<int>* _aggLinks;
    vector<int>* _aggSwitches;
    vector<int>* _coreLinks;
    vector<int>* _coreSwitches;

    int _totalLinks;
    int _totalSwitches;
    void setSingleLinkFailure(int linkid);
    void setRandomLinkFailures(int num, int pos);
    void setSingleSwitchFailure(int switchId);
    void setRandomSwitchFailure(int num, int pos);
    void setRandomSwitchFailure(double ratio, int pos);
    void setRandomLinkFailures(double ratio, int pos);
    Topology*_topo;
    bool isPathOverlapping(route_t*);
    set<int>* _outstandingFailedLinks;
    set<int>* _outstandingFailedSwitches;
    void installFailures();
    bool _useShareBackup =false;
    void updateBackupUsage();
    void printFailures();
};


#endif //SIM_MULTIPLESTEADYLINKFAILURES_H
