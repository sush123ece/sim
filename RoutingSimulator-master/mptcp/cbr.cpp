#include "cbr.h"
#include "math.h"
#include <iostream>
#include "cbrpacket.h"
#include "../datacentre/fattree/fat_tree_topology.h"
#include "queue.h"
////////////////////////////////////////////////////////////////
//  CBR SOURCE
////////////////////////////////////////////////////////////////
// RandomQueue *HostTXQueues[NSRV];

CbrSrc::CbrSrc(EventList &eventlist,linkspeed_bps rate,simtime_picosec active,simtime_picosec idle)
  : EventSource(eventlist,"cbr"),  _bitrate(rate),_crt_id(1),_mss(1000),_flow(NULL)
{
  _period = (simtime_picosec)((pow(10.0,12.0) * 8 * _mss) / _bitrate);
  _sink = NULL;
  _route = NULL;
  _start_active = 0;
  _end_active = 0;
  _idle_time = idle;
  _active_time = active;
  _is_active = false;
}

CbrSrc::CbrSrc(int src, EventList &eventlist,linkspeed_bps rate,simtime_picosec active,simtime_picosec idle)
        : EventSource(eventlist,"cbr"),  _bitrate(rate),_crt_id(1),_mss(1000),_flow(NULL)
{
    _period = (simtime_picosec)((pow(10.0,12.0) * 8 * _mss) / (pow(10.0,9.0)*_bitrate));
    cout << "period is " << _period << endl;
    _src = src;
    _sink = NULL;
    _route = NULL;
    _start_active = 0;
    _end_active = 0;
    _idle_time = idle;
    _active_time = active;
    _is_active = false;
}

void 
CbrSrc::connect(route_t& routeout, CbrSink& sink, simtime_picosec starttime) 
{
  _route = &routeout;
  _sink = &sink;
  _flow.id = id; // identify the packet flow with the CBR source that generated it
  _is_active = true;
  _start_active = starttime;
  _end_active = _start_active + (simtime_picosec)(2.0*drand()*_active_time);
  eventlist().sourceIsPending(*this,starttime);
}

// Added by Sushovan
void
CbrSrc::installCbr(simtime_picosec starttime) {

    _is_active = true;
    _start_active = starttime;
    _end_active = _start_active + (simtime_picosec)(_active_time);
    eventlist().sourceIsPending(*this,starttime);

    cout << "In install cbr" << endl;
}

void 
CbrSrc::doNextEvent() {
    cout << "in do next event" << endl;
 /* if (_idle_time==0||_active_time==0){
      cout << "in 1st if" << endl;
    send_packet();
    return;
  } */

  if (_is_active){
      cout << "in 2nd if" << endl;
    if (eventlist().now()>_end_active){
      _is_active = false;
      return; // for stopping the packet flow after sometime
     // eventlist().sourceIsPendingRel(*this,(simtime_picosec)(2*drand()*_idle_time)); // for making it run infinite time
    }
    else
      send_packet();
  }
  else {
      cout << "in else" << endl;
    _is_active = true;
    _start_active = eventlist().now();
     // _end_active = _start_active + (simtime_picosec)(2.0*drand()*_active_time);
      _end_active = _start_active + (simtime_picosec)(_active_time);
    send_packet();
  }
}

void 
CbrSrc::send_packet() {
    cout << "I am here at src q" << endl;
  Packet* p = CbrPacket::newpkt(_flow, *_route, _crt_id++, _mss);
    cout << "pkt instanciated" << endl;
    cout << _src << endl;
  // add this hostTxqueue
  p->_route = new route_t();
  (p->_route)->push_back(HostTXQueues[_src]); //server queue
  (p->_route)->push_back(pipes_ns_nlp[_src][HOST_POD_SWITCH(_src)]); //server-ToR link
  (p->_route)->push_back(queues_ns_nlp[_src][HOST_POD_SWITCH(_src)]); //ToR switch
  cout << (queues_ns_nlp[_src][HOST_POD_SWITCH(_src)])->_switchId << endl;
    cout << "route pushed back" << endl;


     p->sendOn();

  /*  simtime_picosec how_long = _period;
  simtime_picosec _active_already = eventlist().now()-_start_active;

  if (_active_time!=0&&_idle_time!=0&&_period>0) */

   eventlist().sourceIsPendingRel(*this,_period);
}

////////////////////////////////////////////////////////////////
//  Cbr SINK
////////////////////////////////////////////////////////////////

CbrSink::CbrSink()
  : Logged("cbr")  {
  _received = 0;
  _last_id = 0;
  _cumulative_ack = 0;
}

PacketSink* CbrSink::getDual() { return NULL;}

// Note: _cumulative_ack is the last byte we've ACKed.
// seqno is the first byte of the new packet.
void
CbrSink::receivePacket(Packet& pkt) {
  _received++;

  _cumulative_ack = _received * 1000;
  _last_id = pkt.id();
    cout << "no of rx " << _received << " " << "received pkt id " << pkt.id() << endl;
  pkt.free();
}	

