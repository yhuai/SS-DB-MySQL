#include "mysqlwrapper.h"
#include <cmath>
#include <iostream>
#include <sstream>
#include <string.h>
#include "cook.h"
#include "stopwatch.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <cstdio>
#include <cstdlib>

using namespace std;

void execUpdateSQL (MYSQL *mysql, const std::string &sql, LoggerPtr logger) {
  LOG4CXX_DEBUG(logger, "Running update:" << sql);
  int ret = ::mysql_query (mysql, sql.c_str());
  if (ret != 0) {
    LOG4CXX_ERROR(logger, "failed to execute:" << sql << ". ret=" << ret << " errno=" << ::mysql_errno(mysql) << " errmsg=" << ::mysql_error(mysql));
    throw std::exception();
  }
}
void dropTableIfExists (MYSQL *mysql, const std::string &table, LoggerPtr logger) {
  execUpdateSQL(mysql, "DROP TABLE IF EXISTS " + table, logger);
}
MYSQL_RES* execSelectSQL (MYSQL *mysql, const std::string &sql, LoggerPtr logger, bool fetchPerRow) {
  LOG4CXX_DEBUG(logger, "Running SELECT:" << sql << ". fetch=" << (fetchPerRow ? "per-row" : "once"));
  int ret = ::mysql_query (mysql, sql.c_str());
  if (ret != 0) {
    LOG4CXX_ERROR(logger, "failed to execute:" << sql << ". ret=" << ret << " errno=" << ::mysql_errno(mysql) << " errmsg=" << ::mysql_error(mysql));
    throw std::exception();
  }
  MYSQL_RES *result;
  if (fetchPerRow) {
    result = ::mysql_use_result (mysql);
  } else {
    result = ::mysql_store_result (mysql);
  }
  if (result == NULL) {
    LOG4CXX_ERROR(logger, "failed to retrieve results:" << sql);
    throw std::exception();
  }
  return result;
}
std::vector<int> execSelectIntVectorSQL (MYSQL *mysql, const std::string &sql, int nullValue, LoggerPtr logger) {
  MYSQL_RES *result = execSelectSQL (mysql, sql, logger);
  std::vector<int> res;
  for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
    if (row[0] == NULL) {
      res.push_back(nullValue);
    } else {
      res.push_back(::atol(row[0]));
    }
  }
  ::mysql_free_result (result);
  return res;
}
std::set<int> execSelectIntSetSQL (MYSQL *mysql, const std::string &sql, int nullValue, LoggerPtr logger) {
  MYSQL_RES *result = execSelectSQL (mysql, sql, logger);
  std::set<int> res;
  for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
    if (row[0] == NULL) {
      res.insert(nullValue);
    } else {
      res.insert(::atol(row[0]));
    }
  }
  ::mysql_free_result (result);
  return res;
}
int execSelectIntSQL (MYSQL *mysql, const std::string &sql, int nullValue, LoggerPtr logger) {
  std::vector<int> res = execSelectIntVectorSQL (mysql, sql, nullValue, logger);
  assert (res.size() > 0);
  return res[0];
}
void appendInClause (std::stringstream &str, const std::set<int> &values) {
  std::vector<int> vec;
  vec.insert (vec.end(), values.begin(), values.end());
  appendInClause (str, vec);
}
void appendInClause (std::stringstream &str, const std::vector<int> &values) {
  assert (values.size() > 0);
  str << " (" << values[0];
  for (size_t i = 1; i < values.size(); ++i) {
    str << "," << values[i];
  }
  str << ")";
}


std::string getCurDir(LoggerPtr logger) {
  char pathbuf[512];
  char *res = ::getcwd(pathbuf, 512);
  if (res == NULL) {
    LOG4CXX_ERROR(logger, "failed to getcwd()");
    throw std::exception();
  }
  return pathbuf;
}
int prepareCsvFile(const std::string &tablename, LoggerPtr logger) {
  string pathbuf = getCurDir(logger);
  LOG4CXX_INFO(logger, "current dir=" << pathbuf);
  string csvname = pathbuf + "/" + tablename + ".csv";
  return prepareCsvFileFullPath (csvname, logger);
}
int prepareCsvFileFullPath(const std::string &csvname, LoggerPtr logger) {
  if (std::remove(csvname.c_str()) == 0) {
    LOG4CXX_INFO(logger, "deleted existing file " << csvname << ".");
  }
  int fd = ::open (csvname.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_LARGEFILE, S_IRWXU | S_IRWXG | S_IRWXO);
  if (fd < 0) {
    LOG4CXX_ERROR(logger, "could not create csv file " << csvname << ". errno=" << errno);
  }
  return fd;
}
void closeCsvFile(int fd, LoggerPtr logger) {
  int fsyncRet = ::fsync (fd);
  if (fsyncRet != 0) {
    LOG4CXX_ERROR(logger, "error on fsync a temp file. errno=" << errno);
  }
  int ret = ::close(fd);
  if (ret != 0) {
    LOG4CXX_ERROR(logger, "error on closing a temp file. errno=" << errno);
  }
}

void loadCsvFile(MYSQL *mysql, const std::string &tablename, LoggerPtr logger, bool replace) {
  LOG4CXX_INFO(logger, "loading csv to mysql ...");
  string pathbuf = getCurDir(logger);
  string csvname = pathbuf + "/" + tablename + ".csv";
  std::stringstream loadsql;
  loadsql << "LOAD DATA LOCAL INFILE '" << csvname << "' "
    << (replace ? "REPLACE" : "IGNORE")
    << " INTO TABLE " << tablename << " FIELDS TERMINATED BY ',' LINES TERMINATED BY '\\n'";
  execUpdateSQL (mysql, loadsql.str(), logger);
  LOG4CXX_INFO(logger, "loaded.");
}

void loadIntsToTempTable(MYSQL *mysql, const std::string &temptablename, LoggerPtr logger, const std::vector<int> &results) {
  std::set<int> sets;
  sets.insert (results.begin(), results.end());
  loadIntsToTempTable(mysql, temptablename, logger, sets);
}
void loadIntsToTempTable(MYSQL *mysql, const std::string &temptablename, LoggerPtr logger, const std::set<int> &results) {
  LOG4CXX_INFO(logger, "inserting integers to mysql in bulk...");
  dropTableIfExists (mysql, temptablename, logger);
  execUpdateSQL (mysql, "CREATE TABLE " + temptablename + "(val INT NOT NULL PRIMARY KEY) ENGINE=InnoDB", logger);
  if (results.size () == 0) return;
  int fd = prepareCsvFile(temptablename, logger);

  size_t bufsize = (1 << 18);
  char *buf = new char[bufsize];
  size_t bufused = 0;
  for (std::set<int>::const_iterator it = results.begin(); it != results.end(); ++it) {
    int written = ::sprintf (buf + bufused, "%d\n", *it);
    if (written <= 0) {
      LOG4CXX_ERROR(logger, "failed to sprintf");
      throw new std::exception ();
    }
    bufused += written;
    if (bufused > bufsize * 9 / 10) {
      int writeRet = ::write (fd, buf, bufused);
      if (writeRet == -1) {
        LOG4CXX_ERROR(logger, "error on writing a temp file. errno=" << errno);
      }
      bufused = 0;
    }
  }
  if (bufused > 0) {
    int writeRet = ::write (fd, buf, bufused);
    if (writeRet == -1) {
      LOG4CXX_ERROR(logger, "error on writing a temp file. errno=" << errno);
    }
    bufused = 0;
  }
  delete[] buf;

  closeCsvFile(fd, logger);
  loadCsvFile(mysql, temptablename, logger, false);
}


MysqlConnections::MysqlConnections (const Configs& configs) : _configs(configs), _logger(Logger::getLogger("mysql")), _closed(false) {
  const ConnectionConfig &con = _configs.getConnectionConfig();
  _main = connect(con.getMain());
  for (size_t i = 0; i < con.getSub().size(); ++i) {
    _sub.push_back(connect(con.getSub()[i]));
  }
}

MysqlConnections::~MysqlConnections() {
  closeAll();
}

MYSQL* MysqlConnections::connect (const MysqlHost& host) {
  LOG4CXX_INFO(_logger, "connecting to " << host.toString() << "...");
  MYSQL *temp = ::mysql_init(NULL);
  if (temp == NULL) {
    LOG4CXX_ERROR(_logger, "failed to call mysql_init.");
    throw std::exception();
  }
  MYSQL *ret = ::mysql_real_connect (temp, host.host.c_str(),
    host.user.c_str(), host.pass.c_str(), host.db.c_str(), host.port, NULL, 0);
  if (ret == NULL) {
    LOG4CXX_ERROR(_logger, "failed to connect errno=" << ::mysql_errno(temp));
    throw std::exception();
  }
  return ret;
}
void MysqlConnections::close (const MysqlHost& host, MYSQL *mysql) {
  LOG4CXX_INFO(_logger, "closing connection to " << host.toString() << "...");
  ::mysql_close(mysql);
}

void MysqlConnections::closeAll() {
  if (!_closed) {
    const ConnectionConfig &con = _configs.getConnectionConfig();
    close(con.getMain(), _main);
    _main = NULL;
    for (size_t i = 0; i < con.getSub().size(); ++i) {
      close(con.getSub()[i], _sub[i]);
    }
    _sub.clear();

    _closed = true;
    LOG4CXX_INFO(_logger, "closed.");
  }
}

Image::Image (const MYSQL_ROW &row) {
  int ind = 0;
  imageid = ::atol(row[ind++]);
  xstart = ::atol(row[ind++]);
  ystart = ::atol(row[ind++]);
  xend = ::atol(row[ind++]);
  yend = ::atol(row[ind++]);
  time = ::atol(row[ind++]);
  cycle = ::atol(row[ind++]);
  tablename = row[ind++];
}
ImagePixels Image::getPixels(MYSQL *mysql, LoggerPtr logger, const std::set<int> &tiles) const {
  int width = xend - xstart;
  int height = yend - ystart;
  int minYtile = 1 << 30, minXtile = 1 << 30;
  int maxYtile = -1, maxXtile = -1;
  for (std::set<int>::const_iterator it = tiles.begin(); it != tiles.end(); ++it) {
    int tile = *it;
    int ytile = tile / TILE_X_Y_RATIO;
    int xtile = tile % TILE_X_Y_RATIO;
    if (ytile < minYtile) minYtile = ytile;
    if (xtile < minXtile) minXtile = xtile;
    if (ytile > maxYtile) maxYtile = ytile;
    if (xtile > maxXtile) maxXtile = xtile;
  }
  assert (minYtile >= 0);
  assert (minYtile <= height / TILE_SIZE);
  assert (minXtile >= 0);
  assert (minXtile <= width / TILE_SIZE);
  assert (maxYtile >= 0);
  assert (maxYtile <= height / TILE_SIZE);
  assert (maxXtile >= 0);
  assert (maxXtile <= width / TILE_SIZE);
  assert (maxYtile >= minYtile);
  assert (maxXtile >= minXtile);

  ImagePixels pixels;
  pixels.externalHeight = height;
  pixels.externalWidth = width;
  pixels.internalHeight = (maxYtile - minYtile + 1) * TILE_SIZE;
  pixels.internalWidth = (maxXtile - minXtile + 1) * TILE_SIZE;
  pixels.internalYOffset = minYtile * TILE_SIZE;
  pixels.internalXOffset = minXtile * TILE_SIZE;
  pixels.internalArrayAutoPtr = boost::shared_array<int32_t> (new int32_t[pixels.internalHeight * pixels.internalWidth]);
  pixels.internalArray = pixels.internalArrayAutoPtr.get();
  ::memset (pixels.internalArray, 0, sizeof(int32_t) * pixels.internalHeight * pixels.internalWidth);
  StopWatch watch;
  watch.init();
  int batchStart = *(tiles.begin()), batchEnd = *(tiles.begin());
  for (std::set<int>::const_iterator it = tiles.begin(); it != tiles.end(); ++it) {
    int tile = *it;
    assert (batchEnd <= tile);
    if (tile - batchEnd < 3) {
      batchEnd = tile;
      continue;
    }
    std::stringstream sql;
    sql << "SELECT x,y,pix FROM " << tablename << " WHERE tile BETWEEN " << batchStart << " AND " << batchEnd;
    MYSQL_RES *result = execSelectSQL(mysql, sql.str(), logger);
    for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
      int32_t x = ::atol(row[0]);
      int32_t y = ::atol(row[1]);
      assert (x >= 0);
      assert (x < width);
      assert (y >= 0);
      assert (y < height);
      int32_t pix = ::atol(row[2]);
      pixels.setPixel(x, y, pix);
    }
    ::mysql_free_result (result);
    batchStart = tile;
    batchEnd = tile;
  }
  std::stringstream sql;
  sql << "SELECT x,y,pix FROM " << tablename << " WHERE tile BETWEEN " << batchStart << " AND " << batchEnd;
  MYSQL_RES *result = execSelectSQL(mysql, sql.str(), logger);
  for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
    int32_t x = ::atol(row[0]);
    int32_t y = ::atol(row[1]);
    assert (x >= 0);
    assert (x < width);
    assert (y >= 0);
    assert (y < height);
    int32_t pix = ::atol(row[2]);
    pixels.setPixel(x, y, pix);
  }
  ::mysql_free_result (result);
  watch.stop();
  LOG4CXX_INFO(logger, "retrieved pixels from " << tablename << " in "<<watch.getElapsed() << " microsec");
  return pixels;
}

void getImages (LoggerPtr _logger, MYSQL* mysql, const std::string &sql, std::vector<Image> &images) {
  MYSQL_RES *result = execSelectSQL(mysql, sql.c_str(), _logger);
  for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
    Image image (row);
    images.push_back(image);
  }
  ::mysql_free_result (result);
}

std::vector<Image> ImageTable::getAllImages (MYSQL* mysql) {
  stringstream sql;
  sql << "SELECT * FROM images ORDER BY imageid";
  vector<Image> images;
  getImages(_logger, mysql, sql.str(), images);
  return images;
}

std::vector<Image> ImageTable::getImagesByCycle (MYSQL* mysql, int32_t cycle) {
  stringstream sql;
  sql << "SELECT * FROM images WHERE cycle=" << cycle << " ORDER BY imageid";
  vector<Image> images;
  getImages(_logger, mysql, sql.str(), images);
  return images;
}
Image ImageTable::getImageById (MYSQL* mysql, int32_t imageid) {
  stringstream sql;
  sql << "SELECT * FROM images WHERE imageid=" << imageid;
  MYSQL_RES *result = execSelectSQL(mysql, sql.str(), _logger);
  MYSQL_ROW row = ::mysql_fetch_row (result);
  Image image (row);
  ::mysql_free_result (result);
  return image;
}

void ImageTable::insertImage (MYSQL* mysql, const Image &image) {
  //TODO
}
void ImageTable::deleteImage (MYSQL* mysql, int32_t imageid) {
  //TODO
}

Observation::Observation(const MYSQL_ROW &row) {
  int ind = 0;
  obsid = ::atol(row[ind++]);
  imageid = ::atol(row[ind++]);
  time = ::atol(row[ind++]);
  cycle = ::atol(row[ind++]);
  averageDist = ::atof(row[ind++]);
  pixelSum = ::atoll(row[ind++]);
  center.x = ::atol(row[ind++]);
  center.y = ::atol(row[ind++]);
  bbox.xstart = ::atol(row[ind++]);
  bbox.ystart = ::atol(row[ind++]);
  bbox.xend = ::atol(row[ind++]);
  bbox.yend = ::atol(row[ind++]);
  // polygons are empty with this constructor
}
void Observation::newObserv(Observ &observ, const MYSQL_ROW &row) {
  int ind = 0;
  observ.observId = ::atol(row[ind++]);
  observ.imageId = ::atol(row[ind++]);
  observ.time = ::atol(row[ind++]);
  /*observ.cycle = ::atol(row[*/ind++/*])*/;
  observ.averageDist = ::atof(row[ind++]);
  observ.pixelSum = ::atoll(row[ind++]);
  observ.centroidX = ::atol(row[ind++]);
  observ.centroidY = ::atol(row[ind++]);
  observ.boxxstart = ::atol(row[ind++]);
  observ.boxystart = ::atol(row[ind++]);
  observ.boxxend = ::atol(row[ind++]);
  observ.boxyend = ::atol(row[ind++]);
  // polygons are empty with this constructor
}

Observation::Observation(const Observ &observ, const Image &image) {
  obsid = observ.observId;
  imageid = image.imageid;
  time = image.time;
  cycle = image.cycle;
  averageDist = observ.averageDist;
  pixelSum = observ.pixelSum;
  center.x = observ.centroidX + image.xstart;
  center.y = observ.centroidY + image.ystart;
  bbox.xstart = 1 << 30;
  bbox.ystart = 1 << 30;
  bbox.xend = -1;
  bbox.yend = -1;
  for (size_t i = 0; i < observ.polygons.size(); ++i) {
    int32_t x = observ.polygons[i].first + image.xstart;
    int32_t y = observ.polygons[i].second + image.ystart;
    bbox.xstart = min (bbox.xstart, x);
    bbox.ystart = min (bbox.ystart, y);
    bbox.xend = max (bbox.xend, x);
    bbox.yend = max (bbox.yend, y);
    polygons.push_back(Point(x, y));
  }
}

void ObservationTable::createTable (MYSQL* mysql) {
  LOG4CXX_INFO(_logger, "Creating observations table..");
  execUpdateSQL (mysql, "CREATE TABLE " + _prefix + "observations ("
    " obsid INT NOT NULL PRIMARY KEY,"
    " imageid INT NOT NULL," //  REFERENCES images. to be fair, removed FK. (it has some performance penalty)
    " time INT NOT NULL,"
    " cycle INT NOT NULL,"
    " averageDist REAL NOT NULL,"
    " pixelSum BIGINT NOT NULL,"
    " centerx INT NOT NULL,"
    " centery INT NOT NULL,"
    " boxxstart INT NOT NULL,"
    " boxystart INT NOT NULL,"
    " boxxend INT NOT NULL,"
    " boxyend INT NOT NULL,"
    " center GEOMETRY NOT NULL, bbox GEOMETRY NOT NULL"
    " ) ENGINE=MyISAM", _logger);
  execUpdateSQL (mysql, "CREATE TABLE " + _prefix + "obs_polygons ("
    " obsid INT NOT NULL,"
    " ord INT NOT NULL,"
    " x INT NOT NULL,"
    " y INT NOT NULL,"
    " CONSTRAINT PRIMARY KEY obs_polygons_pk (obsid, ord)"
    " ) ENGINE=InnoDB", _logger);
  LOG4CXX_INFO(_logger, "Created observations table.");
}
void ObservationTable::createIndex (MYSQL* mysql) {
  LOG4CXX_INFO(_logger, "Creating indexes on observations table..");
  execUpdateSQL (mysql, "CREATE SPATIAL INDEX ix_obs_cn ON " + _prefix + "observations (center)", _logger);
  execUpdateSQL (mysql, "CREATE SPATIAL INDEX ix_obs_bb ON " + _prefix + "observations (bbox)", _logger);
  LOG4CXX_INFO(_logger, "Created observations indexes.");
}
void ObservationTable::dropIfExists (MYSQL* mysql) {
  LOG4CXX_INFO(_logger, "Dropping observations table..");
  dropTableIfExists (mysql, _prefix + "observations", _logger);
  dropTableIfExists (mysql, _prefix + "obs_polygons", _logger);
  LOG4CXX_INFO(_logger, "Dropped observations table.");
}

void ObservationTable::getAll (MYSQL* mysql, std::list<Observation>& out) {
  MYSQL_RES *result = execSelectSQL(mysql, "SELECT * FROM " + _prefix + "observations", _logger);
  for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
    Observation obs (row);
    out.push_back(obs);
  }
  ::mysql_free_result (result);
}

void ObservationTable::writeToCsv (CsvBuffer &buffer, const std::list<Observ>& observations, const Image &image) {
  LOG4CXX_INFO(_logger, "writing " << observations.size () << " observations into csv...");
// csv loading of a table with geometry columns is VERY tricky.
// see followings:
// http://forums.mysql.com/read.php?23,138113,139793#msg-139793
// http://dev.mysql.com/doc/refman/5.0/en/load-data.html
// http://dev.mysql.com/doc/refman/5.0/en/populating-spatial-columns.html
  std::list<Observ>::const_iterator iter = observations.begin();
  std::set<int32_t> existingObsids;
  for (size_t i = 0; iter != observations.end(); ++i, ++iter) {
    if (i % 1000 == 0) {
      LOG4CXX_INFO(_logger, "writing " << i << "...");
    }
    if (existingObsids.find(iter->observId) != existingObsids.end()) {
      LOG4CXX_WARN(_logger, "WTF. duplicated observation id: " << (iter->observId) << ". this observation will be ignored."
        " this sometimes (but very rarely) happens without a known reason. ");
      continue;
    }
    existingObsids.insert(iter->observId);
    Observation obs(*iter, image);
    // use ';' to separate columns because GEOMETRY object includes commas
    buffer.written(::sprintf (buffer.curbuf(), "%d;%d;%d;%d;%f;%lld;%d;%d;%d;%d;%d;%d;POINT(%d %d);POLYGON((",
      obs.obsid, obs.imageid,
      obs.time, obs.cycle,
      obs.averageDist, obs.pixelSum,
      obs.center.x, obs.center.y,
      obs.bbox.xstart, obs.bbox.ystart, obs.bbox.xend, obs.bbox.yend,
      obs.center.x, obs.center.y));
    for (size_t j = 0; j < obs.polygons.size(); ++j) {
      buffer.written(::sprintf (buffer.curbuf(), "%d %d,", obs.polygons[j].x, obs.polygons[j].y));
    }
    buffer.written(::sprintf (buffer.curbuf(), "%d %d))\n", obs.polygons[0].x, obs.polygons[0].y));
  }
}
void ObservationTable::writeToCsvPolygon (CsvBuffer &buffer, const std::list<Observ>& observations, const Image &image) {
  LOG4CXX_INFO(_logger, "writing polygons of " << observations.size () << " observations into csv...");
  std::set<int32_t> existingObsids;
  for (std::list<Observ>::const_iterator iter = observations.begin(); iter != observations.end(); ++iter) {
    if (existingObsids.find(iter->observId) != existingObsids.end()) {
      LOG4CXX_WARN(_logger, "WTF. duplicated observation id: " << (iter->observId) << ". polygons of this observation will be ignored."
        " this sometimes (but very rarely) happens without a known reason. ");
      continue;
    }
    existingObsids.insert(iter->observId);
    for (size_t ord = 0; ord < iter->polygons.size(); ++ord) {
      buffer.written(::sprintf (buffer.curbuf(), "%d,%d,%d,%d\n",
          iter->observId, (int32_t) ord, iter->polygons[ord].first + image.xstart, iter->polygons[ord].second + image.ystart));
    }
  }
}

void ObservationTable::insertBulk (MYSQL* mysql, const std::list<Observ>& observations, const Image &image) {
  CsvBuffer buffer (_logger, _prefix + "observations", 4 << 20);
  writeToCsv (buffer, observations, image);
  buffer.close();

  std::stringstream loadsql;
  // very, very ugly...
  loadsql << "LOAD DATA LOCAL INFILE '" << buffer._csvname << "' INTO TABLE "
    << " " + _prefix + "observations FIELDS TERMINATED BY ';' LINES TERMINATED BY '\\n'"
    << "(obsid,imageid,time,cycle,averageDist,pixelSum,centerx,centery,"
    << "boxxstart,boxystart,boxxend,boxyend,"
    << "@var1, @var2)"
    << "SET center=GeomFromText(@var1),bbox=GeomFromText(@var2)";
  execUpdateSQL (mysql, loadsql.str(), _logger);

  // polygons are easier
  CsvBuffer bufferPolygon (_logger, _prefix + "obs_polygons", 4 << 20);
  writeToCsvPolygon (bufferPolygon, observations, image);
  bufferPolygon.close();
  bufferPolygon.loadToMysql(mysql);

  LOG4CXX_INFO(_logger, "Inserted");
}

void ObservationTable::getBulkAsObservList (std::list<Observ> &lst, MYSQL* mysql, const std::string &sql) {
  MYSQL_RES *result = execSelectSQL(mysql, sql, _logger);
  for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
    Observ obs;
    Observation::newObserv(obs, row);
    lst.push_back(obs);
  }
  ::mysql_free_result (result);
}

void ObservationGroupTable::createTable () {
  LOG4CXX_INFO(_logger, "Creating observation groups table..");
  execUpdateSQL (_mysql, "CREATE TABLE obsgroup ("
    " obsgroupid INT NOT NULL PRIMARY KEY,"
    " center_traj GEOMETRY NOT NULL"
    " ) ENGINE=MyISAM", _logger);
  execUpdateSQL (_mysql, "CREATE TABLE obsgroup_obs ("
    " obsgroupid INT NOT NULL," // REFERENCES obsgroup,"
    " obsid INT NOT NULL," // REFERENCES observation,"
    " CONSTRAINT PRIMARY KEY obsgroup_obs_pk (obsgroupid, obsid)"
    " ) ENGINE=InnoDB", _logger);
  execUpdateSQL (_mysql, "CREATE TABLE polygon_trajectory ("
    " obsgroupid INT NOT NULL," // REFERENCES obsgroup,
    " fromtime INT NOT NULL,"
    " totime INT NOT NULL,"
    // This star is observed at pre_obsid at fromtime, 
    // at post_obsid at totime. 
    // We assume the star can be at any point in between at anytime between fromtime to totime.
    // " pre_obsid INT NOT NULL," // REFERENCES observation, -- NULL if first obs
    // " post_obsid INT NOT NULL," // REFERENCES observation, -- NULL if last obs
    // above obsids are not used in queries. don't have to store as attributes.
    " traj GEOMETRY NOT NULL," // trajectory (convex hull) of all polygons of pre-post obs
    " CONSTRAINT PRIMARY KEY polygon_traj_pk (obsgroupid, fromtime)"
    " ) ENGINE=MyISAM", _logger);

  LOG4CXX_INFO(_logger, "Created observation groups table.");
}
void ObservationGroupTable::createIndex () {
  LOG4CXX_INFO(_logger, "Creating indexes on observation groups table..");
  execUpdateSQL (_mysql, "CREATE SPATIAL INDEX ix_obsgr_tr ON obsgroup (center_traj)", _logger);
  // not used so far - execUpdateSQL (_mysql, "CREATE INDEX ix_obsob_ob ON obsgroup_obs (obsid)", _logger); // this is required in addition to the primary key index for efficiently retrieving groups of the given observation
  execUpdateSQL (_mysql, "CREATE SPATIAL INDEX ix_obsgrpl_tr ON polygon_trajectory (traj)", _logger);
  LOG4CXX_INFO(_logger, "Created observation groups indexes.");
}
void ObservationGroupTable::dropIfExists () {
  LOG4CXX_INFO(_logger, "Dropping observation groups table..");
  dropTableIfExists (_mysql, "obsgroup", _logger);
  dropTableIfExists (_mysql, "obsgroup_obs", _logger);
  dropTableIfExists (_mysql, "polygon_trajectory", _logger);
  LOG4CXX_INFO(_logger, "Dropped observation groups table.");
}

CsvBuffer::CsvBuffer (LoggerPtr logger, const std::string &tablename, size_t bufsize) :  _logger(logger), _tablename (tablename), _bufsize (bufsize), _bufused(0), _buf (new char[bufsize]) {
  _fd = prepareCsvFile(_tablename, _logger);
  _pathbuf = getCurDir(_logger);
  _csvname = _pathbuf + "/" + _tablename + ".csv";
}
CsvBuffer::CsvBuffer (LoggerPtr logger, const std::string &tablename, size_t bufsize, const std::string &csvFullpath) :  _logger(logger), _tablename (tablename), _bufsize (bufsize), _bufused(0), _buf (new char[bufsize]) {
  _fd = prepareCsvFileFullPath(csvFullpath, _logger);
  _pathbuf = getCurDir(_logger);
  _csvname = csvFullpath;
}

CsvBuffer::~CsvBuffer () {
  delete[] _buf;
  _buf = NULL;
}

void CsvBuffer::written (int sprintfRet) {
  if (sprintfRet <= 0) {
    LOG4CXX_ERROR(_logger, "failed to sprintf");
    throw new std::exception ();
  }
  _bufused += sprintfRet;
  if (_bufused > _bufsize * 9 / 10) {
    int writeRet = ::write (_fd, _buf, _bufused);
    if (writeRet == -1) {
      LOG4CXX_ERROR(_logger, "error on writing a temp file. errno=" << errno);
    }
    _bufused = 0;
  }
}
void CsvBuffer::close () {
  if (_bufused > 0) {
    int writeRet = ::write (_fd, _buf, _bufused);
    if (writeRet == -1) {
      LOG4CXX_ERROR(_logger, "error on writing a temp file. errno=" << errno);
    }
    _bufused = 0;
  }
  closeCsvFile(_fd, _logger);
  _fd = 0;
}
void CsvBuffer::loadToMysql (MYSQL *mysql, bool replace) {
  loadCsvFile(mysql, _tablename, _logger, replace);
}

