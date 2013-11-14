
#include "dummydata.h"
#include "mysqlwrapper.h"

void DummyData::createDataOnMain () {
  LOG4CXX_INFO(_logger, "creating dummy data...");
  MysqlConnections connections (_configs);
  // TODO

  LOG4CXX_INFO(_logger, "created dummy data.");
}
