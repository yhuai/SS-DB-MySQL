#include "groupImpl.h"
#include "groupBatchImpl.h"
#include "group.h"

class GroupImpl;
class GroupBatchImpl;
  
boost::shared_ptr<Group> Group::newInstance() {
  boost::shared_ptr<Group> g(new GroupImpl());
    return g;
  };


boost::shared_ptr<Group> Group::newBatchInstance() {
  boost::shared_ptr<Group> g(new GroupBatchImpl());
    return g;
  };
void Group::setGroupEngine(boost::shared_ptr<GroupEngine> g) {
  _engine = g;
  };

