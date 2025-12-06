#pragma once

#include <istream>
#include <ostream>
#include <memory>
#include <functional>
#include <string>
#include <future>
#include "Move.h"
#include "uci_client.h" // for type safety

/*
	This header file consists of a utilities dedicated to making UCI parser from the GUI's POV
*/


namespace _ENGINE_PIPE {

	// Exception that _Engine_Pipe could thrown based 
	class _OSException : public std::exception {
		std::string message;

	public:
		// Constructor to initialize the error message
		explicit _OSException(const std::string& msg) : message(msg) {}

		// Override the what() method to return the error message
		const char* what() const noexcept override {
			return message.c_str();
		}
	};

	// Interface encapsulating access to the engine. Concrete implementation are OS specific
	class IEngineProcessPipe {
	public:
		virtual ~IEngineProcessPipe() {};
		
		/*
			Returns true if the underlying engine process is alive.
		*/
		virtual bool isAlive() = 0;

		// return the stream reading from engine stdout 
		virtual std::istream & get_engine_output() = 0;

		// return the stream writing to engine stdin
		virtual std::ostream & get_engine_input () = 0;

		virtual void kill() = 0; // immediately kill the engine process
	};

	// Interface encapsulating proces of starting an engine. Concrete implementation are OS specific
	class IEngineLoader {
	public:
		virtual ~IEngineLoader() {};

		// spawns a new instance of an engine and returns a pipe connected to an engine
		virtual std::shared_ptr<IEngineProcessPipe> load_engine(std::string path, std::string working_dir, std::function<void(const std::string &)> error_callback) = 0;
	};

	/*
		This function blocks the current thread for as long as pipe is alive and outputs lines received from the engine 
		to 'consumer' callback. 

		This function was designed to be called on a separate thread
	*/
	void engine_pipe_listener(std::shared_ptr<IEngineProcessPipe> pipe, std::function<void(std::string)> consumer);
	
}

std::unique_ptr<_ENGINE_PIPE::IEngineLoader> getEngineLoader();

// Service that manages open engine instances
class OpenEnginePipesService {
	std::unique_ptr<_ENGINE_PIPE::IEngineLoader> engine_loader;
	std::unordered_map<int, std::shared_ptr<_ENGINE_PIPE::IEngineProcessPipe>> engines_pipes;
	std::function<void(const std::string& log_line)> logger;
	int id_sequence = 0;

	int assign_id() { return id_sequence++; };

	void remove_closed_pipes();
public:

	OpenEnginePipesService(std::unique_ptr<_ENGINE_PIPE::IEngineLoader> loader, std::function<void(const std::string&)> logger) :
		engine_loader(loader.release()), logger(logger) {
	};

	// return -1 if the opening of the pipe fails, otherwise returns an ID of the engine pipe
	int open(std::string path, std::string working_dir);

	std::vector<std::pair<int, std::shared_ptr<_ENGINE_PIPE::IEngineProcessPipe>>> get_all();

	std::shared_ptr<_ENGINE_PIPE::IEngineProcessPipe> get_pipe_by_id(int id);
};

// todo: create unix port
#define Windows 1 


namespace UCI_SERVER {

	struct callbacks {
		std::function<void(const std::string&, const std::string)> on_id;
		std::function<void()> on_uciok;
		std::function<void()> on_readyok;
		std::function<void(const std::string&, const std::string&)> on_bestmove;
		std::function<void(const std::string&)> on_copyprotection;
		std::function<void(const std::string&)> on_registration;
		std::function<void(const std::string&, const std::vector<std::string>&)> on_info;
		std::function<void(UCI::MemSafeOption)> on_option;
		std::function<void(const std::string&)> logging;
	};

	class ServersideCommandHandler {
		std::vector<UCI::_CommandHandlerSlot> router;


		void logError(const std::string& message) {};

		std::function<void(const std::vector<std::string>&)> no_arg(std::function<void()> foo) { return [foo](const std::vector<std::string>& _) {foo(); }; }
		std::function<void(const std::vector<std::string>&)> wrap_id_callback(std::function<void(const std::string&, const std::string)> on_author);
		std::function<void(const std::vector<std::string>&)> wrap_bestmove_callback(std::function<void(const std::string&, const std::string&)> on_best_move);
		std::function<void(const std::vector<std::string>&)> wrap_info_callback(std::function<void(const std::string, const std::vector<std::string>&)> on_info);
		std::function<void(const std::vector<std::string>&)> wrap_option_callback(std::function<void(UCI::MemSafeOption)> on_option);
		std::function<void(const std::vector<std::string>&)> wrap_command_status_callback(std::function<void(const std::string&)> on_status);
	public:

		ServersideCommandHandler(callbacks& callbacks_) {
			router = {
				{"id", wrap_id_callback(callbacks_.on_id)},
				{"uciok",  no_arg(callbacks_.on_uciok)},
				{"readyok", no_arg(callbacks_.on_readyok)},
				{"bestmove", wrap_bestmove_callback(callbacks_.on_bestmove)},
				{"registration",wrap_command_status_callback(callbacks_.on_registration)},	//  no op, registration is not supported at the moment
				{"copyprotection", wrap_command_status_callback(callbacks_.on_copyprotection)},	//  no op, registration is not supported at the moment
				{"info", wrap_info_callback(callbacks_.on_info)},
				{"option", wrap_option_callback(callbacks_.on_option)}
			};
		}

		void receive_line(std::string line);
	};



}