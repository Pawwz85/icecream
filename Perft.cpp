#include "Perft.h"
#include <chrono>

void __perft_internal(uint64_t& cnt, game_state& gs, int8_t depth)
{
	if (depth == 0) {
		cnt += 1;
		return;
	}

	Move buffer[256];
	Move* buff_end = buffer;

	move_gen::move_generator_legacy_interface(gs, buff_end);

	game_state copy = gs;

	for (Move* m = buffer; m != buff_end; ++m) {
		GameStateUtils::make_move(gs, *m);
		__perft_internal(cnt, gs, depth - 1);
		gs = copy;
	}
	return;
}

perft_results perft(std::string fen, int8_t depth)
{
	game_state gs;
	GameStateUtils::clear(gs);
	GameStateUtils::parse_fen(gs, fen);

	uint64_t counter = 0;
	uint64_t millis = 0;

	std::chrono::high_resolution_clock clock;

	auto start = clock.now();
	__perft_internal(counter, gs, depth);
	auto end = clock.now();

	auto dur = end.time_since_epoch() - start.time_since_epoch();

	double seconds = dur.count() / 1000000000.f;
	perft_results res = { counter, seconds, counter / seconds };

	return res;
}

