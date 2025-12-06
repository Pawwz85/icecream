#include "../../Bitboard.h"
#include "../../MoveGen.h"
#include <iostream>

int main() {
	move_gen::__init_magics();

	Bitboard b;

	size_t index = 36;
	
	Bitboard blockerMask = move_gen::Magics[index][move_gen::Diagonal].mask;

	do {
		std::cout << "\n" << "Showing blockers: \n"<< Bitboards::to_string(blockerMask) << "\n";
		std::cout << "\n" << "Showing secondary: \n" << Bitboards::to_string(move_gen::Magics[index][move_gen::Diagonal].getAttacks(blockerMask).secondary) << "\n";
		blockerMask = (blockerMask - 1) & (move_gen::Magics[index][move_gen::Diagonal].mask);
	} while (blockerMask != move_gen::Magics[index][move_gen::Diagonal].mask);
	
	while (true) {
		std::cout << "Please enter bitboard you want to inspect: ";
		std::cin >> b;
		std::cout << "\n" << Bitboards::to_string(b) << "\n";
	}
}