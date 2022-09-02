#include "utility.hpp"

#include <chrono>
#include <format>

#if _WIN32
#define NOMINMAX
#include <Windows.h>
#elif __linux__
#include <sched.h>
#include <sys/resource.h>
#include <sys/time.h>
#endif


namespace serialpp::benchmark {

	std::string get_timestamp() {
		auto const now = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
		auto str = std::format("{:%Y%m%dT%H%M%S}", now);
		auto const decimal_index = str.rfind('.');
		if (decimal_index != str.npos) {
			str.erase(decimal_index);
		}
		return str;
	}


	bool pin_process() {
		bool result = true;
#if _WIN32
		auto const process = GetCurrentProcess();
		result = result && SetPriorityClass(process, HIGH_PRIORITY_CLASS);
		result = result && SetProcessAffinityMask(process, 1);
#elif __linux__
		cpu_set_t mask;
		CPU_ZERO(&mask);
		CPU_SET(0, &mask);
		result = result && setpriority(PRIO_PROCESS, 0, -10);
		result = result && sched_setaffinity(0, sizeof mask, &mask) == 0;
#else
		result = false;
#endif
		return result;
	}

}
