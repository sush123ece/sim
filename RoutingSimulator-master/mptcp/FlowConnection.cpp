//
// Created by Dingming Wu on 1/16/18.
//

#include "FlowConnection.h"

FlowConnection::FlowConnection(TcpSrc *tcpSrc,  int sid, int src, int dest, uint64_t fs,
                               double arrivalTime_ms) : _tcpSrc(tcpSrc),
                                                          _src(src), _dest(dest),_superId(sid),
                                                          _flowSize_Bytes(fs), _arrivalTimeMs(arrivalTime_ms)
{}


FlowConnection::FlowConnection(TcpSrc *tsrc, int src, int dest):
        _tcpSrc(tsrc), _src(src), _dest(dest)
{}
