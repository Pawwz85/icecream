#include <fstream>
#include <iostream>
#include <cstdlib>
#include <SpellChessGame.h>

const std::string defaultOutputDirectory = "";


std::string concatenateFen(int argc, char* argv[]) {
	std::string part;
	std::string sum;

	for (int i = 1; i < argc; ++i) {
		part = argv[i];
		sum += part + " ";
	}
	return sum;
}

void extractPieceAtFacts(const SpellChessLib::GameState& state, const std::string& dir) {
	const std::string fileName = dir + "\PieceAt.facts";
	std::cout << "Writting to " << fileName<<"\n";
	std::ofstream out;
	out.open(fileName);

	for (SpellChessLib::SquareIndex index = 0; index < 64; ++index)
		if (!state.getSquare(index).isEmpty()){
			auto sq = SpellChessLib::squareIndexToString(index);
			out << "[" << sq[0] << "," << sq[1] << "]\t"; // write square
			out << "[" << (state.getSquare(index).piece.owner == SpellChessLib::White ? "white" : "black") << ",";

			switch (state.getSquare(index).piece.type)
			{
				case SpellChessLib::Pawn:
					out << "pawn]\n";
					break;
				case SpellChessLib::Bishop:
					out << "bishop]\n";
					break;
				case SpellChessLib::Knight:
					out << "knight]\n";
					break;
				case SpellChessLib::Rook:
					out << "rook]\n";
					break;
				case SpellChessLib::Queen:
					out << "queen]\n";
					break;
				case SpellChessLib::King:
					out << "king]\n";
					break;
			}
	}
	out.close();
}

void extractSideToMove(const SpellChessLib::GameState& state, const std::string& dir) {
	const std::string fileName = dir + "\SideToMove.facts";
	std::cout << "Writting to " << fileName << "\n";
	std::ofstream out;
	out.open(fileName);
	out << (state.getSideToMove() == SpellChessLib::White ? "white" : "black");
	out.close();
}

void exctractCastlingRights(const SpellChessLib::GameState& state, const std::string& dir) {
	const std::string fileName = dir + "\HasCastlingRight.facts";
	std::cout << "Writting to " << fileName << "\n";
	std::ofstream out;
	out.open(fileName);

	if (state.canCastle(SpellChessLib::White, SpellChessLib::Kingside))
		out << "white\tkingside\n";

	if (state.canCastle(SpellChessLib::White, SpellChessLib::QueenSide))
		out << "white\tqueenside\n";

	if (state.canCastle(SpellChessLib::Black, SpellChessLib::Kingside))
		out << "black\tkingside\n";

	if (state.canCastle(SpellChessLib::Black, SpellChessLib::QueenSide))
		out << "black\tqueenside\n";

	out.close();
}

void exctractSpells(const SpellChessLib::GameState& state, const std::string& dir) {
	const std::string fileName = dir + "\SpellCounter.facts";
	std::cout << "Writting to " << fileName << "\n";
	std::ofstream out;
	out.open(fileName);
	out << "white\tfreeze\t" << state.getSpell(SpellChessLib::White, SpellChessLib::FreezeSpell).usagesLeft << "\t" << state.getSpell(SpellChessLib::White, SpellChessLib::FreezeSpell).cooldown<<"\n";
	out << "black\tfreeze\t" << state.getSpell(SpellChessLib::Black, SpellChessLib::FreezeSpell).usagesLeft << "\t" << state.getSpell(SpellChessLib::Black, SpellChessLib::FreezeSpell).cooldown << "\n";
	out << "white\tjump\t" << state.getSpell(SpellChessLib::White, SpellChessLib::JumpSpell).usagesLeft << "\t" << state.getSpell(SpellChessLib::White, SpellChessLib::JumpSpell).cooldown << "\n";
	out << "black\tjump\t" << state.getSpell(SpellChessLib::Black, SpellChessLib::JumpSpell).usagesLeft << "\t" << state.getSpell(SpellChessLib::Black, SpellChessLib::JumpSpell).cooldown << "\n";
	out.close();
}


void createSquareFact(const SpellChessLib::SquareIndex& sq, const std::string& dir, const std::string& filename) {
	const std::string fileName = dir + filename;
	std::cout << "Writting to " << fileName << "\n";
	std::ofstream out;
	out.open(fileName);
	if(sq != SpellChessLib::NoSquare)
		out << SpellChessLib::squareIndexToString(sq);
	out.close();
}

void extractFacts(const SpellChessLib::GameState& state, const std::string& dir) {
	extractPieceAtFacts(state, dir);
	extractSideToMove(state, dir);
	exctractCastlingRights(state, dir);
	createSquareFact(state.getEnppassantSquare(), dir, "EnpassantSquare.facts");
	createSquareFact(state.getFreezeSquare(), dir, "FreezeSquare.facts");
	createSquareFact(state.getJumpSquare(), dir, "JumpSquare.facts");
	exctractSpells(state, dir);
}

int main(int argc, char* argv[]) {
	// rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 - - F50/J20/f50/j20
	
	if (argc != 10) {
		std::cerr << "Expected 9 positional arguments, found" << argc - 1 << std::endl;
		return -1;
	}

	std::string outputDirectory;

#pragma warning(suppress : 4996)
	if (const char* env = std::getenv("SpellChessMoveGenDatalogPath"))
		outputDirectory = env;
	else
		outputDirectory = defaultOutputDirectory;

	std::string fen = concatenateFen(argc, argv);
	SpellChessLib::FenParser parser;

	try {
		extractFacts(parser.fromSpellChessFen(fen.c_str()), outputDirectory);
	}
	catch (SpellChessLib::FenValidator::InvalidFenException e) {
		std::cerr << "Provided arguments are invalid spell chess fen";
		return -1;
	}
	

}