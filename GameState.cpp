#include "GameState.h"
#include <sstream>

void GameStateUtils::clear(game_state& gs)
{
	gs.black = gs.white = gs.pawns = gs.bishops = gs.rooks = gs.knights = gs.queens = gs.kings = gs.frozen = gs.jumpable = 0ull;
	
	for (int i = 0; i < 64; ++i) gs.pieces[i] = None;

	for (int side = 0; side < 2; ++side) for (int c = 0; c < 2; ++c)
		gs.castling[c][side] = false;

	gs.props.enpassant_sq = gs.props.freeze_sq = gs.props.jump_sq = -1;

	gs.eval = { 0, 0, 0, 0 };

	if (gs.position_history) delete gs.position_history;
	gs.position_history = new std::vector<pos_history_record>;
	gs.position_history->resize(2048); 
	gs.pos_his_index = 0;

}

void GameStateUtils::make_move(game_state& gs, const Move& m)
{
	assert(gs.zobrist_hash == calculateKeyFromScratch(gs));
	gs.position_history->at(gs.pos_his_index++) = { gs.zobrist_hash, gs.props.half_move_counter > 0};

	// gs.zobrist_hash = calculateKeyFromScratch(gs);
	uint8_t from_ = Move_Utils::from_sq(m);
	uint8_t to_ = Move_Utils::to_sq(m);

	int8_t enp_sq = -1;
	GameStateUtils::Colour side = (GameStateUtils::Colour)gs.props.side_to_move;

	Piece type;


	bool was_capture = true;
	bool reset_half_move_cnt = false;

	const static Bitboard a1 = Bitboards::sq(COL_A, ROW_1);
	const static Bitboard a8 = Bitboards::sq(COL_A, ROW_8);
	const static Bitboard h1 = Bitboards::sq(COL_H, ROW_1);
	const static Bitboard h8 = Bitboards::sq(COL_H, ROW_8);
	const static Bitboard pawn_promotion = Bitboards::row[ROW_1] | Bitboards::row[ROW_8];

	if ((Bitboards::square[from_] | Bitboards::square[to_]) & a1) {
		try_revoke_castling_rights(gs, White, QUEENSIDE);
	}

	if ((Bitboards::square[from_] | Bitboards::square[to_]) & a8) {
		try_revoke_castling_rights(gs, Black, QUEENSIDE);
	}

	if ((Bitboards::square[from_] | Bitboards::square[to_]) & h1) {
		try_revoke_castling_rights(gs, White, KINGSIDE);
	}

	if ((Bitboards::square[from_] | Bitboards::square[to_]) & h8) {
		try_revoke_castling_rights(gs, Black, KINGSIDE);
	}

	if (Move_Utils::is_castle(m)) { 
		was_capture = false;
		uint8_t king_pos = 0;
		uint8_t rook_pos = 0;
		Move_Utils::__determine_post_castling_pos(m, king_pos, rook_pos);
		

		__unchecked_despawn(gs, from_, side, King);
		__unchecked_despawn(gs, to_, side, Rook);
		__unchecked_spawn(gs, rook_pos, side, Rook);
		__unchecked_spawn(gs, king_pos, side, King);

		try_revoke_castling_rights(gs, (GameStateUtils::Colour)side, QUEENSIDE);
		try_revoke_castling_rights(gs, (GameStateUtils::Colour)side, KINGSIDE);


	}
	else {

		type = (Piece)gs.pieces[to_];
		
		if (type == None)
			was_capture = false; 
		else {
			__unchecked_despawn(gs, to_, (GameStateUtils::Colour)(!side), type);
		}

		type = (Piece)gs.pieces[from_];
		__unchecked_despawn(gs, from_, side, type);
		__unchecked_spawn(gs, to_, side, type);


		switch (type) {
		case Pawn:
			reset_half_move_cnt = true;
			if (abs(to_ - from_) == 16)
				enp_sq = (to_ + from_) / 2;

			// pawn promotion
			if (Bitboards::square[to_] & pawn_promotion) {
				__unchecked_despawn(gs, to_, side, Pawn);

				switch (Move_Utils::promotion_target(m))
				{
					case Move_Utils::Bishop:
						__unchecked_spawn(gs, to_, side, Bishop);
						break;
					case Move_Utils::Knight:
						__unchecked_spawn(gs, to_, side, Knight);
						break;
					case Move_Utils::Queen:
						__unchecked_spawn(gs, to_, side, Queen);
						break;
					case Move_Utils::Rook:
						__unchecked_spawn(gs, to_, side, Rook);
						break;
				};
			}

			// en passant
			if (to_ == gs.props.enpassant_sq) {
				uint8_t enpassant_pawn = gs.props.enpassant_sq - ((side == White) ? 8 : -8);
				__unchecked_despawn(gs, enpassant_pawn, (GameStateUtils::Colour)(!side), Pawn);
			}

			break;
		case King:
		
			try_revoke_castling_rights(gs, (GameStateUtils::Colour)side, QUEENSIDE);
			try_revoke_castling_rights(gs, (GameStateUtils::Colour)side, KINGSIDE);

			break;
		}
		reset_half_move_cnt = reset_half_move_cnt || was_capture;

	}

	assert(gs.zobrist_hash == calculateKeyFromScratch(gs));
	gs.zobrist_hash ^= ZobristInstance.enpSquares[1 + gs.props.enpassant_sq];
	gs.zobrist_hash ^= ZobristInstance.freezeSquares[1 + gs.props.freeze_sq];
	gs.zobrist_hash ^= ZobristInstance.jumpSquares[1 + gs.props.jump_sq];

	gs.props.enpassant_sq = enp_sq;
	gs.props.freeze_sq = -1;
	gs.props.jump_sq = -1;
	gs.frozen = 0ull;

	if (Move_Utils::uses_freeze(m)) {
		gs.zobrist_hash ^= ZobristInstance.spellsLeft[side][FREEZE][gs.freeze_spell[side].spells_left];
		gs.zobrist_hash ^= ZobristInstance.spellsLeft[side][FREEZE][gs.freeze_spell[side].spells_left - 1];
		gs.zobrist_hash ^= ZobristInstance.spellsCooldown[side][FREEZE][0];
		gs.zobrist_hash ^= ZobristInstance.spellsCooldown[side][FREEZE][FREEZE_COOLDOWN];

		gs.props.freeze_sq = Move_Utils::freeze_sq(m);
		gs.frozen |= frozen_area[gs.props.freeze_sq];
		gs.freeze_spell[side].spells_left -= 1;
		gs.freeze_spell[side].couldown = FREEZE_COOLDOWN;
	}
	else if (gs.freeze_spell[side].couldown != 0) {
		gs.zobrist_hash ^= ZobristInstance.spellsCooldown[side][FREEZE][gs.freeze_spell[side].couldown];
		gs.freeze_spell[side].couldown -= 1;
		gs.zobrist_hash ^= ZobristInstance.spellsCooldown[side][FREEZE][gs.freeze_spell[side].couldown];
	}
	if (Move_Utils::uses_jump(m)) {
		
		gs.zobrist_hash ^= ZobristInstance.spellsLeft[side][JUMP][gs.jump_spell[side].spells_left];
		gs.zobrist_hash ^= ZobristInstance.spellsLeft[side][JUMP][gs.jump_spell[side].spells_left - 1];
		gs.zobrist_hash ^= ZobristInstance.spellsCooldown[side][JUMP][0];
		gs.zobrist_hash ^= ZobristInstance.spellsCooldown[side][JUMP][JUMP_COOLDOWN];

		gs.jump_spell[side].spells_left -= 1;
		gs.jump_spell[side].couldown = JUMP_COOLDOWN;

		gs.props.jump_sq = Move_Utils::jump_sq(m);
		gs.jumpable = Bitboards::square[gs.props.jump_sq];
	}
	else {
		gs.jumpable = 0ull;
		gs.props.jump_sq = -1;
		if (gs.jump_spell[side].couldown != 0) {
			gs.zobrist_hash ^= ZobristInstance.spellsCooldown[side][JUMP][gs.jump_spell[side].couldown];
			gs.jump_spell[side].couldown -= 1;
			gs.zobrist_hash ^= ZobristInstance.spellsCooldown[side][JUMP][gs.jump_spell[side].couldown];
		}
	}
		
	
	gs.zobrist_hash ^= ZobristInstance.enpSquares[1 + gs.props.enpassant_sq];
	gs.zobrist_hash ^= ZobristInstance.freezeSquares[1 + gs.props.freeze_sq];
	gs.zobrist_hash ^= ZobristInstance.jumpSquares[1 + gs.props.jump_sq];
	gs.zobrist_hash ^= ZobristInstance.sideToMove;

	gs.props.side_to_move = !((bool)gs.props.side_to_move);
	gs.props.move_counter += gs.props.side_to_move == White;
	gs.props.half_move_counter = (reset_half_move_cnt)? 0 : gs.props.half_move_counter + 1;
	assert(gs.zobrist_hash == calculateKeyFromScratch(gs));
}

bool GameStateUtils::can_use_freeze(const game_state& gs, Colour side)
{
	return gs.freeze_spell[side].couldown == 0 && gs.freeze_spell[side].spells_left > 0;
}

bool GameStateUtils::can_use_jump(const game_state& gs, Colour side)
{
	return gs.jump_spell[side].couldown == 0 && gs.jump_spell[side].spells_left > 0;
}

bool GameStateUtils::is_threefold_repetition(const game_state& gs)
{
	ZobristKey hash = gs.zobrist_hash;
	size_t instance_counter = 1;

	if (gs.pos_his_index == 0) return false;

	int32_t i = gs.pos_his_index - 1;


	while(i > 0) {
		if (gs.position_history->at(i).hashVal == hash)
			++instance_counter;
		if (gs.position_history->at(i).isReversible)
			--i;
		else break;

	};

	return instance_counter >= 3;
}

constexpr Bitboard GameStateUtils::__precompute_frozen(uint8_t sq) {
	int8_t t = sq;
	Bitboard res = 0ull;

	int row = sq >> 3;
	int col = sq & 7;

	res |= Bitboards::square[sq];

	if (__IN_RANGE8(col - 1) && __IN_RANGE8(row - 1))
		res |= Bitboards::sq(col - 1, row - 1);

	if (__IN_RANGE8(col - 1) && __IN_RANGE8(row))
		res |= Bitboards::sq(col - 1, row);

	if (__IN_RANGE8(col - 1) && __IN_RANGE8(row + 1))
		res |= Bitboards::sq(col - 1, row + 1);

	if (__IN_RANGE8(col + 1) && __IN_RANGE8(row - 1))
		res |= Bitboards::sq(col + 1, row - 1);

	if (__IN_RANGE8(col + 1) && __IN_RANGE8(row))
		res |= Bitboards::sq(col + 1, row);

	if (__IN_RANGE8(col + 1) && __IN_RANGE8(row + 1))
		res |= Bitboards::sq(col + 1, row + 1);

	if (__IN_RANGE8(col) && __IN_RANGE8(row - 1))
		res |= Bitboards::sq(col, row - 1);


	if (__IN_RANGE8(col) && __IN_RANGE8(row + 1))
		res |= Bitboards::sq(col, row + 1);

	return res;
}
// TODO: add assertions for debug compilations
void GameStateUtils::__unchecked_spawn(game_state& gs, uint8_t sq, Colour colour, Piece piece) {
	Bitboards::set_sq(colour ? gs.black : gs.white, sq);
	
	Bitboard* piece_bitboard = nullptr;

	switch (piece)
	{
	case Piece::Pawn:
		piece_bitboard = &gs.pawns;
		break;
	case Piece::Rook:
		piece_bitboard = &gs.rooks;
		break;
	case Piece::Knight:
		piece_bitboard = &gs.knights;
		break;
	case Piece::Bishop:
		piece_bitboard = &gs.bishops;
		break;
	case Piece::Queen:
		piece_bitboard = &gs.queens;
		break;
	case Piece::King:
		piece_bitboard = &gs.kings;
		break;
	}

	Bitboards::set_sq(*piece_bitboard, sq);
	gs.pieces[sq] = piece;
	gs.zobrist_hash ^= ZobristInstance.pieces[colour][piece][sq];
	gs.eval.end_gm_score += getPestoTableScore_endgame(colour, sq, piece);
	gs.eval.mid_gm_score += getPestoTableScore_middlegame(colour, sq, piece);
	gs.eval.material_balance += (colour == GameStateUtils::White)?piece_weights[piece]: -piece_weights[piece];
	gs.eval.phase += piece_phase_weight[piece];
}

// todo : debug position fen 8/5p2/2K5/8/P4k2/7P/1R4P1/8 b - - 0 1 - - F00/J10/f00/j00
void GameStateUtils::__unchecked_despawn(game_state& gs, uint8_t sq, Colour colour, Piece piece)
{
	assert(gs.pieces[sq] == piece);
	assert((colour == White)? (gs.white & Bitboards::square[sq]): (gs.black & Bitboards::square[sq]));
	Bitboards::clr_sq(colour ? gs.black : gs.white, sq);

	Bitboard* piece_bitboard = nullptr;

	switch (piece)
	{
	case Piece::Pawn:
		piece_bitboard = &gs.pawns;
		break;
	case Piece::Rook:
		piece_bitboard = &gs.rooks;
		break;
	case Piece::Knight:
		piece_bitboard = &gs.knights;
		break;
	case Piece::Bishop:
		piece_bitboard = &gs.bishops;
		break;
	case Piece::Queen:
		piece_bitboard = &gs.queens;
		break;
	case Piece::King:
		piece_bitboard = &gs.kings;
		break;
	default:
		assert("Unreachable path reached.");
		break;
	}

	Bitboards::clr_sq(*piece_bitboard, sq);
	gs.zobrist_hash ^= ZobristInstance.pieces[colour][piece][sq];
	gs.eval.end_gm_score -= getPestoTableScore_endgame(colour, sq, piece);
	gs.eval.mid_gm_score -= getPestoTableScore_middlegame(colour, sq, piece);
	gs.eval.material_balance -= (colour == GameStateUtils::White) ? piece_weights[piece] : -piece_weights[piece];
	gs.eval.phase -= piece_phase_weight[piece];
	gs.pieces[sq] = None;
}

int GameStateUtils::__unckecked_place_pieces(game_state& gs, const std::string& piece_list)
{
	uint8_t square_index = 63;
	int result = 0;

	for (char c : piece_list) {
		switch (c)
		{
			case '8': --square_index;
			case '7': --square_index;
			case '6': --square_index;
			case '5': --square_index;
			case '4': --square_index;
			case '3': --square_index;
			case '2': --square_index;
			case '1': --square_index; break;
		
			
				case '/':
				/* 
				square_index &= ~0x7u; // this sets pointer on the most right square in the grid
				--square_index; // jump to lower row
				*/
				break;
			

			case 'p': __unchecked_spawn(gs, square_index--, Black, Pawn); break;
			case 'r': __unchecked_spawn(gs, square_index--, Black, Rook); break;
			case 'b': __unchecked_spawn(gs, square_index--, Black, Bishop); break;
			case 'n': __unchecked_spawn(gs, square_index--, Black, Knight); break;
			case 'k': __unchecked_spawn(gs, square_index--, Black, King); break;
			case 'q': __unchecked_spawn(gs, square_index--, Black, Queen); break;

			case 'P': __unchecked_spawn(gs, square_index--, White, Pawn); break;
			case 'R': __unchecked_spawn(gs, square_index--, White, Rook); break;
			case 'B': __unchecked_spawn(gs, square_index--, White, Bishop); break;
			case 'N': __unchecked_spawn(gs, square_index--, White, Knight); break;
			case 'K': __unchecked_spawn(gs, square_index--, White, King); break;
			case 'Q': __unchecked_spawn(gs, square_index--, White, Queen); break;

		default:
			result |= 1; // unrecognized symbol in string
			break;
		}
	}

	return result;
}

// TODO: replace castling direction with some constants 
void GameStateUtils::__parse_castling_rights(game_state& gs, const std::string& str)
{
	for (char c : str) {
		switch (c)
		{
		case 'K':
			gs.castling[White][KINGSIDE] = true;
			break;
		case 'Q':
			gs.castling[White][QUEENSIDE] = true;
			break;
		case 'k':
			gs.castling[Black][KINGSIDE] = true;
			break;
		case 'q':
			gs.castling[Black][QUEENSIDE] = true;
			break;
		default:
			break;
		}
	}
}

int GameStateUtils::__parse_number(const std::string& str, int& err)
{
	err = 0;
	int result = 0;

	for (char c : str) {
		if (c < '0' || c > '9') {
			err |= 1;
			return result;
		}

		result *= 10;
		result += (c - '0');
	}


	return result;
}

int GameStateUtils::__parse_square(const std::string& str, int& err)
{
	err = 0;
	if (str[0] == '-')
		return -1;

	if (str.size() != 2)
		return 1;

	int col = 7 - (str[0] - 'a');
	int row = str[1] - '1';

	if (row < 0 || row > 7 || col < 0 || col > 7)
		err |= 1;

	return 8 * row + col;
}



int GameStateUtils::__parse_side_to_move(efferal_state_props& props, const std::string& str)
{

	switch (str[0])
	{
	case 'w':
		props.side_to_move = White;
		return 0;
	case 'b':
		props.side_to_move = Black;
		return 0;
	default:
		break;
	}

	return 1;
}

#define __PARSE_NR_TO_PROPS(X) int res; props.X = __parse_number(str, res); return res;
#define __PARSE_SQ_TO_PROPS(X) int res; props.X = __parse_square(str, res); return res;

int GameStateUtils::__parse_half_move_counter(efferal_state_props& props, const std::string& str)
{
	__PARSE_NR_TO_PROPS(half_move_counter)
}

int GameStateUtils::__parse_move_counter(efferal_state_props& props, const std::string& str)
{
	__PARSE_NR_TO_PROPS(move_counter)
}

int GameStateUtils::__parse_enpassant_info(efferal_state_props& props, const std::string& str)
{
	__PARSE_SQ_TO_PROPS(enpassant_sq)
}

int GameStateUtils::__parse_frozen_square(efferal_state_props& props, const std::string& str)
{
	__PARSE_SQ_TO_PROPS(freeze_sq)
}

int GameStateUtils::__parse_jumpable_square(efferal_state_props& props, const std::string& str)
{
	__PARSE_SQ_TO_PROPS(jump_sq)
}

int GameStateUtils::__parse_spell_info(game_state& gs, const std::string& str)
{

	// step 1: grammar check

	if (str.size() < 15) return 1;

	for (int segment = 0; segment < 4; ++segment) {
		if (str[4*segment] != 'j' && str[4*segment] != 'J' && str[4*segment] != 'F' && str[4*segment] != 'f')
			return 1;

		for (int i = 1; i <= 2; ++i) if (str[4*segment + i] < '0' || str[4*segment + i] > '9')
			return 1;
	}

	if (str[3] != '/' || str[7] != '/' || str[11] != '/') return 1;

	// Step 2: parse

	unsigned int i = 0;

	char spell;
	uint8_t spell_count;
	uint8_t spell_counter;

	while (i < 15) {
		spell = str[i];
		spell_count = str[i + 1] - '0';
		spell_counter = str[i + 2] - '0';

		if (spell == 'j' || spell == 'J') {
			gs.jump_spell[(spell == 'J') ? White : Black].couldown = spell_counter;
			gs.jump_spell[(spell == 'J') ? White : Black].spells_left = spell_count;
		}

		if (spell == 'f' || spell == 'F') {
			gs.freeze_spell[(spell == 'F') ? White : Black].couldown = spell_counter;
			gs.freeze_spell[(spell == 'F') ? White : Black].spells_left = spell_count;
		}

		i += 4;
	}

	return 0;
}

int GameStateUtils::parse_fen(game_state& gs, const std::string& fen_string)
{
	clear(gs);
	int result = 0;
	std::stringstream s(fen_string);

	std::string str;

	s >> str;
	result |= __unckecked_place_pieces(gs, str);

	s >> str;
	result |= __parse_side_to_move(gs.props, str);
	s >> str;
	__parse_castling_rights(gs, str);
	s >> str;
	result |= __parse_enpassant_info(gs.props, str);
	s >> str;
	result |= __parse_half_move_counter(gs.props, str);
	s >> str;
	result |= __parse_move_counter(gs.props, str);
	s >> str;
	result |= __parse_frozen_square(gs.props, str);
	s >> str;
	result |= __parse_jumpable_square(gs.props, str);
	s >> str;
	result |= __parse_spell_info(gs, str);
	
	if (gs.props.freeze_sq != -1) {
		gs.frozen |= frozen_area[gs.props.freeze_sq];
	}

	gs.zobrist_hash = calculateKeyFromScratch(gs);
	return result;
}


void GameStateUtils::init_checks_cache(const game_state& gs, Check_date_cache& cache)
{
	for (size_t i = 0; i < Direction_MemberCount; ++i) {
		cache.offenders[i] = cache.pinned[i] = (uint8_t) - 1;
		cache.checkMasks[i] = cache.pinMasks[i] = 0ULL;
	}
	cache.kingAttackers = 0ULL;
	if (gs.props.side_to_move == White) {
		cache.us = gs.white;
		cache.them = gs.black;
	}
	else {
		cache.us = gs.black;
		cache.them = gs.white;
	}
	
	cache.kingPos = Bitboards::to_index(gs.kings & cache.us);
}

Check_date_cache GameStateUtils::calculate_check_cache(const game_state& gs)
{
	Check_date_cache result;
	init_checks_cache(gs, result);
	cache_checks(gs, result);
	cache_pins(gs, result);
	cache_king_attackers(gs, result);

	return result;
}

ZobristKey GameStateUtils::calculateKeyFromScratch(const game_state& gs)
{
	ZobristKey result = 0;

	if (gs.props.side_to_move == Black)
		result ^= ZobristInstance.sideToMove;

	result ^= ZobristInstance.enpSquares[1 + gs.props.enpassant_sq];
	result ^= ZobristInstance.freezeSquares[1 + gs.props.freeze_sq];
	result ^= ZobristInstance.jumpSquares[1 + gs.props.jump_sq];

	for (int side = 0; side < 2; ++side) {
		for (int board_side = 0; board_side < 2; ++board_side)
					if (gs.castling[side][board_side])
						result ^= ZobristInstance.castlingRights[side][board_side];

		
		result ^= ZobristInstance.spellsLeft[side][JUMP][gs.jump_spell[side].spells_left];
		result ^= ZobristInstance.spellsLeft[side][FREEZE][gs.freeze_spell[side].spells_left];
		result ^= ZobristInstance.spellsCooldown[side][JUMP][gs.jump_spell[side].couldown];
		result ^= ZobristInstance.spellsCooldown[side][FREEZE][gs.freeze_spell[side].couldown];
	}	
		
	for (int i = 0; i < 64; ++i) if (gs.pieces[i] != None) {
		int side = (Bitboards::square[i] & gs.white) ? White : Black;
		result ^= ZobristInstance.pieces[side][gs.pieces[i]][i];
	}
	return result;

}

void __scan_piece_list(std::string& result, const game_state& gs) {
	uint8_t index = 63;
	uint8_t empty_seq;
	Piece piece_type;
	const char piece_symbols[7] = { 0, 'p' - 'a', 'r' - 'a', 'n' - 'a', 'b' - 'a', 'q' - 'a', 'k' - 'a' };
	for (int8_t row = ROW_8; row >= ROW_1; --row) {
		empty_seq = 0;
		for (int8_t col = COL_A; col >= COL_H; --col) {
			piece_type = (Piece)gs.pieces[index];
			if (piece_type == None) {
				empty_seq += 1;
			}
			else {
				if (empty_seq != 0) {
					result += '0' + empty_seq;
				}
				empty_seq = 0;

				char base = (gs.white & (1ull << index)) ? 'A' : 'a';
				result += base + piece_symbols[piece_type];
			}
			--index;
		}
		if (row != ROW_1) result += "/";
	}
	result += " ";
}

void __scan_castling_rights(std::string& result, const game_state& gs) {
	bool anyMatch = false;
	if (gs.castling[GameStateUtils::White][KINGSIDE]) {
		result += "K";
		anyMatch = true;
	}
	if (gs.castling[GameStateUtils::White][QUEENSIDE]) {
		result += "Q";
		anyMatch = true;
	}
	if (gs.castling[GameStateUtils::Black][KINGSIDE]) {
		result += "k";
		anyMatch = true;
	}
	if (gs.castling[GameStateUtils::Black][QUEENSIDE]) {
		result += "q";
		anyMatch = true;
	}

	if (!anyMatch)
		result += "-";

	result += " ";
}

std::string GameStateUtils::export_fen(game_state& gs)
{
	std::string result = "";
	result.reserve(128);

	// step 1: export piece list
	__scan_piece_list(result, gs);

	// step 2: side to move
	result += (gs.props.side_to_move == White) ? "w " : "b ";

	// step 3: castling rights
	__scan_castling_rights(result, gs);
	
	// TODO: step 4:  enpassant square
	// TODO: step 5:  half move counter
	// TODO: step 7:  full move counter
	// TODO: step 8:  freeze square
	// TODO: step 9:  jump square
	// TODO: step 10: spell info

	result.shrink_to_fit();
	return result;
}

#undef __PARSE_NR_TO_PROPS(X)
#undef __PARSE_SQ_TO_PROPS(X)

void GameState_CLI_Display::set_background(ANSI_Colour c)
{
	std::cout << "\x1b[4" << (int)c << "m";
}

void GameState_CLI_Display::set_foreground(ANSI_Colour c)
{
	std::cout << "\x1b[3" << (int)c << "m";
}

void GameState_CLI_Display::reset_colour() {
	set_background(ANSI_BLACK);
	set_foreground(ANSI_WHITE);
}

void GameState_CLI_Display::show_board(game_state& gs) {
	Bitboard mask = (Bitboards::ONE << 63);

	static std::string wh_pieces[7] = { "# ", "P ", "R ", "N ", "B ", "Q ", "K " };
	static std::string bl_pieces[7] = { "# ", "p ", "r ", "n ", "b ", "q ", "k " };

	for (int row = 7; row >= 0; --row) {
		for (int column = 7; column >= 0; --column) {
			int color = (gs.black & mask) ? 1 : 0;

			std::cout << (color ? bl_pieces : wh_pieces)[gs.pieces[8*row + column]];
			reset_colour();

			mask >>= 1;
		}

		std::cout << "\n";
	}
}

Direction calcDir(int x0, int y0, int x1, int y1) {
	int dx = x1 - x0;
	int dy = y1 - y0;

	if (dx == 0 && dy > 0)
		return North;

	if (dx > 0 && dy > 0)
		return NorthEast;

	if (dx > 0 && dy == 0)
		return East;

	if (dx > 0 && dy < 0)
		return SouthEast;

	if (dx == 0 && dy < 0)
		return South;

	if (dx < 0 && dy < 0)
		return SouthWest;

	if (dx < 0 && dy == 0)
		return West;

	if (dx < 0 && dy > 0)
		return NorthWest;

	return OtherDirection;
}

Direction directions[64][64];

void initDirections(){
	for(size_t center = 0; center < 64; ++center) 
		for (size_t sq = 0; sq < 64; ++sq) 
			directions[center][sq] = calcDir(center & 7, center >> 3, sq & 7, sq >> 3);
}
