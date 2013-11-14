#include <cmath>
#include <algorithm>
#include <iostream>
#include "groupImpl.h"

bool GroupImpl::testPair(Observ &o1, Observ &o2)
{
  if(fabs(o1.averageDist - o2.averageDist) > _config.deltaRadius)
    return false;
  if(fabs(o1.pixelSum - o2.pixelSum) > _config.deltaPixelSum)
    return false;

  return true;

}
  
void GroupImpl::testTimeStep(int time, Observ &o)
{
  double N1, N2, radius;
  std::list<Observ> candidates;
  std::list<Observ>::iterator c_pos;
  std::list<GroupID> groups;
  std::list<GroupID>::iterator g_pos;

  N2 = time / _config.T;
  N1 = o.time / _config.T;
  radius = _config.D2*(N1 - N2);
  
  if(_engine)
    {
      // takes care of distance / velocity predicate
      candidates = _engine->getObsInRadius(o.centroidX, o.centroidY, time, radius);


      c_pos = candidates.begin();
        while(c_pos != candidates.end())
	{
	
	  if(testPair(o, *c_pos))
	    {
	
	      groups = _engine->getGroups(c_pos->observId);
	      // if the match does not belong to a group create one
	      if(groups.empty())
		{
		  _engine->createGroup(*c_pos, o);
		}
	      else
		{
		  g_pos = groups.begin();
		  // add to all groups its match is in
		  while(g_pos != groups.end())
		    {
		      _engine->addToGroup(*g_pos, o); 
		      ++g_pos;
		    }
		}
	    }
	  ++c_pos;
	}
    }
  else
    {
      std::cout << "GroupEngine not initialized, exiting..." << std::endl;
      exit(0);
    }

}


void GroupImpl::cookGroup(std::list<Observ> &obs)
{
  std::list<Observ>::iterator o_pos = obs.begin();
  
  int time;
  while(o_pos != obs.end())
    {

      // traverse time backwards
      // do not check this timestamp because the same star cannot be in 
      // two places at once until we start supporting more uncertainty
      
      time = o_pos->time - 1;
      while(_engine->dataAvailable(time))
	{
	  testTimeStep(time, *o_pos);
	  --time;
	}
      ++o_pos;   
    }
}
