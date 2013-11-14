#include <iostream>
#include <algorithm>
#include <cstring>
#include "stubGroup.h"


void StubGroup::generateObsTime(int time)
{
  // generate 5 observations per time step
  // observations are in a 10x10 grid
  // average radius and pixelSum = 1...5 just to keep it simple
 
  obsList olist;
  for(int i = 0; i < 100; ++i)
    {
      Observ o;
      ::memset(&o, 0, sizeof(Observ));
      o.time = time;
      o.centroidX = rand() % 10;
      o.centroidY = rand() % 10;
      o.pixelSum =  rand() % 20;
      o.averageDist = rand() % 20;
      o.observId = _obsid;
      ++_obsid;
      olist.push_back(o);
      
    }
  _obs_state[time] = olist;
    
}


std::list<Observ>  StubGroup::getObsInRadius(int center_x, int center_y, int time, int dist)
{
  obsList::iterator o_pos;
    
  if(_obs_state.find(time) == _obs_state.end()) // if we have no observations for time 
    {
      generateObsTime(time);
    }

  // presume all are within radius for now because it does not matter
  return _obs_state[time];
  
}

std::list<GroupID> StubGroup::getGroups(Oid obsId)
{
  return _group_state[obsId];

}

void StubGroup::addToGroup(GroupID g, Observ obs)
{
  std::cout << "Adding " << obs.observId << " to group " << g << std::endl; 
  _group_state[obs.observId].push_back(g);
}


int StubGroup::createGroup(Observ o1, Observ o2)
{
  std::cout << "Creating group " << _groupid << " comprised of " << o1.observId << ", " << o2.observId << std::endl;
  _group_state[o1.observId].push_back(_groupid);
  _group_state[o2.observId].push_back(_groupid);
  ++_groupid;
  return _groupid - 1;
}


void StubGroup::createNewGroups(std::list<Observ> &unmatched)
{
  std::list<Observ>::iterator u_pos = unmatched.begin();
  while(u_pos != unmatched.end())
    {
      _group_state[u_pos->observId].push_back(_groupid);
      ++_groupid;
      ++u_pos;
    }
}

void StubGroup::groupImage(int imageID)
{
  // imageid = time in this context - one image / time step
  int time = imageID;

  _group_finder->cookGroup(_obs_state[time]);


}

bool StubGroup::dataAvailable(int time)
{
  if(time >= 0 && time < _max_time)
    return true;
  return false;
  
}


std::list<Observ> StubGroup::getObsInBoundingBox(int startx, int starty, int endx, int endy, int time)
{
  obsList::iterator o_pos = _obs_state[time].begin();
  obsList inBB;
  
  while(o_pos != _obs_state[time].end())
    {
      if(startx <= o_pos->centroidX && o_pos->centroidX <= endx && starty <= o_pos->centroidY && o_pos->centroidY <= endy)
	{
	  inBB.push_back(*o_pos);
	}
      ++o_pos;
    }

  return inBB;

}

void StubGroup::reportNewMatches(std::list<std::pair<Observ, Observ> > & groups)
{
  
  std::list<std::pair<Observ, Observ> >::iterator g_pos = groups.begin();
  groupList matches;
  groupList::iterator m_pos;
  while(g_pos != groups.end())
    {
      if(_group_state[g_pos->first.observId].empty())
	{
	  // create new group
	  _group_state[g_pos->first.observId].push_back(_groupid);
	  _group_state[g_pos->second.observId].push_back(_groupid);
	  ++_groupid;
	}
      else
	{
	  // add to first observs groups
	  matches = _group_state[g_pos->first.observId];
	  m_pos = matches.begin();
	  while(m_pos != matches.end())
	    {
	      if(std::find(_group_state[g_pos->second.observId].begin(), _group_state[g_pos->second.observId].end(), *m_pos) == _group_state[g_pos->second.observId].end())
		{
		  _group_state[g_pos->second.observId].push_back(*m_pos);
		}
	      ++m_pos;
	    }
	}
      ++g_pos;
    }

}


void StubGroup::printSummary()
{
  std::map<Oid, groupList>::iterator g_pos = _group_state.begin();
  // transform into group list
  std::map<GroupID, std::list<Oid> > groups;
  std::map<GroupID, std::list<Oid> >::iterator g2_pos;
  std::list<Oid>::iterator i2_pos;
  groupList::iterator i_pos;
  while(g_pos != _group_state.end())
    {
      i_pos = g_pos->second.begin();
      while(i_pos != g_pos->second.end())
	{
	  groups[*i_pos].push_back(g_pos->first);
	  ++i_pos;
	}
      ++g_pos;
    }
  std::cout << "Final summary: "<< std::endl;
  g2_pos = groups.begin();
  while(g2_pos != groups.end())
    {
      std::cout << "Group " << g2_pos->first << ": ";
      i2_pos = g2_pos->second.begin();
      while(i2_pos != g2_pos->second.end())
	{
	  std::cout << *i2_pos << ", ";
	  ++i2_pos;
	}
      std::cout << std::endl;
      ++g2_pos;
    }
  

}
