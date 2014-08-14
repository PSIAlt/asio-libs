#ifndef ASIO_LIBS_FLOWGRAPH
#define ASIO_LIBS_FLOWGRAPH
#include <vector>
#include <boost/asio/io_service.hpp>

namespace ASIOLibs {

class FlowGraphNode;
class FlowGraph {
	FlowGraph(boost::asio::io_service &_io) : io(_io) {};
	typedef boost::function< void() > NodeHandlerType;

	FlowGraphNode &AddNode(FlowGraphNode &parent);

private:
	boost::asio::io_service &io;

};

class FlowGraphNode {
	FlowGraphNode(FlowGraph *_fg) : fg(_fg);

	void AddNext(FlowGraphNode &next);

private:
	typedef std::vector< FlowGraphNode * > nextnodes_list_type;
	FlowGraph *fg;
	nextnodes_list_type nextnodes;
};

}; //namespace

#endif
