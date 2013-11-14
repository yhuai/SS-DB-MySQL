#include "group.h"
#include "configs.h"
#include "stubGroup.h" // mysqlGroup goes here


int main(int argc, char** argv) 
  {

    boost::shared_ptr<Group> grouper(Group::newBatchInstance());
    boost::shared_ptr<StubGroup> stubEngine(new StubGroup(grouper));
    boost::shared_ptr<GroupEngine> genEngine = stubEngine;
    grouper->setGroupEngine(genEngine);

    for(int i = 0; i < 30; ++i)
      {
	stubEngine->groupImage(i);
      }

    stubEngine->printSummary();
  }
