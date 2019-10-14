#ifndef MAIN_H
#define MAIN_H

#include <string>
#include <vector>

#define REROUTE 0

// RRG STUFF
#define R 0		// R = switch-switch ports (or network-ports)
#define SP 0		// P = total ports on switch
#define RTT 30 // Identical RTT microseconds = 0.03 ms [WDM] I change it to 30us to match the link speed.
// FAT
#define K 16
#define RATIO 1
#define NSW K*K*5/4
#define NHOST (K*K*K*RATIO/4)

#define NK (K*K/2)
#define NC (K*K/4)
#define NSRV (K*K*K*RATIO/4)
#define HOST_POD_SWITCH(src) (2*src/(K*RATIO))

#define HOST_POD(src) (src/(NC*RATIO))
#define MIN_POD_ID(pod_id) (pod_id*K/2)
#define MAX_POD_ID(pod_id) ((pod_id+1)*K/2-1)

#define SW_BW 125000 // switch link bandwidth in pps = 1Gbps
#define HOST_NIC SW_BW // host nic speed in pps
#define CORE_TO_HOST 1 // right now it is non-blocking


#define SWITCH_BUFFER 10
#define RANDOM_BUFFER 0
#define FEEDER_BUFFER 1000000 //1GB, feeder queue shouldn't be the bottleneck

#define BACKUPS_PER_GROUP 1

#define LOCAL_REROUTE_DELAY 2//ms
#define GLOBAL_REROUTE_DELAY 200 //ms
#define ASPEN_TREE_DELAY 100
#define CIRCUIT_SWITCHING_DELAY 1 //ms
#define TCP_TIMEOUT_SCANNER_PERIOD 0.9 //ms
#define GLOBAL_LOAD_BALANCING 0

//extern std::vector<std::vector<int>> vec_sw[NK + NK + NK / 2];
//extern std::vector<std::vector<int>> vec_pipe[K * K * K / 2 + NHOST];

extern int *colom1;
extern int *colom2;

//extern int* vec_sw[NK + NK + NK / 2];
//extern int* vec_pipe[K * K * K / 2 + NHOST];

#endif

