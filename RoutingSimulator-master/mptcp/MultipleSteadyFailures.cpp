//
// Created by Dingming Wu on 11/28/17.
//

#include <set>
#include <iostream>
#include "MultipleSteadyFailures.h"


MultipleSteadyFailures::MultipleSteadyFailures(EventList *ev, Topology *topo) : _ev(ev), _topo(topo) {
    _allLinks = new vector<int>();
    _allSwitches = new vector<int>();
    _inNetworkLinks = new vector<int>();
    _inNetworkSwitches = new vector<int>();
    _outstandingFailedLinks = new set<int>();
    _outstandingFailedSwitches = new set<int>();
    _edgeLinks = new vector<int>();
    _edgeSwitches = new vector<int>();
    _aggLinks = new vector<int>();
    _aggSwitches = new vector<int>();
    _coreLinks = new vector<int>();
    _coreSwitches = new vector<int>();

    _totalLinks = NHOST + NK * K / 2 + NK * K / 2;
    _totalSwitches = NK + NK + NK / 2;

    for (int i = 0; i < _totalLinks; i++) {
        _allLinks->push_back(i);
        if (i < NHOST)
            _edgeLinks->push_back(i);
        else if (NHOST <= i && i < NHOST + NK * K / 2)
            _aggLinks->push_back(i);
        else
            _coreLinks->push_back(i);

        if (i >= NHOST)
            _inNetworkLinks->push_back(i);
    }

    for (int i = 0; i < _totalSwitches; i++) {
        _allSwitches->push_back(i);
        if (i < NK)
            _edgeSwitches->push_back(i);
        else if (NK <= i && i < NK * 2)
            _aggSwitches->push_back(i);
        else
            _coreSwitches->push_back(i);

        if (i >= NK)
            _inNetworkSwitches->push_back(i);
    }

    _givenFailedLinks = new set<int>();
    _givenFailedSwitches = new set<int>();
}


void MultipleSteadyFailures::setSingleSwitchFailure(int switchId) {
    _givenFailedSwitches->insert(switchId);
}

void MultipleSteadyFailures::setRandomSwitchFailure(int num, int pos) {
    srand(1);
    if (pos < 0) {
        std::random_shuffle(_allSwitches->begin(), _allSwitches->end(), Topology::myrandom);
        for (int i = 0; i < num; i++) {
            int sid = _allSwitches->at(i);
            _givenFailedSwitches->insert(sid);
        }
    } else if (pos == 0) {
        std::random_shuffle(_edgeSwitches->begin(), _edgeSwitches->end(), Topology::myrandom);
        for (int i = 0; i < num; i++) {
            int sid = _edgeSwitches->at(i);
            _givenFailedSwitches->insert(sid);
        }
    } else if (pos == 1) {
        std::random_shuffle(_aggSwitches->begin(), _aggSwitches->end(), Topology::myrandom);
        for (int i = 0; i < num; i++) {
            int sid = _aggSwitches->at(i);
            _givenFailedSwitches->insert(sid);
        }
    } else if (pos == 2) {
        std::random_shuffle(_coreSwitches->begin(), _coreSwitches->end(), Topology::myrandom);
        for (int i = 0; i < num; i++) {
            int sid = _coreSwitches->at(i);
            _givenFailedSwitches->insert(sid);
        }
    } else {
        std::random_shuffle(_inNetworkSwitches->begin(), _inNetworkSwitches->end(), Topology::myrandom);
        for (int i = 0; i < num; i++) {
            int sid = _inNetworkSwitches->at(i);
            _givenFailedSwitches->insert(sid);
        }
    }
}

void MultipleSteadyFailures::setRandomSwitchFailure(double ratio, int pos) {
    int numSwitches = (int) (ratio * _totalSwitches);
    setRandomSwitchFailure(numSwitches, pos);
}

void MultipleSteadyFailures::updateBackupUsage() {
    multiset<int> *groupFailureCount = new multiset<int>();
    set<int>* mappedSwitches = new set<int>();
    int gid = -1;
    for (int sid:*_givenFailedSwitches) {
        mappedSwitches->insert(sid);
        if (sid < 2 * NK)
            gid = sid / (K / 2);
        else
            gid = 2 * K + (sid - 2 * NK) % (K/2);
        if (groupFailureCount->count(gid) < BACKUPS_PER_GROUP)
            groupFailureCount->insert(gid);
        else
            _outstandingFailedSwitches->insert(sid);
    }
    for (int link:*_givenFailedLinks) {
        pair<Queue *, Queue *> ret = _topo->linkToQueues(link);
        int sid1 = ret.second->_switchId;
        int sid2 = ret.first->_switchId;
        assert(sid1 <= sid2);
        if (sid1 < 0) { //host link failure
            if(mappedSwitches->count(sid2)>0)
                continue;
            mappedSwitches->insert(sid2);
            int gid2 = sid2 / (K / 2);
            if (groupFailureCount->count(gid2) < BACKUPS_PER_GROUP)
                groupFailureCount->insert(gid2);
            else
                _outstandingFailedLinks->insert(link);


        } else if (sid1 < NK) {
            //lp to up link failure
            assert(sid2 >= NK && sid2 < 2 * NK);
            if (mappedSwitches->count(sid1) > 0 && mappedSwitches->count(sid2) > 0)
                continue;
            else if (mappedSwitches->count(sid1) == 0) {
                mappedSwitches->insert(sid1);
                int gid1 = sid1 / (K / 2);
                if (groupFailureCount->count(gid1) < BACKUPS_PER_GROUP)
                    groupFailureCount->insert(gid1);
                else
                    _outstandingFailedLinks->insert(link);
            } else {
                mappedSwitches->insert(sid2);
                int gid2 = sid2 / (K / 2);
                if (groupFailureCount->count(gid2) < BACKUPS_PER_GROUP)
                    groupFailureCount->insert(gid2);
                else
                    _outstandingFailedLinks->insert(link);
            }

        } else {
            // up to core link failure
            assert(sid1 < 2 * NK && sid2 >= 2 * NK);

            if (mappedSwitches->count(sid1) > 0 && mappedSwitches->count(sid2) > 0)
                continue;
            else if (mappedSwitches->count(sid1) == 0) {
                mappedSwitches->insert((sid1));
                int gid1 = sid1 / (K / 2);
                if (groupFailureCount->count(gid1) < BACKUPS_PER_GROUP)
                    groupFailureCount->insert(gid1);
                else
                    _outstandingFailedLinks->insert(link);
            } else {
                mappedSwitches->insert(sid2);
                int gid2 = 2 * K + (sid2 - 2 * NK) % (K / 2);
                if (groupFailureCount->count(gid2) < BACKUPS_PER_GROUP)
                    groupFailureCount->insert(gid2);
                else
                    _outstandingFailedLinks->insert(link);
            }
        }
    }
}

void MultipleSteadyFailures::setRandomLinkFailures(int num, int pos) {
    srand(1);
    if (pos < 0) {
        std::random_shuffle(_allLinks->begin(), _allLinks->end(), Topology::myrandom);
        for (int i = 0; i < num; i++) {
            int linkid = _allLinks->at(i);
            _givenFailedLinks->insert(linkid);
        }
    } else if (pos == 0) {
        std::random_shuffle(_edgeLinks->begin(), _edgeLinks->end(), Topology::myrandom);
        for (int i = 0; i < num; i++) {
            int linkid = _edgeLinks->at(i);
            _givenFailedLinks->insert(linkid);
        }
    } else if (pos == 1) {
        std::random_shuffle(_aggLinks->begin(), _aggLinks->end(), Topology::myrandom);
        for (int i = 0; i < num; i++) {
            int linkid = _aggLinks->at(i);
            _givenFailedLinks->insert(linkid);
        }
    } else if (pos == 2) {
        std::random_shuffle(_coreLinks->begin(), _coreLinks->end(),Topology::myrandom);
        for (int i = 0; i < num; i++) {
            int linkid = _coreLinks->at(i);
            _givenFailedLinks->insert(linkid);
        }
    } else {
        std::random_shuffle(_inNetworkLinks->begin(), _inNetworkLinks->end(), Topology::myrandom);
        for (int i = 0; i < num; i++) {
            int linkid = _inNetworkLinks->at(i);
            _givenFailedLinks->insert(linkid);
        }
    }
}

void MultipleSteadyFailures::setRandomLinkFailures(double ratio, int pos) {
    int numLinks = (int) (ratio * _totalLinks);
    setRandomLinkFailures(numLinks, pos);
}

void MultipleSteadyFailures::setSingleLinkFailure(int linkid) {
    _givenFailedLinks->insert(linkid);
}

void MultipleSteadyFailures::installFailures() {
    set<int> *actualFailedLinks, *actualFailedSwitches;
    if (_useShareBackup) {
        updateBackupUsage();
        actualFailedLinks = _outstandingFailedLinks;
        actualFailedSwitches = _outstandingFailedSwitches;
    } else {
        actualFailedLinks = _givenFailedLinks;
        actualFailedSwitches = _givenFailedSwitches;
    }

    for (int sid: *actualFailedSwitches) {
        _topo->failSwitch(sid);
    }
    for (int link: *actualFailedLinks) {
        _topo->failLink(link);
    }
}

void MultipleSteadyFailures::printFailures() {
    cout <<"GivenFailedSwitchNum: "<<_givenFailedSwitches->size()<<" GivenFailedSwitches: ";
    for (int sid:*_givenFailedSwitches) {
        cout << sid << " ";
    }
    cout<<endl;
    cout <<"GivenFailedLinkNum: "<<_givenFailedLinks->size()<< " GivenFailedLinks: ";
    for (int link:*_givenFailedLinks) {
        cout << link << " ";
    }
    cout << endl;
    if (_useShareBackup) {
        cout <<"ActualFailedSwitchNum: "<<_outstandingFailedSwitches->size()<< " ActualFailedSwitches: ";
        for (int sid:*_outstandingFailedSwitches) {
            cout << sid << " ";
        }
        cout<<endl;
        cout <<"ActualFailedLinkNum: "<<_outstandingFailedLinks->size()<< " ActualFailedLinks: ";
        for (int link:*_outstandingFailedLinks) {
            cout << link << " ";
        }
        cout << endl;
    }
}

