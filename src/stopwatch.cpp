#include <unistd.h>
#include <utility>
#include "stopwatch.hpp"

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

namespace ASIOLibs {

void TimingStat::Add(const std::string &name, uint64_t mksec) {
	total_stat[ name ] += mksec;
}

StopWatch::StopWatch(std::string &&_metric_name, TimingStat *_stat)
	: metric_name(std::forward<std::string>(_metric_name)), stat(_stat) {

#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
	clock_serv_t cclock;
	mach_timespec_t mts;
	host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
	clock_get_time(cclock, &mts);
	mach_port_deallocate(mach_task_self(), cclock);
	ts_start.tv_sec = mts.tv_sec;
	ts_start.tv_nsec = mts.tv_nsec;
#else
	clock_gettime(CLOCK_MONOTONIC, &ts_start);
#endif
}
StopWatch::StopWatch(const std::string &_metric_name, TimingStat *_stat)
	: metric_name(_metric_name), stat(_stat) {

#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
	clock_serv_t cclock;
	mach_timespec_t mts;
	host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
	clock_get_time(cclock, &mts);
	mach_port_deallocate(mach_task_self(), cclock);
	ts_start.tv_sec = mts.tv_sec;
	ts_start.tv_nsec = mts.tv_nsec;
#else
	clock_gettime(CLOCK_MONOTONIC, &ts_start);
#endif
}

StopWatch::~StopWatch() {
	if( stat )
		FlushStat();
}

uint64_t StopWatch::FlushStat() {
	uint64_t e = getElapsed();
	if( stat ) {
		stat->Add(metric_name, e);
		stat = nullptr;
	}
	return e;
}

uint64_t StopWatch::getElapsed() const {
	struct timespec ts;
#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
	clock_serv_t cclock;
	mach_timespec_t mts;
	host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
	clock_get_time(cclock, &mts);
	mach_port_deallocate(mach_task_self(), cclock);
	ts.tv_sec = mts.tv_sec;
	ts.tv_nsec = mts.tv_nsec;
#else
	clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
	uint64_t mksec = (ts.tv_sec - ts_start.tv_sec) * 1000000 +
		(ts.tv_nsec - ts_start.tv_nsec) / 1000;
	return mksec;
}

};
