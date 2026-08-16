#pragma once

#include "Move.h"
#include "GameState.h"
#include "lazy_eval_params.h"

template <class MoveIterator>
class StaticMoveOrdering {
	inline void swap(MoveIterator it1, MoveIterator it2);
public:	
	inline void select( const game_state & gs, MoveIterator begin, MoveIterator end, Move TTMove = 0, Move killer1 = 0, Move killer2 = 0);
};


template <class MoveIterator>
class NoMoveOrdering {
public:
	inline void select(const game_state& gs, MoveIterator begin, MoveIterator end, Move TTMove = 0, Move killer1 = 0, Move killer2 = 0){};
};


template<class MoveIterator>
inline void StaticMoveOrdering<MoveIterator>::swap(MoveIterator it1, MoveIterator it2)
{
	move_gen::MoveCandidate temp = *it1;
	*it1 = *it2;
	*it2 = temp;
}

template<class MoveIterator>
inline void StaticMoveOrdering<MoveIterator>::select(const game_state& gs, MoveIterator begin, MoveIterator end, Move TTMove, Move killer1, Move killer2)
{
	int bestMoveGain = INT_MIN;
	int currentMoveGain;

	const int victim_value[7] = {
		0, // None
		1000, // Pawn
		4000, // Rook
		2000, // knight
		3000, // Bishop
		5000, // Queen
		0	  // king - 0 since we we will return king capture immediataly
	};

	const int attacker_value[7] = {
		0, // None
		500, // Pawn
		200, // Rook
		400, // Knight
		300, // Bishop
		100, // Queen
		0	// king
	};

	for (MoveIterator it = begin; it < end; ++it) {
		uint8_t toSq = Move_Utils::to_sq(it->base);
		uint8_t fromSq = Move_Utils::from_sq(it->base);
		
		if (gs.pieces[toSq] == Piece::King) {
			swap(it, begin);
			return;
		}

		if (it->base == TTMove) {
			currentMoveGain = INT_MAX;
			goto SWAP;
		}

		if (it->base == killer1) {
			currentMoveGain = 500;
			goto SWAP;
		}

		if (it->base == killer2) {
			currentMoveGain = 500;
			goto SWAP;
		}

		if (!Move_Utils::is_castle(it->base)) {
			currentMoveGain = victim_value[gs.pieces[toSq]] + attacker_value[gs.pieces[fromSq]];
		}
		else {
			currentMoveGain = 300;
		}

		if (Move_Utils::uses_jump(it->base))
			currentMoveGain -= 1000;

		
#define GetPesto(sq) ( gs.eval.phase * getPestoTableScore_middlegame(gs.props.side_to_move, sq, gs.pieces[sq]) + (32 - gs.eval.phase) * getPestoTableScore_endgame(gs.props.side_to_move, sq, gs.pieces[sq]))
		currentMoveGain += GetPesto(toSq) / 32;
		currentMoveGain -= GetPesto(fromSq) / 32;
#undef GetPesto

		SWAP:

		if (currentMoveGain > bestMoveGain) {
			swap(it, begin);
			bestMoveGain =  currentMoveGain;
		}

	}
}
