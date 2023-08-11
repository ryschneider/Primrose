#include "log.hpp"

#include <chrono>
#include <stdio.h>
#include <fmt/chrono.h>

namespace Primrose {
	LOG_LEVEL printLevel = LOG_LEVEL::VERBOSE;
}

namespace {
	using namespace Primrose;
	
	static std::string label(LOG_LEVEL level) {
		switch (level) {
			case LOG_LEVEL::VERBOSE: return "VERBOSE";
			case LOG_LEVEL::LOG: return "LOG";
			case LOG_LEVEL::WARNING: return "WARNING";
			case LOG_LEVEL::ERROR: return "ERROR";
		}
	}

	static void send(std::string msg, LOG_LEVEL level) {
		auto clock = std::chrono::system_clock::now();
		std::string saveMsg = fmt::format("{} {:%Y-%m-%d %H:%M:%S}: {}\n", label(level), clock, msg);
		std::string printMsg = fmt::format("{}: {}\n", label(level), msg);

		// write to log file

		if (level == LOG_LEVEL::ERROR) {
			throw std::runtime_error(printMsg);
		} else if (level >= printLevel) {
			printf("%s", printMsg.c_str());
		}
	}
}

void Primrose::verbose(std::string msg) { send(msg, LOG_LEVEL::VERBOSE); }
void Primrose::log(std::string msg) { send(msg, LOG_LEVEL::LOG); }
void Primrose::warning(std::string msg) { send(msg, LOG_LEVEL::WARNING); }
void Primrose::error(std::string msg) { send(msg, LOG_LEVEL::ERROR); }
