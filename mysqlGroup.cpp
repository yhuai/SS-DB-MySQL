#include "mysqlGroup.h"

std::list<Observ>  MySQLGroup::getObsInRadius(int x, int y, int time, int dist)
{
  std::list<Observ> observations;
  std::stringstream sql;


  // todo: find a way to explicitly invoke the index - primary means is MBR* which is a bounding rectangle, not what we want
  sql << "SELECT obsid, centerx, centery, pixelsum, averageDistance, boxxstart, boxystart, boxxend, boxyend  from observations, images where sqrt((centerx - x)*(centerx - x) + (centery - y)*(centery - y)) <= " << dist << " and time = " << time << ";";
  MYSQL_RES *result = execSelectSQL(_connections.getMainMysql(), sql.str(), _logger);
  for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
    Observ o;
    o.observId = ::atol(row[0]);
    o.centroidX = ::atol(row[1]);
    o.centroidX = ::atol(row[2]);
    o.averageDist = ::atof(row[4]);
    o.pixelSum = ::atol(row[3]);
    o.boxxstart = ::atol(row[4]);
    o.boxxend = ::atol(row[5]);
    o.boxystart = ::atol(row[6]);
    o.boxyend = ::atol(row[7]);
    observations.push_back(o);
  }

  return observations;
}  


std::list<GroupID> MySQLGroup::getGroups(Oid obsId)
{
  std::list<GroupID> groups;
  std::stringstream sql;

  sql << "SELECT obsgroupid from obsgroup_obs where obsid = " << obsId << ";";
  MYSQL_RES *result = execSelectSQL(_connections.getMainMysql(), sql.str(), _logger);
  for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
    groups.push_back(::atol(row[0]));
  }

  return groups;
}

void MySQLGroup::addToGroup(GroupID g, Observ obs)
{
 std::stringstream sql;
 sql << "INSERT INTO OBSGROUP_OBS VALUES(" << g << ", " << obs.observId << ");";
 execUpdateSQL(_connections.getMainMysql(), sql.str(), _logger);
 updateBoundingBox(obs, g);
}


void MySQLGroup::createGroup(Observ o1, Observ o2)
{
  std::stringstream sql, sql_add;
  //sql_add << "INSERT INTO OBSGROUP SET ...";
  //sql << "INSERT INTO OBSGROUP_OBS VALUES(" << g << ", " << obs << ");";
  // create group with new uid and insert 2 new members
 execUpdateSQL(_connections.getMainMysql(), sql.str(), _logger);
 createBoundingBox(o1, o2, 1); // 1 should be new groupid
}


void MySQLGroup::groupImage(int imageID)
{

  std::stringstream sql;
  std::list<Observ> observations;
  sql << "SELECT obsid, centerx, centery, pixelsum, averageDistance, boxxstart, boxystart, boxxend, boxyend  FROM OBSERVATIONS where IMAGEID=" << imageID << ";";

  MYSQL_RES *result = execSelectSQL(_connections.getMainMysql(), sql.str(), _logger);
  for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
    Observ o;
    o.observId = ::atol(row[0]);
    o.centroidX = ::atol(row[1]);
    o.centroidX = ::atol(row[2]);
    o.averageDist = ::atof(row[4]);
    o.pixelSum = ::atol(row[3]);
    o.boxxstart = ::atol(row[4]);
    o.boxxend = ::atol(row[5]);
    o.boxystart = ::atol(row[6]);
    o.boxyend = ::atol(row[7]);
    observations.push_back(o);
  }
  // call grouping algorith
  _group_finder->cookGroup(observations);

}

void MySQLGroup::updateBoundingBox(Observ o, GroupID gID)
{
  // get bounding box for star and see if it is outside the group bounding box - if it is expand group bb
  std::stringstream sql, within;
  long int gx_start, gy_start, gx_end, gy_end;


  sql << "SELECT boxxstart, boxystart, boxxend, boxyend from obsgroup where obsgroupid = " << gID << ";";
  MYSQL_RES *result = execSelectSQL(_connections.getMainMysql(), sql.str(), _logger);
  MYSQL_ROW row = ::mysql_fetch_row (result);
  gx_start = ::atol(row[0]);
  gy_start = ::atol(row[1]);
  gx_end = ::atol(row[2]);
  gy_end = ::atol(row[3]);

  // SET @poly = 'Polygon((0 0,0 3,3 3,3 0,0 0))';

  within << "SET @group = 'Polygon((" << gx_start << " " << gy_start << ", " << gx_start << " " << gy_end << ", " << gx_end << " " << gy_end << ", " << gx_end << " " << gy_start << ", " << gx_start << " " << gy_start << "));"
            << "SET @obs = 'Polygon((" << o.boxxstart << " " << o.boxystart << ", " << o.boxxstart << " " << o.boxyend << ", " << o.boxxend << " " << o.boxyend << ", " << o.boxxend << " " << o.boxystart << o.boxxstart << " " << o.boxystart<< "));"
	 << "SELECT MBRWithin(@obs, @group)"; // expand bounding box if this returns true

  // see if it is possible to make a box that is the union of two sub-boxes
 
 
}

void MySQLGroup::createBoundingBox(Observ o1, Observ o2, GroupID gID)
{
  // create bounding box for group based on two observations given
}
