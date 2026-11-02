#pragma once

#include "Move.h"
#include "GameState.h"
#include "History.h"
#include "lazy_eval_params.h"
#include "SEE.h"


template <class MoveIterator>
class StaticMoveOrdering {

	const int victim_value[7] = {
	0, // None
	1000, // Pawn
	4000, // Rook
	2000, // knight
	3000, // Bishop
	5000, // Queen
	0	  // king - 0 since we we will return king capture immediately
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

	inline void swap(MoveIterator it1, MoveIterator it2);
public:	
	inline void select( const game_state & gs, MoveIterator begin, MoveIterator end, Move TTMove = 0, Move killer1 = 0, Move killer2 = 0);
	inline void selectFreeze(const game_state& gs, MoveIterator begin, MoveIterator end, Move TTMove = 0, Move killer1 = 0, Move killer2 = 0);
};


template <class MoveIterator>
class NoMoveOrdering {
public:
	inline void select(const game_state& gs, MoveIterator begin, MoveIterator end, Move TTMove = 0, Move killer1 = 0, Move killer2 = 0){};
	inline void selectFreeze(const game_state& gs, MoveIterator begin, MoveIterator end, Move TTMove = 0, Move killer1 = 0, Move killer2 = 0){};
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
	int bestMoveHistory = INT_MIN;
	int currentMoveGain, currentMoveHistory;

	for (MoveIterator it = begin; it < end; ++it) {
		uint8_t toSq = Move_Utils::to_sq(it->base);
		uint8_t fromSq = Move_Utils::from_sq(it->base);
		
		if (gs.pieces[toSq] == Piece::King) {
			swap(it, begin);
			return;
		}

		if (it->base == TTMove) {
			swap(it, begin);
			return;
		}

		currentMoveHistory = currentMoveGain = 0;

		if (it->base == killer1) {
			currentMoveGain = 0;
			currentMoveHistory = INT_MAX;
			goto SWAP;
		}

		if (it->base == killer2) {
			currentMoveGain = 0;
			currentMoveHistory = INT_MAX;
			goto SWAP;
		}

		if (gs.pieces[toSq] != None) {
			int see = static_exchange_evaluation(gs, fromSq, toSq);
			if (see >= 0)
				currentMoveGain = victim_value[gs.pieces[toSq]] + attacker_value[gs.pieces[fromSq]];
			else
				currentMoveGain = see;
		} 
		
		currentMoveHistory = historyTable[gs.props.side_to_move][gs.pieces[fromSq]][toSq];
		
#define GetPesto(sq) ( gs.eval.phase * getPestoTableScore_middlegame(gs.props.side_to_move, sq, gs.pieces[sq]) + (32 - gs.eval.phase) * getPestoTableScore_endgame(gs.props.side_to_move, sq, gs.pieces[sq]))
		currentMoveHistory += GetPesto(toSq) / 32;
		currentMoveHistory -= GetPesto(fromSq) / 32;
#undef GetPesto

		SWAP:

		if (currentMoveGain > bestMoveGain ||
			(currentMoveGain == bestMoveGain && currentMoveHistory > bestMoveHistory)) {
			swap(it, begin);
			bestMoveGain =  currentMoveGain;
			bestMoveHistory = currentMoveHistory;
		}

	}
}

template<class MoveIterator>
inline void StaticMoveOrdering<MoveIterator>::selectFreeze(const game_state& gs, MoveIterator begin, MoveIterator end, Move TTMove, Move killer1, Move killer2)
{
	int bestMoveGain = INT_MIN;
	int bestMoveHistory = INT_MIN;
	int currentMoveGain, currentMoveHistory;

	for (MoveIterator it = begin; it < end; ++it)
		if(it->movePolicy == move_gen::SpellPolicy_SpellCanBeAddedByPlayer)
		{
			uint8_t toSq = Move_Utils::to_sq(it->base);
			uint8_t fromSq = Move_Utils::from_sq(it->base);

			if (it->base == killer1) {
				swap(it, begin);
				return;
			}

			if (it->base == killer2) {
				swap(it, begin);
				return;
			}

		currentMoveGain = currentMoveHistory = 0;
		
		if (!Move_Utils::is_castle(it->base)) {
			if (gs.pieces[toSq] != None) 
					currentMoveGain = victim_value[gs.pieces[toSq]] + attacker_value[gs.pieces[fromSq]];
		}

		currentMoveHistory = historyTable[gs.props.side_to_move][gs.pieces[fromSq]][toSq];


#define GetPesto(sq) ( gs.eval.phase * getPestoTableScore_middlegame(gs.props.side_to_move, sq, gs.pieces[sq]) + (32 - gs.eval.phase) * getPestoTableScore_endgame(gs.props.side_to_move, sq, gs.pieces[sq]))
		currentMoveHistory += GetPesto(toSq) / 32;
		currentMoveHistory -= GetPesto(fromSq) / 32;
#undef GetPesto

		SWAP :

		if (currentMoveGain > bestMoveGain ||
			(currentMoveGain == bestMoveGain && currentMoveHistory > bestMoveHistory)) {
			swap(it, begin);
			bestMoveGain = currentMoveGain;
			bestMoveHistory = currentMoveHistory;
		}

	}

}
