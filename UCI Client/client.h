#pragma once
#include <string>
#include <vector>
#include "../Move.h"
/*
	This header file provides UCI utilities dedicated to engine's POV.

*/



namespace UCI {

	// header target C++ 14, so we are forced to implement our own function
	std::string formatString(std::string format, ...);

	enum CommandStatus {
		Checking,
		Ok,
		Error
	};

	enum OptionType {
		Check,
		Spin,
		Combo,
		Button,
		String
	};

	struct Option {
		std::string id;
		OptionType type;
		void* content;
	};

	enum engine_score_type {
		Lowerband,
		Exact,
		Upperband
	};

	class InfoStore {

	public:
		void set_depth(uint32_t depth);
		void set_selective_depth(uint32_t depth);
		void set_time_ms(uint32_t time);
		void set_nodes(uint32_t nodes);
		void set_pv(Move* pv_buffer, size_t size);
		void set_multi_pv(int k);
		void set_score(int score, engine_score_type type);
		void set_currmove(Move m);
		void set_currmovenumber(uint32_t number);
		void set_hashfull(uint32_t x);
		void set_nps(uint32_t nps);
		void set_tbhits(uint32_t end_game_table_hits);
		void set_cpuload(uint32_t cpu_load);
		void set_string(uint32_t str);
		void set_refutation(Move m, Move * refutation_line_buffer, size_t size);
		void set_curl_lines(Move** lines, size_t * line_sizes, size_t proc_count);
	};


	class ClientOutputMessageFormatter {
	
	private:

	public:
		std::string id_name(std::string engine_name);
		std::string id_author(std::string engine_author);
		std::string uciok();
		std::string readyok();
		std::string best_move(Move best, Move ponder_move = 0);
		std::string copyprotection(CommandStatus status);
		std::string registration(CommandStatus status);
		std::string option(const std::vector<Option> & supported_options);


		// TODO: add support for 'option' and 'info' commands

	};

}