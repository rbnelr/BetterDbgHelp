#pragma once
#include <stdint.h>
#include <stdio.h>
#include <limits>
#include <algorithm>

namespace kiss {
	uint64_t get_timestamp ();
	extern uint64_t timestamp_freq;

	void sleep_msec (uint32_t msecs);

	struct Timer {
		uint64_t begin;

		static Timer start () {
			return { get_timestamp() };
		}
		float elapsed_sec () {
			uint64_t now = get_timestamp();
			return (float)(now - begin) / (float)timestamp_freq;
		}
	};

	struct TimerPrintZone {
		const char* name;
		int divisor;

		Timer t;
		TimerPrintZone (const char* name, int divisor=0): name{name}, t{Timer::start()}, divisor{divisor} {}
		~TimerPrintZone () {
			auto ms = t.elapsed_sec() * 1000.0f;
			if (divisor == 0) {
				printf("|Timer %s: total %7.3f ms\n", name, ms);
			}
			else {
				printf("|Timer %s: avg %7.3f ms (%dx)\n", name, ms, divisor);
			}
		}
	};
	
	struct TimerMeasurement {
		const char* name;

		float total_sec = 0;
		int count = 0;

		float min_sec = std::numeric_limits<float>::infinity();
		float max_sec = -std::numeric_limits<float>::infinity();

		void push (float sec) {
			total_sec += sec;
			min_sec = std::min(min_sec, sec);
			max_sec = std::max(max_sec, sec);
			count++;
		}

		TimerMeasurement (const char* name): name{name} {}

		void print () {
			auto ms = count > 0 ? (total_sec / (float)count) * 1000000.0f : 0.0f;
			printf("|Timer %-30s: avg %7.3f us (%dx) min=%7.3f max=%7.3f\n", name, ms, count, min_sec*1000000.0f, max_sec*1000000.0f);
		}
	};
	struct TimerMeasureZone {
		TimerMeasurement* meas;
		Timer t;
		TimerMeasureZone (TimerMeasurement* meas): meas{meas}, t{Timer::start()} {}
		~TimerMeasureZone () {
			meas->push(t.elapsed_sec());
		}
	};
}


#define STRINGIFY(name) #name
#define TimerZone(name) auto __##name = kiss::TimerPrintZone(STRINGIFY(name))
#define TimerZoneCount(name, count) auto __##name = kiss::TimerPrintZone(STRINGIFY(name), count)
#define TimerZoneInc(name) __##name.divisor++

#define TimerMeasZone(zone) auto __TimerMeasZone_##__COUNTER__ = kiss::TimerMeasureZone(&zone)
