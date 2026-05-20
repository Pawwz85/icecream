#pragma once

#include "Move.h"
#include "GameState.h"
#include "lazy_eval_params.h"

template <class MoveIterator>
class StaticMoveOrdering {
	inline void swap(MoveIterator it1, MoveIterator it2);
public:	
	inline void select( const game_state & gs, MoveIterator begin, MoveIterator end, Move TTMove = 0);
};


template <class MoveIterator>
class NoMoveOrdering {
public:
	inline void select(const game_state& gs, MoveIterator begin, MoveIterator end, Move TTMove = 0){};
};


template<class MoveIterator>
inline void StaticMoveOrdering<MoveIterator>::swap(MoveIterator it1, MoveIterator it2)
{
	Move temp = *it1;
	*it1 = *it2;
	*it2 = temp;
}

template<class MoveIterator>
inline void StaticMoveOrdering<MoveIterator>::select(const game_state& gs, MoveIterator begin, MoveIterator end, Move TTMove)
{
	int bestMoveGain = INT_MIN;
	int currentMoveGain;
	for (MoveIterator it = begin; it < end; ++it) {
		uint8_t toSq = Move_Utils::to_sq(*it);
		uint8_t fromSq = Move_Utils::from_sq(*it);

		if (*it == TTMove) {
			swap(it, begin);
			return;
		}

		if (!Move_Utils::is_castle(*it)) {
			currentMoveGain = piece_weights[gs.pieces[toSq]] - (piece_weights[gs.pieces[fromSq]]>>4);
		}
		else {
			currentMoveGain = 50;
		}

		if (Move_Utils::uses_jump(*it))
			currentMoveGain -= jump_weight;

		
#define GetPesto(sq) ( gs.eval.phase * getPestoTableScore_middlegame(gs.props.side_to_move, sq, gs.pieces[sq]) + (32 - gs.eval.phase) * getPestoTableScore_endgame(gs.props.side_to_move, sq, gs.pieces[sq]))
		currentMoveGain += GetPesto(toSq) / 32;
		currentMoveGain -= GetPesto(fromSq) / 32;
#undef GetPesto

		if (currentMoveGain > bestMoveGain) {
			swap(it, begin);
			bestMoveGain =  currentMoveGain;
		}

	}
}
