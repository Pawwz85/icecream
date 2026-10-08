#include <TranspositionTable.h>

TTEntry transpositionTable[TTSize];

void clear_transposition_table() {
	for (size_t i = 0; i < TTSize; ++i)
		transpositionTable[i].key = 0;
};