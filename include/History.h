#pragma once

#include "GameState.h"

// History table indexed by [side][Piece][to square]
extern int historyTable[2][7][64];

extern void clear_history_table();

