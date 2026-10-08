#include <search.h>
#include <uci_client.h>
#include <EngineApplication.h>
#include <iostream>

EngineInstance<SearchAlgorithm>* instance = nullptr;

int main(int argc, char** argv) {
	initDirections();
	move_gen::__init_magics();
	std::string line;

	UCI::UCIOutputStream out(&std::cout);

	instance = new EngineInstance<SearchAlgorithm>(&out);

	UCI::InputListener listener(uci_callbacks, &out);

	while (instance->is_running) {
		std::getline(std::cin, line);
		listener.receive_line(line);
	}

	return 0;
}
