#include "uci_server.h"
#include <thread> // for spawning engine thread
#include <sstream>

void _ENGINE_PIPE::engine_pipe_listener(std::shared_ptr<IEngineProcessPipe> pipe, std::function<void(std::string)> consumer)
{

	std::string line;
	while (pipe->isAlive()) {
		std::getline(pipe->get_engine_output(), line);
		consumer(line);
	};

}


#ifdef Windows
/*
	Implementation of engine pipeline for Windows OS
*/
#include "Windows.h"
#include <fstream>
#include <cwchar>
#include <io.h> // windows specific header to write from a pipe
#include <fcntl.h>


std::wstring expand_utf8_string(const std::string& s) {
	int required_size = MultiByteToWideChar(
		CP_UTF8,
		MB_PRECOMPOSED,
		s.c_str(),
		-1, 
		NULL,
		0
	);

	if (required_size == 0)
		return L"";

	wchar_t* buffer = new wchar_t[required_size];

	int success = MultiByteToWideChar(
		CP_UTF8,
		MB_PRECOMPOSED,
		s.c_str(),
		-1,
		buffer,
		required_size
	);

	std::wstring result;

	if (success) {
		result = std::wstring(buffer);
	}

	delete[] buffer;
	return result;
}

// RAII oriented wrapper of windows PIPE
class _AnonymousWindowsPipe {
	HANDLE in_;
	HANDLE out_;

public:
	_AnonymousWindowsPipe();
	~_AnonymousWindowsPipe();

	void try_close_write();
	void try_close_read();

	HANDLE& read() { return in_; };
	HANDLE& write() { return out_; };
};


_AnonymousWindowsPipe::_AnonymousWindowsPipe() {
	SECURITY_ATTRIBUTES saAttr;

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = true;
	saAttr.lpSecurityDescriptor = nullptr;
	bool success = CreatePipe(&in_, &out_, &saAttr, 0);
	
	if (!success || in_ == INVALID_HANDLE_VALUE || out_ == INVALID_HANDLE_VALUE) {
		throw _ENGINE_PIPE::_OSException("An exception occurred during process of pipe creation");
	}

}

_AnonymousWindowsPipe::~_AnonymousWindowsPipe()
{
	try_close_read();
	try_close_write();
}

void _AnonymousWindowsPipe::try_close_write()
{
	if (out_ && out_ != INVALID_HANDLE_VALUE) CloseHandle(out_);
	out_ = 0;
}

void _AnonymousWindowsPipe::try_close_read()
{
	if (in_ && in_ != INVALID_HANDLE_VALUE) CloseHandle(in_);
	in_ = 0;
}

class WinEngineProcessPipe : public _ENGINE_PIPE::IEngineProcessPipe {
	PROCESS_INFORMATION  engine_proces_info;
	std::shared_ptr<_AnonymousWindowsPipe> eng_input;
	std::shared_ptr<_AnonymousWindowsPipe> eng_output;

	std::ofstream engine_input_stream_writer;
	std::ifstream engine_output_stream_reader;


	FILE* pipe_read = nullptr;
	FILE* pipe_write = nullptr;
	int read_fd;
	int write_fd;

	void make_ostream_to_pipe(_AnonymousWindowsPipe& pipe);
	void make_istream_to_pipe(_AnonymousWindowsPipe& pipe);
public:

	WinEngineProcessPipe(const PROCESS_INFORMATION& p_info, std::shared_ptr<_AnonymousWindowsPipe> in, std::shared_ptr<_AnonymousWindowsPipe> out) :
		engine_proces_info(p_info), eng_input(in), eng_output(out)
	{
		assert(isAlive());
		make_ostream_to_pipe(*in);
		make_istream_to_pipe(*out);
	};

	~WinEngineProcessPipe() {
		if (pipe_read)	fclose(pipe_read);
		if (pipe_write) fclose(pipe_write);
	}

	// Odziedziczono za poœrednictwem elementu IEngineProcessPipe
	bool isAlive() override;
	void kill()	   override;
	std::istream& get_engine_output() override { return engine_output_stream_reader; };
	std::ostream& get_engine_input() override { return engine_input_stream_writer; };
};

class WinEngineLoader : public _ENGINE_PIPE::IEngineLoader {
public:

	// Odziedziczono za poœrednictwem elementu IEngineLoader
	std::shared_ptr<_ENGINE_PIPE::IEngineProcessPipe> load_engine(std::string path, std::string working_dir, std::function<void(const std::string&)> error_callback) override;
};

std::shared_ptr<_ENGINE_PIPE::IEngineProcessPipe> WinEngineLoader::load_engine(std::string path_utf8, std::string working_dir_utf8, std::function<void(const std::string&)> error_callback)
{
	std::wstring path = expand_utf8_string(path_utf8);
	std::wstring working_dir = expand_utf8_string(working_dir_utf8);

	// Windows requires modifiable c style buffer 
	wchar_t path_to_engine[1024];

	if (path.size() >= 1024) {
		error_callback("Provided path is too long, path must be at most 1023 long");
		return nullptr;
	}
	

	std::shared_ptr<_AnonymousWindowsPipe> engine_in;
	std::shared_ptr<_AnonymousWindowsPipe> engine_out;


	try {
		engine_in = std::make_shared<_AnonymousWindowsPipe>();
		engine_out = std::make_shared<_AnonymousWindowsPipe>();
	}
	catch ( _ENGINE_PIPE::_OSException e) {
		error_callback("Creation of a pipe failed");
		return nullptr;
	}


	wcscpy_s(path_to_engine, 1024, path.c_str());


	// Prepare startup info
	STARTUPINFOW startup_info;
	ZeroMemory(&startup_info, sizeof(STARTUPINFOW));

	startup_info.cb = sizeof(STARTUPINFOW);
	startup_info.dwFlags = STARTF_USESTDHANDLES;
	startup_info.hStdInput = engine_in->read(); 
	startup_info.hStdOutput = engine_out->write();

	PROCESS_INFORMATION proces_info;

	ZeroMemory(&proces_info, sizeof(proces_info));

	SECURITY_ATTRIBUTES saAttr;

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = true;
	saAttr.lpSecurityDescriptor = nullptr;
	
	bool success = CreateProcessW(
	path_to_engine, 
	nullptr,	  // command line arguments - null since all important stuff is set by UCI
	&saAttr,	  // proc. security attributes
	nullptr,	  // thread. security attributes
	true,		  // handle inheritance. True, since engine must inherit open handles to pipe
	0,	  // creation flags - boot engine in windowless mode
	nullptr,	  // env variables used by engine - in our case: none,
	working_dir.c_str(),
	&startup_info,
	&proces_info
	);


	if (!success) {
		error_callback("Failed to start the engine process, make sure the path is correct");
		return nullptr;
	}

	DWORD status;
	if (!GetExitCodeProcess(proces_info.hProcess, &status) || status != STILL_ACTIVE) {
		error_callback("Engine process started but crashed immediately");
		return nullptr;
	}

	// close handles that GUI no longer needs
	//engine_in->try_close_read();
	//engine_out->try_close_write();
	
	std::shared_ptr<WinEngineProcessPipe> result = nullptr;
	try {
		result = std::make_shared<WinEngineProcessPipe>(proces_info, engine_in, engine_out);
		assert(result->isAlive());
	}
	catch ( _ENGINE_PIPE::_OSException e) {
		error_callback(std::string() + "Failed to to create EnginePipe object: " + e.what());
	}
	
	return result;
}

void WinEngineProcessPipe::make_ostream_to_pipe(_AnonymousWindowsPipe& pipe)
{
	HANDLE pipe_write_handle = pipe.write();

	pipe_write = nullptr;

	// Convert HANDLE to file descriptor
	int fd = _open_osfhandle(reinterpret_cast<intptr_t>(pipe_write_handle), _O_WRONLY | _O_TEXT);
	if (fd == -1) {
		throw _ENGINE_PIPE::_OSException("Failed to get pipe file descriptor");
	}

	write_fd = fd;
	pipe_write = _fdopen(fd, "a");

	if(!pipe_write) {
		_close(fd);
		throw _ENGINE_PIPE::_OSException("Failed to get pipe file descriptor");
	}

	engine_input_stream_writer = std::ofstream(pipe_write);
}

void WinEngineProcessPipe::make_istream_to_pipe(_AnonymousWindowsPipe& pipe)
{
	HANDLE pipe_read_handle = pipe.read();

	pipe_read = nullptr;

	// Convert HANDLE to file descriptor
	int fd = _open_osfhandle(reinterpret_cast<intptr_t>(pipe_read_handle), _O_RDONLY | _O_TEXT);
	if (fd == -1) {
		throw _ENGINE_PIPE::_OSException("Failed to get pipe file descriptor");
	}

	read_fd = fd;
	pipe_read = _fdopen(fd, "r");

	if (!pipe_read) {
		_close(fd);
		throw _ENGINE_PIPE::_OSException("Failed to get pipe file descriptor");
	}

	engine_output_stream_reader = std::ifstream(pipe_read);
}

bool WinEngineProcessPipe::isAlive()
{
	HANDLE handle = engine_proces_info.hProcess;
		
	DWORD status;
	
	if (!GetExitCodeProcess(handle, &status)) {
		throw _ENGINE_PIPE::_OSException("Failed to get process status");
	}

	return status == STILL_ACTIVE;
}

void WinEngineProcessPipe::kill()
{
	if(engine_proces_info.hProcess && engine_proces_info.hProcess != INVALID_HANDLE_VALUE)
	(void*)TerminateProcess(engine_proces_info.hProcess, -1);
}




#endif // Windows

void OpenEnginePipesService::remove_closed_pipes()
{
	std::vector<int> closed_pipes_ids;

	for (auto pair : engines_pipes)
		if (!pair.second->isAlive())
			closed_pipes_ids.push_back(pair.first);

	for (int id : closed_pipes_ids)
		engines_pipes.erase(id);
}

int OpenEnginePipesService::open(std::string path, std::string working_dir)
{
	std::shared_ptr<_ENGINE_PIPE::IEngineProcessPipe> pipe = engine_loader->load_engine(path, working_dir, logger);

	if (pipe == nullptr) {
		return -1;
	}

	int result = assign_id();

	engines_pipes[result] = pipe;

	return result;
}

std::vector<std::pair<int, std::shared_ptr<_ENGINE_PIPE::IEngineProcessPipe>>> OpenEnginePipesService::get_all()
{
	std::vector<std::pair<int, std::shared_ptr<_ENGINE_PIPE::IEngineProcessPipe>>> result;

	for (auto& p : engines_pipes) result.push_back(p);

	return result;
}

std::shared_ptr<_ENGINE_PIPE::IEngineProcessPipe> OpenEnginePipesService::get_pipe_by_id(int id)
{
	auto it = engines_pipes.find(id);

	if (it == engines_pipes.end())
		return nullptr;

	return it->second;
}

std::function<void(const std::vector<std::string>&)> UCI_SERVER::ServersideCommandHandler::wrap_id_callback(std::function<void(const std::string&, const std::string)> on_author)
{
	return [this, on_author](const std::vector<std::string>& args) {
		if (args.size() < 3) {
			this->logError("Engine send id command with too few arguments");
			return;
		}

		std::string payload = args[2];

		for (size_t i = 3; i < args.size(); ++i) {
			payload += " " + args[i];
		}

		on_author(args[1], payload);
	};
}

std::function<void(const std::vector<std::string>&)> UCI_SERVER::ServersideCommandHandler::wrap_bestmove_callback(std::function<void(const std::string&, const std::string&)> on_best_move)
{
	return [this, on_best_move](const std::vector<std::string>& args) {
		if (args.size() < 2) {
			this->logError("Engine send incomplete best move command");
			return;
		}

		if (args.size() < 4)
			on_best_move(args[1], "0000");
		else
			on_best_move(args[1], args[3]);

	};
}

std::function<void(const std::vector<std::string>&)> UCI_SERVER::ServersideCommandHandler::wrap_info_callback(std::function<void(const std::string, const std::vector<std::string>&)> on_info)
{
	return [this, on_info](const std::vector<std::string>& args) {
		const std::string sequence_options[2] = { "refutation" , "currline" };

		size_t i = 1;

		std::string option_id;
		std::vector<std::string> option_params;
		
		while (i < args.size()) {

			option_params.clear();
			option_id = args[i++];

			if (i == args.size()) break;

			if (option_id == sequence_options[0] || option_id == sequence_options[1]) {
				while (i < args.size()) {
					option_params.push_back(args[i++]);
				}

			}
			else {
				option_params.push_back(args[i++]);
			}

			on_info(option_id, args);
		}
	};
}

std::function<void(const std::vector<std::string>&)> UCI_SERVER::ServersideCommandHandler::wrap_option_callback(std::function<void(UCI::MemSafeOption)> on_option)
{
	return [this, on_option](const std::vector<std::string>& args) {
		
		const std::unordered_map<std::string, UCI::OptionType> options = {
			{"check", UCI::Check},
			{"spin", UCI::Spin},
			{"combo", UCI::Combo},
			{"button", UCI::Button},
			{"string", UCI::String}
		};

		if (args.size() < 5) {
			this->logError("Engine send incomplete option declaration");
			return;
		}

		std::string option_name = args[2];
		std::string option_type_str = args[4];

		UCI::OptionType option_type;

		auto it = options.find(option_type_str);

		if (it == options.end()) {
			this->logError("Engine send unrecognized option type");
			return;
		}

		option_type = it->second;

		bool check_value = false;
		std::string str_value = "<empty>";
		UCI::spin_option spin_value = { 0, 0, 0 };
		UCI::combo_option combo_value = { "<empty>", {} };
		size_t i = 0;

		switch (option_type)
		{
		case UCI::Check:
			if (args.size() >= 7 && args[5] == "default") {
				check_value = args[6] == "true";
			}
			on_option(UCI::MemSafeOption(option_name, check_value));
			break;
		case UCI::Spin:

			while (i < args.size()) {
				str_value = args[i++];

				if (str_value == "default" && i < args.size()) {
					str_value = args[i++];
					spin_value.default_ = atoi(str_value.c_str());
				}

				if (str_value == "min" && i  < args.size()) {
					str_value = args[i++];
					spin_value.min = atoi(str_value.c_str());
				}

				if (str_value == "max" && i  < args.size()) {
					str_value = args[i++];
					spin_value.max = atoi(str_value.c_str());
				}
			}
			on_option(UCI::MemSafeOption(option_name, spin_value));
			break;
		case UCI::Combo:
			while (i < args.size()) {
				str_value = args[i++];

				if (str_value == "default" && i  < args.size()) {
					combo_value.default_ = args[i++];
				}

				if (str_value == "var" && i < args.size()) {
					combo_value.supported_values.push_back(args[i++]);
				}
			}
			on_option(UCI::MemSafeOption(option_name, combo_value));
			break;
		case UCI::Button:
			on_option(option_name);
			break;
		case UCI::String:
			if (args.size() >= 7 && args[5] == "default") str_value = args[6];
			on_option(UCI::MemSafeOption(option_name, str_value));
			break;
		default:
			break;
		}

	};
}

std::function<void(const std::vector<std::string>&)> UCI_SERVER::ServersideCommandHandler::wrap_command_status_callback(std::function<void(const std::string&)> on_status)
{
	return [this, on_status](const std::vector<std::string>& args) {
		if (args.size() >= 2)
			on_status(args[1]);
		else
			this->logError("Engine set ill formatted command status for copyprotection or registration");
	};
}

void UCI_SERVER::ServersideCommandHandler::receive_line(std::string line)
{
	std::vector<std::string> tokens;
	std::stringstream sstream(line);
	std::string token;

	while (sstream) {
		token = "";
		sstream >> token;
		if (token.size()) tokens.push_back(token);
	}

	// empty line received, ignore it

	if (tokens.empty()) return;

	std::string command = tokens[0];

	for (auto& route : router)
		if (route.command == command) {
			route.handler(tokens);
			return;
		}
}

std::unique_ptr<_ENGINE_PIPE::IEngineLoader> getEngineLoader()
{
	// TODO: implement Engine Loader functionality for UNIX and return it here
	#ifdef Windows
	return std::make_unique<WinEngineLoader>();
	#endif
}
