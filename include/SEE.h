#pragma once

#include <cmath>
#include "Bitboard.h"
#include "GameState.h"
#include "MoveGen.h" // for get_square_attackers

inline Bitboard get_least_valuable_attacker(const game_state& gs, Bitboard attack_mask, bool isBlack, Piece& piece) {
	Bitboard subset;
	Bitboard us = isBlack ? gs.black : gs.white;
	attack_mask &= us;

#define CHECK_ATTACKER_TYPE(gsEntry, pType) subset = gs.gsEntry & attack_mask; \
	if (subset) { \
		piece = pType; \
		return subset ^ (subset & (subset - 1)); \
	}

	CHECK_ATTACKER_TYPE(pawns, Pawn);
	CHECK_ATTACKER_TYPE(knights, Knight);
	CHECK_ATTACKER_TYPE(bishops, Bishop);
	CHECK_ATTACKER_TYPE(rooks, Rook);
	CHECK_ATTACKER_TYPE(queens, Queen);
	CHECK_ATTACKER_TYPE(kings, King);


#undef CHECK_ATTACKER_TYPE

	return 0ull;
};

inline Bitboard see_consider_xray(const game_state& gs, const Bitboard& occ, uint8_t origin) {
	Bitboard mask = (gs.bishops | gs.queens) & move_gen::Magics[origin][move_gen::Diagonal].getAttacks(occ).primary;
	mask |= (gs.rooks | gs.queens) & move_gen::Magics[origin][move_gen::Orthogonal].getAttacks(occ).primary;
	return mask;
};

inline int static_exchange_evaluation(const game_state& gs, uint8_t fromSq, uint8_t toSq) {
	int gain[32], d = 0;
	Bitboard mayXray = gs.pawns | gs.rooks | gs.bishops | gs.queens;
	Bitboard fromSet = Bitboards::square[fromSq];
	Bitboard occupied = gs.white | gs.black;
	Bitboard attack_mask = move_gen::get_square_attackers(gs, toSq);
	Bitboard seen_attackers = attack_mask;

	Piece target = (Piece)gs.pieces[toSq];
	Piece attackingPiece= (Piece)gs.pieces[fromSq];

	gain[d] = piece_weights[target];

	bool isBlack = gs.props.side_to_move;

	do {
		isBlack = !isBlack;
		d++;
		gain[d] = piece_weights[attackingPiece] - gain[d - 1];
		attack_mask ^= fromSet;
		occupied ^= fromSet;

		if (fromSet & mayXray) {
			Bitboard tmp = see_consider_xray(gs, occupied, toSq);
			attack_mask |= tmp & ~seen_attackers;
			seen_attackers = tmp;
		}
		
		fromSet = get_least_valuable_attacker(gs, attack_mask, isBlack, attackingPiece);
	} while (fromSet);

	while (--d)
		gain[d - 1] = -std::max(-gain[d-1], gain[d]);

	return gain[0];
};