#include "MoveGen.h"

move_gen::__magic_nr_key move_gen::Magics[64][2];

inline uint32_t _calculate_index(Bitboard blockers, Bitboard magic_nr, uint8_t bits_used) {
	return (magic_nr * blockers) >> (64 - bits_used);
}

__Array<move_gen::__magic_nr_key> move_gen::__generate_magic_numbers(void(*compute_attacks)(int8_t sq, const Bitboard& blockers, Bitboard& primary, Bitboard& secondary))
{
	__Array<__magic_nr_key> result = { 64, new __magic_nr_key[64] };
	Bitboard candidate, full_mask, current_mask;
	std::map<Bitboard, __primarry_and_secondary_attacks> seen;
	std::uniform_int_distribution<Bitboard> distribution(0, -1);

	std::mt19937 rng = std::default_random_engine();

	bool magic_number_found = false;
	uint64_t failure_counter;
	Bitboard primary, secondary;

	uint32_t max_index = 0;
	for (uint8_t sq = 0; sq < 64; ++sq) {
		magic_number_found = false;
		failure_counter = 0ull;
		max_index = 0;

		for (uint8_t bits_used = 6; !magic_number_found; ) {
			seen.clear();
			candidate = distribution(rng) & distribution(rng);
			current_mask = 0;
			compute_attacks(sq, current_mask, full_mask, full_mask);
			full_mask &= computeRelevantSquaresForMagics(sq);
			//std::cout << "Full mask: " << full_mask << "\n";
			current_mask = full_mask;
			do {
				compute_attacks(sq, current_mask, primary, secondary);
				Bitboard index = _calculate_index(current_mask, candidate, bits_used);
				max_index = (index > max_index) ? index : max_index;
				if (seen.find(index) == seen.end()) {
					seen.insert({ index, {primary, secondary} });
				}
				else if (seen.at(index).primary != primary || seen.at(index).secondary != secondary) {
					// Conflict! generate another number

					++failure_counter;

					goto TRY_AGAIN;
				}

				current_mask = (current_mask - 1) & full_mask;

			} while (current_mask != full_mask);
			magic_number_found = true;
			result.elements[sq].bits_used = bits_used;
			result.elements[sq].magic_nr = candidate;
			result.elements[sq].mask = full_mask;
			result.elements[sq].size = max_index + 1;
			result.elements[sq].table = nullptr;
		TRY_AGAIN:
			if ( failure_counter > (1ull << bits_used) / 20) {
				bits_used += 1;
				failure_counter = 0ull;
			}
		}

		//std::cout << "Sq: " << (int)sq << ", magic: " << result.elements[sq].magic_nr <<", size: "<< (int)result.elements[sq].size<< "\n";
	}

	return result;
}

size_t move_gen::__prepare_lookup_table(SliderMovementType movementType, void (*compute_attacks)(int8_t sq, const Bitboard& blockers, Bitboard& primary, Bitboard& secondary)) {
	
	size_t cumulativeSize = 0;

	for (size_t squareIndex = 0; squareIndex < 64; ++squareIndex)
		cumulativeSize += move_gen::Magics[squareIndex][movementType].size;

	move_gen::Magics[0][movementType].table = new __primarry_and_secondary_attacks[cumulativeSize];
	size_t current_offset = 0;

	for (size_t squareIndex = 0; squareIndex < 64; ++squareIndex) {
		Magics[squareIndex][movementType].table = move_gen::Magics[0][movementType].table + current_offset;
		current_offset += Magics[squareIndex][movementType].size;

		Bitboard full_mask, current_mask;
		Bitboard primary, secondary;

		current_mask = 0;
		full_mask = Magics[squareIndex][movementType].mask;
		current_mask = full_mask;
		do {
			compute_attacks(squareIndex, current_mask, primary, secondary);
			Bitboard index = Magics[squareIndex][movementType].calculate_index(current_mask);
			Magics[squareIndex][movementType].table[index] = { primary, secondary };
			current_mask = (current_mask - 1) & full_mask;
		} while (current_mask != full_mask);
	}

	return cumulativeSize;
}

void move_gen::__init_magics()
{
	__Array<__magic_nr_key> rook_magics_arr = __generate_magic_numbers_for_rook();
	__Array<__magic_nr_key> bishop_magics_arr = __generate_magic_numbers_for_bishop();

	for (size_t i = 0; i < 64; ++i) {
		Magics[i][Diagonal] = bishop_magics_arr.elements[i];
		Magics[i][Orthogonal] = rook_magics_arr.elements[i];
	}

	size_t bishop_size = __prepare_lookup_table(Diagonal, __precompute_bishop_attacks);
	size_t rook_size  = __prepare_lookup_table(Orthogonal, __precompute_rook_attacks);

	delete [] rook_magics_arr.elements;
	delete [] bishop_magics_arr.elements;
	std::cout << "Bishops: " << (bishop_size * sizeof(__primarry_and_secondary_attacks)  >> 10) << " kB\n";
	std::cout << "Rooks: " << (rook_size * sizeof(__primarry_and_secondary_attacks) >> 10) << " kB\n";
}

Bitboard move_gen::get_square_attackers(const game_state& gs, int_fast8_t sq)
{
	Bitboard result = 0;

	result |= knight_attacks[sq] & gs.knights;
	result |= king_attacks[sq] & gs.kings;

	Bitboard blockers = (gs.white | gs.black) & ~gs.kings;

	result |= (gs.bishops | gs.queens) & Magics[sq][Diagonal].getAttacks(blockers).primary;
	result |= (gs.rooks | gs.queens)   & Magics[sq][Orthogonal].getAttacks(blockers).primary;
	
	Bitboard left_pawns = gs.pawns & ~Bitboards::column[COL_H];
	Bitboard right_pawns = gs.pawns & ~Bitboards::column[COL_A];
	const Bitboard& sqBitboard = Bitboards::square[sq];

	result |= (sqBitboard << 9) & left_pawns & gs.black;
	result |= (sqBitboard >> 7) & left_pawns & gs.white;
	result |= (sqBitboard << 7) & right_pawns & gs.black;
	result |= (sqBitboard >> 9) & right_pawns& gs.white;
	
	return result;
}

Bitboard move_gen::get_castling_attackers(const game_state& gs, GameStateUtils::Colour side, bool isQueenSideCastling) {
	Bitboard enemy = gs.props.side_to_move ? gs.white : gs.black;
	Bitboard kingPath = castlingKingPath[gs.props.side_to_move][isQueenSideCastling];
	Bitboard mustContain = 0;
	uint_fast8_t _buff[3];
	uint_fast8_t* it = _buff;
	Bitboards::bitboard_arr_scan(kingPath, it);
	for (int i = 0; i < 3; ++i)
		mustContain |= enemy & move_gen::get_square_attackers(gs, _buff[i]);

	return mustContain;
}

void GameStateUtils::cache_king_attackers(const game_state& gs, Check_date_cache& cache) {
	cache.kingAttackers = move_gen::get_square_attackers(gs, cache.kingPos) & cache.them;
}

void GameStateUtils::cache_pins(const game_state& gs, Check_date_cache& cache)
{
	Bitboard us, them;

	if (gs.props.side_to_move == GameStateUtils::White) {
		us = gs.white;
		them = gs.black;
	}
	else {
		us = gs.black;
		them = gs.white;
	}

	uint8_t & kingPos = cache.kingPos;
	
	Direction dir;

	Bitboard snipers = move_gen::Magics[kingPos][move_gen::Diagonal].getAttacks(0ull).secondary & (gs.queens | gs.bishops) & them;
	snipers |= move_gen::Magics[kingPos][move_gen::Orthogonal].getAttacks(0ull).secondary & (gs.queens | gs.rooks) & them;
	uint_fast8_t buff[8];
	uint_fast8_t* end = buff, * it = buff;
	Bitboards::bitboard_arr_scan(snipers, end);

	while (it != end) {
		dir = directions[kingPos][*it];
		Bitboard & tmp = cache.pinMasks[dir] = Bitboards::ray_between_with_caching(kingPos, *it) | Bitboards::square[*it];

		if (tmp & us) {
			cache.pinned[dir] = Bitboards::to_index(tmp & us);
			cache.offenders[dir] = *it;
		}
		
		
		++it;
	}
}

void GameStateUtils::cache_checks(const game_state& gs, Check_date_cache& cache)
{
	Bitboard us, them;

	if (gs.props.side_to_move == GameStateUtils::White) {
		us = gs.white;
		them = gs.black;
	}
	else {
		us = gs.black;
		them = gs.white;
	}

	uint8_t& kingPos = cache.kingPos;

	Direction dir;

	Bitboard snipers = move_gen::Magics[kingPos][move_gen::Diagonal].getAttacks(0ull).primary & (gs.queens | gs.bishops) & them;
	snipers |= move_gen::Magics[kingPos][move_gen::Orthogonal].getAttacks(0ull).primary & (gs.queens | gs.rooks) & them;
	uint_fast8_t buff[8];
	uint_fast8_t* end = buff, * it = buff;
	Bitboards::bitboard_arr_scan(snipers, end);

	while (it != end) {
		dir = directions[kingPos][*it];
		cache.checkMasks[dir] = Bitboards::ray_between_with_caching(kingPos, *it) | Bitboards::square[*it];
		cache.offenders[dir] = *it;
		++it;
	}
}

bool GameStateUtils::is_move_legal(const game_state& gs, const Move& m) {
	
	Move buffer[512];
	Move* end = buffer;
	game_state copy = gs;
	move_gen::move_generator_legacy_interface(copy, end);
	
	Move base = 0;
	Move t;
	for (Move* it = buffer; it != end; ++it) {
		t = *it;
		if (Move_Utils::from_sq(t) != Move_Utils::from_sq(m))
			continue;
		if (Move_Utils::to_sq(t) != Move_Utils::to_sq(m))
			continue;

		if (Move_Utils::uses_jump(t)) {
			if (!Move_Utils::uses_jump(m) || Move_Utils::jump_sq(m) != Move_Utils::jump_sq(t) || Move_Utils::uses_freeze(m))
				return false;
		}
		base = t;
		break;
	}

	if (base == 0)
		return false;

	if (Move_Utils::uses_freeze(m)) {
		return can_use_freeze(gs, (GameStateUtils::Colour)gs.props.side_to_move);
	}

	return true;
}
