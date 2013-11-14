#ifndef MYSQLWRAPPEr_H
#define MYSQLWRAPPEr_H

#include <list>
#include <set>
#include <map>
#include <sstream>
#include <vector>
#include <utility>
#include <log4cxx/logger.h>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
using namespace log4cxx;

#include <mysql.h>

#include "configs.h"

void execUpdateSQL (MYSQL *mysql, const std::string &sql, LoggerPtr logger);
void dropTableIfExists (MYSQL *mysql, const std::string &table, LoggerPtr logger);
// NOTE: this function stores all results at one time (calls mysql_store_result).
MYSQL_RES* execSelectSQL (MYSQL *mysql, const std::string &sql, LoggerPtr logger, bool fetchPerRow = false);
std::vector<int> execSelectIntVectorSQL (MYSQL *mysql, const std::string &sql, int nullValue, LoggerPtr logger);
std::set<int> execSelectIntSetSQL (MYSQL *mysql, const std::string &sql, int nullValue, LoggerPtr logger);
int execSelectIntSQL (MYSQL *mysql, const std::string &sql, int nullValue, LoggerPtr logger);
void appendInClause (std::stringstream &str, const std::set<int> &values);
void appendInClause (std::stringstream &str, const std::vector<int> &values);

// for CSV bulk loading
std::string getCurDir(LoggerPtr logger);
int prepareCsvFile(const std::string &tablename, LoggerPtr logger);
int prepareCsvFileFullPath(const std::string &csvname, LoggerPtr logger);
void closeCsvFile(int fd, LoggerPtr logger);
void loadCsvFile(MYSQL *mysql, const std::string &tablename, LoggerPtr logger, bool replace = false);
void loadIntsToTempTable(MYSQL *mysql, const std::string &temptablename, LoggerPtr logger, const std::vector<int> &results);
void loadIntsToTempTable(MYSQL *mysql, const std::string &temptablename, LoggerPtr logger, const std::set<int> &results);

class CsvBuffer {
public:
  CsvBuffer (LoggerPtr logger, const std::string &tablename, size_t bufsize);
  CsvBuffer (LoggerPtr logger, const std::string &tablename, size_t bufsize, const std::string &csvFullpath);
  ~CsvBuffer ();
  // variable arguments functions can't be wrapped. so, this is not write(), but written()
  void written (int sprintfRet);
  void close ();
  void loadToMysql (MYSQL *mysql, bool replace = false);
  char * curbuf () const { return _buf + _bufused; }

  LoggerPtr _logger;
  std::string _tablename;
  std::string _pathbuf;
  std::string _csvname;
  int _bufsize;
  int _bufused;
  char *_buf;
  int _fd;
};

// MysqlConnections
// all connections this program is retaining
// don't add too many features. we don't want to re-invent mysql++.
class MysqlConnections {
public:
  MysqlConnections (const Configs& configs);
  ~MysqlConnections();
  void closeAll();

  MYSQL* getMainMysql() {return _main; }
  MYSQL* getSubMysql(size_t index) { return _sub[index]; }
  size_t getSubMysqlCount() const { return _sub.size(); }

private:
  MYSQL* connect (const MysqlHost& host);
  void close (const MysqlHost& host, MYSQL*);
  const Configs& _configs;
  MYSQL * _main;
  std::vector<MYSQL*> _sub;
  LoggerPtr _logger;
  bool _closed;
};

// container for pixels. this is cheap to pass by value.
struct ImagePixels {
  int externalWidth, externalHeight; // apparent size
  int internalWidth, internalHeight; // could be smaller than external
  int internalXOffset, internalYOffset;
  int32_t* internalArray; // raw pointer. fast to use
  boost::shared_array<int32_t> internalArrayAutoPtr; // to automatically revoke internalArray
  inline int32_t getPixel (int x, int y) const {
    return internalArray[(y - internalYOffset) * internalWidth +  x - internalXOffset];
  }
  inline void setPixel (int x, int y, int32_t pix) {
    assert (y - internalYOffset >= 0);
    assert (y - internalYOffset < internalHeight);
    assert (x - internalXOffset >= 0);
    assert (x - internalXOffset < internalWidth);
    internalArray[(y - internalYOffset) * internalWidth +  x - internalXOffset] = pix;
  }
};

struct Image {
  Image () {};
  Image (const MYSQL_ROW &row);
  int32_t imageid;
  int32_t xstart;
  int32_t ystart;
  int32_t xend;
  int32_t yend;
  int32_t time;
  int32_t cycle;
  std::string tablename;
  std::string toString() const {
    std::stringstream str;
    str << "imageid=" << imageid << ", xstart=" << xstart << ", ystart=" << ystart << ", xend=" << xend
        << ", yend=" << yend << ", time=" << time << ", cycle=" << cycle << ", tablename=" << tablename;
    return str.str();
  }
  int32_t getWidth () const { return xend - xstart; }
  int32_t getHeight () const { return yend - ystart; }
  ImagePixels getPixels(MYSQL *mysql, LoggerPtr logger, const std::set<int> &tiles) const;
};

// represents the image table to store tiny summaries of images.
// CREATE TABLE images (
//  imageid INT NOT NULL PRIMARY KEY,
//  xstart INT NOT NULL,
//  ystart INT NOT NULL,
//  xend INT NOT NULL,
//  yend INT NOT NULL,
//  time INT NOT NULL,
//  cycle INT NOT NULL,
//  tablename VARCHAR(20) NOT NULL -- ex. 'Raw_0001', 'Raw_0002'
//)
class ImageTable {
public:
  ImageTable () : _logger(Logger::getLogger("ImageTable")) {};

  std::vector<Image> getAllImages (MYSQL* mysql);
  std::vector<Image> getImagesByCycle (MYSQL* mysql, int32_t cycle);
  Image getImageById (MYSQL* mysql, int32_t imageid);
  void insertImage (MYSQL* mysql, const Image &image);
  void deleteImage (MYSQL* mysql, int32_t imageid);
private:
  LoggerPtr _logger;
};

// x-y lengthes for one tile
// tile is a chunk in Raw_xxx
#define TILE_SIZE 100
#define TILE_X_Y_RATIO 1000

struct Point {
  Point() : x(0), y(0) {}
  Point(int32_t xArg, int32_t yArg) : x(xArg), y(yArg) {}
  int32_t x, y;
};
struct Rect {
  int32_t xstart, ystart, xend, yend;
};

struct Observ; // defined in cook.h
// Represents a star.
struct Observation {
  Observation() {};
  Observation(const MYSQL_ROW &row);
  static void newObserv(Observ &observ, const MYSQL_ROW &row);
  Observation(const Observ &observ, const Image &image);
  int32_t obsid; // unique id for observation. start from 1
  int32_t imageid;
  float averageDist;
  long long pixelSum;
  int32_t time, cycle; // denormalized data for convenience. can be retrieved from imageid.
  Point center; // location of this observation defined as the center of polygons. in world coordinate.
  Rect bbox; // bounding box of polygons. in world coordinate.
  std::vector<Point> polygons; // polygons that define this observation in world coordinate. This is empty when constructed by MYSQL_ROW. (need to complement in separate method)
};

// CREATE TABLE observations (
//   obsid INT NOT NULL PRIMARY KEY,
//   imageid INT NOT NULL REFERENCES images,
//   time INT NOT NULL, -- denormalized data for convenience
//   cycle INT NOT NULL, -- denormalized data for convenience
//   averageDist REAL NOT NULL,
//   pixelSum BIGINT NOT NULL,
// -- x/y of center of polygons that define this star
//   centerx INT NOT NULL,
//   centery INT NOT NULL,
// -- bounding box of all polygons of this star
//   boxxstart INT NOT NULL,
//   boxystart INT NOT NULL,
//   boxxend INT NOT NULL,
//   boxyend INT NOT NULL,
//   center GEOMETRY, bbox GEOMERTY
// ) ENGINE=MyISAM;
// note that this table has to be on MyISAM to use spatial index
// also manges obs_polygons.
class ObservationTable {
public:
  ObservationTable (const std::string &prefix = "") : _logger(Logger::getLogger("ObservationTable")), _prefix(prefix) {};
  void createTable (MYSQL* mysql);
  void createIndex (MYSQL* mysql);
  void dropIfExists (MYSQL* mysql);
  void getAll (MYSQL* mysql, std::list<Observation>& out); // basically just for debugging.
  void insertBulk (MYSQL* mysql, const std::list<Observ>& observations, const Image &image);
  void writeToCsv (CsvBuffer &buffer, const std::list<Observ>& observations, const Image &image);
  void writeToCsvPolygon (CsvBuffer &buffer, const std::list<Observ>& observations, const Image &image);
  void getBulkAsObservList (std::list<Observ>&, MYSQL* mysql, const std::string &sql);
private:
  LoggerPtr _logger;
  std::string _prefix; // for making a temporary table with different name in the same table scheme
};

// CREATE TABLE obsgroup (
//   obsgroupid INT NOT NULL PRIMARY KEY,
//   first_obsid INT NOT NULL REFERENCES observation,
//   centroidx INT NOT NULL,
//   centroidy INT NOT NULL,
//   boxxstart INT NOT NULL, -- bbox of all centroids of this group's observations
//   boxystart INT NOT NULL,
//   boxxend INT NOT NULL,
//   boxyend INT NOT NULL,
//   center GEOMETRY NOT NULL, // center of all centers of observations
//   bbox GEOMETRY NOT NULL, // bbox of all polygons of observations (=of all bboxes)
//   center_traj GEOMETRY NOT NULL -- trajectory defined by centroids in Q8. for Q9, use polygon_trajectory
// ) ENGINE=MyISAM;
// also manges obsgroup_obs and polygon_trajectory
class ObservationGroupTable {
public:
  ObservationGroupTable (MYSQL* mysql) : _mysql(mysql), _logger(Logger::getLogger("ObservationGroupTable")) {};
  void createTable ();
  void createIndex ();
  void dropIfExists ();

private:
  MYSQL* _mysql;
  LoggerPtr _logger;
};

#endif // MYSQLWRAPPEr
