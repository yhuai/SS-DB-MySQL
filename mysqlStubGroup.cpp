#include <iostream>
#include "mysqlStubGroup.h"


std::list<Observ>  MysqlStubGroup::getObsInRadius(int center_x, int center_y, int time, int dist)
{
  /*  obsList::iterator o_pos;

  if(_obs_state.find(time) == _obs_state.end()) // if we have no observations for time 
    {
      generateObsTime(time);
    }

  // presume all are within radius for now because it does not matter
  return _obs_state[time];
  */
  std::cout << "Not yet implemented in this context.\n";
  exit(0);
}

std::list<GroupID> MysqlStubGroup::getGroups(Oid obsId)
{
  return _group_state[obsId];

}

void MysqlStubGroup::addToGroup(GroupID g, Observ obs)
{
  std::cout << "Adding " << obs.observId << " to group " << g << std::endl; 
  _group_state[obs.observId].push_back(g);
}


int MysqlStubGroup::createGroup(Observ o1, Observ o2)
{
  std::cout << "Creating group " << _groupid << " comprised of " << o1.observId << ", " << o2.observId << std::endl;
  _group_state[o1.observId].push_back(_groupid);
  _group_state[o2.observId].push_back(_groupid);
  ++_groupid;
  return _groupid - 1;
}


void MysqlStubGroup::groupImage(int imageID)
{
  // imageid = time in this context

  
  std::stringstream sql;
  std::list<Observ> observations;
  sql << "SELECT obsid, centerx, centery, pixelsum, averageDist  FROM observations where TIME=" << imageID << ";"; 

  MYSQL_RES *result = execSelectSQL(_connections.getMainMysql(), sql.str(), _logger);
  for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
    Observ o;
    o.observId = ::atol(row[0]);
    o.centroidX = ::atol(row[1]);
    o.centroidY = ::atol(row[2]);
    o.averageDist = ::atof(row[4]);
    o.pixelSum = ::atol(row[3]);
    o.time = imageID;
    observations.push_back(o);
  }
  std::cout << "Got " << observations.size() << " observations for timestep " << imageID << std::endl;
  // call grouping algorith
  _group_finder->cookGroup(observations);


}

bool MysqlStubGroup::dataAvailable(int time)
{
  // TODO: extend this later?

  if(time >= 0)
    return true;
  return false;
  
}


void MysqlStubGroup::addBatchToGroup(std::list<std::pair<GroupID, Observ> > & additions)
{
  std::cout <<"Batching not yet implemented in stub." << std::endl;
  exit(0);
}

std::list<int> MysqlStubGroup::createGroupsBatched(std::list<std::pair<Observ, Observ> > & groups)
{
  std::cout <<"Batching not yet implemented in stub." << std::endl;
  exit(0);
  
}


std::list<Observ> MysqlStubGroup::getObsInBoundingBox(int startx, int starty, int endx, int endy, int time)
{
  std::stringstream sql;
  std::list<Observ> observations;
  sql << "SELECT obsid, centerx, centery, pixelsum, averageDist  FROM observations where TIME=" << time << " AND CENTERX >= " << startx << " AND CENTERX <= " << endx << " AND CENTERY >= " << starty  
      << " AND CENTERY <= " << endy << ";";
  MYSQL_RES *result = execSelectSQL(_connections.getMainMysql(), sql.str(), _logger);
  for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
    Observ o;
    o.observId = ::atol(row[0]);
    o.centroidX = ::atol(row[1]);
    o.centroidY = ::atol(row[2]);
    o.averageDist = ::atof(row[4]);
    o.pixelSum = ::atol(row[3]);
    observations.push_back(o);
  }
  ///std::cout << sql.str() << std::endl;
  //std::cout << "Got " << observations.size() << " observations in bounding box (" << startx << ", " << starty << ") (" << endx << ", " << endy << ")." << std::endl;
  return observations;

}

void MysqlStubGroup::reportNewMatches(std::list<std::pair<Observ, Observ> > & groups)
{
  
  std::list<std::pair<Observ, Observ> >::iterator g_pos = groups.begin();
  groupList matches;
  groupList::iterator m_pos;
  while(g_pos != groups.end())
    {
      if(_group_state[g_pos->first.observId].empty())
	{
	  // create new group
	  std::cout << "Creating new group " << _groupid << " comprised of " << g_pos->first.observId << " and " << g_pos->second.observId << std::endl;
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
	      std::cout << "Adding " << g_pos->second.observId << " to group " << *m_pos << std::endl;
	      _group_state[g_pos->second.observId].push_back(*m_pos);
	      ++m_pos;
	    }
	}
      ++g_pos;
    }

}
