#pragma once

#include <vector>
#include <ctime>
#include <cmath>
#include <functional>
#include "GameState.h"
#include "MoveGen.h"
#include "TranspositionTable.h"
#include "FreezeHeuristic.h"
#include "SEE.h"
#include "uci_client.h" // for uci complient search method

const int QuiescenceThreshold = 2; // 2 ply for quiesearch
const int freezePreSearchLimit = 2;

const unsigned long stop_condition_frequency_mask = (1ull << 13) - 1;
extern std::atomic_bool grimoire_mode;
extern std::atomic_int32_t grimoire_bounds;
extern std::atomic_int32_t grimoire_suggestion_count;

const int infinity =   100001;
const int mateValue = -100000;

const size_t max_ply = 50;

inline int decrementMateDistance(int score) {

	if (score > 90000)
		return score - 1;
	if (score < -90000)
		return score + 1;
	return score;
}

inline bool isMateValue(int score) {
	return score > 90000;
}

inline int getMateDistance(int score) {
	return 100000 - abs(score);
}

struct Grimoire_Suggestion {
	Move move;
	int eval;
};

#define SEARCH_TEMPLATE_PARAMS template <class EvalFunction, class MoveOrdering>
#define SEARCH Search<EvalFunction, MoveOrdering>

SEARCH_TEMPLATE_PARAMS
class Search {
	EvalFunction evaluator;

	uint64_t node_counter;
	uint64_t terminal_node_counter;

	uint64_t root_hash;
	uint16_t root_pos_history_index;

	Move killerMoves[max_ply][2];

	std::vector<Grimoire_Suggestion> grimoireSuggestions;

	void insertSuggestion(const Grimoire_Suggestion& suggestion);

	std::string extractPv(const game_state& gs) const;

	template <bool quiescence>
	inline Move _pickBestMove(game_state& gs, int depth, int alpha, int beta, int& outEval);

	template <bool quiescence>
	inline void generateFreezeMoves(game_state& gs, int depth, int alpha, int beta, Move base, Move*& it);

	inline void trySavingKillerMove(const game_state & gs, int pliesSinceRoot, Move killerMove);

public:

	class IStopCondition {
	public:
		virtual ~IStopCondition() {};
		virtual bool condition_reached() = 0;
	};

	class BasicStopCondition: public IStopCondition{
		std::function<bool()> _pred;
	public:
		BasicStopCondition(std::function<bool()> func) : _pred(func) {};
		bool condition_reached() {
			return _pred();
		}
	};

private:
	std::vector<IStopCondition*> stop_conditions;
public:


	Search() : evaluator() {

	};

	virtual ~Search() {
		clear_stop_conditions();
	};

	template<bool quiescence>	
	int alpha_beta_search(game_state& gs, int depth, int alpha, int beta, Move & bestMove) {
		Bitboard us, them;
	
		bool can_use_freeze = gs.freeze_spell[gs.props.side_to_move].couldown == 0 && gs.freeze_spell[gs.props.side_to_move].spells_left > 0;
		bool isRootNode = gs.pos_his_index == root_pos_history_index;
		bool isGrimoireNode = isRootNode && grimoire_mode;
		
		int currentPly = gs.pos_his_index - root_pos_history_index;
		int nextDepth;

		if (!isRootNode)
			bestMove = 0;

		if constexpr (!quiescence) {

			if (gs.inCheck())
				depth++;	// apply extension when in check

			if (depth == 0) 
				return alpha_beta_search<true>(gs, depth, alpha, beta, bestMove);
			nextDepth = depth - 1;
		}
		else
			nextDepth = 0;

		/*
			Step 1. Check stop conditions
		*/

		if (gs.props.side_to_move == GameStateUtils::White) {
			us = gs.white;
			them = gs.black;
		}
		else {
			them = gs.white;
			us = gs.black;
		}

		++node_counter;

		if ((us & gs.kings) == 0) {
			++terminal_node_counter;
			return mateValue;
		}
		
		if (GameStateUtils::is_repetition(gs) && !isRootNode) {
			++terminal_node_counter;
			return 0;
		}

		if ((node_counter & stop_condition_frequency_mask) == 0) {
			for (IStopCondition* condition : stop_conditions)
				if (condition->condition_reached())
					throw std::exception("Stop condition reached");
		}

		/*
			Step  2: Initialize local variables
		*/
		int local_score;
		GameStateUtils::Colour side = (GameStateUtils::Colour)gs.props.side_to_move;

		bool foundMoveGreaterThanAlpha = false;
		int currentValue = mateValue;

		TTEntry& TT_entry = transpositionTable[calculate_index(gs.zobrist_hash)];

		TTEntry::Flag flag = TTEntry::EXACT;

		Move TTMove = 0;
		MoveOrdering move_ordering;

		/*
			Step 3. In quiescence search, set current value to current evaluation
		*/
		if constexpr (quiescence) {
			int eval = evaluator(gs);
			currentValue = eval;

			// we are already better than beta, cutoff
			if (eval >= beta)
				return eval;

			// if eval is GE than alpha we will not fail low 
			if (eval >= alpha) {
				foundMoveGreaterThanAlpha = true;
				alpha = eval;
			} 
		}

		/*
			Step 4. Check transposition table
		*/
		if (TT_entry.key == gs.zobrist_hash && !grimoire_mode) {
			if (TT_entry.depth >= depth) {
				int TTScore = TT_entry.eval;
				if (TT_entry.flag == TTEntry::EXACT) {
					bestMove = TT_entry.bestMove;
					return TTScore;
				}


				if (TT_entry.flag == TTEntry::LOWER) {
					alpha = std::max(alpha, TTScore);

					if (alpha >= beta) {
						bestMove = TT_entry.bestMove;
						return TTScore;
					}

				}

				if (TT_entry.flag == TTEntry::UPPER && TTScore < alpha) {
					bestMove = TT_entry.bestMove;
					return TTScore;
				};

			}
			TTMove = TT_entry.bestMove;
		}
		
		/*
			Step 4a. If TTMove is present, search it first
		*/
		game_state copy;
		Move ignored;
		move_gen::MoveCandidate move_buffer[1024];
		move_gen::MoveCandidate* end = move_buffer;

		if (TTMove != 0) {
			copy = gs;
			GameStateUtils::make_move(copy, TTMove);

			local_score = -decrementMateDistance(alpha_beta_search<quiescence>(copy, nextDepth, -beta, -alpha, ignored));

			if (local_score >= alpha) {
				alpha = local_score;
				foundMoveGreaterThanAlpha = true;
			}

			if (local_score > currentValue) {
				currentValue = local_score;
				bestMove = TTMove;
			}

			if (alpha >= beta) {
				flag = TTEntry::LOWER;
				bestMove = TTMove;
				goto TT_WRITE;
			}
		}
		

		/*
			Step 5. Generate move candidates
		*/

		if constexpr (!quiescence)
			move_gen::move_generator(gs, end);
		else
			move_gen::quiescence_move_generator(gs, end);

		// checkmate or stalemate detection
		if constexpr (!quiescence)
			if (end == move_buffer)
				return gs.inCheck() ? mateValue : 0;


		/*
			Step 6. Search cheap moves
		*/
		for (move_gen::MoveCandidate* move_it = move_buffer; move_it != end; ++move_it) {
			copy = gs;
			move_ordering.select(copy, move_it, end, TTMove, killerMoves[currentPly][0], killerMoves[currentPly][1]);

			GameStateUtils::make_move(copy, move_it->base);

			local_score = -decrementMateDistance(alpha_beta_search<quiescence>(copy, nextDepth, -beta, -alpha, ignored));

			if (local_score >= alpha) {
				alpha = local_score;
				foundMoveGreaterThanAlpha = true;
			}

			if (isGrimoireNode && local_score > -grimoire_bounds) {
				insertSuggestion({ move_it->base, local_score });
			};

			if (local_score > currentValue) {
				currentValue = local_score;
				bestMove = move_it->base;
			}

			if (alpha >= beta) {
				flag = TTEntry::LOWER;
				bestMove = move_it->base;
				trySavingKillerMove(gs, currentPly, bestMove);
				goto TT_WRITE;
			}
		};
		
		/*
			Step 7. Consider spells
		*/
		if (can_use_freeze) {

			Move freeze_moves[2048];
			Move* freeze_moves_end;

			for (move_gen::MoveCandidate* candidate = move_buffer; candidate != end; ++candidate) {
				if (candidate->movePolicy == move_gen::SpellPolicy_SpellCanBeAddedByPlayer) {
					freeze_moves_end = freeze_moves;
					generateFreezeMoves<quiescence>(gs, depth, alpha, beta, candidate->base, freeze_moves_end);

					for (Move* move_it = freeze_moves; move_it != freeze_moves_end; ++move_it) {
						copy = gs;

						GameStateUtils::make_move(copy, *move_it);

						local_score = -decrementMateDistance(alpha_beta_search<quiescence>(copy, nextDepth, -beta, -alpha, ignored));

						if (local_score >= alpha) {
							alpha = local_score;
							foundMoveGreaterThanAlpha = true;
						}

						if (isGrimoireNode && local_score > -grimoire_bounds) {
							insertSuggestion({ *move_it, local_score });
						};

						if (local_score > currentValue) {
							currentValue = local_score;
							bestMove = *move_it;
						}

						if (alpha >= beta) {
							flag = TTEntry::LOWER;
							bestMove = *move_it;
							goto TT_WRITE;
						}
					};
				}
			}
				
			}


		flag = foundMoveGreaterThanAlpha ? TTEntry::EXACT : TTEntry::UPPER;

	TT_WRITE:
		TT_entry.key = gs.zobrist_hash;
		TT_entry.depth = depth;
		TT_entry.flag = flag;
		TT_entry.eval = currentValue;
		TT_entry.bestMove = bestMove;

		return currentValue;
		
	}

	void pickBestMove(game_state& gs, int depth, Move & outMove, int & outEvaluation, UCI::UCIOutputStream& out) {
		node_counter = 0;
		terminal_node_counter = 0;
		const int alpha = -infinity;
		const int beta = infinity;

		root_hash = gs.zobrist_hash;
		grimoireSuggestions.clear();
		outEvaluation = alpha_beta_search<false>(gs, depth, alpha, beta, outMove);

		if (grimoire_mode) {
			std::string grimoireInfo = "string grimoire";
			for (size_t i = 0; i < grimoire_suggestion_count && i < grimoireSuggestions.size(); ++i) {
				grimoireInfo += " " + UCI::formatMove(grimoireSuggestions[i].move);
			}
			out << grimoireInfo;
		};

	}

	Move uciCompliantIterativeDeepening(game_state& gs, UCI::go_params& params, UCI::UCIOutputStream & out);

	void clear_stop_conditions();
	void add_stop_condition(IStopCondition * condition);
};

SEARCH_TEMPLATE_PARAMS
template <bool quiescence>
inline void SEARCH::generateFreezeMoves(game_state& gs, int depth, int alpha, int beta, Move base, Move*& it)
{
	auto & side = gs.props.side_to_move;

	// We don't want the opponent response to contain freeze
	uint8_t them = 1 - side;

	int ignored = 0;

	game_state copy = gs;
	GameStateUtils::make_move(copy, base);
			
	// call 'null freeze'
	GameStateUtils::cast_null_freeze(copy, side);

	Move killer = _pickBestMove<quiescence>(copy, depth / 2, -beta, -alpha, ignored);

	if (killer)
		generate_freezes(gs, base, killer, it);
}

SEARCH_TEMPLATE_PARAMS
template <bool quiescence>
inline Move SEARCH::_pickBestMove(game_state& gs, int depth, int alpha, int beta, int & outEval)
{
	Move result = 0;
	outEval = alpha_beta_search<quiescence>(gs, depth, alpha, beta, result);
	return result;
}

SEARCH_TEMPLATE_PARAMS
inline void SEARCH::insertSuggestion(const Grimoire_Suggestion& suggestion) {
	for(auto it = grimoireSuggestions.cbegin(); it != grimoireSuggestions.cend(); ++it)
		if (suggestion.eval > it->eval) {
			grimoireSuggestions.insert(it, suggestion);
			return;
		}
	grimoireSuggestions.push_back(suggestion);
};

SEARCH_TEMPLATE_PARAMS
inline Move SEARCH::uciCompliantIterativeDeepening(game_state& gs, UCI::go_params& params, UCI::UCIOutputStream& out)
{
	clock_t deadline;

	node_counter = 0;
	root_pos_history_index = gs.pos_his_index;

	for (auto i = 0; i < max_ply; ++i)
		killerMoves[i][0] = killerMoves[i][1] = 0;

	if (params.move_time > 0 && !params.infinite_mode) {
		deadline = clock() + params.move_time - 10;   
		add_stop_condition(new BasicStopCondition([deadline]() {return clock() > deadline; }));
	}

	const auto& movingSideTimeControl = (gs.props.side_to_move == GameStateUtils::White) ? params.white_time_control : params.black_time_control;
	
	if (movingSideTimeControl.time_left > 0) {
		deadline = clock() + movingSideTimeControl.time_left / 20 + movingSideTimeControl.time_inc / 2;
		add_stop_condition(new BasicStopCondition([deadline]() {return clock() > deadline; }));
	}

	if (params.nodes_limit > 0) {
		add_stop_condition(new BasicStopCondition([this, &params]() {return this->terminal_node_counter > params.nodes_limit; }));
	}

	
	unsigned int max_ply = (params.depth_limit > 0) ? params.depth_limit : -1;
	
	int eval = 0;
	Move result = 0;
	pickBestMove(gs, 1, result, eval, out);

	int32_t target_depth = 2;
	bool exception_found = false;

	clock_t iter_start;

	out << UCI::formatString("score %s", UCI::formatScore(eval).c_str());

	while (target_depth <= max_ply && !exception_found ) {
		iter_start = clock();
		try {
			pickBestMove(gs, target_depth, result, eval, out);
			std::string pvLine = extractPv(gs);
			out << UCI::formatString("depth %d", target_depth);
			if(!pvLine.empty())	out << UCI::formatString("pv %s",	 pvLine.c_str());
			out << UCI::formatString("score %s", UCI::formatScore(eval).c_str());
			out << UCI::formatString("nodes %d", int32_t(node_counter));
			out << UCI::formatString("nps %d", int32_t(CLOCKS_PER_SEC * node_counter / (float((clock() - iter_start) + 0.0000000001))));
		}
		catch (std::exception e) {
			exception_found = true;
		}; // catch stop condition reached exceptions

		++target_depth;
	}

	return result;

}

SEARCH_TEMPLATE_PARAMS
std::string SEARCH::extractPv(const game_state& gs) const {
	std::string result = " ";
	game_state current_state = gs;
	bool hasNext;

	do {
		hasNext = false;
		size_t index = calculate_index(current_state.zobrist_hash);
		const TTEntry& entry = transpositionTable[index];

		if (entry.key != current_state.zobrist_hash)
			break;

		if (entry.bestMove != 0) {
			result += UCI::formatMove(entry.bestMove) + " ";
			GameStateUtils::make_move(current_state, entry.bestMove);
			hasNext = true;
		}	else break;

	} while (hasNext);
	result.pop_back(); // remove trailling space character

	return result;
};

SEARCH_TEMPLATE_PARAMS
inline void SEARCH::clear_stop_conditions()
{
	for (IStopCondition* p : stop_conditions)
		delete p;

	stop_conditions.clear();
}

SEARCH_TEMPLATE_PARAMS
inline void SEARCH::add_stop_condition(IStopCondition* condition)
{
	stop_conditions.push_back(condition);
}

SEARCH_TEMPLATE_PARAMS
inline void SEARCH::trySavingKillerMove(const game_state& gs, int pliesSinceRoot, Move killerMove){

	// TODO: discard promotions to and enpassants too
	bool isQuiet = gs.pieces[Move_Utils::to_sq(killerMove)] == Piece::None &&
		!Move_Utils::uses_freeze(killerMove);

	if (isQuiet) {
		killerMoves[pliesSinceRoot][0] = killerMoves[pliesSinceRoot][1];
		killerMoves[pliesSinceRoot][1] = killerMove;
	};

}


#undef SEARCH_TEMPLATE_PARAMS
#undef SEARCH