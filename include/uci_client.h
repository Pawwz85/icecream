#pragma once
#include <string>
#include <vector>
#include <map>
#include "Move.h"
#include "GameState.h"
#include <iostream>
#include <functional>

/*
	This header file provides UCI utilities dedicated to engine's POV.

*/





namespace UCI {

	// header target C++ 14, so we are forced to implement our own function
	std::string formatString(std::string format, ...);
	std::string formatSquare(uint8_t sq);
	std::string formatMove(Move m);
	std::string formatScore(int score);


	uint8_t parseSquare (const std::string str, int & err);
	Move	parseMove	(const std::string str, const game_state & gs, int & err);

	enum CommandStatus {
		Checking,
		Ok,
		Error
	};

	std::string formatCommandStatus(CommandStatus status);
	
	enum OptionType {
		Check,
		Spin,
		Combo,
		Button,
		String
	};

	struct combo_option {
		std::string default_;
		std::vector<std::string> supported_values;
	};

	struct spin_option {
		int32_t default_;
		int32_t min;
		int32_t max;
	};

	
	// deprecated
	struct Option {
		std::string id;
		OptionType type;
		
		/*
			The actual type of content depends on option type:

			check -> bool*
			spin -> spin_wheel*
			combo -> combo*
			button -> nullptr
			string -> std::string*
		*/
		
		void* content;
	};

	/*
		The 'Option' struct above is troublesome for several reasons, mostly because their struct nature suggests their are not possessing
		ownership over the 'content*' pointer content! To fix it, the MemSafeOption provides a set of enhancements focused on type safety and
		resource lifecycle management
	*/
	class MemSafeOption {
		std::string id_;
		OptionType type_;
		void* content;
		
		template <class T>
		void  dispose_content_internal() {
			T* view = (T*)content;
			delete view;
		}

		void dispose_content();

		void* deep_copy_content() const;
	public:
		MemSafeOption() : id_("<empty>"), type_(Button), content(nullptr) {};
		MemSafeOption(std::string id) : id_(id), type_(Button), content(nullptr) {};  // creates a 'button' type option 
		MemSafeOption(std::string id, bool default_) : id_(id), type_(Check), content(new bool(default_)) {}; // creates a 'check' type option with the provided default value 
		MemSafeOption(std::string id, std::string default_) : id_(id), type_(String), content(new std::string(default_)) {}; // creates a 'check' type option with the provided default value 
		MemSafeOption(std::string id, const char* default_) : id_(id), type_(String), content(new std::string(default_)) {}; // c-string option constructor. 
		MemSafeOption(std::string id, const spin_option& content) : id_(id), type_(Spin), content(new spin_option(content)) {};
		MemSafeOption(std::string id, const combo_option& content) : id_(id), type_(Combo), content(new combo_option(content)) {};
		MemSafeOption(const MemSafeOption& other): id_(other.id_), type_(other.type_), content(other.deep_copy_content()) {};
		MemSafeOption(MemSafeOption&& other) noexcept : id_(other.id_), type_(other.type_), content(other.content) { other.content = nullptr; other.type_ = Button; };
		~MemSafeOption() { dispose_content(); };

		const OptionType  & type() const { return type_; };
		const std::string& id()  const { return id_; };

		std::string& str_content() { return *(std::string*)content; };
		const std::string& str_content() const { return *(std::string*)content; };

		bool& check_content() { return *(bool*)content; };
		const bool& check_content() const { return *(bool*)content; };
		
		spin_option & spin_content() { return *(spin_option*)content; };
		const spin_option& spin_content() const { return *(spin_option*)content; };

		combo_option& combo_content() { return *(combo_option*)content; };
		const combo_option& combo_content() const { return *(combo_option*)content; };

		MemSafeOption& operator=(const MemSafeOption& other) {
			dispose_content();
			id_ = other.id_;
			type_ = other.type_;
			content = other.deep_copy_content();
			return *this;
		}
	};

	std::string formatOptionContent(MemSafeOption option);

	enum engine_score_type {
		Lowerband,
		Exact,
		Upperband
	};

	class ClientOutputMessageFormatter {
	private:
	public:
		std::string id_name(std::string engine_name) const;
		std::string id_author(std::string engine_author) const;
		std::string uciok() const ;
		std::string readyok() const;
		std::string best_move(Move best, Move ponder_move = 0) const;
		std::string copyprotection(CommandStatus status) const;
		std::string registration(CommandStatus status) const;
		std::string info(std::string payload) const;
		std::string option(const std::vector<MemSafeOption> & supported_options) const;


		// TODO: add support for 'option' and 'info' commands

	};

	// A collection of utility classes for 

	struct EngineID {
		std::string name;
		std::string author;
	};


	class UCIOK   {};
	class ReadyOK {};

	static const EngineID engine_hello = { "Icecream", "Pawwz85" };



	struct player_time_control {
		bool infinite;
		unsigned int time_left;
		unsigned int time_inc;
	};

	struct go_params {
		std::vector<Move>* search_moves;
		player_time_control white_time_control;
		player_time_control black_time_control;
		unsigned int moves_to_go;
		unsigned int depth_limit; // if 0, depth limit is not set
		unsigned int nodes_limit; // if 0, no limit
		unsigned int move_time; // if 0, use engine time management

		bool ponder_mode;
		bool mate_search_mode;
		bool infinite_mode;
	};

	go_params make_default_go_params();


	class UCIOutputStream {
		ClientOutputMessageFormatter formatter;
		std::ostream* out;

	public:
		
		UCIOutputStream(std::ostream* out) : out(out) {};
		
		inline UCIOutputStream& operator<<(const UCIOK& _) {
			*out << formatter.uciok() << std::endl;
			return *this;
		}
		inline UCIOutputStream& operator<<(const ReadyOK& _) {
			*out << formatter.readyok() << std::endl;
			return *this;
		}

		inline UCIOutputStream& operator<<(std::string string) {
			*out << formatter.info(string) << std::endl;
			return *this;
		}
		inline UCIOutputStream& operator<<(const EngineID& id_object) {
			*out << formatter.id_name(id_object.name) << std::endl;
			*out << formatter.id_author(id_object.author) << std::endl;
			return *this;
		}

		inline UCIOutputStream& operator<<(const Move& move) {
			*out << formatter.best_move(move) << std::endl;
			return *this;
		}

		inline UCIOutputStream& operator<<(const std::vector<MemSafeOption>& supportedOptions) {
			*out << formatter.option(supportedOptions) << std::endl;
			return *this;
		}
	};

	struct UCI_Client_callbacks {
		void (*on_uci)();
		void (*on_debug)(bool);
		void (*on_isready)();
		void (*on_setoption)(std::string id, std::string);
		void (*on_register)(std::string payload);
		void (*on_uci_newgame)();
		void (*on_position)(std::string fen, const std::vector<Move>& moves);
		void (*on_go)(const go_params & params);
		void (*on_stop)();
		void (*on_ponderhit)();
		void (*on_quit)();
		void (*on_unsupported_command)(std::string prompt);
	};

	struct _CommandHandlerSlot {
		std::string command;
		std::function<void(const std::vector<std::string>&)> handler;
	};

	static std::function<void(const std::vector<std::string>&)> noArgumentHandler(void (*function)()) {
		return [function](const std::vector<std::string>& _) {function(); };
	}

	class InputListener {
		const UCI_Client_callbacks callbacks;
		std::vector<_CommandHandlerSlot> router;
		
		UCIOutputStream* out;

		void logError(const std::string & message);

		// argument parsers for various UCI commands
		std::function<void(const std::vector<std::string>&)> wrapDebugCallback();
		std::function<void(const std::vector<std::string>&)> wrapSetOptionCallback();
		//std::function<void(const std::vector<std::string>&)> wrapRegisterCallback();
		std::function<void(const std::vector<std::string>&)> wrapPositionCallback();
		std::function<void(const std::vector<std::string>&)> wrapGoCallback();

		std::vector<Move> _parse_move_vector(const std::vector<std::string>& args, size_t moves_index, game_state& gs, int& error_code);

	public:
		InputListener(UCI_Client_callbacks callbacks, UCIOutputStream* out) : callbacks(callbacks), out(out) {
			
			router = {
				{"uci",			noArgumentHandler(callbacks.on_uci)},
				{"debug",		wrapDebugCallback()}, 
				{"isready",		noArgumentHandler(callbacks.on_isready)},
				{"setoption",	wrapSetOptionCallback()},
				{"register",	[](const std::vector<std::string>& _) {}},	//  no op, registration is not supported at the moment
				{"ucinewgame",	noArgumentHandler(callbacks.on_uci_newgame)},
				{"position",	wrapPositionCallback()},
				{"go",			wrapGoCallback()},
				{"stop",		noArgumentHandler(callbacks.on_stop)},
				{"ponderhit",	noArgumentHandler(callbacks.on_ponderhit)},
				{"quit",		noArgumentHandler(callbacks.on_quit)}
			};
		};
		void receive_line(std::string & line);
	};
};
