#pragma once

#include <cstdint>
#include <assert.h>
#include "Bitboard.h"
#include <random>
#include "macros.h"
// this class defines a set of utility classes that are Zobrist aware

typedef uint64_t ZobristKey;

const int ZobristSeed = 0xDEADBEEF;

class ZobristKeysTable {
	std::default_random_engine random_engine;
	std::uniform_int_distribution<uint64_t> distribution;
	ZobristKey generateKey();
public:
	ZobristKeysTable();
	ZobristKey pieces[2][7][64];
	ZobristKey castlingRights[2][2];
	ZobristKey spellsLeft[2][2][MAX_SPELL_COUNT - 1];
	ZobristKey spellsCooldown[2][2][6];
	ZobristKey enpSquares[65];
	ZobristKey jumpSquares[65];
	ZobristKey freezeSquares[65];
	ZobristKey sideToMove;

};

extern ZobristKeysTable ZobristInstance;