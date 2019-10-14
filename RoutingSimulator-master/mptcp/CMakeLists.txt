# Create a library called "mptcp" which includes the source files
# The extension is already found. Any number of sources could be listed here.
file(GLOB HEADERS *.h)
add_library (mptcp eventlist.cpp tcppacket.cpp pipe.cpp queue.cpp tcp.cpp mtcp.cpp loggers.cpp
        logfile.cpp clock.cpp config.cpp network.cpp qcn.cpp exoqueue.cpp randomqueue.cpp
        cbr.cpp cbrpacket.cpp tcp_periodic.cpp tcp_transfer.cpp ${HEADERS} SingleDynamicFailureEvent.cpp MultipleSteadyFailures.cpp FlowConnection.cpp)

# Make sure the compiler can find include files for our mptcp library
# when other libraries or executables link to mptcp
target_include_directories (mptcp PUBLIC ${PROJECT_SOURCE_DIR}/mptcp)