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

const unsigned long stop_condition_frequency_mask = (1ull << 11) - 1;
extern std::atomic_bool grimoire_mode;
extern std::atomic_int32_t grimoire_bounds;
extern std::atomic_int32_t grimoire_suggestion_count;

const int infinity =   100001;
const int mateValue = -100000;

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
	std::vector<Grimoire_Suggestion> grimoireSuggestions;

	void insertSuggestion(const Grimoire_Suggestion& suggestion);

	template <bool quiescence>
	int _alpha_beta_search(game_state& gs, int depth, int alpha, int beta, Move& bestMove, Move* begin, Move* end);
	template <bool quiescence>
	inline Move _pickBestMove(game_state& gs, int depth, int alpha, int beta, int& outEval);
	//int search_moves_with_freeze(game_state& gs, int depth, int alpha, int beta, FreezeHeuristic* heuristic);
	template <bool quiescence>
	int search_moves_without_freeze(game_state& gs, int depth, int alpha, int beta, Move & bestMove);

	template <bool quiescence>
	inline void generateFreezeMoves(game_state& gs, int depth, int alpha, int beta, Move*& it);

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
	
		bool can_use_freeze = gs.freeze_spell[gs.props.side_to_move].couldown == 0 && gs.freeze_spell[gs.props.side_to_move].spells_left > 0 && depth >= freezePreSearchLimit;
		bool isRootNode = gs.pos_his_index == root_pos_history_index;

		if constexpr (!quiescence) {
			if (depth <= QuiescenceThreshold) {
				return alpha_beta_search<true>(gs, depth, alpha, beta, bestMove);
			}
		}


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
		
		if (depth == 0) {
			++terminal_node_counter;
			return evaluator(gs);
		}
			
		if(!can_use_freeze)
			return search_moves_without_freeze<quiescence>(gs, depth, alpha, beta, bestMove);
		else {
			Move freeze_buffer[4096];
			Move* end = freeze_buffer;
			generateFreezeMoves<quiescence>(gs, depth, alpha, beta, end);
			return _alpha_beta_search<quiescence>(gs, depth, alpha, beta, bestMove, freeze_buffer, end);
		}

		
	}

	void pickBestMove(game_state& gs, int depth, Move & outMove, int & outEvaluation, UCI::UCIOutputStream& out) {
		node_counter = 0;
		terminal_node_counter = 0;
		const int alpha = -infinity;
		const int beta = infinity;

		root_hash = gs.zobrist_hash;
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
inline void SEARCH::generateFreezeMoves(game_state& gs, int depth, int alpha, int beta, Move*& it)
{
	move_gen::MoveCandidate base_search[4096];
	move_gen::MoveCandidate* end = base_search;
	
	if constexpr (!quiescence)
		move_gen::move_generator(gs, end);
	else
		move_gen::quiescence_move_generator(gs, end);

	auto & side = gs.props.side_to_move;

	// We don't want the opponent response to contain freeze
	uint8_t them = 1 - side;
	uint8_t enemyCooldown = std::max((uint16_t)1, gs.freeze_spell[them].couldown);

	int ignored = 0;


	for (move_gen::MoveCandidate* m = base_search; m < end; ++m) 
		if (m->movePolicy == move_gen::SpellPolicy_SpellCanBeAddedByPlayer) {
			game_state copy = gs;
			GameStateUtils::make_move(copy, m->base);
			
			// call 'null freeze'
			GameStateUtils::cast_null_freeze(copy, side);

			// prevent opponent for freezing this turn
			GameStateUtils::set_freeze_cooldown(copy, them, enemyCooldown);

			Move killer = _pickBestMove<quiescence>(copy, depth / 2, -beta, -alpha, ignored);
			generate_freezes(gs, m->base, killer, it);
		}
		else {
			*it = m->base;
			++it;
		}
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
template <bool quiescence>
inline int SEARCH::_alpha_beta_search(game_state& gs, int depth, int alpha, int beta, Move& bestMove, Move* begin, Move* end)
{

	if ((terminal_node_counter & stop_condition_frequency_mask) == 0) {

		for (IStopCondition* condition : stop_conditions)
			if (condition->condition_reached())
				throw std::exception("Stop condition reached");
	}
	
	int local_score;
	int local_depth = depth;
	GameStateUtils::Colour side = (GameStateUtils::Colour)gs.props.side_to_move;
	
	bool foundMoveGreaterThanAlpha = false;
	int currentValue = mateValue;


	if constexpr (quiescence) {
		int eval = evaluator(gs);

		// we are already better than beta, cutoff
		if (eval >= beta) 
			return eval;

		// if eval is GE than alpha we will not fail low 
		if (eval >= alpha) {
			foundMoveGreaterThanAlpha = true;
			alpha = currentValue = eval;
		}

	}

	TTEntry& TT_entry = transpositionTable[calculate_index(gs.zobrist_hash)];

	TTEntry::Flag flag = TTEntry::EXACT;

	Move TTMove = 0;

	if constexpr (!quiescence) {
		if (end == begin) return gs.inCheck ? mateValue : 0; // checkmate or stalemate
	}
	
	if (TT_entry.key == gs.zobrist_hash) {
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

				if (isMateValue(TTScore))
					local_depth = std::min(depth, getMateDistance(currentValue));
			}

			if (TT_entry.flag == TTEntry::UPPER && TTScore < alpha) {
				bestMove = TT_entry.bestMove;
				return TTScore;
			};
				
		}
		TTMove = TT_entry.bestMove;
	}

	bool grimoire_node = grimoire_mode && gs.zobrist_hash == root_hash;
	if (grimoire_node)
		grimoireSuggestions.clear();


	game_state copy;

	MoveOrdering move_ordering;

	Move ignored;

	for (Move* m = begin; m < end; ++m) {
		copy = gs;
		move_ordering.select(copy, m, end, TTMove);

		// Don't bother examining loosing capture sequences
		if constexpr (quiescence) {
			if (static_exchange_evaluation(gs, Move_Utils::from_sq(*m), Move_Utils::to_sq(*m)) < 0)
				continue; //
		};

		// check if castling is legal
		// TODO: make move generator split out only legal castling
		if (Move_Utils::is_castle(*m)
			&& !Move_Utils::uses_freeze(*m)
			&& move_gen::get_castling_attackers(gs, side, Bitboards::square[Move_Utils::to_sq(*m)] & Bitboards::column[COL_A]))
			continue;

		GameStateUtils::make_move(copy, *m);

		assert(local_depth > 0);
		local_score = -decrementMateDistance(alpha_beta_search<quiescence>(copy, local_depth - 1, -beta, -alpha, ignored));

		if (local_score >= alpha) {
			alpha = local_score;
			foundMoveGreaterThanAlpha = true;
		}

		if (grimoire_node && local_score > -grimoire_bounds) {
			insertSuggestion({ *m, local_score });
		};
		
		if (local_score > currentValue) {
			currentValue = local_score;
			bestMove = *m;

			if (isMateValue(currentValue))
				local_depth = std::min(depth, getMateDistance(currentValue));
		}

		if ( alpha >= beta) {
			flag = TTEntry::LOWER;
			bestMove= *m;
			goto TT_WRITE;
		}
		
	}

	flag = foundMoveGreaterThanAlpha ? TTEntry::EXACT : TTEntry::UPPER;

	if (bestMove == 0)
		bestMove = *begin;

	TT_WRITE:
	TT_entry.key = gs.zobrist_hash;
	TT_entry.depth = depth;
	TT_entry.flag = flag;
	TT_entry.eval = currentValue;
	TT_entry.bestMove = bestMove;

	return currentValue;
}

SEARCH_TEMPLATE_PARAMS
template <bool quiescence>
inline int SEARCH::search_moves_without_freeze(game_state& gs, int depth, int alpha, int beta, Move & bestMove)
{
	// note this function is not called in quiescence search
	// note #2 this function is calls only when freezes are not possible

	Move move_buffer[1024];
	Move* end = move_buffer;
	if constexpr (!quiescence) 
		move_gen::move_generator_legacy_interface(gs, end);
	else 
		move_gen::quiescence_move_generator_legacy_interface(gs, end);
	      
	return _alpha_beta_search<quiescence>(gs, depth, alpha, beta, bestMove, move_buffer, end);
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

	while (target_depth <= max_ply && !exception_found && abs(eval) < 90000 ) {
		iter_start = clock();
		try {
			pickBestMove(gs, target_depth, result, eval, out);
			out << UCI::formatString("depth %d", target_depth);
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

#undef SEARCH_TEMPLATE_PARAMS
#undef SEARCH