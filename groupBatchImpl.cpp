#include <cmath>
#include <algorithm>
#include <iostream>
#include <sstream>
#include "groupBatchImpl.h"


int GroupBatchImpl::calculateDistance(Observ &o1, Observ &o2)
{
  int dX = o1.centroidX - o2.centroidX;
  int dY = o1.centroidY - o2.centroidY;
  int distance = sqrt(dX*dX + dY*dY);
  
  return distance;
  
}


std::string GroupBatchImpl::printObs(Observ &o)
{
  std::ostringstream s;
  s << "Observation: " << o.observId << " (" << o.centroidX << ", " << o.centroidY << ") sum: " << o.pixelSum << " averageDist " << o.averageDist;
  return s.str();
}

bool GroupBatchImpl::testPair(Observ &o1, Observ &o2, int radius)
{
  //std::cout << "Tried " << printObs(o1) << " to " << printObs(o2) << " radius: " << radius << std::endl;

  if(calculateDistance(o1, o2) > radius)
    return false;
  if(fabs(o1.averageDist - o2.averageDist) > _config.deltaRadius)
    return false;
  if(fabs(o1.pixelSum - o2.pixelSum) > _config.deltaPixelSum)
    return false;
  
  

  return true;

}


void GroupBatchImpl::testTimeStep(int time, std::list<Observ> &obs, std::vector<int> & matched)
{
  double N1, N2, radius;
  int xstart, xend, ystart, yend;
  std::list<Observ> candidates;
  std::list<Observ>::iterator c_pos, o_pos;
  int obs_idx;

  N2 = time / _config.T;
  N1 = obs.front().time / _config.T;
  radius = _config.D2*(N1 - N2);
  
  xstart = _boxxstart - radius;
  xend = _boxxend + radius;
  ystart = _boxystart - radius;
  yend = _boxyend + radius;


  //std::cout << "Querying bounding box (" << xstart << ", " << ystart << ") (" << xend << ", " << yend << ")." << " radius: " << radius << std::endl;

  candidates = _engine->getObsInBoundingBox(xstart, ystart, xend, yend, time);
      
      c_pos = candidates.begin();
      o_pos = obs.begin();
      obs_idx = 0;
      while(o_pos != obs.end())
	{
	  c_pos = candidates.begin();
	  while(c_pos != candidates.end())
	    {
	      if(testPair(*o_pos, *c_pos, radius))
		{
		  // this order is important - pre-existing one first
		  _new_pairs.push_back(std::pair<Observ, Observ>(*c_pos, *o_pos));
		  matched[obs_idx] = 1;
		}
	      ++c_pos;
	    }
	  ++obs_idx;
	  ++o_pos;
	}
}
  
void GroupBatchImpl::setupBoundingBox(std::list<Observ> &obs)
{
  _boxxstart = _boxxend = obs.front().centroidX;
  _boxystart = _boxyend = obs.front().centroidY;

  std::list<Observ>::iterator o_pos = obs.begin();
  ++o_pos;
  while(o_pos != obs.end())
    {
      if(o_pos->centroidX < _boxxstart)
	{
	  _boxxstart = o_pos->centroidX;
	}
      if(o_pos->centroidX > _boxxend)
	{
	  _boxxend = o_pos->centroidX;
	}


      if(o_pos->centroidY < _boxystart)
	{
	  _boxystart = o_pos->centroidY;
	}
      if(o_pos->centroidY > _boxyend)
	{
	  _boxyend = o_pos->centroidY;
	}
      ++o_pos;
    }
  //std::cout << "Got bounding box (" << _boxxstart << ", " << _boxystart << ") (" << _boxxend << ", " << _boxyend << ")" << std::endl;
}


void GroupBatchImpl::cookGroup(std::list<Observ> &obs)
{
  std::list<Observ>::iterator o_pos = obs.begin();
  _new_pairs.clear();  
  setupBoundingBox(obs);
  std::cout << "Starting at timestep " << obs.front().time << std::endl;
  int time = obs.front().time - 1; // traverse one step back
  std::vector<int> matched; // record whether each observation was placed in a group

  int i, observation_count = obs.size();
  std::list<Observ> unmatched;
  for(i = 0; i < observation_count; ++i)
    {
      matched.push_back(0);
    }
      
  

      // traverse time backwards
      // do not check this timestamp because the same star cannot be in 
      // two places at once until we start supporting more uncertainty
  if(_engine)  {
   while(_engine->dataAvailable(time))
	{
	  //std::cout << "Testing timestep " << time << std::endl;
	  testTimeStep(time, obs, matched);
	  --time;
	}
  }else
    {
      std::cout << "GroupEngine not initialized, exiting..." << std::endl;
      exit(0);
    }

  // iterate over all observations to find ones that are group-less so we can give them new groups of one obs
  o_pos = obs.begin();
  i = 0;

  while(o_pos != obs.end())
    {
      if(matched[i] == 0)
	{
	  unmatched.push_back(*o_pos);
	}
      ++o_pos;
      ++i;
    }
  //std::cout << unmatched.size() << " unmatched observations out of " << obs.size() << " inputted." << std::endl;
  _engine->reportNewMatches(_new_pairs);
  _engine->createNewGroups(unmatched);
}
