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

		if (*it == TTMove) {
			swap(it, begin);
			return;
		}

		if (!Move_Utils::is_castle(*it)) {
			currentMoveGain = piece_weights[Move_Utils::to_sq(*it)] - (piece_weights[Move_Utils::from_sq(*it)]>>4);
		}
		else {
			currentMoveGain = 50;
		}

		if (Move_Utils::uses_jump(*it))
			currentMoveGain -= spell_weights[JUMP];

		if (currentMoveGain > bestMoveGain) {
			swap(it, begin);
			bestMoveGain =  currentMoveGain;
		}

	}
}
