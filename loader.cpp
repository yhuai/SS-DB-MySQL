#include "configs.h"
#include "loader.h"

#include "stopwatch.h"
#include "mysqlwrapper.h"
#include "cook.h"
#include "cookgroup.h"
#include <string.h>
#include <sstream>
#include <set>
#include <map>
#include <cstdio>
#include <cstdlib>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/make_shared.hpp>
/////////////////////////////////////////////////////////////////////
//                For OBSERVATION COOKING
/////////////////////////////////////////////////////////////////////
class RdbmsPixelProvider : public PixelProvider {
public:
  RdbmsPixelProvider (MYSQL *mysql, LoggerPtr logger, const Image &image)
    : _mysql(mysql), _logger(logger), _image(image) {
    _internalX = 0;
    _internalY = 0;
    _externalY = 0;
    _width = _image.xend - _image.xstart;
    _height = _image.yend - _image.ystart;
    // if these aren't valid, you should exec this SQL because the format of bench.pos was changed.
    // UPDATE images SET xend=xstart+<imagesize>,yend=ystart+<imagesize>,time=imageid,cycle=floor(imageid/20),tablename=CONCAT('bench_',LPAD(CAST(imageid AS CHAR),4,'0'))
    assert (_width > 0);
    assert (_height > 0);
    _pixBuffer = new int32_t[_width * TILE_SIZE];
    ::memset(_pixBuffer, 0, sizeof(int32_t) * _width * TILE_SIZE);
    _cutoffCnt = 0;
  }
  ~RdbmsPixelProvider() {
    delete[] _pixBuffer;
  }
  int getImageWidth() { return _width; }
  int getImageHeight() { return _height; }
  void getCurrentPixel(PixVal &out) {
    out.coord.first = _internalX;
    out.coord.second = _externalY;
    out.val = _pixBuffer[(_width * _internalY) + _internalX];
  }
  bool moveToFirstPixel() {
    _internalX = 0;
    _internalY = 0;
    _externalY = 0;
    return fetchNext(0);
  }
  bool moveToNextPixel() {
    ++_internalX;
    if (_internalX >= _width) {
      _internalX = 0;
      ++_internalY;
      ++_externalY;
      if (_internalY >= TILE_SIZE) {
        _internalY = 0;
        return fetchNext(_externalY);
      } else {
        return _externalY < _height;
      }
    }
    return _internalX < _width && _externalY < _height;
  }
  void onInitialize() {};
  void onNewObservation(Observ &obs) {
    if (obs.polygons.size() <= 50) {
      obs.imageId = _image.imageid;
      _observations.push_back(obs);
    } else {
      _cutoffCnt = 0;
    }
  }
  void onFinalize() {
    LOG4CXX_INFO(_logger, "stored observations:" << _observations.size() << ", ignored (too long) obs: " << _cutoffCnt);
  }

  std::list<Observ>& getObservations() { return _observations; };
private:
  MYSQL *_mysql;
  LoggerPtr _logger;
  const Image &_image;
  int _width, _height;
  int _internalX, _internalY; // zero-based. exactly corresponds to pixBuffer's index
  int _externalY; // zero-based. "X" has no internal/external
  int32_t *_pixBuffer; // buffer to provide pixels strictly in x->y order
  std::list<Observ> _observations;
  int _cutoffCnt;
  bool fetchNext (int startingY) {
    ::memset(_pixBuffer, 0, sizeof(int32_t) * _width * TILE_SIZE);
    int yTile = startingY / TILE_SIZE;
    int32_t startTile = yTile * TILE_X_Y_RATIO;
    int32_t endTile = (yTile + 1) * TILE_X_Y_RATIO - 1;
    std::stringstream sql;
    sql << "SELECT x,y,pix FROM " << _image.tablename
      << " WHERE tile BETWEEN " << startTile << " AND " << endTile;
    MYSQL_RES *result = execSelectSQL(_mysql, sql.str(), _logger);
    bool hasResult = false;
    for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
      int32_t rawX = ::atol(row[0]);
      int32_t rawY = ::atol(row[1]);
      // now the generated files contain zero-based x/y values.
      // int32_t x = rawX - _image.xstart;
      int32_t x = rawX;
      assert (x >= 0);
      assert (x < _width);
      // int32_t y = rawY - _image.ystart - startingY;
      int32_t y = rawY - startingY;
      assert (y >= 0);
      assert (y < TILE_SIZE);
      int32_t pix = ::atol(row[2]);
      _pixBuffer[(_width * y) + x] = pix;
      hasResult = true;
    }
    ::mysql_free_result (result);
    return hasResult;
  }
};

void Loader::load(int threshold) {
  LOG4CXX_INFO(_logger, "loading data... threshold=" << threshold);

  // we assume raw data are already loaded by insertToMySQL perl script.
  ImageTable imageTable;
  std::vector<Image> images = imageTable.getAllImages(_connections.getMainMysql());

  ObservationTable obsTable;
  obsTable.dropIfExists(_connections.getMainMysql());
  obsTable.createTable(_connections.getMainMysql());

  int nextOid = 1;
  StopWatch watchCook;
  watchCook.init();
  int64_t insertTime = 0;
  int64_t totalObsCnt = 0;
  // first, cook each image to extract polygons and observations
  for (size_t i = 0; i < images.size(); ++i) {
    const Image &image = images[i];
    LOG4CXX_INFO(_logger, "cooking " << image.tablename << "...");
    RdbmsPixelProvider provider (_connections.getMainMysql(), _logger, image);
    Cook cook(provider, threshold);
    cook.setNextOid(nextOid);
    cook.cookRawImage();
    nextOid = cook.getNextOid();
    int64_t obsCnt = (int64_t) provider.getObservations().size();
    totalObsCnt += obsCnt;
    LOG4CXX_INFO(_logger, "cooked " << image.tablename << ". " << obsCnt << " observations (" << totalObsCnt << " in total)");

    StopWatch watchInsert;
    watchInsert.init();
    obsTable.insertBulk(_connections.getMainMysql(), provider.getObservations(), image);
    watchInsert.stop();
    insertTime += watchInsert.getElapsed();
  }
  watchCook.stop();
  LOG4CXX_INFO(_logger, "cooked and stored all observations/polygons. "
    << watchCook.getElapsed() << " microsec (" << insertTime << " microsec for insertion)..");

  obsTable.createIndex(_connections.getMainMysql());
  LOG4CXX_INFO(_logger, "loaded data.");
}

void Loader::loadOneImage(int threshold, int imageId) {
  LOG4CXX_INFO(_logger, "cooking one image (" << imageId << ")... threshold=" << threshold);
  ImageTable imageTable;
  Image image = imageTable.getImageById(_connections.getMainMysql(), imageId);
  StopWatch watchCook;
  watchCook.init();
  RdbmsPixelProvider provider (_connections.getMainMysql(), _logger, image);
  Cook cook(provider, threshold);
  cook.setNextOid(1);
  cook.cookRawImage();

  StopWatch watchInsert;
  watchInsert.init();
  ObservationTable obsTable ("tmp_"); // add prefix to create a temporary table
  obsTable.dropIfExists(_connections.getMainMysql());
  obsTable.createTable(_connections.getMainMysql());
  obsTable.insertBulk(_connections.getMainMysql(), provider.getObservations(), image);
  obsTable.createIndex(_connections.getMainMysql());
  watchInsert.stop();

  watchCook.stop();
  LOG4CXX_INFO(_logger, "cooked " << image.tablename << ". " << provider.getObservations().size() << " observations. " << watchCook.getElapsed() << " microsec (" << watchInsert.getElapsed() << " microsec for insertion).");
}

void Loader::loadParallelFinish(int nodes) {
  LOG4CXX_INFO(_logger, "loadParallelFinish:start. nodes=" << nodes);
  StopWatch watchInsert;
  watchInsert.init();
  ObservationTable obsTable;
  obsTable.dropIfExists(_connections.getMainMysql());
  obsTable.createTable(_connections.getMainMysql());
  LOG4CXX_INFO(_logger, "loading observations...");
  for (int i = 0; i < nodes; ++i) {
    std::stringstream loadsql;
    loadsql << "LOAD DATA INFILE '"
      << "/data/federated_mysql/csvs/tempcsvs" << i << "/observations_parallel.csv"
      << "' IGNORE INTO TABLE observations FIELDS TERMINATED BY ';' LINES TERMINATED BY '\\n'"
      << "(obsid,imageid,time,cycle,averageDist,pixelSum,centerx,centery,"
      << "boxxstart,boxystart,boxxend,boxyend,"
      << "@var1, @var2)"
      << "SET center=GeomFromText(@var1),bbox=GeomFromText(@var2)";
    execUpdateSQL (_connections.getMainMysql(), loadsql.str(), _logger);
  }
  LOG4CXX_INFO(_logger, "loading obs_polygons...");
  for (int i = 0; i < nodes; ++i) {
    std::stringstream loadsql;
    loadsql << "LOAD DATA INFILE '"
      << "/data/federated_mysql/csvs/tempcsvs" << i << "/obs_polygons_parallel.csv"
      << "' IGNORE INTO TABLE obs_polygons FIELDS TERMINATED BY ',' LINES TERMINATED BY '\\n'";
    execUpdateSQL (_connections.getMainMysql(), loadsql.str(), _logger);
  }
  obsTable.createIndex(_connections.getMainMysql());
  watchInsert.stop();
  LOG4CXX_INFO(_logger, "loadParallelFinish:end. time=" << watchInsert.getElapsed() << " microsec");
}
void Loader::loadParallel(int threshold, int startingObservationId) {
  LOG4CXX_INFO(_logger, "loading observaiotns in parallel ... threshold=" << threshold
    << ", startingObservationId=" << startingObservationId);
  
  ObservationTable obsTable; // actually will not insert to table in this function.
  // MainMysql : this node
  
  // just write CSVs to disk which will be loaded remotely from master node
  CsvBuffer buffer (_logger, "observations", 4 << 20, "/data/tempcsvs/observations_parallel.csv");
  CsvBuffer bufferPolygon (_logger, "obs_polygons", 4 << 20, "/data/tempcsvs/obs_polygons_parallel.csv");

  // image table should exist in every node but only contains the images in the node
  ImageTable imageTable;
  std::vector<Image> images = imageTable.getAllImages(_connections.getMainMysql());
  StopWatch watchCook;
  watchCook.init();
  int nextOid = startingObservationId;
  int64_t insertTime = 0;
  int64_t totalObsCnt = 0;
  for (size_t i = 0; i < images.size(); ++i) {
    const Image &image = images[i];
    LOG4CXX_INFO(_logger, "cooking " << image.tablename << "...");
    RdbmsPixelProvider provider (_connections.getMainMysql(), _logger, image);
    Cook cook(provider, threshold);
    cook.setNextOid(nextOid);
    cook.cookRawImage();
    nextOid = cook.getNextOid();
    int64_t obsCnt = (int64_t) provider.getObservations().size();
    totalObsCnt += obsCnt;
    LOG4CXX_INFO(_logger, "cooked " << image.tablename << ". " << obsCnt << " observations (" << totalObsCnt << " in total)");

    StopWatch watchInsert;
    watchInsert.init();
    obsTable.writeToCsv (buffer, provider.getObservations(), image);
    obsTable.writeToCsvPolygon (bufferPolygon, provider.getObservations(), image);
    watchInsert.stop();
    insertTime += watchInsert.getElapsed();
  }
  buffer.close();
  bufferPolygon.close();
  watchCook.stop();
  LOG4CXX_INFO(_logger, "cooked and stored all observations/polygons. "
    << watchCook.getElapsed() << " microsec (" << insertTime << " microsec for writing CSV)..");

  LOG4CXX_INFO(_logger, "loaded observaiotns");
}

/////////////////////////////////////////////////////////////////////
//                For OBSERVATION GROUP COOKING
/////////////////////////////////////////////////////////////////////
struct ObsPolygonForGroup {
  int obsid;
  int x, y;
};
#define REGION_SIZE 1000
class RdbmsCookGroupCallbacks : public CookGroupCallbacks {
public:
  RdbmsCookGroupCallbacks (MYSQL *mysql, LoggerPtr logger, const std::vector<Image> &images)
    : _mysql(mysql), _logger(logger), _images(images), _nextGroupId(0) {
    _allObsCount = execSelectIntSQL(_mysql, "SELECT COUNT(*) FROM observations", 0, _logger);
    _allObsAutoPtr = boost::shared_array<ObsPos> (new ObsPos[_allObsCount]);
    _allObs = _allObsAutoPtr.get();
    ::memset (_allObs, 0, sizeof (ObsPos) * _allObsCount);

    for (size_t i = 0; i < images.size(); ++i) {
      _obsSpatial.push_back (std::map<int, std::vector<int> > ());
    }
    LOG4CXX_INFO(_logger, "retrieving " << _allObsCount << " observations, " << _allObsPolygonsCount << " polygons..");
    MYSQL_RES *result = execSelectSQL(_mysql, "SELECT obsid,time,centerx,centery FROM observations ORDER BY obsid", _logger, true /*to save memory*/);
    int count = 0;
    int prevTime = -1;
    std::set <int> finishedTimes;
    for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
      int obsid = ::atol(row[0]);
      int time = ::atol(row[1]);
      int centerx = ::atol(row[2]);
      int centery = ::atol(row[3]);
      _allObs [count].obsid = obsid;
      _allObs [count].time = time;
      _allObs [count].centerx = centerx;
      _allObs [count].centery = centery;
      if (prevTime != time) {
        assert (finishedTimes.find (time) == finishedTimes.end()); // should be time clustered
        _timeIndexes [time] = count;
        LOG4CXX_INFO(_logger, "_timeIndexes[" << time << "]=" << count);
        prevTime = time;
        finishedTimes.insert (time);
      }
      int region = toRegion(centerx, centery);
      std::map<int, std::vector<int> > &spatialMap = _obsSpatial[time];
      if (spatialMap.find (region) == spatialMap.end()) {
        spatialMap[region] = std::vector<int> ();
      }
      spatialMap[region].push_back (count);
      ++ count;
    }
    ::mysql_free_result (result);
    assert (count == _allObsCount);

    _allObsPolygonsCount = execSelectIntSQL(_mysql, "SELECT COUNT(*) FROM obs_polygons", 0, _logger);
    _allObsPolygonsAutoPtr = boost::shared_array<ObsPolygonForGroup> (new ObsPolygonForGroup[_allObsPolygonsCount]);
    _allObsPolygons = _allObsPolygonsAutoPtr.get();
    ::memset (_allObsPolygons, 0, sizeof (ObsPolygonForGroup) * _allObsPolygonsCount);
    MYSQL_RES *result2 = execSelectSQL(_mysql, "SELECT obsid,x,y FROM obs_polygons ORDER BY obsid,ord", _logger, true /*to save memory*/);
    int count2 = 0;
    for (MYSQL_ROW row = ::mysql_fetch_row (result2); row != NULL; row = ::mysql_fetch_row (result2)) {
      _allObsPolygons [count2].obsid = ::atol(row[0]);
      _allObsPolygons [count2].x = ::atol(row[1]);
      _allObsPolygons [count2].y = ::atol(row[2]);
      ++ count2;
    }
    ::mysql_free_result (result2);
    assert (count2 == _allObsPolygonsCount);
    LOG4CXX_INFO(_logger, "retrieved " << _allObsCount << " observations.");

    getAllGroups();
  }

  void getCandidateMatches(std::vector<ObsPos> &observations,
    float D2, int T, int originTime,
    float startx, float starty, float endx, float endy) {
    if (originTime <= 0) return;
    if (T > originTime) T = originTime;
    int totalCnt = 0, positiveCnt = 0;
    StopWatch watch;
    watch.init();
/*
    uses on memory map as below instead of issuing SQL each time
    sql << "SELECT obsid FROM observations"
      << " WHERE time BETWEEN " << (originTime - T) << " AND " << (originTime - 1)
      << " AND Contains(GeomFromText('Polygon(("
      << (startx - D2 * T) << " " << (starty - D2 * T) << ","
      << (endx + D2 * T) << " " << (starty - D2 * T) << ","
      << (endx + D2 * T) << " " << (endy + D2 * T) << ","
      << (startx - D2 * T) << " " << (endy + D2 * T) << ","
      << (startx - D2 * T) << " " << (starty - D2 * T)
      << "))'), center)";
    }
*/
    for (int backtime = 1; backtime <= T; ++backtime) {
      int time = originTime - backtime;
      for (int yRegion = (starty- D2 * backtime) / REGION_SIZE; yRegion <= (endy + D2 * backtime) / REGION_SIZE; ++yRegion) {
        for (int xRegion = (startx - D2 * backtime) / REGION_SIZE; xRegion <= (endx + D2 * backtime) / REGION_SIZE; ++xRegion) {
          int region = toRegion(xRegion * REGION_SIZE, yRegion * REGION_SIZE);
          const std::map<int, std::vector<int> > &spatialMap = _obsSpatial[time];
          std::map<int, std::vector<int> >::const_iterator it = spatialMap.find (region);
          if (it != spatialMap.end()) {
            for (size_t k = 0; k < it->second.size(); ++k) {
              ++totalCnt;
              int ind = it->second[k];
              const ObsPos &pos = _allObs[ind];
              assert (pos.time == time);
              assert (region == toRegion(pos.centerx, pos.centery));
              float xSqDist = std::min((pos.centerx - startx) * (pos.centerx - startx), (pos.centerx - endx) * (pos.centerx - endx));
              if (startx <= pos.centerx && pos.centerx <= endx) xSqDist = 0;
              float ySqDist = std::min((pos.centery - starty) * (pos.centery - starty), (pos.centery - endy) * (pos.centery - endy));
              if (starty <= pos.centery && pos.centery <= endy) ySqDist = 0;
              const float EPSILON = 0.0001f;
              if (xSqDist + ySqDist <= D2 * (originTime - pos.time) * D2 * (originTime - pos.time) + EPSILON) {
                ++positiveCnt;
                observations.push_back (pos);
              }
            }
          }
        }
      }
    }
    watch.stop();
    LOG4CXX_INFO(_logger, "totalCnt=" << totalCnt << ", positiveCnt=" << positiveCnt << ", elapsed =" << watch.getElapsed() << " microsec");
  }

  void getObservationsInImage(std::vector<ObsPos> &observations, int imageId) {
    StopWatch watch;
    watch.init();
    std::map<int, int>::const_iterator it = _timeIndexes.find(imageId);
    assert (it != _timeIndexes.end());
    assert (it->second >= 0);
    assert (it->second < _allObsCount);
    for (int index = it->second; index < _allObsCount && _allObs[index].time == imageId; ++index) {
      observations.push_back (_allObs[index]);
    }
    watch.stop();
  }
  ImagePos getImagePos (int imageId) {
    assert (imageId >= 0);
    assert (imageId < (int) _images.size());
    const Image &image = _images[imageId];
    ImagePos ret;
    ret.id = image.imageid;
    assert (ret.id == imageId);
    ret.time = image.time;
    ret.startx = image.xstart;
    ret.endx = image.xend;
    ret.starty = image.ystart;
    ret.endy = image.yend;
    return ret;
  }
  void onNewMatches (const std::vector<ObsMatch> &matches) {
    LOG4CXX_INFO(_logger, "newly found " << matches.size() << " pairs");
    int count = matches.size();
    boost::shared_array<int> newObsids(new int[count]);
    int *newObsidsRaw = newObsids.get();
    ::memset (newObsidsRaw, 0, sizeof (int) * count);
    boost::shared_array<int> existingObsids(new int[count]);
    int *existingObsidsRaw = existingObsids.get();
    ::memset (existingObsidsRaw, 0, sizeof (int) * count);
    _pairsInThisCycleCount.push_back (count);
    _pairsInThisCycleNewObsId.push_back (newObsids);
    _pairsInThisCycleExistingObsId.push_back (existingObsids);
    for (int i = 0; i < count; ++i) {
      const ObsMatch &match = matches[i];
      newObsidsRaw[i] = match.newObsid;
      existingObsidsRaw[i] = match.existingObsid;
    }
  }

  int getNextGroupId () const { return _nextGroupId; }
  void setNextGroupId (int nextGroupId) { _nextGroupId = nextGroupId; }
  struct GroupInfo {
    GroupInfo () {}
    GroupInfo (int groupIdArg, const std::vector <int>& obsIdsArg) : groupId(groupIdArg), obsIds(obsIdsArg) {}
    int groupId;
    std::vector <int> obsIds;
  };
  void processCurrentCycle () {
    // construct what to write back to mysql.
    LOG4CXX_INFO(_logger, "accumulating groups...");
    for (int timeOffset = 0; timeOffset < (int) _pairsInThisCycleCount.size() ; ++timeOffset) {
      int pairCount = _pairsInThisCycleCount [timeOffset];
      int *pairsNewObsIds = _pairsInThisCycleNewObsId [timeOffset].get();
      int *pairsExistingObsIds = _pairsInThisCycleExistingObsId [timeOffset].get();

      // first, construct pairs for each new observations
      std::map <int, std::vector<int> > newPairs; // map<newobsid, existing>
      for (int i = 0; i < pairCount; ++i) {
        int newObsid = pairsNewObsIds[i];
        int existingObsid = pairsExistingObsIds[i];
        if (newPairs.find (newObsid) == newPairs.end()) {
          newPairs[newObsid] = std::vector<int> ();
        }
        newPairs[newObsid].push_back (existingObsid);
      }

      // process pairs for each *new* obs
      for (std::map <int, std::vector<int> >::const_iterator it = newPairs.begin(); it != newPairs.end(); ++it) {
        int newObsid = it->first;
        const std::vector<int> existingObsids = it->second;
        // take union of existing groups
        std::set<int> groupIds;
        for (size_t i = 0; i < existingObsids.size(); ++i) {
          int existingObsid = existingObsids[i];
          if (groupsTo.find (existingObsid) != groupsTo.end()) {
            // merge into existing groups. existingObsid is not affected
            const std::vector<int> &existingGroupIds = groupsTo [existingObsid];
            groupIds.insert (existingGroupIds.begin(), existingGroupIds.end());
          } else {
            // form a new group. both existingObsid and newObsid are affected
            int groupId = _nextGroupId++;
            groupIds.insert (groupId);
            std::vector<int> thePair;
            thePair.push_back (existingObsid);
            groupsFrom [groupId] = thePair;
            std::vector<int> groupIds;
            groupIds.push_back (groupId);
            groupsTo [existingObsid] = groupIds;
          }
        }
        // and then append to each group (otherwise, we might append the new observation twice to a group)
        for (std::set<int>::const_iterator jt = groupIds.begin(); jt != groupIds.end(); ++jt) {
          int groupId = *jt;
          groupsFrom[groupId].push_back (newObsid);
        }
        groupsTo [newObsid] = std::vector<int> (groupIds.begin(), groupIds.end());
      }
    }
    _pairsInThisCycleNewObsId.clear();
    _pairsInThisCycleExistingObsId.clear();
    _pairsInThisCycleCount.clear();
    LOG4CXX_INFO(_logger, "accumulated groups. " << groupsFrom.size() << " groups.");
  }
  void loadToMysql () {
    CsvBuffer csvGr (_logger, "obsgroup", 1 << 22); // custom geomtext csv
    CsvBuffer csvGrob (_logger, "obsgroup_obs", 1 << 20); // usual csv
    CsvBuffer csvGrtr (_logger, "polygon_trajectory", 1 << 22); // custom geomtext csv

    for (std::map<int, std::vector<int> >::const_iterator it = groupsFrom.begin(); it != groupsFrom.end(); ++it) {
      int groupId = it->first;

      // sort by and group by time
      std::map <int, std::vector<ObsPos*> > obsidInTime; // map <time, obs>. could be multiple obsid in a time
      for (size_t j = 0; j < it->second.size(); ++j) {
        int obsid = it->second[j];
        int obsidIndex = binarySearchObsId (obsid);
        ObsPos *obs = _allObs + obsidIndex;
        if (obsidInTime.find (obs->time) == obsidInTime.end()) {
          obsidInTime[obs->time] = std::vector<ObsPos*>();
        }
        obsidInTime[obs->time].push_back (obs);
      }

      csvGr.written(::sprintf (csvGr.curbuf(), "%d;LINESTRING(", groupId));
      int prevTime = -1;
      std::vector<ObsPos*> prevObs;
      bool firstObs = true;
      for (std::map <int, std::vector<ObsPos*> >::const_iterator cur = obsidInTime.begin(); cur != obsidInTime.end(); ++cur) {
        const std::vector<ObsPos*> &curObs = cur->second;
        for (size_t j = 0; j < curObs.size(); ++j) {
          ObsPos *obs = curObs[j];
          csvGr.written(::sprintf (csvGr.curbuf(), firstObs ? "%d %d" : ",%d %d", obs->centerx, obs->centery));
          if (firstObs) firstObs = false;
          csvGrob.written(::sprintf (csvGrob.curbuf(), "%d,%d\n", groupId, obs->obsid));
        }
        if (prevTime >= 0) { // write polygon trajectory
          csvGrtr.written(::sprintf (csvGrtr.curbuf(), "%d;%d;%d;POLYGON((", groupId, prevTime, cur->first));
          int firstX = -1, firstY = -1;
          bool first = true;
          for (size_t j = 0; j < prevObs.size(); ++j) {
            int obsid = prevObs[j]->obsid;
            int polygonIndex = binarySearchObsIdPolygons (obsid);
            for (int k = polygonIndex; k < _allObsPolygonsCount && _allObsPolygons[k].obsid == obsid; ++k) {
              csvGrtr.written(::sprintf (csvGrtr.curbuf(), "%d %d,", _allObsPolygons[k].x, _allObsPolygons[k].y));
              if (first) {
                firstX = _allObsPolygons[k].x;
                firstY = _allObsPolygons[k].y;
                first = false;
              }
            }
          }
          for (size_t j = 0; j < curObs.size(); ++j) {
            int obsid = curObs[j]->obsid;
            int polygonIndex = binarySearchObsIdPolygons (obsid);
            for (int k = polygonIndex; k < _allObsPolygonsCount && _allObsPolygons[k].obsid == obsid; ++k) {
              csvGrtr.written(::sprintf (csvGrtr.curbuf(), "%d %d,", _allObsPolygons[k].x, _allObsPolygons[k].y));
            }
          }
          csvGrtr.written(::sprintf (csvGrtr.curbuf(), "%d %d))\n", firstX, firstY));
        }
        prevTime = cur->first;
        prevObs = cur->second;
      }
      csvGr.written(::sprintf (csvGr.curbuf(), ")\n"));
    }
    csvGr.close ();
    csvGrob.close ();
    csvGrtr.close ();
    LOG4CXX_INFO(_logger, "wrote to CSV.");
    // csvGr/csvGrtr uses custom csv loading
    // LOAD DATA REPLACE for in-place-update is slow. Instead of that,
    // let's just re-make the table. We anyway have all data here.
    ObservationGroupTable groupTable(_mysql);
    groupTable.dropIfExists();
    groupTable.createTable();
    csvGrob.loadToMysql(_mysql); // csvGrob uses usual loading
    execUpdateSQL (_mysql, "LOAD DATA LOCAL INFILE '" + csvGr._csvname + "' INTO TABLE "
      + csvGr._tablename + " FIELDS TERMINATED BY ';' LINES TERMINATED BY '\\n'"
      + "(obsgroupid,@var1) SET center_traj=GeomFromText(@var1)", _logger);
    execUpdateSQL (_mysql, "LOAD DATA LOCAL INFILE '" + csvGrtr._csvname + "' INTO TABLE "
      + csvGrtr._tablename + " FIELDS TERMINATED BY ';' LINES TERMINATED BY '\\n'"
      + "(obsgroupid,fromtime,totime,@var1) SET traj=GeomFromText(@var1)", _logger);
  }
private:
  MYSQL *_mysql;
  LoggerPtr _logger;
  std::map <int, int> _timeIndexes;
  const std::vector<Image> &_images;
  int _nextGroupId;

  // shared_array + raw_ptr idiom
  boost::shared_array<ObsPos> _allObsAutoPtr;
  ObsPos *_allObs;
  int _allObsCount;
  // use on memory indexing rather querying mysql each time
  std::vector<std::map<int, std::vector<int> > > _obsSpatial; // vector<time> : map <region, obsPositions(not obsid!)>

  boost::shared_array<ObsPolygonForGroup> _allObsPolygonsAutoPtr;
  ObsPolygonForGroup *_allObsPolygons;
  int _allObsPolygonsCount;

  std::vector<boost::shared_array<int> > _pairsInThisCycleNewObsId;
  std::vector<boost::shared_array<int> > _pairsInThisCycleExistingObsId;
  std::vector<int> _pairsInThisCycleCount;

  std::map<int, std::vector<int> > groupsFrom; //map <groupid, vector<obsid> >
  std::map<int, std::vector<int> > groupsTo; //map <obsid, vector<groupid> >

  // returns the index in _allObs for given obsid
  int binarySearchObsId (int obsid) const {
    int low = 0;
    int high = _allObsCount - 1;
    while (low <= high) {
      int mid = (low + high) / 2;
      int midVal = _allObs[mid].obsid;
      if (midVal < obsid) low = mid + 1;
      else if (midVal > obsid) high = mid - 1;
      else return mid; // key found
    }
    assert (false);
    return -1;  // key not found.
  }

  // returns the *first* index in _allObsPolygons for given obsid
  int binarySearchObsIdPolygons (int obsid) const {
    int low = 0;
    int high = _allObsPolygonsCount - 1;
    int index = -1;
    while (low <= high) {
      int mid = (low + high) / 2;
      int midVal = _allObsPolygons[mid].obsid;
      if (midVal < obsid) low = mid + 1;
      else if (midVal > obsid) high = mid - 1;
      else {
        index = mid;
        break; // key found
      }
    }
    assert (index >= 0);
    // this might not be the first one, so backtrack
    int bindex;
    for (bindex = index; bindex >= 0 && _allObsPolygons[bindex].obsid == obsid; --bindex);
    assert (_allObsPolygons[bindex + 1].obsid == obsid);
    assert (bindex == -1 || _allObsPolygons[bindex].obsid != obsid);
    return bindex + 1;
  }
  void getAllGroups () {
    MYSQL_RES *result = execSelectSQL(_mysql, "SELECT obsgroupid,obsid FROM obsgroup_obs ORDER BY obsgroupid,obsid", _logger);
    int currentGroupId = -1;
    int cnt = 0;
    std::vector<int> currentObsids;
    for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
      int groupid = ::atol(row[0]);
      int obsid = ::atol(row[1]);
      if (currentGroupId != groupid) {
        if (currentGroupId >= 0) {
          groupsFrom [currentGroupId] = currentObsids;
          ++cnt;
        }
        currentGroupId = groupid;
        currentObsids.clear();
      }
      currentObsids.push_back (obsid);
      if (groupsTo.find (obsid) == groupsTo.end()) {
        groupsTo[obsid] = std::vector<int>();
      }
      groupsTo[obsid].push_back (groupid);
    }
    if (currentGroupId >= 0) {
      groupsFrom [currentGroupId] = currentObsids;
      ++cnt;
    }
    ::mysql_free_result (result);
    LOG4CXX_INFO(_logger, "retrieved " << cnt << " groups...");
  }
  inline int toRegion (int x, int y) {
    assert (x >= 0);
    assert (x < (REGION_SIZE << 16));
    return ((y / REGION_SIZE) << 16) + (x / REGION_SIZE);
  }
};

void Loader::loadGroup(float D2, int T) {
  LOG4CXX_INFO(_logger, "grouping observations... D2(max velocity [cells/time])=" << D2 << ", T(max backtracking [time])=" << T);
  ObservationGroupTable groupTable(_connections.getMainMysql());
  groupTable.dropIfExists();
  groupTable.createTable();

  ImageTable imageTable;
  std::vector<Image> images = imageTable.getAllImages(_connections.getMainMysql());

  {
    RdbmsCookGroupCallbacks callbacks (_connections.getMainMysql(), _logger, images);
    CookGroup cookGroup (callbacks, D2, T);
    // we cook and insert to mysql per cycle as specified in the benchmark spec.
    // the most efficient way is to cook all cycles and insert to mysql at once,
    // but it doesn't reflect what the astronomy project actually does.
    for (int cycle = 0; (int) images.size() - cycle * 20 > 0; ++cycle) {
      LOG4CXX_INFO(_logger, "cooking groups in " << cycle << "th cycle...");
      // all groups found in this cycle are inserted to mysql together for efficiency
      cookGroup.cook(cycle * 20, std::min((cycle + 1) * 20, (int) images.size()));
      callbacks.processCurrentCycle();
      LOG4CXX_INFO(_logger, "done cycle=" << cycle);
    }
    callbacks.loadToMysql();
    LOG4CXX_INFO(_logger, "loaded groups. max groupid=" << callbacks.getNextGroupId());
  }
  LOG4CXX_INFO(_logger, "deleted RdbmsCookGroupCallbacks");

  groupTable.createIndex();
  LOG4CXX_INFO(_logger, "finished grouping observations.");
}
