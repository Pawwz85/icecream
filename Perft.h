#pragma once
#include "GameState.h"
#include "MoveGen.h"


void __perft_internal(uint64_t& cnt, game_state& gs, int8_t depth);


struct perft_results {
	uint64_t node_counter;
	double seconds;
	double node_per_seconds;
};

perft_results perft(std::string fen, int8_t depth);