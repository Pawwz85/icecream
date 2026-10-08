#include <Zobrist.h>


ZobristKey ZobristKeysTable::generateKey()
{
    return distribution(random_engine);
}

ZobristKeysTable::ZobristKeysTable()
{
    this->random_engine.seed(ZobristSeed);

    for (int side = 0; side < 2; ++side) {
        for (int piece_type = 0; piece_type < 7; piece_type++)
            for (int i = 0; i < 64; ++i)
                pieces[side][piece_type][i] = generateKey();

        for (int b_side = 0; b_side < 2; ++b_side)
            castlingRights[side][b_side] = generateKey();

        for (int spell = 0; spell < 2; ++spell) {
            for (int count = 0; count < MAX_SPELL_COUNT; ++count) {
                spellsLeft[side][spell][count] = generateKey();
            }

            for (int cooldown = 0; cooldown <= JUMP_COOLDOWN; cooldown++) {
                spellsCooldown[side][spell][cooldown] = generateKey();
            }
        }

    }
    
    sideToMove = generateKey();

    for (int i = 0; i < 65; ++i) {
        enpSquares[i] = generateKey();
        jumpSquares[i] = generateKey();
        freezeSquares[i] = generateKey();
    }
        

}

ZobristKeysTable ZobristInstance;