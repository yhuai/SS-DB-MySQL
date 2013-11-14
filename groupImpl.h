
#ifndef GROUP_IMPL_H
#define GROUP_IMPL_H

#include <list>
#include <vector> 
#include <map>

#include "cook.h"
#include "group.h"

class Group;

class Config {
 public:
  int timeWindow;
  double T; // scale factor from demo spec
  double D2;
  double deltaRadius; /// max radius difference (absolute)                                                                                                                                                 
  double deltaPixelSum;   /// absolute difference                                                                                                                                                          
  static Config defaultConfig() {
    Config c;
    c.T = 30/5; // <# of images> / 5, now: 3000/5 - tiny, next: 400 / 5 - small setting
    c.D2 = 1/c.T; // "easy" setting in benchmark spec
    c.deltaRadius = 2;
    c.deltaPixelSum = 1;
    return c;
    } 
};


class GroupImpl : public Group
{
 public:
 GroupImpl() : _config(Config::defaultConfig()) {}; // default config
 GroupImpl(Config c) : _config(c)  {};
    // takes in a list of observations for each frame (in sequence), iterates over them and returns groups
    // groups consist of a list of observation ids belonging to a group
  void cookGroup(std::list<Observ> &obs);
  
 private:
  Config _config;

  // find candidate matches for an observation at a given time, test each of them 
  void testTimeStep(int time_offset, Observ &a);
  bool testPair(Observ& a, Observ& b);

};
#endif // GROUP_IMPL_H
