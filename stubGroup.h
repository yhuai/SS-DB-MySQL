#ifndef _STUB_ENGINE_
#define _STUB_ENGINE_

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
class StubGroup : public GroupEngine {
 public:
   
 StubGroup(boost::shared_ptr<Group> g) :  GroupEngine(g), _groupid(0), _obsid(0) {       


    // generate 3 million observations, 1000 per timestep
    _max_time = 3000;
    srand(time(0)); 
    for(int i = 0; i < _max_time; ++i)
      {
	generateObsTime(i);
      }

  };
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

  std::list<Observ> getObsInBoundingBox(int startx, int starty, int endx, int endy, int time);

  // new pairings found by batch grouper
  void reportNewMatches(std::list<std::pair<Observ, Observ> > & groups);

  // create groups of one for new unmatched observations
  void createNewGroups(std::list<Observ> &unmatched);

  void printSummary();

 private:

  std::map<int, obsList> _obs_state; // map time stamp to observations 
  int _max_time;
  std::map<Oid, groupList> _group_state;
  int _groupid;
  int _obsid;

  void generateObsTime(int time);

};

#endif //_STUB_ENGINE
