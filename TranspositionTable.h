#pragma once
#include "Zobrist.h"
#include "Move.h"



struct TTEntry {
    ZobristKey key;
    int eval;
    int depth;
    enum Flag { EXACT, LOWER, UPPER } flag;
    Move bestMove;
};

const size_t TTSize = 104857600ull/ sizeof(TTEntry) + 7; // around 100 mb

inline size_t calculate_index(ZobristKey key) {
    return key % TTSize;
}

static TTEntry transpositionTable[TTSize];
