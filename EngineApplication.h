#pragma once

#include "search.h"
#include "uci_client.h"
#include "eval.h"
#include "MoveOrdering.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
/*
	This file contains top level classes that defines engine control flow. 
*/


typedef  Search<StandardEval, StaticMoveOrdering<Move*>> SearchAlgorithm;

struct SearchStatus {
	bool done;
	unsigned int ply_reached;
	int eval;
	Move best_move;
};

enum SearchThreadState {
	Idle,
	Searching
};

class SearchStopCondition{
	
	SearchStatus* thread_status;

protected:

	const SearchStatus& get_search_status() {
		return *thread_status;
	}

public:

	void link_with_search_status(SearchStatus* status) {
		this->thread_status = status;
	}

	virtual bool operator()() = 0;
};

class ISearcherThreadHandle {
public:

	virtual ~ISearcherThreadHandle() {};
	
	/*
		This method starts a new search with given parameters.
		Calling thread will be blocked in case search thread is already busy.
	*/
	virtual void start_search(const game_state & position, const UCI::go_params & go_parameters) = 0;

	/*
		Stops any current ongoing search as quickly as possible.
		Calling thread will be blocked until search confirms that it is stopped
	*/
	virtual void stop_search() = 0;


	// kills thread to free resources
	virtual void terminate_thread() = 0;

	// spawn 
	virtual void spawn_searcher_thread() = 0;

	virtual SearchThreadState get_thread_state() = 0;
	
};


namespace SearcherThreadImplementation {

	enum _searcher_thread_command {
		Search,		// payload: game state + stop condition
		Stop,		// payload: none, used primary as a mean to 
		Terminate,   // payload: none
		Noop		// this shouldn't be used by UCI thread, this is used more
	};

	// critical section used to pass commands to searching thread
	struct critical_section {
		std::mutex mutex;
		std::condition_variable	cv;

		SearchThreadState thread_state;
		_searcher_thread_command command;
		game_state search_position;	  // position to search
		UCI::go_params search_params; //  search params

	};

	template <class Search_>
	class _stop_command_received : public Search_::IStopCondition {

		critical_section* crit_section;
		bool condition_met_;
		public:

		_stop_command_received(critical_section* section) : crit_section(section), condition_met_(false) {};

		bool condition_meet() {
			return condition_met_;
		}

		// Odziedziczono za poœrednictwem elementu IStopCondition
		bool condition_reached() override
		{
			std::lock_guard<std::mutex> guard(crit_section->mutex);
			
			if (crit_section->command == Stop) {
				condition_met_ = true;
				return true;
			}

			return false;
		}
	};

	


	template <typename Search_>
	void search_main(critical_section* section) {
		
		UCI::UCIOutputStream uci_out(&std::cout);
		
		std::unique_lock<std::mutex> lock(section->mutex);
		
		Search_ engine = Search_();

		game_state gs;
		UCI::go_params params;
		_stop_command_received<Search_>* user_issued_stop = nullptr;
		Move m;
		bool is_running = true;
		while (is_running) {
			section->cv.wait(lock);

			switch (section->command){
			case SearcherThreadImplementation::Search:

				gs = section->search_position; // copying the game state so we can release the lock safely during search
				params = section->search_params;
				
				engine.clear_stop_conditions();
				user_issued_stop = new _stop_command_received<Search_>(section);
				engine.add_stop_condition(user_issued_stop);

				// TODO: add additional stop conditions based  on context of go_params
				
				section->thread_state = Searching;
				lock.unlock(); // we are releasing the lock now, so stop command could be received to interrupt the engine
				m = engine.uciCompliantIterativeDeepening(gs, params, uci_out);
				lock.lock();
				section->thread_state = Idle;

				if (params.infinite_mode && !user_issued_stop->condition_meet()) {
					/*
						This branch of code is reached only if we ended search before 'stop' command in infinite mode.
						Now we must wait to GUI to send the 'stop', 'ponderhit' or 'quit' command.
					*/
					while ( section->command != Stop && section->command != Terminate) 
						section->cv.wait_for(lock, std::chrono::milliseconds(500));
				}

				uci_out << m;

				break;
			case SearcherThreadImplementation::Stop:
				// NO-OP, because we are already done
			case SearcherThreadImplementation::Noop:
				
				break;
			
			case SearcherThreadImplementation::Terminate:
					is_running = false;
				break;
		
			default:
				break;
			}
		}
	}


	template <typename Search_>
	class SearcherThreadHandle : public ISearcherThreadHandle {
		critical_section* critical_section;
		std::thread* searcher_thread;

		void init_critical_section();
		void terminate_thread() override final;
		void spawn_searcher_thread() override final;

	public:
		SearcherThreadHandle(SearcherThreadImplementation::critical_section* section) : critical_section(section) {
			init_critical_section();
			spawn_searcher_thread();
		};

		~SearcherThreadHandle() {
			terminate_thread();
			searcher_thread->join();
		}

		// Odziedziczono za poœrednictwem elementu ISearcherThreadHandle
		void start_search(const game_state& position, const UCI::go_params& go_parameters) override final;
		void stop_search() override final;

		SearchThreadState get_thread_state() override final;
	};

	template<typename Search_>
	inline void SearcherThreadHandle<Search_>::start_search(const game_state& position, const UCI::go_params& go_parameters)
	{
		std::lock_guard<std::mutex> lock_guard(critical_section->mutex);

		critical_section->search_params = go_parameters;
		critical_section->search_position = position;
		critical_section->command = Search;
		critical_section->cv.notify_one();
	}

	template<typename Search_>
	inline void SearcherThreadHandle<Search_>::stop_search()
	{
		std::lock_guard<std::mutex> lock_guard(critical_section->mutex);
		critical_section->command = Stop; 
		critical_section->cv.notify_one();
	}

	template<typename Search_>
	inline SearchThreadState SearcherThreadHandle<Search_>::get_thread_state()
	{
		std::lock_guard<std::mutex> lock_guard(critical_section->mutex);
		return critical_section->thread_state;
	}

	template<typename Search_>
	inline void SearcherThreadHandle<Search_>::init_critical_section()
	{
		std::lock_guard<std::mutex> guard(critical_section->mutex);
		critical_section->thread_state =	Idle;
		critical_section->command =			Stop;
		critical_section->cv.notify_one();
	}

	template <typename Search_>
	inline void SearcherThreadHandle<Search_>::terminate_thread() {
		std::lock_guard<std::mutex> guard(critical_section->mutex);
		critical_section->command = Terminate;
		critical_section->cv.notify_one();
	}

	template<typename Search_>
	inline void SearcherThreadHandle<Search_>::spawn_searcher_thread()
	{
		searcher_thread = new std::thread(search_main<Search_>, critical_section);
	}

}



template <class Search_>
class EngineInstance {
	SearcherThreadImplementation::critical_section section;
	SearcherThreadImplementation::SearcherThreadHandle <Search_> searcher;
	game_state current_position;
	bool position_received;
	UCI::UCIOutputStream* out;

public:
	bool is_running;
	std::vector<UCI::MemSafeOption> supportedOptions;


	EngineInstance(UCI::UCIOutputStream* out) : out(out) , searcher(&section), is_running(true) {init(); };
	void init();
	void receive_position(const game_state pos);
	void go(const UCI::go_params& params);
	void stop();
	void quit();
	UCI::UCIOutputStream& output_stream() { return *out; };
};

extern EngineInstance<SearchAlgorithm>* instance;

// TODO: implement UCI callbacks

template<class Search_>
inline void EngineInstance<Search_>::init()
{
	GameStateUtils::clear(current_position);
	position_received = false;
	supportedOptions.emplace_back("GrimoireMode", false);
	supportedOptions.emplace_back("GrimoireBounds", UCI::spin_option({125, 10, 1000}));

}

template<class Search_>
inline void EngineInstance<Search_>::receive_position(const game_state pos)
{
	position_received = true;
	current_position = pos;
}

template<class Search_>
inline void EngineInstance<Search_>::go(const UCI::go_params& params)
{
	if (position_received) {
		searcher.start_search(current_position, params);
		position_received = false;
	}
	else {
		*out << "string Error! Expected 'position' command before 'go'";
	}
}

template<class Search_>
inline void EngineInstance<Search_>::stop()
{
	searcher.stop_search();
}

template<class Search_>
inline void EngineInstance<Search_>::quit()
{
	is_running = false;
}

void handle_uci() {
	if (instance) {
		instance->output_stream() << UCI::UCIOK();
		instance->output_stream() << UCI::engine_hello;
		instance->output_stream() << instance->supportedOptions;
	}
}

void handle_debug(bool v) {
	// TODO: forward value
}

void handle_is_ready() {
	if (instance)
		instance->output_stream() << UCI::ReadyOK();
}

void handle_go(const UCI::go_params& params) {
	if (instance) instance->go(params);
}

void handle_set_option(std::string id, std::string value) {
	
	if (id == "GrimoireMode" && (value == "true" || value == "false")) {
		grimoire_mode = value[0] == 't';
	}

	if (id == "GrimoireBounds" ) {
		// todo: check if grimoire bounds is numeric
		grimoire_bounds = atoi(value.c_str());
	};
}

void handle_uci_newgame() {
	// TODO: flush TT
}

void handle_position(std::string fen, const std::vector<Move>& moves) {

	game_state position;
	GameStateUtils::parse_fen(position, fen);

	for (Move m : moves)
		GameStateUtils::make_move(position, m);

	if (instance)
		instance->receive_position(position);
}

void handle_stop() {
	if (instance) instance->stop();
}

void handle_ponder_hit() {
	// TODO: handle
}

void handle_quit() {
	if (instance) instance->quit();
}

void handle_unsuported_command(std::string prompt) {
	if (instance) instance->output_stream() << UCI::formatString("string Unrecognised command: %s", prompt.c_str());
}

const UCI::UCI_Client_callbacks uci_callbacks{
	handle_uci,
	handle_debug,
	handle_is_ready,
	handle_set_option,
	nullptr,
	handle_uci_newgame,
	handle_position,
	handle_go,
	handle_stop,
	handle_ponder_hit,
	handle_quit,
	handle_unsuported_command
};