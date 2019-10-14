#ifndef MTCP_H
#define MTCP_H

/*
 * A MTCP source and sink
 */

#include <math.h>
#include <list>
#include "config.h"
#include "network.h"
#include "tcp.h"
#include "eventlist.h"

#define USE_AVG_RTT 0
#define UNCOUPLED 1

#define FULLY_COUPLED 2
//params for fully coupled
#define A 1
#define B 2

#define COUPLED_INC 3
#define COUPLED_TCP 4

#define COUPLED_EPSILON 5
#define COUPLED_SCALABLE_TCP 6

class MultipathTcpSink;

class MultipathTcpSrc : public PacketSink, public EventSource {
public:
  MultipathTcpSrc(char cc_type,EventList & ev,MultipathTcpLogger* logger,double epsilon = 0.1);
	void addSubflow(TcpSrc* tcp);
	void receivePacket(Packet& pkt);

// should really be private, but loggers want to see:

	uint32_t inflate_window(uint32_t cwnd,int newly_acked,uint32_t mss);
	uint32_t deflate_window(uint32_t cwnd, uint32_t mss);
	void window_changed();
	void doNextEvent();
	double compute_a();
	uint32_t compute_a_scaled();
	uint32_t compute_a_tcp();
	double compute_alfa();

	uint64_t compute_total_bytes();

	uint32_t a;
	// Connectivity; list of subflows
	list<TcpSrc*> _subflows; // list of active subflows for this connection
	double _alfa;
private:
	MultipathTcpLogger* _logger;
	uint32_t compute_total_window();

	char _cc_type;

	double _e;

	// Mechanism
};

class MultipathTcpSink : public PacketSink {
public:
	MultipathTcpSink();
	void addSubflow(TcpSink* tcp);
	void receivePacket(Packet& pkt);
	// Connectivity
	list<TcpSink*> _subflows;
private:
	// Mechanism

	TcpAck::seq_t _cumulative_ack; // the packet we have cumulatively acked
	list<TcpAck::seq_t> _received; // list of packets above a hole, that we've received
	};
#endif
