#pragma once
#include "Bitboard.h"
#include "GameState.h"
#include "Move.h"
#include "Utils.h"
#include <random>

#include <map>
namespace move_gen {

	enum SpellPolicy {
		SpellPolicy_SpellCanBeAddedByPlayer,
		SpellPolicy_SpellFixedByMoveGenerator
	};

	
	struct MoveCandidate {
		Move base;
		SpellPolicy movePolicy;
	};

#define __MOVE_OFFSET_CASE(X, Y) if (__IN_RANGE8(row + (X)) && __IN_RANGE8(col + (Y))) res |= Bitboards::sq(col +(Y), row+(X))
	constexpr Bitboard __precompute_knight_attack(uint8_t sq) {

		Bitboard res = 0ull;

		int row = sq >> 3;
		int col = sq & 7;
		
		__MOVE_OFFSET_CASE( -2, -1);
		__MOVE_OFFSET_CASE( -2,  1);
		__MOVE_OFFSET_CASE(  2, -1);
		__MOVE_OFFSET_CASE(  2,  1);
		__MOVE_OFFSET_CASE( -1, -2);
		__MOVE_OFFSET_CASE( -1,  2);
		__MOVE_OFFSET_CASE(  1, -2);
		__MOVE_OFFSET_CASE(  1,  2);

		return res;
	}

	constexpr Bitboard __precompute_king_attack(uint8_t sq) {
		Bitboard res = 0ull;

		int row = sq >> 3;
		int col = sq & 7;

		__MOVE_OFFSET_CASE(-1, -1);
		__MOVE_OFFSET_CASE(-1, 1);
		__MOVE_OFFSET_CASE(1, -1);
		__MOVE_OFFSET_CASE(1, 1);
		__MOVE_OFFSET_CASE(-1, 0);
		__MOVE_OFFSET_CASE( 1, 0);
		__MOVE_OFFSET_CASE(0, -1);
		__MOVE_OFFSET_CASE(0, 1);

		return res;
	}

#undef __MOVE_OFFSET_CASE

	// in the context of spell chess, secondary attacks are attacks that are only possible by using the jump sq
	constexpr void __precompute_ray_attack(int8_t piece_pos_x, int8_t piece_pos_y, int8_t ray_dx, int8_t ray_dy, const Bitboard & blockers, Bitboard& primary, Bitboard& secondary) {
		int8_t blockers_intersected = 0;
		int8_t x = piece_pos_x + ray_dx;
		int8_t y = piece_pos_y + ray_dy;

		Bitboard mask = 0;
		while (__IN_RANGE8(x) && __IN_RANGE8(y) && blockers_intersected < 2) {
			mask = Bitboards::row[y] & Bitboards::column[x];

			x += ray_dx;
			y += ray_dy;
			switch (blockers_intersected)
			{
			case 0:
				primary |= mask; break;
			case 1:
				secondary |= mask; break;
			default: break;


			}

			if (blockers & mask) blockers_intersected += 1;
		}
	}

	static void __precompute_bishop_attacks(int8_t sq, const Bitboard& blockers, Bitboard& primary, Bitboard& secondary) {
		primary = secondary = 0ull;
		int8_t x = sq & 7;
		int8_t y = sq >> 3;
		__precompute_ray_attack(x, y,  1,  1, blockers, primary, secondary);
		__precompute_ray_attack(x, y,  1, -1, blockers, primary, secondary);
		__precompute_ray_attack(x, y, -1,  1, blockers, primary, secondary);
		__precompute_ray_attack(x, y, -1, -1, blockers, primary, secondary);
	}

	static void __precompute_rook_attacks(int8_t sq, const Bitboard& blockers, Bitboard& primary, Bitboard& secondary) {
		primary = secondary = 0ull;
		int8_t x = sq & 7;
		int8_t y = sq >> 3;
		__precompute_ray_attack(x, y,  1,  0, blockers, primary, secondary);
		__precompute_ray_attack(x, y, -1,  0, blockers, primary, secondary);
		__precompute_ray_attack(x, y,  0,  1, blockers, primary, secondary);
		__precompute_ray_attack(x, y,  0, -1, blockers, primary, secondary);
	}

	static Bitboard computeRelevantSquaresForMagics(int8_t sq) {
		int8_t x = sq & 7;
		int8_t y = sq >> 3;
		
		Bitboard result = 0ull;
		if (x) result |= Bitboards::column[0];
		if (x != 7) result |= Bitboards::column[7];
		if (y) result |= Bitboards::row[0];
		if (y != 7) result |= Bitboards::row[7];
		return ~result;
	}

	struct __primarry_and_secondary_attacks {
		Bitboard primary;
		Bitboard secondary;
	};

	struct __magic_nr_key {
		uint8_t bits_used;
		Bitboard magic_nr;
		Bitboard mask;
		uint32_t size;
		__primarry_and_secondary_attacks* table;
		
		inline uint32_t calculate_index(const Bitboard& blockers) const {
			return (magic_nr* (blockers & mask)) >> (64 - bits_used);
		}

		inline __primarry_and_secondary_attacks getAttacks(const Bitboard& blockers) const {
			return table[calculate_index(blockers)];
		}
	};

	enum SliderMovementType : uint_fast8_t {
		Diagonal = 0,
		Orthogonal = 1
	};

	__Array<__magic_nr_key> __generate_magic_numbers(void (*compute_attacks)(int8_t sq, const Bitboard& blockers, Bitboard& primary, Bitboard& secondary));

	size_t __prepare_lookup_table(SliderMovementType movementType, void (*compute_attacks)(int8_t sq, const Bitboard& blockers, Bitboard& primary, Bitboard& secondary));

	static __Array<__magic_nr_key> __generate_magic_numbers_for_rook() {
		return __generate_magic_numbers(__precompute_rook_attacks);
	}

	static __Array<__magic_nr_key>  __generate_magic_numbers_for_bishop() {
		return __generate_magic_numbers(__precompute_bishop_attacks);
	}

	extern __magic_nr_key Magics[64][2];


	void __init_magics();

	const Bitboard knight_attacks[64] = { ENUMERATE_FUNC_64_ELEMENTS(__precompute_knight_attack, 0) };
	const Bitboard king_attacks[64] = { ENUMERATE_FUNC_64_ELEMENTS(__precompute_king_attack, 0) };
	
	template <bool white_to_move>
	inline uint8_t get_king_index(const game_state& gs) {
		Bitboard kings = gs.kings;
		if constexpr (white_to_move)
			kings &= gs.white;
		else
			kings &= gs.black;
		return kings ? Bitboards::to_index(kings) : (uint8_t) -1;
	}
	extern Bitboard get_square_attackers(const game_state& gs, int_fast8_t sq);
	extern Bitboard get_jump_square_attackers(const game_state& gs, int_fast8_t sq);
	extern Bitboard get_castling_attackers(const game_state& gs, GameStateUtils::Colour side, bool isQueenSideCastling);
	
	inline void calculate_pins(const game_state& gs, Bitboard & blockers, Bitboard & pinners, const Bitboard & us, const Bitboard & them) {
		assert((gs.kings & us) != 0);
		uint8_t ksq = Bitboards::to_index(gs.kings & us);
		
		pinners = blockers = 0ull;
		Bitboard snipers = Magics[ksq][Diagonal].getAttacks(blockers).secondary & (gs.queens | gs.bishops) & them;
		snipers			|= Magics[ksq][Orthogonal].getAttacks(blockers).secondary & (gs.queens | gs.rooks) & them;
		uint_fast8_t buff[4];
		uint_fast8_t* end = buff, *it = buff;
		Bitboards::bitboard_arr_scan(snipers, it);

		Bitboard blocker;
		while (it != end) {
			blocker = Bitboards::ray_between_with_caching(ksq, *it);
			blockers |= blocker;
			if (blocker & us)
				pinners |= Bitboards::square[*it];
			++it;
		}
	}

	template <class MOVE_ITERATOR, bool only_captures>
	void knight_move_generator(uint8_t sq, const Bitboard & us, const Bitboard& them, MOVE_ITERATOR & iterator) {
		Bitboard temp;
		uint_fast8_t buffer[64];
		uint_fast8_t* ptr;

		ptr = buffer;
		
		if constexpr (!only_captures)
			temp = knight_attacks[sq] & ~us;
		else
			temp = knight_attacks[sq] & them;

		Bitboards::bitboard_arr_scan(temp, ptr);
		for (uint_fast8_t* it = buffer; it != ptr; ++it) {
			*iterator = Move_Utils::build_move(sq, *it, 0, 0);
			iterator++;
		}
		
	}

	template <class MOVE_ITERATOR, bool only_captures>
	void king_move_generator(uint8_t sq, const Bitboard& us, const Bitboard& them, MOVE_ITERATOR& iterator) {
		Bitboard temp;
		uint_fast8_t buffer[64];
		uint_fast8_t* ptr = buffer;

		if constexpr (!only_captures)
			temp = king_attacks[sq] & ~us;
		else
			temp = king_attacks[sq] & them;

		Bitboards::bitboard_arr_scan(temp, ptr);
		for (uint_fast8_t* it = buffer; it != ptr; ++it) {
			*iterator = Move_Utils::build_move(sq, *it, 0, 0);
			iterator++;
		}
		
	}

	template <class MOVE_ITERATOR, SliderMovementType  movement, bool only_captures>
	void slider_move_generator(uint8_t slider, Bitboard& blockers, const Bitboard& us, const Bitboard& them, MOVE_ITERATOR & iterator, bool generate_jump_spell) {		
		Bitboard t;
		uint_fast8_t buffer[64];
		uint_fast8_t* sq_iter_end;
		int_fast8_t jump_sq;

		const __primarry_and_secondary_attacks& attacks = Magics[slider][movement].getAttacks(blockers);

		if constexpr (!only_captures)
			t = attacks.primary & ~us;
		else
			t = attacks.primary & them;

		sq_iter_end = buffer;
		Bitboards::bitboard_arr_scan(t, sq_iter_end);
		for (uint_fast8_t* sq_it = buffer; sq_it != sq_iter_end; ++sq_it) {
			*iterator = Move_Utils::build_move(slider, *sq_it, 0, 0);
			++iterator;
		}


		if (attacks.secondary && generate_jump_spell) {

			if constexpr (!only_captures)
				t = attacks.secondary & ~us;
			else
				t = attacks.secondary & them;

			sq_iter_end = buffer;
			Bitboards::bitboard_arr_scan(t, sq_iter_end);
			for (uint_fast8_t* sq_it = buffer; sq_it != sq_iter_end; ++sq_it) {
				assert(Bitboards::hasExactlyOneBitSet(Bitboards::ray_between_with_caching(slider, *sq_it) & blockers));
				jump_sq = Bitboards::to_index(Bitboards::ray_between_with_caching(slider, *sq_it) & blockers);
				*iterator = Move_Utils::build_move(slider, *sq_it, 0, jump_sq, 8u);
				++iterator;
			}
		}
		
	}

	template<class MOVE_ITERATOR>
	void __scan_pawn_moves(const Bitboard & reachable, int8_t offset, MOVE_ITERATOR & iterator) {
		const Bitboard promotion_squares = Bitboards::row[ROW_1] | Bitboards::row[ROW_8];
		Bitboard t = reachable & promotion_squares;
		uint_fast8_t buffer[64];
		uint_fast8_t* buffer_end = buffer;
		Bitboards::bitboard_arr_scan(t, buffer_end);

		for (uint_fast8_t* it = buffer; it != buffer_end; ++it) {
			*iterator = Move_Utils::build_move(*it - offset, *it, 0, 0, ((Move_Utils::Queen) << 6) | 4u);
			++iterator;
			*iterator = Move_Utils::build_move(*it - offset, *it, 0, 0, ((Move_Utils::Rook) << 6) | 4u);
			++iterator;
			*iterator = Move_Utils::build_move(*it - offset, *it, 0, 0, ((Move_Utils::Knight) << 6) | 4u);
			++iterator;
			*iterator = Move_Utils::build_move(*it - offset, *it, 0, 0, ((Move_Utils::Bishop) << 6) | 4u);
			++iterator;
		}

		t = reachable & ~promotion_squares;
		buffer_end = buffer;
		Bitboards::bitboard_arr_scan(t, buffer_end);

		for (uint_fast8_t* it = buffer; it != buffer_end; ++it) {
			*iterator = Move_Utils::build_move(*it - offset, *it, 0, 0);
			++iterator;
		}
	}

	template <class MOVE_ITERATOR, bool white_to_move> 
	void __pawn_pushes_generator(const Bitboard& our_pawns, Bitboard& blockers, MOVE_ITERATOR & iterator) {
		Bitboard reachable;
		int8_t offset;
		if constexpr (white_to_move) {
			reachable = (our_pawns << 8) & ~blockers;
			offset = +8;
		}
		else {
			reachable = (our_pawns >> 8) & ~blockers;
			offset = -8;
		}

		__scan_pawn_moves(reachable, offset, iterator);
	}

	template <class MOVE_ITERATOR, bool white_to_move>
	void __double_pawn_pushes_generator(const Bitboard& our_pawns, const Bitboard& blockers, const Bitboard & nonJumpableBlockers, MOVE_ITERATOR & iterator,  bool can_use_jump) {
		Bitboard reachable;
		int8_t offset;
		Bitboard t;
		if constexpr (white_to_move) {
			reachable = (our_pawns << 16) & ~blockers & Bitboards::row[ROW_4];
			offset = +8;
			t = reachable & (nonJumpableBlockers << 8);
		}
		else {
			reachable = (our_pawns >> 16) & ~blockers & Bitboards::row[ROW_5];
			offset = -8;
			t = reachable & (nonJumpableBlockers >> 8);
		}

		uint_fast8_t buffer[64];
		uint_fast8_t* buffer_end = buffer;
		Bitboards::bitboard_arr_scan(t, buffer_end);

		// double pushes requiring jump spell
		if(can_use_jump)
		for (uint_fast8_t* it = buffer; it != buffer_end; ++it) {
			*iterator = Move_Utils::build_move(*it - 2 * offset, *it, 0, *it - offset, 8u);
			++iterator;
		}

		t = reachable ^ t;
		buffer_end = buffer;
		Bitboards::bitboard_arr_scan(t, buffer_end);

		// regular double push
		for (uint_fast8_t* it = buffer; it != buffer_end; ++it) {
			*iterator = Move_Utils::build_move(*it - 2 * offset, *it, 0, 0, 0);
			++iterator;
		}
	}

	template<bool white_to_move, int8_t dir>
	void  __init_pawn_capture_params(Bitboard& out, int8_t& offset, const Bitboard & target, const Bitboard & our_pawns) {

		constexpr int8_t __col = (dir == -1) ? 7 : 0;

		if constexpr (white_to_move) {
			out = (our_pawns << (8 + dir)) & target & ~Bitboards::column[__col];
			offset = +8 + dir;

		}
		else {
			out = (our_pawns >> (8 - dir)) & target & ~Bitboards::column[__col];
			offset = -8 + dir;
		}
	}

	template <class MOVE_ITERATOR, bool white_to_move>
	void __pawn_capture_generator(const Bitboard& our_pawns, const Bitboard & us, const Bitboard& enemy, int8_t enpassant_sq, MOVE_ITERATOR &  iterator) {
		Bitboard enpassant = (enpassant_sq == -1) ? 0 : Bitboards::square[enpassant_sq];
		enpassant &= ~us; // prevent rare case of capturing our own piece using an enpassant
		Bitboard target = enemy | enpassant;
		Bitboard t;
		int8_t offset;

		__init_pawn_capture_params<white_to_move, 1>(t, offset, target, our_pawns);
		__scan_pawn_moves(t, offset, iterator);
		__init_pawn_capture_params<white_to_move, -1>(t, offset, target, our_pawns);
		__scan_pawn_moves(t, offset, iterator);

	}

	template <class MOVE_ITERATOR, bool white_to_move>
	void __kingside_castling_generator(game_state & gs, const Bitboard & blockers, const Bitboard& attacked, MOVE_ITERATOR & iterator, bool allow_unsafe_castling) {

		Bitboard king_side_castling;
		bool side;
		Move m;
		uint8_t from, to;
		Bitboard kingPath;
		if constexpr (white_to_move) {
			from = Bitboards::to_index(Bitboards::sq(COL_E, ROW_1));
			to = Bitboards::to_index(Bitboards::sq(COL_H, ROW_1));
			side = GameStateUtils::White;
			kingPath = Bitboards::sq(COL_E, ROW_1) | Bitboards::sq(COL_F, ROW_1) | Bitboards::sq(COL_G, ROW_1);
		}
		else {
			from = Bitboards::to_index(Bitboards::sq(COL_E, ROW_8));
			to = Bitboards::to_index(Bitboards::sq(COL_H, ROW_8));
			side = GameStateUtils::Black;
			kingPath = Bitboards::sq(COL_E, ROW_8) | Bitboards::sq(COL_F, ROW_8) | Bitboards::sq(COL_G, ROW_8);
		}
		m = Move_Utils::build_move(from, to, 0, 0, 32u);

		if (gs.frozen & (Bitboards::square[from] | Bitboards::square[to]))
			return;

		king_side_castling = Bitboards::between(from, to);
		if (gs.castling[side][KINGSIDE] && ((blockers & king_side_castling) == 0) && ((kingPath & attacked) == 0 || allow_unsafe_castling)) {
			*iterator = m;
			++iterator;
		}
	}

	template <class MOVE_ITERATOR, bool white_to_move>
	void __queenside_castling_generator(game_state& gs, const Bitboard& blockers, const Bitboard & attacked, MOVE_ITERATOR& iterator, bool can_use_jump, bool allow_unsafe_castling) {

		Bitboard queen_side_castling;
		Bitboard kingPath;
		bool side;
		Move m;
		uint8_t from, to, jumpable;
		if constexpr (white_to_move) {
			from = Bitboards::to_index(Bitboards::sq(COL_E, ROW_1));
			to = Bitboards::to_index(Bitboards::sq(COL_A, ROW_1));
			jumpable = Bitboards::to_index(Bitboards::sq(COL_B, ROW_1));
			side = GameStateUtils::White;
			kingPath = Bitboards::sq(COL_E, ROW_1) | Bitboards::sq(COL_D, ROW_1) | Bitboards::sq(COL_C, ROW_1);
		}
		else {
			from = Bitboards::to_index(Bitboards::sq(COL_E, ROW_8));
			to = Bitboards::to_index(Bitboards::sq(COL_A, ROW_8));
			jumpable = Bitboards::to_index(Bitboards::sq(COL_B, ROW_8));
			side = GameStateUtils::Black;
			kingPath = Bitboards::sq(COL_E, ROW_8) | Bitboards::sq(COL_D, ROW_8) | Bitboards::sq(COL_C, ROW_8);

		}
		m = Move_Utils::build_move(from, to, 0, 0, 32u);
		queen_side_castling = Bitboards::between(from, to);
	
		if (gs.frozen & (Bitboards::square[from] | Bitboards::square[to]))
			return;

		if (gs.castling[side][QUEENSIDE] && ((blockers & queen_side_castling) == 0) && ((kingPath & attacked) == 0 || allow_unsafe_castling)) {
			*iterator = m;
			++iterator;
		}
	}

	template <bool white>
	Bitboard generate_attack_mask(const game_state& gs, const Bitboard& ignored) {
		Bitboard us;

		if constexpr (white) {
			us = gs.white;
		}
		else {
			us = gs.black;
		}

		us &= ~ignored;

		Bitboard result = 0;

		Bitboard without_pawns = us & ~gs.pawns;
		Bitboard blockers = (gs.white | gs.black) & ~gs.kings;

		uint_fast8_t our_pieces[64];
		uint_fast8_t* our_pieces_end = our_pieces;
		Bitboards::bitboard_arr_scan(us, our_pieces_end);

		for (uint_fast8_t* piece = our_pieces; piece < our_pieces_end; ++piece) {
			switch (gs.pieces[*piece]) {
			case Knight:
				result |= knight_attacks[*piece];
				break;

			case Bishop:
				result |= Magics[*piece][Diagonal].getAttacks(blockers).primary;
				break;
			case Rook:
				result |= Magics[*piece][Orthogonal].getAttacks(blockers).primary;
				break;
			case Queen:
				result |= Magics[*piece][Diagonal].getAttacks(blockers).primary;
				result |= Magics[*piece][Orthogonal].getAttacks(blockers).primary;
				break;
			case King:
				result |= king_attacks[*piece];
				break;
			}
		}

		if constexpr (white) {
			result |= (us & gs.pawns & ~Bitboards::column[0]) << 7;
			result |= (us & gs.pawns & ~Bitboards::column[7]) << 9;
		}
		else {
			result |= (us & gs.pawns & ~Bitboards::column[0]) >> 9;
			result |= (us & gs.pawns & ~Bitboards::column[7]) >> 7;
		}

		return result;
	}

	/*
		Generates all valid move candidates from base moves by adding freeze spell locations. All generated spells will contain all squares 
		specified in mustContain argument and none of the squares specified in forbiddenSq argument. Also algorithm will not output two move candidates that are logically equivalent
		(freeze the same pieces), or a candidate where some superior location exists.

		In the specification above the term "superior" means that either:
		- The candidate freezes the same set of enemy pieces as the alternative, but freezes less our pieces
		- The candidate freezes the same set of our pieces as the alternative, but freezes more enemy pieces

	*/
	template <typename CANDIDATE_MOVE_ITERATOR>
	inline void generate_spell_candidates(CANDIDATE_MOVE_ITERATOR& out, const Move& base, const Bitboard& mustContain, const Bitboard& forbiddenSq, const int8_t & currentFreezeSq) {
		Bitboard result = (Bitboard) -1;
		uint_fast8_t buffer[64];
		uint_fast8_t* end = buffer;
		Bitboards::bitboard_arr_scan(mustContain, end);

		for (uint_fast8_t* it = buffer; it != end; ++it)
			result &= Bitboards::frozen_area(*it);
		
		end = buffer;
		Bitboards::bitboard_arr_scan(forbiddenSq, end);

		for (uint_fast8_t* it = buffer; it != end; ++it)
			result &= Bitboards::not_frozen_area(*it);

		if (currentFreezeSq != -1)
			result &= ~Bitboards::square[currentFreezeSq];

		// todo: further reduce amount of candidate move by implementing redundancy logic

		end = buffer;
		Bitboards::bitboard_arr_scan(result, end);

		Move tmp;
		for (uint_fast8_t* it = buffer; it != end; ++it) {
			*out = { Move_Utils::addFreezeSquare(base, *it), SpellPolicy_SpellFixedByMoveGenerator };
			++out;
		}

	}

	template <typename CANDIDATE_MOVE_ITERATOR, bool is_white_to_move>
	void assign_spell_policy_for_non_king_pieces(const game_state & gs, const Move & base, CANDIDATE_MOVE_ITERATOR & out, const Check_data_cache & cache) {
		
		Bitboard squaresToBeNeutralisedByFreeze;
		bool canUseFreeze = GameStateUtils::can_use_freeze(gs, is_white_to_move ? GameStateUtils::White : GameStateUtils::Black);

		squaresToBeNeutralisedByFreeze = cache.kingAttackers;
		uint8_t from = Move_Utils::from_sq(base);
		uint8_t to = Move_Utils::to_sq(base);
		Direction d = directions[cache.kingPos][to];
			
		squaresToBeNeutralisedByFreeze &= ~Bitboards::square[to];

		if (cache.checkMasks[d] & Bitboards::square[to])
			squaresToBeNeutralisedByFreeze &= ~Bitboards::square[cache.offenders[d]];		

		d = directions[cache.kingPos][from];

		if (from == cache.pinned[d] && (Bitboards::square[to] & cache.pinMasks[d]) == 0) {
			squaresToBeNeutralisedByFreeze |= Bitboards::square[cache.offenders[d]];
		}


		bool capturesEnemyKing = gs.kings & (1ull << to);

		if (squaresToBeNeutralisedByFreeze && !capturesEnemyKing) {
		
			if (Move_Utils::uses_jump(base) || !canUseFreeze)
				return; 
			
			generate_spell_candidates(out, base, squaresToBeNeutralisedByFreeze, Bitboards::square[from], gs.props.freeze_sq);
		}
		else {
			*out = { base, Move_Utils::uses_jump(base) ? SpellPolicy_SpellFixedByMoveGenerator: SpellPolicy_SpellCanBeAddedByPlayer };
			++out;
		}
	}

	template <typename CANDIDATE_MOVE_ITERATOR, bool is_white_to_move>
	void assign_spell_policy_for_king(const game_state& gs,const Move & base, CANDIDATE_MOVE_ITERATOR& out, const Check_data_cache& cache) {
		Bitboard squaresToBeNeutralisedByFreeze;
		auto side = is_white_to_move ? GameStateUtils::White : GameStateUtils::Black;
		bool canUseFreeze = GameStateUtils::can_use_freeze(gs, side);
		
		int_fast8_t from = Move_Utils::from_sq(base);
		int_fast8_t to = Move_Utils::to_sq(base);

		squaresToBeNeutralisedByFreeze = Move_Utils::is_castle(base) ? get_castling_attackers(gs, side, to == from + 4) : get_square_attackers(gs, to);

		squaresToBeNeutralisedByFreeze &= cache.them;

		if (squaresToBeNeutralisedByFreeze) {
			if (canUseFreeze)
				generate_spell_candidates(out, base, squaresToBeNeutralisedByFreeze, Bitboards::square[from], gs.props.freeze_sq);
		}
		else {
			*out = { base, Move_Utils::uses_jump(base) ? SpellPolicy_SpellFixedByMoveGenerator : SpellPolicy_SpellCanBeAddedByPlayer };
			++out;
		}
	}

	template<typename CANDIDATE_MOVE_ITERATOR, bool is_white_to_move, bool only_captures>
	void _sided_move_generator(game_state& gs, CANDIDATE_MOVE_ITERATOR & iterator) {
		Bitboard t;
		Check_data_cache  & check_cache = gs.check_data;

		const Bitboard& us = check_cache.us;
		const Bitboard& them = check_cache.them;

		bool can_use_jump, can_use_freeze;
		Bitboard blockers = (gs.white | gs.black);
		Bitboard nonJumpableBlockers = blockers & ~gs.jumpable;
		
		Move buffer[256];
		Move* internal_iterator = buffer;

		const Bitboard notFrozen = ~gs.frozen;
		if constexpr (is_white_to_move) {
			can_use_jump = gs.jump_spell[GameStateUtils::White].couldown == 0 && gs.jump_spell[GameStateUtils::White].spells_left > 0;
			can_use_freeze = gs.freeze_spell[GameStateUtils::White].couldown == 0 && gs.freeze_spell[GameStateUtils::White].spells_left > 0;
		}
		else {
			can_use_jump = gs.jump_spell[GameStateUtils::Black].couldown == 0 && gs.jump_spell[GameStateUtils::Black].spells_left > 0;
			can_use_freeze = gs.freeze_spell[GameStateUtils::Black].couldown == 0 && gs.freeze_spell[GameStateUtils::Black].spells_left > 0;
		}

		t = us & notFrozen;

		uint_fast8_t our_pieces[64];
		uint_fast8_t* our_pieces_end = our_pieces;
		Bitboards::bitboard_arr_scan(t, our_pieces_end);
		for (uint_fast8_t* sq = our_pieces; sq != our_pieces_end; ++sq) {
			switch (gs.pieces[*sq]) {
			case Bishop:
				slider_move_generator<Move*, Diagonal, only_captures>(*sq, nonJumpableBlockers, us, them, internal_iterator, can_use_jump);
				break;
			case Rook:
				slider_move_generator<Move*, Orthogonal, only_captures>(*sq, nonJumpableBlockers, us, them, internal_iterator, can_use_jump);
				break;
			case Knight:
				knight_move_generator<Move*, only_captures>(*sq, us, them, internal_iterator);
				break;
			case King:
				king_move_generator<Move*, only_captures>(*sq, us, them, internal_iterator);
				break;
			case Queen:
				slider_move_generator<Move*, Diagonal, only_captures>(*sq, nonJumpableBlockers, us, them, internal_iterator, can_use_jump);
				slider_move_generator<Move*, Orthogonal, only_captures>(*sq, nonJumpableBlockers, us, them, internal_iterator, can_use_jump);
				break;
			}


		}

		t = gs.pawns & us & notFrozen;

		if constexpr (!only_captures) {
			__pawn_pushes_generator<Move*, is_white_to_move>(t, blockers, internal_iterator);
			__double_pawn_pushes_generator<Move*, is_white_to_move>(t, blockers, nonJumpableBlockers, internal_iterator, can_use_jump);
		}


		__pawn_capture_generator<Move*, is_white_to_move>(t, us, them, gs.props.enpassant_sq, internal_iterator);

		if constexpr (!only_captures) {
			Bitboard enemyAttackMask = generate_attack_mask<!is_white_to_move>(gs, 0);
			__kingside_castling_generator<Move*, is_white_to_move>(gs, blockers, enemyAttackMask, internal_iterator, can_use_freeze);
			__queenside_castling_generator<Move*, is_white_to_move>(gs, blockers, enemyAttackMask, internal_iterator, can_use_jump, can_use_freeze);
		}
		

		for (Move* it = buffer; it != internal_iterator; ++it) {
			if (gs.pieces[Move_Utils::from_sq(*it)] == King)
				assign_spell_policy_for_king<CANDIDATE_MOVE_ITERATOR, is_white_to_move>(gs, *it, iterator, check_cache);
			else
				assign_spell_policy_for_non_king_pieces<CANDIDATE_MOVE_ITERATOR, is_white_to_move>(gs, *it, iterator, check_cache);
		}


	}

	template<typename CANDIDATE_MOVE_ITERATOR> 
	void move_generator(game_state& gs, CANDIDATE_MOVE_ITERATOR & iterator) {
		if (gs.props.side_to_move == GameStateUtils::White)
			_sided_move_generator<CANDIDATE_MOVE_ITERATOR, true, false> (gs, iterator);
		else
			_sided_move_generator<CANDIDATE_MOVE_ITERATOR, false, false>(gs, iterator);
	}

	template <typename MOVE_ITERATOR>
	void move_generator_legacy_interface(game_state& gs, MOVE_ITERATOR& iterator) {
		MoveCandidate candidate_buffer[512];
		MoveCandidate* internal_it = candidate_buffer;
		move_generator<MoveCandidate*>(gs, internal_it);

		for (MoveCandidate* it = candidate_buffer; it != internal_it; ++it) {
			*iterator = it->base;
			++iterator;
		}
	}

	template<typename CANDIDATE_MOVE_ITERATOR>
	void quiescence_move_generator(game_state& gs, CANDIDATE_MOVE_ITERATOR& iterator) {

		if (gs.props.side_to_move == GameStateUtils::White)
			_sided_move_generator<CANDIDATE_MOVE_ITERATOR, true, true>(gs, iterator);
		else
			_sided_move_generator<CANDIDATE_MOVE_ITERATOR, false, true>(gs, iterator);
	}
	
	template <typename MOVE_ITERATOR>
	void quiescence_move_generator_legacy_interface(game_state& gs, MOVE_ITERATOR& iterator) {
		MoveCandidate candidate_buffer[512];
		MoveCandidate* internal_it = candidate_buffer;
		quiescence_move_generator<MoveCandidate*>(gs, internal_it);

		for (MoveCandidate* it = candidate_buffer; it != internal_it; ++it) {
			*iterator = it->base;
			++iterator;
		}
	}



	/*
		TODO:
		 Test various scenarios
	*/

}