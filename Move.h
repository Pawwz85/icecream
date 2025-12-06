#pragma once
#include <cstdint>

typedef uint32_t Move;


namespace Move_Utils {
	enum Promotion_Target {
		Queen,
		Rook,
		Bishop,
		Knight
	};

	static uint8_t jump_sq(const Move& m) {
		return m & 63;
	}

	static uint8_t freeze_sq(const Move& m) {
		return (m >> 6) & 63;
	}

	static uint8_t to_sq(const Move& m) {
		return (m >> 12) & 63;
	}
	static uint8_t from_sq(const Move& m) {
		return (m >> 18) & 63;
	}

	static Promotion_Target promotion_target(const Move& m) {
		return (Promotion_Target)(m >> 30);
	}

	inline bool is_castle(const Move & m) {
		return m & (1u << 29);
	}

	inline bool uses_freeze(const Move& m) {
		return m & (1u << 28);
	}

	inline bool uses_jump(const Move& m) {
		return m & (1u << 27);
	}

	inline bool is_promotion(const Move& m) {
		return m & (1u << 26);
	}

	static Move addFreezeSquare(Move base, uint_fast8_t freeze_sq) {
		Move m = base;
		m &= ~(63 << 6);
		m |= freeze_sq << 6;
		m |= 1u << 28;
		return m;
	};

	static Move build_move(uint8_t from, uint8_t to, uint8_t freeze_sq, uint8_t jump_sq, uint8_t miscs = 0u) {
		Move result = miscs;
		result <<= 6u;
		result |= (Move)from;
		result <<= 6u;
		result |= (Move)to;
		result <<= 6u;
		result |= (Move)freeze_sq;
		result <<= 6u;
		result |= (Move)jump_sq;
		return result;
	}

	static uint8_t buildMiscs(bool uses_freeze, bool uses_jump, bool is_promotion, Promotion_Target target) {
		uint8_t result = target << 6;

		if (uses_freeze)
			result |= 16u;

		if (uses_jump)
			result |= 8u;

		if (is_promotion)
			result |= 4u;

		return result;

	}


	inline void __determine_post_castling_pos(const Move& m, uint8_t& king_pos, uint8_t& rook_pos) {
		int8_t org_king_pos = from_sq(m);
		int8_t org_rook_pos = to_sq(m);
		int8_t step = (org_rook_pos > org_king_pos) ? 1 : -1;

		rook_pos = org_king_pos + step;
		king_pos = rook_pos + step;
	}
	
}
