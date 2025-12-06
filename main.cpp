#include <iostream>
#include "GameState.h"
#include "MoveGen.h"
#include "Perft.h"


int main() {

	Bitboard b;

	for (unsigned int i = 0; i < 64; ++i) {
		std::cout << "Square nr: " << i << "\n";
		//std::cout << Bitboards::to_string(move_gen::knight_attacks[i]);
		std::cout<< Bitboards::to_string(move_gen::knight_attacks[i]) << "\n";
		std::cout << "\n";
	}

	move_gen::__init_magics();

	game_state gs;
	GameStateUtils::clear(gs);
	GameState_CLI_Display::show_board(gs);

	GameStateUtils::__unckecked_place_pieces(gs, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");

	Move moves[512];
	Move* end = moves;
	std::string fen;


	while (1) {
		std::cout << "Enter Elixir fen: \n";
		std::getline(std::cin, fen);

		GameStateUtils::parse_fen(gs, fen);
		GameState_CLI_Display::show_board(gs);
		
		for (int i = 1; i < 10; ++i) {
			auto res = perft(fen, i);
			std::cout << "Depth " << i << ", nodes: " << res.node_counter << ", time: " << res.seconds<<", nodes per sec: "<<res.node_per_seconds<<"\n";
		}


	}
	

}