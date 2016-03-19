#include <unistd.h>
#include <stdexcept>
#include <string>
#include <boost/bind.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include "flowgraph.hpp"

namespace ASIOLibs {

FlowGraph::FlowGraph(boost::asio::io_service &_io) : io(_io), res(RR_INPROGRESS) {
}

FlowGraph::~FlowGraph() {
	for( auto &i : rootnodes ) {
		delete i;
	}
}

GraphNode *FlowGraph::AddRoot(callback_type cb) {
	GraphNode *n = new GraphNode(this, cb);
	rootnodes.push_back(n);
	return n;
}

struct NodeJoiner {
	NodeJoiner(FlowGraph *fg_, size_t sz) : fg(fg_), want_nodes_cnt(sz), want_nodes_cnt_orig(sz) {
		assert( sz > 0 );
	}

	bool operator() (GraphNode *n) {
		if( reset_counter != fg->getResetCounter() || want_nodes_cnt == 0 ) {
			//If base FlowGraph was reset, we need to start node counting from begining
			reset_counter = fg->getResetCounter();
			want_nodes_cnt = want_nodes_cnt_orig;
		}
		//Will not be done until we receive signals from all linked nodes
		bool done = want_nodes_cnt-- == 0;
		return done;
	}

	FlowGraph *fg;
	size_t want_nodes_cnt, want_nodes_cnt_orig;
	uint32_t reset_counter=0;
};
GraphNode *FlowGraph::AddJoin(std::vector< GraphNode* > nodes) {
	if( nodes.empty() )
		throw std::invalid_argument("FlowGraph::AddJoin: empty join list");

	//Add & first link
	GraphNode *joinnode = nodes[0]->Add( NodeJoiner(this, nodes.size()) );
	//Link all others
	for( size_t i=1; i<nodes.size(); i++ ) {
		nodes[i]->AddNext(joinnode);
	}
	return joinnode;
}

FlowGraph::RunResult FlowGraph::Run() {
	if( rootnodes.empty() )
		return res;
	//All we need is to run root & wait is_finished
	for( auto &i : rootnodes ) {
		io.post( boost::bind(&GraphNode::run, i) );
	}

	while( res == RR_INPROGRESS ) {
		io.run_one();
	}
	return res;
}

void FlowGraph::Finish(RunResult r) {
	res = r;
}

GraphNode::GraphNode(FlowGraph *_fg, const FlowGraph::callback_type &_cb) : fg(_fg), cb(_cb), is_done(false) {
}

GraphNode::~GraphNode() {
	for( auto &i : nextnodes ) {
		delete i;
	}
}

GraphNode *GraphNode::Add(FlowGraph::callback_type cb) {
	GraphNode *n = new GraphNode(fg, cb);
	nextnodes.push_back(n);
	return n;
}

void GraphNode::AddNext(GraphNode *next) {
	nextnodes.push_back(next);
}

void GraphNode::done() {
	if( !is_done ) {
		is_done=true;
		RunChilds();
	}
}

void GraphNode::run() {
	bool r = cb(this);
	if( r && !is_done )
		done();
}

void GraphNode::RunChilds() {
	for( auto &i : nextnodes ) {
		fg->io.post( boost::bind(&GraphNode::run, i) );
	}
}

};
