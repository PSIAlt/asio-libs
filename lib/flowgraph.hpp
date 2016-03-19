#pragma once
#ifndef _ASIO_LIBS_FLOWGRAPH_HPP_
#define _ASIO_LIBS_FLOWGRAPH_HPP_
#include <vector>
#include <boost/asio/io_service.hpp>

/* Lib purpose is to describe, schedule and execute async plain

Execution plan:
          root_node
         /         \
     work1         work2
     /   \           |
 work11  work12      |
     \      |       /
        \   |    /
          work3
(anonymous "All work is done")

Usage code examples:
FlowGraph fg(io);
auto root_node = fg.AddRoot( [](GraphNode *n) -> bool {
	std::cout << "Root node started" << std::endl;
	return true; //Signal that job is done
} );
auto work1 = root_node->Add( boost::bind(do_work1, _1) );
auto work2 = root_node->Add( boost::bind(do_work2, _1) );

auto work11 = work1->Add( boost::bind(do_work11, _1) );
auto work12 = work1->Add( boost::bind(do_work12, _1) );

auto work3 = fg.AddJoin({work11, work12, work2});

work3->Add( [](GraphNode *n) -> bool {
	std::cout << "All work is done" << std::endl;
	io.post( [n]() {
		n->done(); //Complete after series of async operations
	});
	return false; //Not complete for now
} );

fg.Run();
std::cout << "Flowgraph request completed" << std::endl;

Now execution follows plan:
1. lambda root_node is executed
2. do_work1 & do_work2 executed independenly in parallel
3. After work1 is done, do_work11 & do_work12 executed in parallel (probably in parallel with work2 too)
4. work3 is a stub work which await work 11,12,2 to finish and then become finished itself
5. Last work is executed and reports all graph is completed
6. method `fg.run()` exits and execution continues

*/
namespace ASIOLibs {

struct GraphNode;

// Container class for query plan
struct FlowGraph {
	typedef std::function< bool(GraphNode *n) > callback_type;
	enum RunResult {
		RR_OK,
		RR_ERR,
		RR_INPROGRESS
	};

	FlowGraph(boost::asio::io_service &_io);
	~FlowGraph();

	GraphNode *AddRoot(callback_type cb);
	GraphNode *AddJoin(std::vector< GraphNode* > nodes);
	RunResult Run();
	void Reset() { reset_counter++; }
	uint32_t getResetCounter() const { return reset_counter; }

private:
	void Finish(RunResult r = RR_OK);
	friend struct GraphNode;
	boost::asio::io_service &io;
	RunResult res;
	std::vector< GraphNode* > rootnodes;
	uint32_t reset_counter = 0;
};

struct GraphNode {
	GraphNode(FlowGraph *_fg, const FlowGraph::callback_type &_cb);
	~GraphNode();
	GraphNode *Add(FlowGraph::callback_type cb); //Add new node
	FlowGraph *GetFG() { return fg; }
	void AddNext(GraphNode *next); //Link this node with next

	void done(); //This method will post nextnodes to io_service
	friend struct FlowGraph;
private:
	void run();
	void RunChilds();
	FlowGraph *fg;
	FlowGraph::callback_type cb;
	std::vector< GraphNode* > nextnodes;
	bool is_done;
};

};//ns

#endif
