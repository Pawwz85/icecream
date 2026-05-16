#pragma once

#include <vector>
#include <ctime>
#include <functional>
#include "GameState.h"
#include "MoveGen.h"
#include "TranspositionTable.h"
#include "FreezeHeuristic.h"
#include "uci_client.h" // for uci complient search method

const int QuiescenceThreshold = 2; // 2 ply for quiesearch
const int freezePreSearchLimit = 2;

const unsigned long stop_condition_frequency_mask = (1ull << 11) - 1;
extern std::atomic_bool grimoire_mode;
extern std::atomic_int32_t grimoire_bounds;
extern std::atomic_int32_t grimoire_suggestion_count;

int constexpr MateValue(GameStateUtils::Colour color) {
	if (color == GameStateUtils::White)
		return -100000;
	else
		return  100000;
}

const int mateScore[2] = { MateValue((GameStateUtils::Colour)0), MateValue((GameStateUtils::Colour)1) };


inline int decrementMateDistance(int score) {

	if (score > 90000)
		return score - 1;
	if (score < 90000)
		return score + 1;
	return score;
}

inline bool isMateValue(int score, GameStateUtils::Colour side) {
	return (score > 90000 && !side) || (score < -90000 && side);
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
	std::vector<Grimoire_Suggestion> grimoireSuggestions;

	void insertSuggestion(const Grimoire_Suggestion& suggestion);

	template <bool quiescence>
	int _alpha_beta_search(game_state& gs, int depth, int alpha, int beta, Move& bestMove, Move* begin, Move* end);
	template <bool quiescence>
	inline Move _pickBestMove(game_state& gs, int depth, int alpha, int beta, int& outEval);
	//int search_moves_with_freeze(game_state& gs, int depth, int alpha, int beta, FreezeHeuristic* heuristic);
	template <bool quiescence>
	int search_moves_without_freeze(game_state& gs, int depth, int alpha, int beta, Move & bestMove);
	inline bool isAlphaBetaCutOff(const int& localScore, const int & alpha, const int & beta, GameStateUtils::Colour side);
	inline bool isBetterScoreThan(const int& score1, const int& score2, GameStateUtils::Colour side);

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
			return mateScore[gs.props.side_to_move];
		}
			
		
		if (depth == 0) {
			++terminal_node_counter;
			return evaluator(gs);
		}
			

		if (GameStateUtils::is_threefold_repetition(gs)) {
			++terminal_node_counter;
			return 0;
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

	Move pickBestMove(game_state& gs, int depth, int & outEvaluation, UCI::UCIOutputStream& out) {
		node_counter = 0;
		terminal_node_counter = 0;
		int alpha = -100000;
		int beta = 100000;

		root_hash = gs.zobrist_hash;
		auto result = _pickBestMove<false>(gs, depth, alpha, beta, outEvaluation);

		if (grimoire_mode) {
			std::string grimoireInfo = "string grimoire";
			for (size_t i = 0; i < grimoire_suggestion_count && i < grimoireSuggestions.size(); ++i) {
				grimoireInfo += " " + UCI::formatMove(grimoireSuggestions[i].move);
			}
			out << grimoireInfo;
		};

		return result;
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

	int ignored = 0;

	for (move_gen::MoveCandidate* m = base_search; m < end; ++m) 
		if (m->movePolicy == move_gen::SpellPolicy_SpellCanBeAddedByPlayer) {
			game_state copy = gs;
			GameStateUtils::make_move(copy, m->base);
			
			copy.zobrist_hash ^= ZobristInstance.spellsLeft[side][FREEZE][copy.freeze_spell[side].spells_left];
			copy.zobrist_hash ^= ZobristInstance.spellsCooldown[side][FREEZE][copy.freeze_spell[side].couldown];
			copy.freeze_spell[side].spells_left -= 1;
			copy.freeze_spell[side].couldown = FREEZE_COOLDOWN; 
			copy.zobrist_hash ^= ZobristInstance.spellsLeft[side][FREEZE][copy.freeze_spell[side].spells_left];
			copy.zobrist_hash ^= ZobristInstance.spellsCooldown[side][FREEZE][copy.freeze_spell[side].couldown];

			Move killer = _pickBestMove<quiescence>(copy, depth / 2, alpha, beta, ignored);
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
	int best_score = MateValue(side);

	if constexpr (quiescence) {
		best_score = evaluator(gs);
	}

	TTEntry& TT_entry = transpositionTable[calculate_index(gs.zobrist_hash)];

	TTEntry::Flag flag = TTEntry::EXACT;

	Move TTMove = 0;

	if constexpr (!quiescence) {
		if (end == begin) return gs.inCheck ? mateScore[gs.props.side_to_move] : 0; // checkmate or stalemate
	}

	bestMove = *begin;
	
	if (TT_entry.key == gs.zobrist_hash) {
		if (TT_entry.depth >= depth) {

			if (TT_entry.flag == TTEntry::EXACT) {
				bestMove = TT_entry.bestMove;
				return TT_entry.eval;
			}
			

			if (TT_entry.flag == TTEntry::LOWER && gs.props.side_to_move == GameStateUtils::White) {
				best_score = TT_entry.eval;
				if (isMateValue(best_score, side))
					local_depth = std::min(depth, getMateDistance(best_score));
			}
				

			if (TT_entry.flag == TTEntry::UPPER && gs.props.side_to_move == GameStateUtils::Black) {
				best_score = TT_entry.eval;
				if (isMateValue(best_score, side))
					local_depth = std::min(depth, getMateDistance(best_score));
			}
				

			TTMove = TT_entry.bestMove;
		}
	
	}

	bool grimoire_node = grimoire_mode && gs.zobrist_hash == root_hash;
	if (grimoire_node)
		grimoireSuggestions.clear();


	game_state copy;

	int* local_alpha = gs.props.side_to_move ? & alpha : & best_score;
	int* local_beta = gs.props.side_to_move ? & best_score : &beta ;
	int cutOffValue = gs.props.side_to_move ? beta : alpha;

	MoveOrdering move_ordering;

	Move ignored;

	for (Move* m = begin; m < end; ++m) {
		copy = gs;

		move_ordering.select(copy, m, end, TTMove);

		// check if castling is legal
		// TODO: make move generator split out only legal castling
		if (Move_Utils::is_castle(*m)
			&& !Move_Utils::uses_freeze(*m)
			&& move_gen::get_castling_attackers(gs, side, Bitboards::square[Move_Utils::to_sq(*m)] & Bitboards::column[COL_A]))
			continue;

		GameStateUtils::make_move(copy, *m);

		assert(local_depth > 0);
		local_score = decrementMateDistance(alpha_beta_search<quiescence>(copy, local_depth - 1, *local_alpha, *local_beta, ignored));

		if (grimoire_node && (side == GameStateUtils::White ? local_score : -local_score) > -grimoire_bounds) {
			insertSuggestion({ *m, (side == GameStateUtils::White ? local_score : -local_score) });
		};

		if (isAlphaBetaCutOff(local_score, *local_alpha, *local_beta, side)) {
			best_score = local_score;
			flag = (local_score < alpha) ? TTEntry::LOWER: TTEntry::UPPER;
			bestMove= *m;
			goto TT_WRITE;
		}
		
		if (isBetterScoreThan(local_score, best_score, side)) {
			best_score = local_score;
			bestMove = *m;

			if (isMateValue(best_score, side))
				local_depth = std::min(depth, getMateDistance(best_score));
		}
	
	}

	TT_WRITE:
	TT_entry.key = gs.zobrist_hash;
	TT_entry.depth = depth;
	TT_entry.flag = flag;
	TT_entry.eval = best_score;
	TT_entry.bestMove = bestMove;

	return best_score;
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
inline bool SEARCH::isAlphaBetaCutOff(const int& localScore, const int& alpha, const int& beta, GameStateUtils::Colour side)
{

	if (side == GameStateUtils::White)
		return localScore > beta;
	else
		return localScore < alpha;
}

SEARCH_TEMPLATE_PARAMS
inline bool SEARCH::isBetterScoreThan(const int& score1, const int& score2, GameStateUtils::Colour side)
{
	if (side == GameStateUtils::White)
		return score1 > score2;
	else
		return score1 < score2;
}


SEARCH_TEMPLATE_PARAMS
inline Move SEARCH::uciCompliantIterativeDeepening(game_state& gs, UCI::go_params& params, UCI::UCIOutputStream& out)
{
	clock_t deadline;

	node_counter = 0;

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
	Move result = pickBestMove(gs, 1, eval, out);

	int32_t target_depth = 2;
	bool exception_found = false;

	clock_t iter_start;

	out << UCI::formatString("score %s", UCI::formatScore(gs.props.side_to_move == GameStateUtils::Black ? -eval: eval).c_str());

	while (target_depth <= max_ply && !exception_found && abs(eval) < 90000 ) {
		iter_start = clock();
		try {
			result = pickBestMove(gs, target_depth, eval, out);
			out << UCI::formatString("depth %d", target_depth);
			out << UCI::formatString("score %s", UCI::formatScore(gs.props.side_to_move == GameStateUtils::Black ? -eval : eval).c_str());
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