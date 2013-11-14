#ifndef _MYSQL_STUB_ENGINE_
#define _MYSQL_STUB_ENGINE_

#include <boost/shared_ptr.hpp>
#include <list>
#include <map>
#include <ctime>
#include <log4cxx/logger.h>

#include "groupEngine.h"
#include "group.h"
#include "cook.h"
#include "configs.h"
#include "mysqlwrapper.h"

using namespace log4cxx;

typedef std::list<Observ> obsList;
typedef std::list<GroupID> groupList;

// simple stub to test grouper
class MysqlStubGroup : public GroupEngine {
 public:
   
 MysqlStubGroup(const Configs &configs, boost::shared_ptr<Group> g) :  GroupEngine(g), _configs(configs), _logger(Logger::getLogger("Group")), _connections(_configs), _groupid(0) {       srand(time(0)); };
  // returns a list of observation structs that are within the radius provided (dist) and were recorded the time specified
  std::list<Observ>  getObsInRadius(int center_x, int center_y, int time, int dist);

  // get a list of groups associated with an observation id
  std::list<GroupID> getGroups(Oid obsId);

  // add an observation to a pre-existing group
  void addToGroup(GroupID g, Observ obs);

  // create a new group comprised of the two observations provided
  int createGroup(Observ o1, Observ o2);

  // give it an image id, queries db and gets list of obs to pass on to groupImpl
  void groupImage(int imageID);
  
  bool dataAvailable(int time);

  void addBatchToGroup(std::list<std::pair<GroupID, Observ> > & additions);

  std::list<int> createGroupsBatched(std::list<std::pair<Observ, Observ> > & groups);


  std::list<Observ> getObsInBoundingBox(int startx, int starty, int endx, int endy, int time);
  void reportNewMatches(std::list<std::pair<Observ, Observ> > & groups);
 private:
  const Configs &_configs;
  LoggerPtr _logger;
  MysqlConnections _connections;
  std::map<int, obsList> _obs_state; // map time stamp to observations 

  std::map<Oid, groupList> _group_state;
  int _groupid;



};

#endif //_STUB_ENGINE
