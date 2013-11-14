#ifndef _GROUP_H
#define _GROUP_H

#include <boost/shared_ptr.hpp>
#include <list>

#include "cook.h"
#include "groupEngine.h"


class GroupEngine;

class Group {

 public:
  virtual ~Group() {}; // this class will be extended
  // instantiate a new instance of the default constructor of group
  // this can be extended later to support other constructors and
  //grouping algos depending on arguments
  static boost::shared_ptr<Group> newInstance();

  // create a batched grouper - may take longer search time, but minimizes the calls to db
  static boost::shared_ptr<Group> newBatchInstance();
  
  // sets pointer to group engine which will pull data from 
  // data management system
  void setGroupEngine(boost::shared_ptr<GroupEngine> g);
  void releaseGroupEngine() { _engine.reset(); }

  // to be completed by groupImpl
  // find which group(s) each observation belongs to
  virtual void cookGroup(std::list<Observ> &obs) = 0;


 protected:
  // hold pointer to data management engine
  // never fail to call releaseGroupEngine() after use to avoid circular reference
  boost::shared_ptr<GroupEngine> _engine;

};

#endif //_GROUP_
