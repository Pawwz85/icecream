#pragma once

#include "Bitboard.h"
#include "GameState.h"
#include "MoveGen.h"

namespace FreezeInternals {
	struct FreezeHeuristicCache {
		uint_fast8_t candidates[9];
		bool obsolete[9];
	};

	inline void init_freeze_heuristic_cache(FreezeHeuristicCache& cache, int_fast8_t sq, const game_state& gs) {
		for (int_fast32_t i = 0; i < 9; ++i)
			cache.candidates[i] = 255;
		uint_fast8_t* it = cache.candidates;
		Bitboard area = GameStateUtils::frozen_area[sq];

		if (gs.props.freeze_sq == sq)
			area ^= Bitboards::square[sq]; // rare edge case - chess.com doesn't allow casting freeze on the same square the opponent did on its turn

		Bitboards::bitboard_arr_scan(GameStateUtils::frozen_area[sq] & ~Bitboards::square[sq], it);
	};

	// returns true if 'area1' is strictly preffered to be frozen than 'area2'
	inline bool better_than(game_state& gs, const Bitboard& us, const Bitboard& enemy, const Bitboard& area1, const Bitboard& area2) {

		// return true if and only if:
			// 1. We are freezing the same pieces belonging to 'us', but enemy pieces in area2 are subset of area1
			// 2. We are freezing the same enemy pieces, but our pieces in area1 are subset of those in area2
		Bitboard usFrozen1 = us & area1, usFrozen2 = us & area2;
		Bitboard themFrozen1 = enemy & area1, themFrozen2 = enemy & area2;

		if (usFrozen1 == usFrozen2)
			return (themFrozen2 & themFrozen1) == themFrozen2;

		if (themFrozen1 == themFrozen2)
			return (usFrozen1 & usFrozen2) == usFrozen1;

		return false;
	}

	inline void prune_freeze_candidates(FreezeHeuristicCache& cache, game_state& gs, int_fast8_t forbidden_sq, Bitboard mustContain = 0) {
		Bitboard candidate_areas[9];
		for (int i = 0; i < 9; ++i) {
			candidate_areas[i] = GameStateUtils::frozen_area[cache.candidates[i]];
			cache.obsolete[i] = cache.candidates[i] == 255 || (Bitboards::square[forbidden_sq] & candidate_areas[i]) || ((mustContain & candidate_areas[i]) != mustContain);
		}

		Bitboard us, enemy;
		if (gs.props.side_to_move == GameStateUtils::White) {
			us = gs.white;
			enemy = gs.black;
		}
		else {
			enemy = gs.white;
			us = gs.black;
		}

		for (int i = 0; i < 9; ++i) if (!cache.obsolete[i]) {
			for (int j = 0; j < i; ++j)
				if (!cache.obsolete[j] && better_than(gs, us, enemy, candidate_areas[i], candidate_areas[j]))
					cache.obsolete[j] = true;
				else if (better_than(gs, us, enemy, candidate_areas[j], candidate_areas[i])) {
					cache.obsolete[i] = true;
					break;
				}
		}
	}
};

inline void generate_freezes(game_state& gs, const Move& base, const Move & killer, Move*& it) {
	Bitboard mustContain = 0; // we don't have to contain anything

	if (Move_Utils::is_castle(base)) {
		bool isQueenSideCastling = Bitboards::square[Move_Utils::to_sq(base)] & Bitboards::column[COL_A];
		mustContain = move_gen::get_castling_attackers(gs, (GameStateUtils::Colour)gs.props.side_to_move, isQueenSideCastling);
	}

	if (killer && !Move_Utils::uses_jump(base)) {
		FreezeInternals::FreezeHeuristicCache cache;
		FreezeInternals::init_freeze_heuristic_cache(cache, Move_Utils::from_sq(killer), gs);
		FreezeInternals::prune_freeze_candidates(cache, gs, Move_Utils::from_sq(base));

		for (int i = 0; i < 9; ++i) if (!cache.obsolete[i] && cache.candidates[i] != gs.props.freeze_sq)
			*it++ = Move_Utils::addFreezeSquare(base, cache.candidates[i]);
	};
};