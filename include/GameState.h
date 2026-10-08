#pragma once

#include "Bitboard.h"
#include "Move.h"
#include "Zobrist.h"
#include "lazy_eval_params.h"
#include <iostream> // for CLI display


struct spell_info {
	uint16_t spells_left;
	uint16_t couldown;
};

#define KINGSIDE 0
#define QUEENSIDE 1

const Bitboard castlingKingPath[2][2] = {
	{
	 Bitboards::sq(COL_E, ROW_1) | Bitboards::sq(COL_F, ROW_1) | Bitboards::sq(COL_G, ROW_1),
	 Bitboards::sq(COL_E, ROW_1) | Bitboards::sq(COL_D, ROW_1) | Bitboards::sq(COL_C, ROW_1)
	},
	{
	 Bitboards::sq(COL_E, ROW_8) | Bitboards::sq(COL_F, ROW_8) | Bitboards::sq(COL_G, ROW_8),
	 Bitboards::sq(COL_E, ROW_8) | Bitboards::sq(COL_D, ROW_8) | Bitboards::sq(COL_C, ROW_8)
	}
};

enum Piece {
	None,
	Pawn,
	Rook,
	Knight,
	Bishop,
	Queen,
	King
};

enum Direction {
	North,
	NorthEast,
	East,
	SouthEast,
	South,
	SouthWest,
	West,
	NorthWest,
	OtherDirection, // value used for all other directions
	Direction_MemberCount
};

extern Direction directions[64][64];

void initDirections();


struct efferal_state_props {
	uint32_t half_move_counter;
	uint32_t move_counter;
	uint8_t side_to_move;
	int8_t enpassant_sq; // -1 for no en passant
	int8_t freeze_sq; // -1 for no freeze
	int8_t jump_sq; // -1 for no jump
};

struct pos_history_record {
	ZobristKey hashVal;
	uint32_t isReversible; // either 0 or 1, 1 if this position was reached by playing reversible move
};

struct incremental_eval_stats {
	int material_balance;
	int end_gm_score;
	int mid_gm_score;
	int slider_values[2];
	uint8_t phase;
};

struct Check_data_cache {
	Bitboard kingAttackers;
	Bitboard us;
	Bitboard them;
	Bitboard pinMasks[Direction_MemberCount];
	Bitboard checkMasks[Direction_MemberCount];
	uint8_t  pinned[Direction_MemberCount];
	uint8_t  offenders[Direction_MemberCount];
	uint8_t  kingPos;
};

struct game_state {

	game_state() : position_history(nullptr) {};

	uint64_t zobrist_hash;
	Bitboard black;
	Bitboard white;
	Bitboard pawns;
	Bitboard knights;
	Bitboard bishops;
	Bitboard rooks;
	Bitboard queens;
	Bitboard kings; // a waste of space, but left here for consistency of logic

	Bitboard frozen;
	Bitboard jumpable;

	incremental_eval_stats eval;
	efferal_state_props props; 
	Check_data_cache check_data;

	spell_info freeze_spell[2];
	spell_info jump_spell[2];
	
	bool castling[2][2];
	uint8_t pieces[64];

	uint16_t pos_his_index;
	std::vector<pos_history_record>* position_history;

	inline bool inCheck() const {
		return check_data.kingAttackers != 0;
	};

};

namespace GameStateUtils {
	enum Colour {
		White,
		Black
	};

	void clear(game_state& gs);

	inline void try_revoke_castling_rights(game_state& gs, Colour side, uint8_t board_side) {
		if (gs.castling[side][board_side]) {
			gs.zobrist_hash ^= ZobristInstance.castlingRights[side][board_side];
		}
		gs.castling[side][board_side] = false;
	}

	void make_move(game_state& gs, const Move &  m);
	void make_null_move(game_state& gs);

	// this function is NOT meant to be used inside engine logic, but to validate if moves from external source are legal
	bool is_move_legal(const game_state& gs, const Move& m);
	bool can_use_freeze(const game_state& gs, Colour side);
	bool can_use_jump(const game_state& gs, Colour side);

	bool is_threefold_repetition(const game_state& gs);
	bool is_repetition(const game_state& gs);


	static constexpr Bitboard __precompute_frozen(uint8_t sq) {
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


	const Bitboard frozen_area[64] = { ENUMERATE_FUNC_64_ELEMENTS(__precompute_frozen, 0) };

	void __unchecked_spawn  (game_state & gs, uint8_t sq, Colour colour, Piece piece);
	void __unchecked_despawn(game_state & gs, uint8_t sq, Colour colour, Piece piece);

	// expects piece_list formatted in FEN format like 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR' 
	int __unckecked_place_pieces(game_state& gs, const std::string & piece_list);
	
	void __parse_castling_rights(game_state& gs, const std::string& str);

	int __parse_number(const std::string& str, int& err);
	int __parse_square(const std::string& str, int& err);
	
	int __parse_side_to_move(efferal_state_props& props, const std::string& str);

	// expects str to contain a decimal number, or -
	int __parse_half_move_counter(efferal_state_props& props, const std::string& str);

	// expects str to contain a decimal number, or -
	int __parse_move_counter(efferal_state_props& props, const std::string& str);


	// TODO: implement those functions bellow 

	// expects str to contain a decimal number, or -
	int __parse_enpassant_info(efferal_state_props& props, const std::string& str);



	// expects str to contain a decimal number, or -
	int __parse_frozen_square(efferal_state_props& props, const std::string& str);

	// expects str to contain a decimal number, or -
	int __parse_jumpable_square(efferal_state_props& props, const std::string& str);



	/*
		Format:
		F0010/J0110/f0205/j0205 
		
		Can be checked by grammar [[FfJj][0-9][0-9][0-9][0-9]]

		Each segment starts with the letter j(ump) or f(reeze).
		Capital letter means that information is meant for white, otherwise for black player.

		Next 2 digits represents amount of spells left, while other 2 digits represents half moves left for player to be able to cast this spell
	*/

	int __parse_spell_info(game_state& gs, const std::string& str);

	int parse_fen(game_state& gs, const std::string& fen_string);

	void cache_pins(game_state& gs);
	void cache_checks(game_state& gs);
	void cache_king_attackers(game_state& gs);
	void init_checks_cache (game_state& gs);

	void calculate_check_cache(game_state& gs);

	ZobristKey calculateKeyFromScratch(const game_state& gs);

	std::string export_fen(game_state& gs);

	inline void cast_null_freeze(game_state& gs, const uint8_t& side) {
		gs.zobrist_hash ^= ZobristInstance.spellsLeft[side][FREEZE][gs.freeze_spell[side].spells_left];
		gs.zobrist_hash ^= ZobristInstance.spellsCooldown[side][FREEZE][gs.freeze_spell[side].couldown];
		gs.freeze_spell[side].spells_left -= 1;
		gs.freeze_spell[side].couldown = FREEZE_COOLDOWN;
		gs.zobrist_hash ^= ZobristInstance.spellsLeft[side][FREEZE][gs.freeze_spell[side].spells_left];
		gs.zobrist_hash ^= ZobristInstance.spellsCooldown[side][FREEZE][gs.freeze_spell[side].couldown];
	};

	inline void set_freeze_cooldown(game_state& gs, const uint8_t& side, const uint8_t& value) {
		gs.zobrist_hash ^= ZobristInstance.spellsCooldown[side][FREEZE][gs.freeze_spell[side].couldown];
		gs.freeze_spell[side].couldown = value;
		gs.zobrist_hash ^= ZobristInstance.spellsCooldown[side][FREEZE][value];
	};
}

namespace GameState_CLI_Display {
	enum ANSI_Colour {
		ANSI_BLACK,
		ANSI_RED,
		ANSI_GREEN,
		ANSI_YELLOW,
		ANSI_BLUE,
		ANSI_MAGENTA,
		ANSI_CYAN,
		ANSI_WHITE
	};

	void set_background(ANSI_Colour c);
	void set_foreground(ANSI_Colour c);
	void reset_colour();

	void show_board(game_state& gs);
}