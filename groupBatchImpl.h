
#ifndef GROUP_BATCH_IMPL_H
#define GROUP_BATCH_IMPL_H

#include <list>
#include <vector> 
#include <map>

#include "cook.h"
#include "group.h"
#include "groupImpl.h"

class Group;

class GroupBatchImpl : public Group
{
 public:
 GroupBatchImpl() : _config(Config::defaultConfig()), _boxxstart(0), _boxystart(0), _boxxend(0), _boxyend(0) {}; // default config
 GroupBatchImpl(Config c) : _config(c)  {};
    // takes in a list of observations for each frame (in sequence), iterates over them and returns groups
    // groups consist of a list of observation ids belonging to a group
  void cookGroup(std::list<Observ> &obs);
  
 private:
  Config _config;
  int _boxxstart, _boxystart, _boxxend, _boxyend;
  // observations to be added to pre-existing groups
  //std::list<std::pair<GroupID, Observ> > _additions;
  std::list<std::pair<Observ, Observ> > _new_pairs;

  // find candidate matches for an observation at a given time, test each of them 
  void testTimeStep(int time, std::list<Observ> &obs, std::vector<int> & matches);
  bool testPair(Observ& a, Observ& b, int radius);
  int calculateDistance(Observ &o1, Observ &o2);
  void setupBoundingBox(std::list<Observ> &obs);
  std::string printObs(Observ &o);
};
#endif // GROUP_IMPL_H
