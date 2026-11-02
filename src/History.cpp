#include <History.h>

#include <cstddef>

int historyTable[2][7][64];

void clear_history_table() {
    for (int i = 0; i < 7; ++i)
        for (int j = 0; j < 64; ++j)
            historyTable[0][i][j] = historyTable[1][i][j] = 0;
}