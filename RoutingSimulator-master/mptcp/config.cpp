#include <math.h>
#include <sstream>
#include "config.h"
#include "tcppacket.h"

double drand() {
    int r = rand();
    int m = RAND_MAX;
    double d = (double) r / (double) m;
    return d;
}

int pareto(int xm, int mean) {
    double oneoveralpha = ((double) mean - xm) / mean;

    return (int) ((double) xm / pow(drand(), oneoveralpha));
}

double exponential(double lambda) {
    return -log(drand()) / lambda;
}

simtime_picosec timeFromSec(double secs) {
    simtime_picosec psecs = (simtime_picosec) (secs * 1000000000000.0);
    return psecs;
}

simtime_picosec timeFromMs(double msecs) {
    simtime_picosec psecs = (simtime_picosec) (msecs * 1000000000);
    return psecs;
}

simtime_picosec timeFromMs(int msecs) {
    simtime_picosec psecs = (simtime_picosec) (msecs) * 1000000000;
    return psecs;
}

simtime_picosec timeFromUs(double usecs) {
    simtime_picosec psecs = (simtime_picosec) (usecs * 1000000);
    return psecs;
}

double timeAsMs(simtime_picosec ps) {
    double ms_ = (double) (ps / 1000000000.0);
    return ms_;
}

double timeAsUs(simtime_picosec ps) {
    double us_ = (double) (ps / 1000000.0);
    return us_;
}

double timeAsSec(simtime_picosec ps) {
    double s_ = (double) ps / 1000000000000.0;
    return s_;
}

mem_b memFromPkt(double pkts) {
    mem_b m = (mem_b) (ceil(pkts * TcpPacket::DEFAULTDATASIZE));
    return m;
}

linkspeed_bps speedFromMbps(uint64_t Mbitps) {
    uint64_t bps;
    bps = Mbitps * 1000000;
    return bps;
}

linkspeed_bps speedFromMbps(double Mbitps) {
    double bps = Mbitps * 1000000;
    return (linkspeed_bps) bps;
}

linkspeed_bps speedFromKbps(uint64_t Kbitps) {
    uint64_t bps;
    bps = Kbitps * 1000;
    return bps;
}

linkspeed_bps speedFromPktps(double packetsPerSec) {
    double bitpersec = packetsPerSec * 8 * TcpPacket::DEFAULTDATASIZE;
    linkspeed_bps spd = (linkspeed_bps) bitpersec;
    return spd;
}

double speedAsPktps(linkspeed_bps bps) {
    double pktps = ((double) bps) / (8.0 * TcpPacket::DEFAULTDATASIZE);
    return pktps;
}

mem_pkts memFromPkts(double pkts) {
    return (int) (ceil(pkts));
}

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
