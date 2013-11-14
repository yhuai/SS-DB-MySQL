#ifndef _GROUP_ENGINE_
#define _GROUP_ENGINE_

#include <boost/shared_ptr.hpp>
#include <list>

#include "cook.h"
#include "group.h"


// temp for dbug
class Group;
// abstract class that mysql or scidb will inherit from to interface with the grouper
class GroupEngine {
 public:
 GroupEngine(boost::shared_ptr<Group> g) : _group_finder(g) {};
  // returns a list of observation structs that are within the radius provided (dist) and were recorded the time specified
  virtual std::list<Observ>  getObsInRadius(int centerx, int centery, int time, int dist) = 0;

  // get a list of groups associated with an observation id
  virtual std::list<GroupID> getGroups(Oid obsId) = 0;

  // get all observations in a bounding box at a given time
  virtual std::list<Observ> getObsInBoundingBox(int startx, int starty, int endx, int endy, int time) = 0;

  // add an observation to a pre-existing group
  virtual void addToGroup(GroupID g, Observ obs) = 0;

  // create a new group comprised of the two observations provided, returns new group id
  virtual int  createGroup(Observ o1, Observ o2) = 0;

  // report new pairs found regardless of past group affiliations
  virtual void reportNewMatches(std::list<std::pair<Observ, Observ> > & groups) = 0;
  
  // report unmatched observations; put these each in a new group 
  virtual void createNewGroups(std::list<Observ> &unmatched) = 0;

  // give it an image id, queries db and gets list of obs to pass on to groupImpl
  virtual void groupImage(int imageID) = 0;

  // test to see if there is data available at a timestep; will terminate grouping traversal when this returns 0
  virtual bool dataAvailable(int time) = 0;
 protected:
    boost::shared_ptr<Group> _group_finder;
};

#endif //_GROUP_ENGINE
