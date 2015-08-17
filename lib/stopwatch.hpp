#pragma once
#ifndef _ASIO_LIBS_STOPWATCH_HPP_
#define _ASIO_LIBS_STOPWATCH_HPP_
#include <ctime>
#include <string>
#include <map>

namespace ASIOLibs {

// all metrics is in microseconds

struct TimingStat {
	void Add(const std::string &name, uint64_t mksec);
	const std::map< std::string, uint64_t > &Stat() const { return total_stat; }

private:
	std::map< std::string, uint64_t > total_stat;
};

struct StopWatch {
	StopWatch(std::string &&metric_name, TimingStat *_stat = nullptr);
	StopWatch(const std::string &metric_name, TimingStat *_stat = nullptr);
	~StopWatch();
	uint64_t getElapsed() const;
	uint64_t FlushStat();
	const std::string &getMetricName() const { return metric_name; }

private:
	struct timespec ts_start;
	std::string metric_name;
	TimingStat *stat;
};

};

#endif
