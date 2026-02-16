#ifndef HFTDERIBIT_ENV_H
#define HFTDERIBIT_ENV_H

#include <cstdlib>
#include <string>
#include <stdexcept>

namespace deribit {

	inline std::string get_env(const char* var) {
		const char* val = std::getenv(var);
		if (!val) {
			throw std::runtime_error(std::string("Missing environment variable: ") + var);
		}
		return std::string(val);
	}

}

#endif
