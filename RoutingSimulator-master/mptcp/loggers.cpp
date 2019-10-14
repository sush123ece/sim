#include "loggers.h"
#include <iostream>
#include <cfloat>

using namespace std;

QueueLoggerSampling::QueueLoggerSampling(simtime_picosec period, EventList &eventlist)
: EventSource(eventlist,"QueuelogSampling"),
        _queue(NULL), _lastlook(0), _period(period), _lastq(0), 
	_seenQueueInD(false), _cumidle(0), _cumarr(0), _cumdrop(0)
	{	
	eventlist.sourceIsPendingRel(*this,0);
	}

void
QueueLoggerSampling::doNextEvent() 
{
	eventlist().sourceIsPendingRel(*this,_period);
	if (_queue==NULL) return;
	mem_b queuebuff = _queue->_maxsize;
	if (!_seenQueueInD) { // queue size hasn't changed in the past D time units
		_logfile->writeRecord(QUEUE_APPROX,_queue->id,QUEUE_RANGE,(double)_lastq,(double)_lastq,(double)_lastq);
		_logfile->writeRecord(QUEUE_APPROX,_queue->id,QUEUE_OVERFLOW,0,0,(double)queuebuff);
		}
	else { // queue size has changed
		_logfile->writeRecord(QUEUE_APPROX,_queue->id,QUEUE_RANGE,(double)_lastq,(double)_minQueueInD,(double)_maxQueueInD);
		_logfile->writeRecord(QUEUE_APPROX,_queue->id,QUEUE_OVERFLOW,-(double)_lastIdledInD,(double)_lastDroppedInD,(double)queuebuff);
		}
	_seenQueueInD=false;
	simtime_picosec now = eventlist().now();
	simtime_picosec dt_ps = now-_lastlook;
	_lastlook = now;
	if ((_queue!=NULL) & (_queue->_queuesize==0)) _cumidle += timeAsSec(dt_ps); // if the queue is empty, we've just been idling
	_logfile->writeRecord(QUEUE_RECORD,_queue->id,CUM_TRAFFIC,_cumarr,_cumidle,_cumdrop);
}


void
QueueLoggerSampling::logQueue(Queue& queue, QueueEvent ev, Packet &pkt) {
	if (_queue==NULL) _queue=&queue;
	assert(&queue==_queue);
	_lastq = queue._queuesize;
	if (!_seenQueueInD) {
		_seenQueueInD=true;
		_minQueueInD=queue._queuesize;
		_maxQueueInD=_minQueueInD;
		_lastDroppedInD=0;
		_lastIdledInD=0;
		_numIdledInD=0;
		_numDropsInD=0;
		}
	else {
		_minQueueInD=min(_minQueueInD,queue._queuesize);
		_maxQueueInD=max(_maxQueueInD,queue._queuesize);
		}
	simtime_picosec now = eventlist().now();
	simtime_picosec dt_ps = now-_lastlook;
	double dt = timeAsSec(dt_ps);
	_lastlook = now;
	switch(ev) {
		case PKT_SERVICE: // we've just been working
			break;
		case PKT_ENQUEUE:
			_cumarr += timeAsSec(queue.drainTime(&pkt));
			if (queue._queuesize>pkt.size()) // we've just been working
				{}
			else { // we've just been idling 
				mem_b idledwork = queue.serviceCapacity(dt_ps);
				_cumidle += dt; 
				_lastIdledInD = idledwork;
				_numIdledInD++;
				}
			break;
		case PKT_DROP: // assume we've just been working
			assert(queue._queuesize>=pkt.size()); // it is possible to drop when queue is idling, but this logger can't make sense of it
			double localdroptime = timeAsSec(queue.drainTime(&pkt));
			_cumarr += localdroptime;
			_cumdrop += localdroptime;
			_lastDroppedInD = pkt.size();
			_numDropsInD++;
			break;
		}
	}

AggregateTcpLogger::AggregateTcpLogger(simtime_picosec period, EventList& eventlist)
:	EventSource(eventlist,"bunchofflows"),
	_period(period)
	{
	eventlist.sourceIsPending(*this,period);
	}

void
AggregateTcpLogger::monitorTcp(TcpSrc& tcp) {
	_monitoredTcps.push_back(&tcp);
	}

void
AggregateTcpLogger::doNextEvent() {
	eventlist().sourceIsPending(*this,max(eventlist().now()+_period,_logfile->_starttime));
	double totunacked=0;
	double toteffcwnd=0;
	double totcwnd=0;
	int numflows=0;
	for (tcplist_t::iterator i = _monitoredTcps.begin(); i!=_monitoredTcps.end(); i++) {
		TcpSrc* tcp = *i;
		uint32_t cwnd = tcp->_cwnd;
		uint32_t unacked = tcp->_unacked;
		uint32_t effcwnd = tcp->_effcwnd;
		totcwnd += cwnd;
		toteffcwnd += effcwnd;
		totunacked += unacked;
		numflows++;
		}
	_logfile->writeRecord(TcpLogger::TCP_RECORD,id,TcpLogger::AVE_CWND,totcwnd/numflows,totunacked/numflows,toteffcwnd/numflows);
	}


SinkLoggerSampling::SinkLoggerSampling(simtime_picosec period, EventList& eventlist):
  EventSource(eventlist,"SinkSampling"), _last_time(0), _period(period)
{
  eventlist.sourceIsPendingRel(*this,0);
}

void SinkLoggerSampling::monitorSink(DataReceiver* sink){
  _sinks.push_back(sink);
  _last_seq.push_back(sink->cumulative_ack());
  _last_rate.push_back(0);

  TcpSrc* src = ((TcpSink*)sink)->_src;

  if (src!=NULL&&src->_mSrc!=NULL){
    if (_multipath_src.find(src->_mSrc)==_multipath_src.end()){
      _multipath_src[src->_mSrc] = 0;
      //      printf("RR\n");
    }
  }
  //else printf("BLAH %x %x\n",src,src->_mSrc);
}

void SinkLoggerSampling::doNextEvent(){
	eventlist().sourceIsPendingRel(*this,_period);  
	simtime_picosec now = eventlist().now();
	simtime_picosec delta = now - _last_time;
	_last_time = now;

        map <int, double> instant_rate;
        map <int, double> cumulative_rate;

	for (uint64_t i=0;i<_sinks.size();i++){
	  if (_last_seq[i]<=_sinks[i]->cumulative_ack()){//this deals with resets for periodic sources
	    TcpAck::seq_t deltaB = _sinks[i]->cumulative_ack() - _last_seq[i];
	    double rate = deltaB * 1000000000000.0 / delta;//Bps
	    if(rate>0)
	    {
                cout << "Throughput " << rate*8/1024/1024 << " Mbps, sink " << _sinks[i]->get_id() << " flow " << _sinks[i]->get_super_id() << " time " << now/1000000000000 << endl;
/*		cout << "MAPPING MTCPADDRESS  to subflows " << endl;
		if (((TcpSink*)_sinks[i])->_mSink != 0 || ((TcpSink*)_sinks[i])->_mSink != NULL) {

			cout << "Time " << now/1000000000000 << " Parent MTCP-flow " << ((TcpSink*)_sinks[i])->_mSink << " has subflow sink-id = " << _sinks[i]->get_id() << endl;*/
//			list<TcpSink*> mysubs = (((TcpSink*)_sinks[i])->_mSink)->_subflows;
		/*	//for (int j=0; j < mysubs.size(); j++){
			for (list<TcpSink*>::const_iterator ci = mysubs.begin(); ci != mysubs.end(); ++ci)
				cout << "MAP ANKIT " << _sinks[i]->get_id() << " to "  << (*ci)->get_id() << endl;
		}*/
                
                int index = _sinks[i]->get_super_id();
		double curnt_rate = rate*8/1024/1024;
                if (instant_rate.find(index) == instant_rate.end()) {
                	instant_rate[index] = curnt_rate;
		}
		else {
			double new_rate = instant_rate[index] + curnt_rate;
			instant_rate[index] = new_rate; 
		}

		curnt_rate = _sinks[i]->cumulative_ack()*1000000000000.0/now*8/1024/1024;
                if (cumulative_rate.find(index) == cumulative_rate.end()) {
                	cumulative_rate[index] = curnt_rate;
		}
		else {
			double new_rate = cumulative_rate[index] + curnt_rate;
			cumulative_rate[index] = new_rate; 
		}
	    }

	    typedef map<int, double>::iterator it_type;
	    int flow_count = 0;
	    double average_instant_rate = 0;
	    double average_cumulative_rate = 0;
	    double min_instant_rate = DBL_MAX;
	    double min_cumulative_rate = DBL_MAX;

	    for(it_type iterator = instant_rate.begin(); iterator != instant_rate.end(); iterator++) {
		flow_count++;
		average_instant_rate += iterator->second;
		if (iterator->second < min_instant_rate) {
			min_instant_rate = iterator->second;
		}
		//cout << "time " << now/1000000000000 << " flow " << iterator->first << " instant rate " << iterator->second << " Mbps" << endl;
            }
	    for(it_type iterator = cumulative_rate.begin(); iterator != cumulative_rate.end(); iterator++) {
		average_cumulative_rate += iterator->second;
		if (iterator->second < min_cumulative_rate)
			min_cumulative_rate = iterator->second;
		//cout << "time " << now/1000000000000 << " flow " << iterator->first << " cumulative throughput " << iterator->second << " Mbps" << endl;
            }
	    //cout << "time " << now/1000000000000 << " average instant rate " << average_instant_rate << " Mbps" << endl;
	    //cout << "time " << now/1000000000000 << " average cumulative rate " << average_cumulative_rate << " Mbps" << endl;
	    //cout << "time " << now/1000000000000 << " min instant rate " << min_instant_rate << " Mbps" << endl;
	    //cout << "time " << now/1000000000000 << " min cumulative rate " << min_cumulative_rate << " Mbps" << endl;
	    
	    _logfile->writeRecord(TcpLogger::TCP_SINK,_sinks[i]->get_id(),TcpLogger::RATE,rate,_sinks[i]->drops(),_sinks[i]->cumulative_ack());
	    _last_rate[i] = rate;

	    TcpSrc* src = ((TcpSink*)_sinks[i])->_src;

	    if (src->_mSrc!=NULL){
	      _multipath_src[src->_mSrc] += rate;
	    }
	  }
	  _last_seq[i] = _sinks[i]->cumulative_ack();
	}

	multipath_map::iterator it;

	int m_count = 0;
	for (it = _multipath_src.begin();it!=_multipath_src.end();it++){
	  MultipathTcpSrc* mtcp = (MultipathTcpSrc*)(*it).first;
	
	  list<TcpSrc*> mysubs = mtcp->_subflows;
	  for (list<TcpSrc*>::const_iterator ci = mysubs.begin(); ci != mysubs.end(); ++ci)
		  //cout << "MAP ANKIT " << m_count << " to "  << (*ci)->id << endl;

	  m_count++;
	  double rate = (double)(*it).second;

	  _logfile->writeRecord(MultipathTcpLogger::MTCP,mtcp->id,MultipathTcpLogger::RATE,mtcp->a,mtcp->_alfa,rate);

	  _multipath_src[mtcp] = 0;
	}
}
