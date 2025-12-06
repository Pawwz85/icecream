#include "uci_client.h"
#include "Bitboard.h"
#include "GameState.h" // for FEN string correctness testing
#include <stdarg.h>
#include <sstream>


std::string UCI::formatString(std::string format, ...)
{
	std::string result = "";
	int phase = 0;
	va_list args;
	va_start(args, format);
	std::string str;
	int32_t decimal;

	char buff[32];
	char* it;
	for (char c : format) {

		if (phase) goto PHASE_2;
		
		if (c == '%')
			phase = 1;
		else
			result.push_back(c);
		
		continue;

	PHASE_2:

		if (c == '%' || c == 'd' || c == 's') {
			// TODO: handle writing parameter to format

			switch (c) {
				case '%':
					result += "%";
					break;
				case 's':
					str = va_arg(args, const char*);
					result += str;
					break;
				case 'd':
					decimal = va_arg(args, int32_t);
					if (decimal < 0) { result.push_back('-'); decimal *= -1; }
					it = buff;

					do { *it++ = '0' + (decimal % 10); decimal/=10; } while (decimal != 0);
					do { result.push_back(*(--it)); } while (it != buff);
					break;

			}

		}
		else {
			result.push_back('%');
			result.push_back(c);
		}

		phase = 0;
	}

	return result;
}

std::string UCI::formatSquare(uint8_t sq)
{
	uint8_t y = sq / 8;
	uint8_t x = sq % 8;
	std::string result = "a1";

	result[0] += 7 - x;
	result[1] += y;
	return result;
}

std::string UCI::formatMove(Move m)
{
	uint8_t from = Move_Utils::from_sq(m);
	uint8_t to = Move_Utils::to_sq(m);
	int8_t freeze_sq = Move_Utils::freeze_sq(m);
	int8_t jump_sq = Move_Utils::jump_sq(m);

	bool usesFreeze = Move_Utils::uses_freeze(m);
	bool usesJump = Move_Utils::uses_jump(m);

	uint8_t _;
	// set to square to be equal king position post castling
	if (Move_Utils::is_castle(m)) {
		Move_Utils::__determine_post_castling_pos(m, to, _);
	}



	std::string base = "";

	base = formatString("%s%s", formatSquare(from).c_str(), formatSquare(to).c_str());

	const std::string prom_chars[4] = { "q", "r", "b", "n" };

	if (Move_Utils::is_promotion(m))
		base += prom_chars[Move_Utils::promotion_target(m)];
	

	if (usesFreeze)
		return formatString("freeze@%s&%s", formatSquare(freeze_sq).c_str(), base.c_str());

	if (usesJump)
		return formatString("jump@%s&%s", formatSquare(jump_sq).c_str(), base.c_str());

	return base;
}

std::string UCI::formatScore(int score)
{
	if (abs(score) < 90000)
		return formatString("cp %d", int32_t(score));

	// mate distance in full moves
	int32_t mate_distance = (100001 - abs(score))/2;

	if (score < 0)
		mate_distance *= -1;

	return formatString("mate %d", mate_distance);
}

// parses square into engines coordinates system. Sets '2' bit in case of any errors 
uint8_t UCI::parseSquare(const std::string str, int& err)
{

	if (str.size() != 2) {
		err |= 2; 
		return 0;
	}

	uint8_t x = 7 - (str[0] - 'a');
	uint8_t y = str[1] - '1';

	if (x < 0 || x > 7 || y < 0 || y > 7) {
		err |= 2;
		return 0;
	}
	return 8*y + x;
}

Move UCI::parseMove(const std::string str, const game_state & gs, int& err)
{
	/*
	Move format:


	<sq><sq> for move without spells, for example e2e4
	freeze@<sq>&<base_move> for base_move that uses freeze on sq square. For example: freeze@e7&e2e4 freeze@g2&e8h8
	jump@<sq>&<base_move> for base_move that casts jump on sq square. For example: jump@a5&a1a8, jump@b1&e1a1
		
	*/

	uint8_t freeze_sq	= 0;
	uint8_t jump_sq		= 0;
	uint8_t from_sq		= 0;
	uint8_t to_sq		= 0;
	uint8_t misc		= 0;
	bool uses_freeze	= false;
	bool uses_jump		= false;
	bool is_castling	= false;
	bool is_promotion	= false;
	Move_Utils::Promotion_Target promotion_target = (Move_Utils::Promotion_Target)0;


	size_t base_move_offset = 0;

	if (str.substr(0, 7) == "freeze@") {
		uses_freeze = true;
		freeze_sq = parseSquare(str.substr(7, 2), err);
		base_move_offset = 10; // freeze@sq&

		if (str.size() < 9 || str[9] != '&') {
			err |= 4;
			return 0;
		}
	}

	if (str.substr(0, 5) == "jump@") {
		uses_jump = true;
		jump_sq = parseSquare(str.substr(5, 2), err);
		base_move_offset = 8; // jump@sq&

		if (str.size() < 7 || str[7] != '&') {
			err |= 4;
			return 0;
		}
	}

	const std::string base_move = str.substr(base_move_offset);
	char promotion_char = 'q';

	from_sq = parseSquare(base_move.substr(0, 2), err);
	to_sq	= parseSquare(base_move.substr(2, 2), err);

	if (base_move.size() > 4) {
		promotion_char = base_move[4];
		is_promotion = true;
	}
	
	
	switch (promotion_char) {
		case 'q':
			promotion_target = Move_Utils::Queen;
			break;
		case 'n':
			promotion_target = Move_Utils::Knight;
			break;
		case 'b':
			promotion_target = Move_Utils::Bishop;
			break;
		case 'r':
			promotion_target = Move_Utils::Rook;
			break;
		default:
			err |= 8;
			is_promotion = false;
	}

	// checks if move is castling. 
	// note that determining legality of the move, will be done later
	if (err == 0 && gs.pieces[from_sq] == King ) {
		int8_t x1 = from_sq % 8;
		int8_t x2 = to_sq % 8;
		
		is_castling = abs(x1 - x2) == 2;
	
		// our engine denote castling as king capturing rook
		if (is_castling) {
			to_sq = (to_sq&~7u) + (x2 > x1 ? 7: 0);
		}

	}



	if (is_promotion) {
		misc |= 4u;
		misc |= promotion_target << 6;
	}
	if (uses_jump)   misc |= 8u;
	if (uses_freeze) misc |= 16u;
	if (is_castling) misc |= 32u;


	return Move_Utils::build_move(
		from_sq,
		to_sq,
		freeze_sq,
		jump_sq, 
		misc
	);
}

std::string UCI::formatCommandStatus(CommandStatus status)
{
	static const char* words[3] = { "checking", "ok", "error" };
	return words[status];
}

std::string UCI::formatOptionContent(MemSafeOption option)
{
	std::string accumulator = "";
	switch (option.type())
	{
		case Check:
			return formatString("type check default %s", option.check_content() ? "true" : "false");
		case Spin:
			return formatString("type spin default %d min %d max %d", option.spin_content().default_, option.spin_content().min, option.spin_content().max);
		case Combo:
			for (std::string s : option.combo_content().supported_values) {
				accumulator += " var " + s;
			}
			return formatString("type combo default %s %s", option.combo_content().default_.c_str(), accumulator.c_str());
		case Button:
			return "type button";
		case String:
			return formatString("type string %s", option.str_content().empty() ? "<empty>" : option.str_content().c_str());
		
	}
}

UCI::go_params UCI::make_default_go_params()
{
	go_params params;

	params.black_time_control = { true, 0, 0 };
	params.white_time_control = { true, 0, 0 };
	params.search_moves = nullptr;
	params.moves_to_go = 0;
	params.move_time =	 0;
	params.nodes_limit = 0;
	params.depth_limit = 0;


	params.infinite_mode =	false;
	params.ponder_mode =	false;
	params.mate_search_mode = false;
	return params;
}

std::string UCI::ClientOutputMessageFormatter::id_name(std::string engine_name) const
{
	return formatString("id name %s", engine_name.c_str());
}

std::string UCI::ClientOutputMessageFormatter::id_author(std::string engine_author) const
{
	return formatString("id author %s", engine_author.c_str());
}

std::string UCI::ClientOutputMessageFormatter::uciok() const
{
	return "uciok";
}

std::string UCI::ClientOutputMessageFormatter::readyok() const
{
	return "readyok";
}

std::string UCI::ClientOutputMessageFormatter::best_move(Move best, Move ponder_move) const
{
	return ponder_move ?
		formatString("bestmove %s %s", formatMove(best).c_str(), formatMove(ponder_move).c_str()) :
		formatString("bestmove %s", formatMove(best).c_str());
}

std::string UCI::ClientOutputMessageFormatter::copyprotection(CommandStatus status) const
{
	return formatString("copyprotection %s", formatCommandStatus(status).c_str());
}

std::string UCI::ClientOutputMessageFormatter::registration(CommandStatus status) const
{
	return formatString("registration %s", formatCommandStatus(status).c_str());
}

std::string UCI::ClientOutputMessageFormatter::info(std::string payload) const
{
	return formatString("info %s", payload.c_str());
}

std::string UCI::ClientOutputMessageFormatter::option(const std::vector<MemSafeOption>& supported_options) const
{

	std::string result = "";

	for (MemSafeOption option : supported_options) {
		result += formatString("option name %s %s", option.id().c_str(), formatOptionContent(option).c_str());
	}

	return result;
}

void UCI::InputListener::logError(const std::string& message)
{
	*out << formatString("string %s", message.c_str());
}

std::function<void(const std::vector<std::string>&)> UCI::InputListener::wrapDebugCallback()
{
	return [this](const std::vector<std::string> & args) {
		
		if (args.size() < 2) {
			this->logError("Expected debug [on/off], found debug");
			return;
		}

		const std::string& status = args[1];

		if (status != "on" && status != "off") {
			this->logError("Expected debug [on/off], found debug " + status);
			return;
		};

		this->callbacks.on_debug(status[1] == 'n');
	};
}

std::function<void(const std::vector<std::string>&)> UCI::InputListener::wrapSetOptionCallback()
{
	return [this](const std::vector<std::string>& args) {

		if (args.size() < 3) {
			this->logError("Expected at least 2 positional arguments for setoption command");
			return;
		}

		if (args[1] != "name") {
			this->logError("Expected setoption name <id>, found setoption " + args[1] + " <id>");
			return;
		}

		const std::string& id = args[2];

		std::string value = ""; // this can be empty for the button option, verification will be done inside callback, we are only parsing args here
		

		// set value option
		if (args.size() > 4 && args[3] == "value" && args[4] != "<empty>") {
			for (size_t i = 4; i < args.size(); ++i)
				value += args[i] + " ";
		}

		this->callbacks.on_setoption(id, value);
		};
}

std::vector<Move> UCI::InputListener::_parse_move_vector(const std::vector<std::string>& args, size_t moves_index, game_state & gs, int & error_code ) {
	Move m;
	std::vector<Move> moves;
	for (size_t i = moves_index + 1; i < args.size(); ++i) {

		m = UCI::parseMove(args[i], gs, error_code);
		if (error_code) {
			this->logError(UCI::formatString("position command contains invalid move: %s", args[i].c_str()));
			return moves;
		}

		if (!GameStateUtils::is_move_legal(gs, m)) {
			this->logError(UCI::formatString("position command contains illegal move: %s", args[i].c_str()));
			error_code |= 1;
			return moves;
		}

		GameStateUtils::make_move(gs, m);
		moves.push_back(m);
	}

	return moves;
}

std::function<void(const std::vector<std::string>&)> UCI::InputListener::wrapPositionCallback()
{
	/*
		POSITION COMMAND FORMAT

		// 1. STANDARD CHESS START POSITION
		position startpos [moves <move1> <move2> ...]

		// 2. SPELL CHESS START POSITION
		position spellstartpos [moves <move1> <move2> ...]

		// 3. STANDARD CHESS FEN
		position <piece_list> <w/b> <castling_rights> <enpassant> <halfmove_clock> <fullmove_number>
					[moves <move1> <move2> ...]

		// 4. SPELL CHESS EXTENDED FEN
		position <piece_list> <w/b> <castling_rights> <enpassant> <halfmove_clock> <fullmove_number>
					<freeze_sq> <jump_sq> <spell_info>
					[moves <move1> <move2> ...]
	*/
	return [this](const std::vector<std::string>& args) {
		
		if (args.size() < 2) {
			this->logError("position command can not be empty");
			return;
		}

		std::string fen = "";
		const std::string& pos = args[1];

		size_t moves_index = 0;
		bool moves_tok_present = false;
		for (moves_index; moves_index < args.size(); ++moves_index)
			if (moves_tok_present = (args[moves_index] == "moves"))
				break;

		if (pos == "startpos" || pos == "spellstartpos") {
			fen = pos[1] == 't' ?
				"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 - - F00/J00/f00/j00" :
				"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 - - F50/J20/f50/j20";
		}
		else {

			if (args[1] != "fen") {
				this->logError("Expected position [fen/startpos/spellstartpos]");
			}

			size_t fen_tokens = moves_index - 2;

			if (fen_tokens != 6 && fen_tokens != 9) {
				this->logError(
					moves_tok_present ? "Incorrect Fen received, expected 6 or 9 components" : "Incorrect FEN received, missing 'moves' token"
				);
				return;
			}
			
			fen = args[2];
			for (size_t i = 1; i < fen_tokens; ++i) {
				fen += " " + args[2 + i];
			}

			// insert spell chess component for standard chess position
			if (fen_tokens == 6)  fen += " - - F00/J00/f00/j00";
		};

		game_state gs;
		int32_t error_code = GameStateUtils::parse_fen(gs, fen);

		if (error_code) {
			this->logError(formatString("Fen validation failed with %d error code", error_code));
			return;
		}

		std::vector<Move> moves = _parse_move_vector(args, moves_index, gs, error_code);
		if (error_code) return; // error was logged inside _parse_move_vector() call. 
		this->callbacks.on_position(fen, moves);
	};

}

std::function<void(const std::vector<std::string>&)> UCI::InputListener::wrapGoCallback()
{
	return [this](const std::vector<std::string>& args) {
		go_params result = make_default_go_params();

		size_t i = 1;
		std::string temp;
		while (i < args.size()) {

			const std::string& arg = args[i++];

			if (arg == "ponder") {
				result.ponder_mode = true;
				continue;
			}

			if (arg == "wtime") {
				temp = args[i++];

				result.white_time_control.infinite = false;
				result.white_time_control.time_left = std::stoi(temp);
				continue;
			}

			if (arg == "btime") {
				temp = args[i++];

				result.black_time_control.infinite = false;
				result.black_time_control.time_left = std::stoi(temp);
				continue;
			}

			if (arg == "winc") {
				temp = args[i++];

				result.white_time_control.infinite = false;
				result.white_time_control.time_inc = std::stoi(temp);
				continue;
			}

			if (arg == "binc") {
				temp = args[i++];

				result.black_time_control.infinite = false;
				result.black_time_control.time_inc = std::stoi(temp);
				continue;
			}

			if (arg == "movestogo") {
				temp = args[i++];

				result.moves_to_go = std::stoi(temp);
				continue;
			}
			
			if (arg == "depth") {
				temp = args[i++];

				result.depth_limit = std::stoi(temp);
				continue;
			}

			if (arg == "nodes") {
				temp = args[i++];

				result.nodes_limit = std::stoi(temp);
				continue;
			}

			if (arg == "mate") {
				temp = args[i++];

				result.depth_limit = std::stoi(temp);
				result.mate_search_mode = true;
				continue;
			}

			if (arg == "movetime") {
				temp = args[i++];

				result.move_time = std::stoi(temp);
				continue;
			}

			if (arg == "infinite") {
				result.infinite_mode = true;
				continue;
			}

			if (arg == "searchmoves ") {
				this->logError("Warning! searchmoves parameter is not supported yet!");
				continue;
			}

			this->logError(formatString("Not supported go parameter: %s", arg.c_str()));
		}

		callbacks.on_go(result);
	};
}

void UCI::InputListener::receive_line(std::string& line)
{

	std::vector<std::string> tokens;
	std::stringstream sstream(line);
	std::string token;
	
	while (sstream) {
		token = "";
		sstream >> token;
		if(token.size()) tokens.push_back(token);
	}

	// empty line received, ignore it

	if (tokens.empty()) return;

	std::string command = tokens[0];

	for (auto& route : router)
		if (route.command == command) {
			route.handler(tokens);
			return;
		}
	
	callbacks.on_unsupported_command(line);
}

void UCI::MemSafeOption::dispose_content() {
	switch (type_)
	{
	case UCI::Check:
		dispose_content_internal<bool>();
		break;
	case UCI::Spin:
		dispose_content_internal<spin_option>();
		break;
	case UCI::Combo:
		dispose_content_internal<combo_option>();
		break;
	case UCI::Button:
		dispose_content_internal<nullptr_t>();
		break;
	case UCI::String:
		dispose_content_internal<std::string>();
		break;
	default:
		break;
	}
}

void* UCI::MemSafeOption::deep_copy_content() const {
	switch (type_)
	{
	case UCI::Check:
		return new bool(check_content());
	case UCI::Spin:
		return new spin_option(spin_content());
	case UCI::Combo:
		return new combo_option(combo_content());
	case UCI::String:
		return new std::string(str_content());
	default:
		return nullptr;
	}
}
