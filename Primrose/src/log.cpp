#include "log.hpp"

#include <chrono>
#include <stdio.h>

namespace Primrose {
	LOG_LEVEL printLevel = LOG_LEVEL::LOG;
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
//		static const auto clock = ;
		static auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::string fullMsg = fmt::format("{} {}: {}\n", label(level), std::ctime(&time), msg);

		// write to log file

		if (level == LOG_LEVEL::ERROR) {
			throw std::runtime_error("ERROR: " + msg);
		} else if (level >= printLevel) {
			printf("%s", fullMsg.c_str());
		}
	}
}

void Primrose::verbose(std::string msg) { send(msg, LOG_LEVEL::VERBOSE); }
void Primrose::log(std::string msg) { send(msg, LOG_LEVEL::LOG); }
void Primrose::warning(std::string msg) { send(msg, LOG_LEVEL::WARNING); }
void Primrose::error(std::string msg) { send(msg, LOG_LEVEL::ERROR); }
