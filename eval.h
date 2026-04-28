#pragma once

#include"GameState.h"
#include "MoveGen.h"
#include "macros.h"
#include "lazy_eval_params.h"

// we are using class, so we can pass eval function as template parameter



// allow to extract and set weight and runtime in order to allow find the best engine using training
class MaterialEvalFunction {
	const int pawn_weight = 100;
	const int bishop_weight = 350;
	const int knight_weight = 300;
	const int rook_weight = 550;
	const int queen_weight = 1000;

	const int jump_weight = 350;
	const int freeze_weight = 150;

public:

	inline int aproximate(const game_state& gs) {
		int score = 0;
		score += pawn_weight * (__popcnt64(gs.pawns & gs.white) - __popcnt64(gs.pawns & gs.black));
		score += bishop_weight * (__popcnt64(gs.bishops & gs.white) - __popcnt64(gs.bishops & gs.black));
		score += knight_weight * (__popcnt64(gs.knights & gs.white) - __popcnt64(gs.knights & gs.black));
		score += rook_weight * (__popcnt64(gs.rooks & gs.white) - __popcnt64(gs.rooks & gs.black));
		score += queen_weight * (__popcnt64(gs.queens & gs.white) - __popcnt64(gs.queens & gs.black));

		score += jump_weight * (gs.jump_spell[GameStateUtils::White].spells_left - gs.jump_spell[GameStateUtils::Black].spells_left);
		score += freeze_weight * (gs.freeze_spell[GameStateUtils::White].spells_left - gs.freeze_spell[GameStateUtils::Black].spells_left);

		return score;
	}

	inline int operator()(const game_state& gs) {
		return aproximate(gs);
	}
};


class StandardEval {
	MaterialEvalFunction materialCounter;

	template <bool white_to_move>
	inline int king_safety(const game_state& gs) {

		Bitboard them, us, all;
		bool enemy_has_jump_left;
		if constexpr (white_to_move) {
			us = gs.white;
			them = gs.black;
			enemy_has_jump_left = gs.jump_spell[GameStateUtils::Black].spells_left > 0;
		}
		else {
			us = gs.black;
			them = gs.white;
			enemy_has_jump_left = gs.jump_spell[GameStateUtils::White].spells_left > 0;
		}
		all = us | them;
		uint8_t king_pos = Bitboards::to_index(us & gs.kings);
		int score = 0;
		
		// Step 1. add penalty for having rooks/queens attacking the king

		// 100 % weight for king on open file, 100% weight for king shielded by one piece, 50% weight for piece shielded with 2 pieces, 25% weight for 3 pieces.
		const static int slider_attack_weight_factor[8] = { 128, 96, 16, 4, 0, 0, 0, 0 };

		Bitboard attackers = move_gen::Magics[king_pos][move_gen::Orthogonal].table[0].primary & (gs.queens | gs.rooks) & them;
		Bitboard blockers;
		uint8_t shield_thickness;
		uint_fast8_t buffer[16];
		uint_fast8_t* end = buffer;

		Bitboards::bitboard_arr_scan(attackers, end);
		for (uint_fast8_t* it = buffer; it < end; ++it) {
			blockers = Bitboards::ray_between_with_caching(king_pos, *it) & all; // enemy pawns can move only orthogonaly most of the time, so their are relatively save shield from rooks
			shield_thickness = __popcnt64(blockers) + !enemy_has_jump_left;
			score -= (350 * slider_attack_weight_factor[shield_thickness]) >> 7;
		}

		attackers = move_gen::Magics[king_pos][move_gen::Diagonal].table[0].primary& (gs.queens | gs.bishops)& them;
		end = buffer;
		Bitboards::bitboard_arr_scan(attackers, end);
		for (uint_fast8_t* it = buffer; it < end; ++it) {
			blockers = Bitboards::ray_between_with_caching(king_pos, *it) & all;
			shield_thickness = __popcnt64(blockers) + !enemy_has_jump_left;
			score -= (350 * slider_attack_weight_factor[shield_thickness]) >> 7;
		}

		return score;

	};

public:
	inline int operator()(const game_state& gs) {
		
		// step 1. Get base eval from lazy evaluation
		int score = aproximate(gs);

		// step 3. King safety scores. 

		score += king_safety<true>(gs) - king_safety<false>(gs);

		return score;
	}

	inline int aproximate(const game_state& gs) {
		int base_eval = gs.eval.material_balance + ((gs.eval.end_gm_score * (32 - gs.eval.phase)) + (gs.eval.mid_gm_score * gs.eval.phase)) / 32;

		int score = base_eval
			+ spell_weights[FREEZE] * (gs.freeze_spell[GameStateUtils::White].spells_left - gs.freeze_spell[GameStateUtils::Black].spells_left);


		Bitboard sliders = gs.bishops | gs.rooks | gs.queens;


		// step 2. Add spell scores. Note that if white has no longer any slider left, jump will plummit in value

		if (sliders & gs.white)
			score += spell_weights[JUMP] * gs.jump_spell[GameStateUtils::White].spells_left;
		else
			score += (spell_weights[JUMP] * gs.jump_spell[GameStateUtils::White].spells_left) >> 3;

		if (sliders & gs.black)
			score -= spell_weights[JUMP] * gs.jump_spell[GameStateUtils::Black].spells_left;
		else
			score -= (spell_weights[JUMP] * gs.jump_spell[GameStateUtils::Black].spells_left) >> 3;
		return score;
	}
};


class LazyEval {
public:

	inline int operator()(const game_state& gs) {
		return aproximate(gs);
	};

	inline int  aproximate(const game_state& gs) {

		int base_eval = gs.eval.material_balance + ((gs.eval.end_gm_score * (32 - gs.eval.phase)) + (gs.eval.mid_gm_score * gs.eval.phase)) / 32;

		int score = base_eval
			+ spell_weights[FREEZE] * (gs.freeze_spell[GameStateUtils::White].spells_left - gs.freeze_spell[GameStateUtils::Black].spells_left)
			+ spell_weights[JUMP] * (gs.jump_spell[GameStateUtils::White].spells_left - gs.jump_spell[GameStateUtils::Black].spells_left);

		return score;
	};
};

/*
	This class is used to heuristically pick the 'freeze' square. This heuristic works as follows: 
	
	1. First generate 'null-freeze'. Null freeze consumes a freeze spell, but doesn't act anything.
	2. For every possible move done with 'null_freeze', record the strongest opponent reply.
	3. After null-freeze search completes, this heuristic will for each move, where to case freeze effectively.
	4. For each move, we now have to consider only:
		- no freeze move (preserving the freeze for the next turn)
		- 9 moves adjacent to the opponent strongest reply figure.
	
		Note, that these 9 freezes to consider is worst case scenario, as this number can be prune down:
		- If two freezes cover the same pieces, they are considered identical.
		- Freeze that covers an extra enemy piece is *nearly always* preffered. 
		- Freeze that doesn't cover our pieces is *nearly always* preffered. 

		Those rules can be used to reduce list of freezes to absolute.
*/

class NullFreezeHeuristic {
	Move killers[512]; // arrays containing opponents best responses
	int index; //index of the node in the parent node.
	int size;

	bool obsolete[9];
	uint_fast8_t candidates[9];

	// returns true if 'area1' is strictly preffered to be frozen than 'area2'
	inline bool better_than(game_state& gs, const Bitboard & us, const Bitboard & enemy, const Bitboard& area1, const Bitboard& area2) {
		
		// return true if and only if:
			// 1. We are freezing the same pieces belonging to 'us', but enemy pieces in area2 are subset of area1
			// 2. We are freezing the same enemy pieces, but our pieces in area1 are subset of those in area2
		Bitboard usFrozen1 = us&area1, usFrozen2 = us & area2;
		Bitboard themFrozen1 = enemy & area1, themFrozen2 = enemy & area2;
		
		if (usFrozen1 == usFrozen2 )
			return (themFrozen2 & themFrozen1) == themFrozen2;

		if (themFrozen1 == themFrozen2)
			return (usFrozen1 & usFrozen2) == usFrozen1;

		return false;

	}

	inline bool is_equivalent(game_state& gs, const Bitboard& area1, const Bitboard& area2) {
		Bitboard blockers = gs.white | gs.black;
		return (blockers & area1) == (blockers & area2);
	}

	inline void prepare_candidates(int_fast8_t sq, const game_state & gs) {
		for (int_fast32_t i = 0; i < 9; ++i) candidates[i] = 255;
		uint_fast8_t* it = candidates;
		Bitboard area = GameStateUtils::frozen_area[sq];
		
		if (gs.props.freeze_sq == sq)
			area ^= Bitboards::square[sq]; // rare edge case - chess.com doesn't allow casting freeze on the same square the opponent did in his turn

		Bitboards::bitboard_arr_scan(GameStateUtils::frozen_area[sq] & ~Bitboards::square[sq], it);
	}

	// if this function becomes bottleneck, use other heuristic to pick the best freezing square first.
	inline void prune_candidates(game_state& gs, int_fast8_t forbidden_sq, Bitboard mustContain = 0) {

		Bitboard candidate_areas[9];
		for (int i = 0; i < 9; ++i) {
			candidate_areas[i] = GameStateUtils::frozen_area[candidates[i]];
			obsolete[i] = candidates[i] == 255 || (Bitboards::square[forbidden_sq] & candidate_areas[i]) || ((mustContain & candidate_areas[i]) != mustContain);
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

		for (int i = 0; i < 8; ++i) if (!obsolete[i]) {
				
			for (int j = i + 1; j < 9; ++j) {
					if(better_than(gs, us, enemy, candidate_areas[i], candidate_areas[j]) || is_equivalent(gs, candidate_areas[i], candidate_areas[j]))
						obsolete[j] = true;
					else if (better_than(gs, us, enemy, candidate_areas[j], candidate_areas[i])) {
						obsolete[i] = true;
						break;
					}
			}
		}
			
	}

public:
	inline void setSize(int size) { this->size = size; };
	inline void setIndex(int i) { index = i; };
	inline void recordBestResponse(const Move& response) {
		killers[index] = response;
	}

	inline void generate_freezes(game_state & gs, int index, const Move& base, Move*& it) {

		/*
			TODO: according to chess.com rules, doing castling while being attacked by unfrozen piece is illegal.
			// TODO: prune base move if it is castling, and king is being attacked or any square being attacked by the unfrozen pieces are being attacked
		
		*/
		Bitboard mustContain = 0; // we don't have to contain anything
		
		if (Move_Utils::is_castle(base)) {
			bool isQueenSideCastling = Bitboards::square[Move_Utils::to_sq(base)] & Bitboards::column[COL_A];
			mustContain = move_gen::get_castling_attackers(gs,(GameStateUtils::Colour) gs.props.side_to_move, isQueenSideCastling);
		}
		else {
			*it++ = base; // always consider that preserving a spell might be beneficial
		}


		
		if (killers[index] && !Move_Utils::uses_jump(base)) {
			prepare_candidates(Move_Utils::from_sq(killers[index]), gs);
			prune_candidates(gs, Move_Utils::from_sq(base));

			for (int i = 0; i < 9; ++i) if (!obsolete[i] && candidates[i] != gs.props.freeze_sq)
				*it++ = Move_Utils::addFreezeSquare(base, candidates[i]);
		}


	}

};


