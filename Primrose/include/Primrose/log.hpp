#ifndef PRIMROSE_LOG_HPP
#define PRIMROSE_LOG_HPP

#include <string>
#include <fmt/format.h>

namespace Primrose {
	enum class LOG_LEVEL {
		VERBOSE = 1,
		LOG = 2,
		WARNING = 3,
		ERROR = 4
	};

	extern LOG_LEVEL printLevel;

	void verbose(std::string msg);
	void log(std::string msg);
	void warning(std::string msg);
	void error(std::string msg);
}

#endif
