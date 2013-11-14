#ifndef LOADER_H
#define LOADER_H

#include "configs.h"
#include "mysqlwrapper.h"
#include <log4cxx/logger.h>
using namespace log4cxx;

// Loader
// class to load data into mysql
class Loader {
public:
  Loader (const Configs &configs) : _configs(configs), _logger(Logger::getLogger("Loader")), _connections(_configs) {};
  void load(int threshold);
  void loadOneImage(int threshold, int imageId);
  // parallel version of observation cooking
  void loadParallelFinish(int nodes); // executed once on master node to load observation tables
  void loadParallel(int threshold, int startingObservationId); // executed on each node

  void loadGroup(float D2, int T);
  // no parallel version for group cooking. it's simply done on master node (and no need to parallelize; it's fast)
private:
  const Configs &_configs;
  LoggerPtr _logger;
  MysqlConnections _connections;
};

#endif // LOADER_H
