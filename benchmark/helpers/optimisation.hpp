#pragma once

#include <atomic>


#if _MSC_VER
#define FORCE_INLINE __forceinline
#elif __GNUC__ || __clang__
#define FORCE_INLINE __attribute__((always_inline))
#else
#define FORCE_INLINE
#endif


namespace serialpp::benchmark {

	// The following two functions are used to defeat dead store optimisation, with minimal runtime overhead.
	// The idea comes from Google Benchmark's DoNotOptimize() and ClobberMemory().
	// See here: https://github.com/google/benchmark/blob/main/include/benchmark/benchmark.h#L440
	// I'm not convinced the techniques are 100% legit or foolproof, but they seem to work for the major compilers.

	// Causes an object to be visible as a side effect (probably).
	FORCE_INLINE static void make_side_effect(void* p) {
		static std::atomic<void*> volatile v;
		v = p;
	}

	// Forces the compiler to flush reads and writes to objects visible as side effects (probably).
	FORCE_INLINE static void memory_fence() noexcept {
		std::atomic_signal_fence(std::memory_order_seq_cst);
	}

}
