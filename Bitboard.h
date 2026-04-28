#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include "macros.h"

typedef  uint64_t Bitboard;

// utility functions of Bitboards.h
namespace _Bitboard_dev {

	/*
		Simple, iterative & intuitive implementation of a bit scanner.
	*/
	template < typename UINT, typename Iterator>
	void iterative_bit_scan(const UINT& b, Iterator& it) {
		UINT mask = UINT(1u);
		uint8_t index = 0;

		while (mask) {
			if (b & mask) {
				*it = index;
				++it;
			}

			++index;
			mask <<= 1;
		}
	}

	template < typename UINT, typename Iterator>
	void __lzcnt_based_bit_scanner(const UINT& b, Iterator& it) {
		UINT x = b;
		while (x) {
			*it = _tzcnt_u64(x);
			x ^= 1ull<<*it;
			++it;
		}
	}

	template <typename Iterator, uint8_t chunk_value>
	void __chunk_scanner(Iterator& it, const uint8_t & offset) {
		if constexpr (chunk_value & 1u)
			*it++ = offset + 0;
		if constexpr (chunk_value & 2u)
			*it++ = offset + 1;
		if constexpr (chunk_value & 4u)
			*it++ = offset + 2;
		if constexpr (chunk_value & 8u)
			*it++ = offset + 3;
		if constexpr (chunk_value & 16u)
			*it++ = offset + 4;
		if constexpr (chunk_value & 32u)
			*it++ = offset + 5;
		if constexpr (chunk_value & 64u)
			*it++ = offset + 6;
		if constexpr (chunk_value & 128u)
			*it++ = offset + 7;
	}
	typedef  void(*__ChunkScanner)(uint_fast8_t*&, const uint8_t&);
#define __CHUNK_SCANNER_CALLBACK(X) __chunk_scanner<uint_fast8_t*, uint8_t(X)>
	const __ChunkScanner __chunk_scanners[256] = {ENUMERATE_FUNC_256_ELEMENTS(__CHUNK_SCANNER_CALLBACK, 0)};
	template < typename UINT>
	void __lookup_based_bit_scanner(const UINT& b, uint_fast8_t*& it) {
		UINT mask = 255u;
		UINT x = b;
		__ChunkScanner callback;;
		for (uint8_t offset = 0; offset < 64; offset += 8) {
			callback = __chunk_scanners[x & mask];
			callback(it, offset);
			x >>= 8;
		}
	}

	static std::string bitboard_to_string(Bitboard b, const char * zero__, const char * one__) {
		/*
			Returns a string visualisation of a bitboard

			b - bitboard to format
			zero__ - String to render in place of `0` in the bitboard
			one__  - String to render in place of `1` in the bitboard
		*/
		Bitboard mask = (0x1ull)<<63;
		std::string result;

		for (int row = 7; row >= 0; --row) {
			result.append("\n");

			for (int column = 0; column < 8; ++column) {
				result.append((b & mask) ? one__: zero__);
				mask >>= 1;
			}

		}

		return result;
	}

	constexpr Bitboard lshift(Bitboard b) {
		return b << 1;
	}

	constexpr Bitboard lshift8(Bitboard b) {
		return b << 8;
	}

	constexpr bool freeze_pred(uint8_t center, uint8_t sq) {
		int x0 = center % 8, y0 = center / 8;
		int x1 = sq % 8, y1 = sq / 8;
		return std::max(std::abs(x0 - x1), std::abs(y0 - y1)) <= 1;
	}

	constexpr bool not_freeze_pred(uint8_t center, uint8_t sq) {
		return !freeze_pred(center, sq);
	}
}

// 
namespace Bitboards {
	const Bitboard ONE = 0x0000000000000001;
	static void(* bitboard_arr_scan)(const Bitboard& b, uint_fast8_t*& it) = _Bitboard_dev::__lzcnt_based_bit_scanner<Bitboard, uint_fast8_t* >;
	static void(* bitboard_vec_scan)(const Bitboard& b, std::vector<uint_fast8_t>::iterator& v) = _Bitboard_dev::__lzcnt_based_bit_scanner<Bitboard, std::vector<uint_fast8_t>::iterator>;
	static std::string to_string(const Bitboard& b) {
		return _Bitboard_dev::bitboard_to_string(b, "# ", "X ");
	}

#define __BITBOARDS_ROW(X) ENUMERATE_EIGHT_ITEMS(_Bitboard_dev::lshift, ONE<<(X))
	const Bitboard square[64] = { __BITBOARDS_ROW(0), __BITBOARDS_ROW(8), __BITBOARDS_ROW(16), __BITBOARDS_ROW(24), 
								  __BITBOARDS_ROW(32), __BITBOARDS_ROW(40), __BITBOARDS_ROW(48), __BITBOARDS_ROW(56) };
#undef __BITBOARDS_ROW
	
	const Bitboard row[8]    = { ENUMERATE_EIGHT_ITEMS(_Bitboard_dev::lshift8, 255) };
	const Bitboard column[8] = { ENUMERATE_EIGHT_ITEMS(_Bitboard_dev::lshift, 0x101010101010101ull) };

#define __LESS(X) ((1ull<<(X)) - 1)
#define __MORE(X) ((Bitboard)(-2) << (X) )
	const Bitboard less[64] = { ENUMERATE_FUNC_64_ELEMENTS(__LESS, 0) };
	const Bitboard more[64] = { ENUMERATE_FUNC_64_ELEMENTS(__MORE, 0) };

	inline Bitboard between(int8_t sq1, int8_t sq2) {
		return (sq1 > sq2) ? more[sq2] & less[sq1] : less[sq2] & more[sq1];
	}

	inline bool hasExactlyOneBitSet(const Bitboard& b) {
		return b && ((b & (b - 1)) == 0);
	}


	constexpr Bitboard ray_between(int8_t sq1, int8_t sq2) {
		
		int8_t x1 = sq1 % 8, x2 = sq2 % 8;
		int8_t y1 = sq1 / 8, y2 = sq2 / 8;
		
		Bitboard result = square[sq1];
		if (sq1 == sq2)
			return 0; 
		
		if (x1 != x2 && y1 != y2 && x1 + y1 != x2 + y2 && x1 - y1 != x2 - y2)
			return 0;

		Bitboard mask = result;

		while (mask != square[sq2]) {

			if (x2 < x1) mask >>= 1;
			if (x2 > x1) mask <<= 1;
			if (y2 < y1) mask >>= 8;
			if (y2 > y1) mask <<= 8;
			
			result |= mask;
		}

		result &= ~square[sq2];
		result &= ~square[sq1];

		return result;
	}

	inline Bitboard ray_between_with_caching(int8_t sq1, int8_t sq2) {
		static Bitboard* cache = nullptr;
		size_t index = 64 * sq1 + sq2;
		
		if (!cache) {
			cache = new Bitboard[64 * 64];

			for (int8_t i = 0; i < 64; ++i)
				for (int8_t j = 0; j < 64; ++j)
					cache[64 * i + j] = ray_between(i, j);
		}

		return cache[index];

	}

	inline uint8_t to_index(Bitboard square) {
		return (uint8_t)_tzcnt_u64(square);
	}
	


#define ROW_1 0
#define ROW_2 1
#define ROW_3 2
#define ROW_4 3
#define ROW_5 4
#define ROW_6 5
#define ROW_7 6
#define ROW_8 7

#define COL_A 7
#define COL_B 6
#define COL_C 5
#define COL_D 4
#define COL_E 3
#define COL_F 2
#define COL_G 1
#define COL_H 0

	constexpr uint8_t to_index(uint8_t col_, uint8_t row_) {
		return (row_ << 3) + col_;
	}

	constexpr Bitboard sq(uint8_t col_, uint8_t row_) {
		return row[row_] & column[col_];
	}

	inline void clr_sq(Bitboard& b, uint8_t sq) { b &= ~square[sq]; }
	inline void set_sq(Bitboard& b, uint8_t sq) { b |=  square[sq]; }


	template <bool (*Pred)(uint8_t, uint8_t)>
	constexpr std::array<Bitboard, 64> lookupTableForPred() {
		std::array<Bitboard, 64> result;
		Bitboard temp;

		for (uint8_t index = 0; index < 64; ++index) {
			temp = 0ULL;
			for (uint8_t sq = 0; sq < 64; ++sq)
				if (Pred(index, sq))
					temp |= 1ULL << sq;
			result[index] = temp;
		}
		return result;
	}

	template <bool (*Pred)(uint8_t, uint8_t)>
	const Bitboard& look_up_predicate(const uint8_t& square) {
		static const auto table = lookupTableForPred<Pred>();
		return table[square];
	}

	static const Bitboard & (*frozen_area)(const uint8_t & index) = look_up_predicate<_Bitboard_dev::freeze_pred>;
	static const Bitboard & (*not_frozen_area)(const uint8_t& index) = look_up_predicate<_Bitboard_dev::not_freeze_pred>;

};

