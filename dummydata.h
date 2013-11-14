// Classes to load dummy data for testcases and debugging.

#ifndef DUMMAYDATA_H
#define DUMMAYDATA_H

#include "configs.h"
#include <log4cxx/logger.h>
using namespace log4cxx;

class DummyData {
public:
  DummyData (const Configs &configs) : _configs(configs), _logger(Logger::getLogger("Dummydata")) {};
  // create dummy data on main database
  void createDataOnMain ();
private:
  const Configs &_configs;
  LoggerPtr _logger;
}; 



#endif // DUMMAYDATA_H
