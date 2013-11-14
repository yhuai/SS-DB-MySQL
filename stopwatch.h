// StopWatch
// Class to check elapsed time.

#ifndef STOPWATCH_H
#define STOPWATCH_H

#include <sys/time.h>

class StopWatch {
public:
  StopWatch() {
    init();
  }
  void init() {
    ::gettimeofday(&starttime_, NULL);
  }
  void stop() {
    ::gettimeofday(&stoptime_, NULL);
  }
  // returns elapsed time between init() and stop() in microseconds.
  int64_t getElapsed() const {
    timeval resulttime;
    timersub(&stoptime_, &starttime_, &resulttime);
    int64_t wallClock = resulttime.tv_sec * 1000000 + resulttime.tv_usec;
    return wallClock;
  }
  timeval starttime_, stoptime_;
};

#endif// STOPWATCH_H
