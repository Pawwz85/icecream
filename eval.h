#pragma once

#include"GameState.h"
#include "MoveGen.h"
#include "macros.h"
#include "lazy_eval_params.h"

// we are using class, so we can pass eval function as template parameter

const int sideMultiplier[2] = { 1, -1 };
const int king_under_slider_attack_penalty = 157;


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
		return aproximate(gs) * sideMultiplier[gs.props.side_to_move];
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

		// 100 % weight for king on open file, 100% weight for king shielded by one piece, 25% weight for piece shielded with 2 pieces, 6.25% weight for 3 pieces.
		const static int slider_attack_weight_factor[8] = { 128, 128, 32, 8, 0, 0, 0, 0 };

		Bitboard attackers = move_gen::Magics[king_pos][move_gen::Orthogonal].table[0].primary & (gs.queens | gs.rooks) & them;
		Bitboard blockers;
		uint8_t shield_thickness;
		uint_fast8_t buffer[16];
		uint_fast8_t* end = buffer;

		Bitboards::bitboard_arr_scan(attackers, end);
		for (uint_fast8_t* it = buffer; it < end; ++it) {
			blockers = Bitboards::ray_between_with_caching(king_pos, *it) & all;
			shield_thickness = __popcnt64(blockers) + !enemy_has_jump_left;
			score -= (king_under_slider_attack_penalty * slider_attack_weight_factor[shield_thickness]) >> 7;
		}

		attackers = move_gen::Magics[king_pos][move_gen::Diagonal].table[0].primary& (gs.queens | gs.bishops)& them;
		end = buffer;
		Bitboards::bitboard_arr_scan(attackers, end);
		for (uint_fast8_t* it = buffer; it < end; ++it) {
			blockers = Bitboards::ray_between_with_caching(king_pos, *it) & all;
			shield_thickness = __popcnt64(blockers) + !enemy_has_jump_left;
			score -= (king_under_slider_attack_penalty * slider_attack_weight_factor[shield_thickness]) >> 7;
		}

		return score;

	};


public:
	inline int operator()(const game_state& gs) {
		
		// step 1. Get base eval from lazy evaluation
		int score = aproximate(gs);

		// step 2. King safety scores. 
		score += king_safety<true>(gs) - king_safety<false>(gs);

		return score * sideMultiplier[gs.props.side_to_move];
	}

	inline int aproximate(const game_state& gs) {
		int base_eval = gs.eval.material_balance;
		int pesto_bonus = ((gs.eval.end_gm_score * (32 - gs.eval.phase)) + (gs.eval.mid_gm_score * gs.eval.phase)) / 32;

		base_eval += pesto_bonus * 11 / 4;

		int score = base_eval;

		// step 2. Add freeze bonus

		score += freezeWeights[gs.freeze_spell[GameStateUtils::White].spells_left];
		score -= freezeWeights[gs.freeze_spell[GameStateUtils::Black].spells_left];

		// step 3. Add jump bonus

		int jump_bonus;

		jump_bonus = gs.eval.slider_values[GameStateUtils::White] * jumpBonusCoefficient[gs.jump_spell[GameStateUtils::White].spells_left];
		jump_bonus >>= 8;

		score += jump_bonus;

		jump_bonus = gs.eval.slider_values[GameStateUtils::Black] * jumpBonusCoefficient[gs.jump_spell[GameStateUtils::Black].spells_left];
		jump_bonus >>= 8;

		score -= jump_bonus;

		return score;
	}
};


class LazyEval {
public:

	inline int operator()(const game_state& gs) {
		return aproximate(gs) * sideMultiplier[gs.props.side_to_move];
	};

	inline int  aproximate(const game_state& gs) {

		int base_eval = gs.eval.material_balance + ((gs.eval.end_gm_score * (32 - gs.eval.phase)) + (gs.eval.mid_gm_score * gs.eval.phase)) / 32;

		int score = base_eval
			+ jump_weight * (gs.jump_spell[GameStateUtils::White].spells_left - gs.jump_spell[GameStateUtils::Black].spells_left);

		score += freezeWeights[gs.freeze_spell[GameStateUtils::White].spells_left];
		score -= freezeWeights[gs.freeze_spell[GameStateUtils::Black].spells_left];

		return score;
	};
};

