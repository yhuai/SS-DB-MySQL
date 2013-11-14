#ifndef _MYSQL_GROUP_
#define  _MYSQL_GROUP_

#include <boost/shared_ptr.hpp>
#include <list>
#include <log4cxx/logger.h>

#include "groupEngine.h"
#include "group.h"
#include "configs.h"
#include "mysqlwrapper.h"

using namespace log4cxx;

class MySQLGroup : public GroupEngine
{
  // create instance of mysql grouper
 MySQLGroup(const Configs &configs, boost::shared_ptr<Group> g) : GroupEngine(g), _configs(configs), _logger(Logger::getLogger("Group")), _connections(_configs) {};

    // returns a list of observation structs that are within the radius provided (dist) and were recorded the time specified
  std::list<Observ>  getObsInRadius(int center_x, int center_y, int time, int dist);

  // get a list of groups associated with an observation id
  std::list<GroupID> getGroups(Oid obsId);

  // add an observation to a pre-existing group
  void addToGroup(GroupID g, Observ obs);

  // create a new group comprised of the two observations provided
  void createGroup(Observ o1, Observ o2);

  // give it an image id, queries db and gets list of obs to pass on to groupImpl
  void groupImage(int imageID);
 private:
  const Configs &_configs;
  LoggerPtr _logger;
  MysqlConnections _connections;


  void updateBoundingBox(Observ o, GroupID gID);
  void createBoundingBox(Observ o1, Observ o2, GroupID gID);
};

#endif
