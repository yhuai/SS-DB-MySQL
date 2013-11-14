
#include "queries.h"
#include "mysqlwrapper.h"
#include "stopwatch.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <cstdio>
#include <cstdlib>

#include <cstring>
#include <sstream>
#include <utility>
#include <vector>
#include <map>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include <boost/thread.hpp>

using namespace std;

// tiles to be accessed are like following
//  |--------------------------------------------> (x)
//  |---------------aaaaaaaaaaaaaaa-------------- stripe a
//  |---------------bbbbbbbbbbbbbbb-------------- stripe b
//  |---------------ccccccccccccccc-------------- stripe c
//  |---------------ddddddddddddddd-------------- stripe d
//  |---------------eeeeeeeeeeeeeee-------------- stripe e
//  |---------------fffffffffffffff-------------- stripe f
//  |--------------------------------------------
//  |--------------------------------------------
//  |
//  v (y)
// thus, we issue a query for each 'stripe' to make the most of
// clustered index on (tile,y,x)
void Benchmark::composeStripes (vector <pair<int32_t, int32_t> > &stripes /*out*/,
  int xstart/*X1*/, int xlen/*U1*/, int ystart/*Y1*/, int ylen/*V1*/) {
  int32_t originXTile = xstart / TILE_SIZE;
  int32_t originYTile = ystart / TILE_SIZE;
  int32_t xTileStride = (xlen - 1) / TILE_SIZE;
  int32_t yTileStride = (ylen - 1) / TILE_SIZE;
  for (int32_t yTile = 0; yTile <= yTileStride; ++yTile) {
    int32_t startTile = (originYTile + yTile) * TILE_X_Y_RATIO + originXTile;
    int32_t endTile = (originYTile + yTile) * TILE_X_Y_RATIO + originXTile + xTileStride;
    stripes.push_back(pair<int32_t, int32_t>(startTile, endTile));
  }
  LOG4CXX_INFO(_logger, "extracting " << stripes.size() << " stripes...");
}

void Benchmark::runQ1 (int32_t cycle, const std::string &attribute,
  int xstart/*X1*/, int xlen/*U1*/, int ystart/*Y1*/, int ylen/*V1*/) {
  StopWatch watch;
  watch.init();
  ImageTable imageTable;
  std::vector<Image> images = imageTable.getImagesByCycle(_connections.getMainMysql(), cycle);
  int64_t globalCount = 0;
  double globalSum = 0.0f;
  for (size_t i = 0; i < images.size(); ++i) {
    const Image &image = images[i];
    LOG4CXX_INFO(_logger, "aggregating on " << image.tablename << "...");
    vector <pair<int32_t, int32_t> > stripes; // start/end tiles for each stripe
    composeStripes(stripes, xstart, xlen, ystart, ylen);
    for (size_t j = 0; j < stripes.size(); ++j) {
      stringstream sql;
      sql << "SELECT SUM(" << attribute << "),COUNT(*) FROM " << image.tablename
        << " WHERE tile BETWEEN " << stripes[j].first << " AND " << stripes[j].second
        << " AND x BETWEEN " << xstart << " AND " << (xstart + xlen - 1)
        << " AND y BETWEEN " << ystart << " AND " << (ystart + ylen - 1);
      MYSQL_RES *result = execSelectSQL(_connections.getMainMysql(), sql.str(), _logger);
      MYSQL_ROW row = ::mysql_fetch_row (result);
      double localSum = row[0] == NULL ? 0.0f : ::atof(row[0]);
      int64_t localCount = ::atol(row[1]);
      LOG4CXX_INFO(_logger, "localSum:" << localSum << ", localCount:" << localCount);
      globalSum += localSum;
      globalCount += localCount;
      ::mysql_free_result (result);
    }
  }
  double globalAvg = globalSum / globalCount;
  watch.stop();
  LOG4CXX_INFO(_logger, "Ran Q1: " << watch.getElapsed() << " microsec. result = " << globalAvg);
}

void Benchmark::runQ1All (int32_t cycle, const std::string &attribute) {
  StopWatch watch;
  watch.init();
  ImageTable imageTable;
  std::vector<Image> images = imageTable.getImagesByCycle(_connections.getMainMysql(), cycle);
  int64_t globalCount = 0;
  double globalSum = 0.0f;
  for (size_t i = 0; i < images.size(); ++i) {
    const Image &image = images[i];
    LOG4CXX_INFO(_logger, "aggregating on " << image.tablename << "...");
    stringstream sql;
    sql << "SELECT SUM(" << attribute << "),COUNT(*) FROM " << image.tablename;
    MYSQL_RES *result = execSelectSQL(_connections.getMainMysql(), sql.str(), _logger);
    MYSQL_ROW row = ::mysql_fetch_row (result);
    double localSum = row[0] == NULL ? 0.0f : ::atof(row[0]);
    int64_t localCount = ::atol(row[1]);
    LOG4CXX_INFO(_logger, "localSum:" << localSum << ", localCount:" << localCount);
    globalSum += localSum;
    globalCount += localCount;
    ::mysql_free_result (result);
  }
  double globalAvg = globalSum / globalCount;
  watch.stop();
  LOG4CXX_INFO(_logger, "Ran Q1: " << watch.getElapsed() << " microsec. result = " << globalAvg);
}

void _runQ1AllParallel (size_t nodes, size_t nodeId, MYSQL *mysql, int32_t cycle, std::string attribute/*copied object*/, std::vector<Image> images/*copied object*/, int64_t *nodeLocalCount, double *nodeLocalSum) {
  stringstream nodeIdStr;
  nodeIdStr << nodeId;
  LoggerPtr logger(Logger::getLogger("Thread-" + nodeIdStr.str()));
  LOG4CXX_INFO(logger, "Started.");
  for (size_t i = 0; i < images.size(); ++i) {
    const Image &image = images[i];
    if (image.imageid % nodes != nodeId) continue; // round robin distribution
    LOG4CXX_INFO(logger, "aggregating on " << image.tablename << "...");
    stringstream sql;
    sql << "SELECT SUM(" << attribute << "),COUNT(*) FROM " << image.tablename;
    MYSQL_RES *result = execSelectSQL(mysql, sql.str(), logger);
    MYSQL_ROW row = ::mysql_fetch_row (result);
    double localSum = row[0] == NULL ? 0.0f : ::atof(row[0]);
    int64_t localCount = ::atol(row[1]);
    LOG4CXX_INFO(logger, "localSum:" << localSum << ", localCount:" << localCount);
    *nodeLocalSum += localSum;
    *nodeLocalCount += localCount;
    ::mysql_free_result (result);
  }
  LOG4CXX_INFO(logger, "Ended.");
}

void Benchmark::runQ1AllParallel (int32_t cycle, const std::string &attribute) {
  size_t nodes = _connections.getSubMysqlCount();
  LOG4CXX_INFO(_logger, "parallel Q1 on " << nodes << " nodes.");

  StopWatch watch;
  watch.init();

  ImageTable imageTable;
  std::vector<Image> images = imageTable.getImagesByCycle(_connections.getMainMysql(), cycle);

  boost::thread_group threadGroup;
  std::vector<int64_t*> nodeLocalCounts;
  std::vector<double*> nodeLocalSums;
  for (size_t i = 0; i < nodes; ++i) {
    int64_t *nodeLocalCount = new int64_t;
    *nodeLocalCount = 0;
    nodeLocalCounts.push_back (nodeLocalCount);
    double *nodeLocalSum = new double;
    *nodeLocalSum = 0.0f;
    nodeLocalSums.push_back (nodeLocalSum);
    
    boost::thread *threadPtr = new boost::thread(_runQ1AllParallel, nodes, (int) i, _connections.getSubMysql(i), cycle, attribute, images, nodeLocalCount, nodeLocalSum);
    threadGroup.add_thread(threadPtr);
  }
  threadGroup.join_all();

  int64_t globalCount = 0;
  double globalSum = 0.0f;
  for (size_t i = 0; i < nodes; ++i) {
    globalCount += *nodeLocalCounts[i];
    delete nodeLocalCounts[i];
    globalSum += *nodeLocalSums[i];
    delete nodeLocalSums[i];
  }
  double globalAvg = globalSum / globalCount;

  watch.stop();
  LOG4CXX_INFO(_logger, "Ran Parallel Q1: " << watch.getElapsed() << " microsec. count=" << globalCount << ", sum=" << globalSum << ", result = " << globalAvg);
}

void Benchmark::runQ3 (int32_t cycle, bool all, size_t nodes, size_t nodeId,
  int xstart/*X3*/, int xlen/*U3*/, int ystart/*Y3*/, int ylen/*V3*/) {
  LOG4CXX_INFO(_logger, "Running Q3: " << (nodes == 0 ? "" : "distributed") << "," << (all ? "all" : "") << "...");
  if (all) {
    ImageTable imageTable;
    std::vector<Image> images = imageTable.getAllImages(_connections.getMainMysql());
    xstart = 0;
    xlen = images[0].getWidth();
    ystart = 0;
    ylen = images[0].getHeight();
  }
  int32_t shrankXSize = (xlen * 3 / 10) + 1;
  int32_t shrankYSize = (ylen * 3 / 10) + 1;
  StopWatch watch;
  watch.init();
  int64_t cookElapsed = 0, insertElapsed = 0;
  ImageTable imageTable;
  std::vector<Image> images = imageTable.getImagesByCycle(_connections.getMainMysql(), cycle);
  for (size_t i = 0; i < images.size(); ++i) {
    const Image &image = images[i];
    // Q3's distributed version is to simply execute this on each node.
    // we don't do the same as Q1 because we don't need to collect results from each node.
    if (nodes > 0 && (image.imageid % nodes != nodeId)) continue; // round robin distribution
    LOG4CXX_INFO(_logger, "shrinking " << image.tablename << "...");
    std::vector<string> sqls;
    if (all) {
      // even in 'all', split the query per 5 TILE lines to save memory consumption.
      const size_t TILE_LINES_BATCH = 5;
      for (size_t j = 0; j < ((size_t) image.getHeight()) / TILE_SIZE; j += TILE_LINES_BATCH) {
        stringstream sql;
        sql << "SELECT x,y,pix,var,valid FROM " << image.tablename
          << " WHERE tile BETWEEN " << (j * TILE_X_Y_RATIO)
          << " AND " << ((j + TILE_LINES_BATCH) * TILE_X_Y_RATIO - 1);
        sqls.push_back (sql.str());
      }
    } else {
      vector <pair<int32_t, int32_t> > stripes; // start/end tiles for each stripe
      composeStripes(stripes, xstart, xlen, ystart, ylen);
      for (size_t j = 0; j < stripes.size(); ++j) {
        stringstream sql;
        sql << "SELECT x,y,pix,var,valid FROM " << image.tablename
          << " WHERE tile BETWEEN " << stripes[j].first << " AND " << stripes[j].second
          << " AND x BETWEEN " << xstart << " AND " << (xstart + xlen - 1)
          << " AND y BETWEEN " << ystart << " AND " << (ystart + ylen - 1);
        sqls.push_back (sql.str());
      }
    }

    ShrunkPixel *shrunkPixels = new ShrunkPixel[shrankXSize * shrankYSize]; // shrunkPixels[y * shrankXSize + x]
    ::memset (shrunkPixels, 0, sizeof (ShrunkPixel) * shrankXSize * shrankYSize);
    StopWatch watchCook;
    watchCook.init();
    for (size_t j = 0; j < sqls.size(); ++j) {
      const string &sql = sqls[j];
      MYSQL_RES *result = execSelectSQL(_connections.getMainMysql(), sql, _logger);
      for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
        int ind = 0;
        int x = ::atoi(row[ind++]); //  - xstartGlobal
        int y = ::atoi(row[ind++]); //  - ystartGlobal
        assert (x >= 0);
        assert (x < xlen);
        assert (y >= 0);
        assert (y < ylen);
        int pix = ::atoi(row[ind++]);
        int var = ::atoi(row[ind++]);
        int valid = ::atoi(row[ind++]);
        int shrankXLeft = x * 3 / 10, shrankXRight = shrankXLeft + 1;
        int shrankYTop = y * 3 / 10, shrankYBottom = shrankYTop + 1;
        // this pixel can contribute to up to 4 (left/right, top/bottom) shrank pixels.
        for (int shrankX = shrankXLeft; shrankX <= shrankXRight; ++shrankX) {
          float weight = 9.0f / 100.0f; // the weight when this cell *fully* contributes
          if (shrankX == shrankXLeft) {
            if (x % 10 != 3) weight /= 3.0f;
            else if (x % 10 != 6) weight /= 1.5f;
          } else {
            if (x % 10 != 3) weight /= 1.5f;
            else if (x % 10 != 6) weight /= 3.0f;
            else continue;
          }
          for (int shrankY = shrankYTop; shrankY <= shrankYBottom; ++shrankY) {
            if (shrankY == shrankYTop) {
              if (y % 10 != 3) weight /= 3.0f;
              else if (y % 10 != 6) weight /= 1.5f;
            } else {
              if (y % 10 != 3) weight /= 1.5f;
              else if (y % 10 != 6) weight /= 3.0f;
              else continue;
            }
            ShrunkPixel *thePixel = shrunkPixels + (shrankY * shrankXSize + shrankX);
            thePixel->pixAvg += weight * pix;
            thePixel->varAvg += weight * var;
            // other values are ANDed
            if (thePixel->used == 0) {
              thePixel->valid_and = valid;
              thePixel->used = 1;
            } else {
              thePixel->valid_and &= valid;
            }
          }
        }
      }
      ::mysql_free_result (result);
    }
    watchCook.stop();
    LOG4CXX_INFO(_logger, "shrank " << image.tablename << ". " << watchCook.getElapsed() << " microsec");
    cookElapsed += watchCook.getElapsed();

    StopWatch watchInsert;
    watchInsert.init();
    outputToMysqlQ3Bulk (image, shrunkPixels, shrankXSize, shrankYSize);
    watchInsert.stop();
    LOG4CXX_INFO(_logger, "written shrunk " << image.tablename << ". " << watchInsert.getElapsed() << " microsec");
    insertElapsed += watchInsert.getElapsed();

    delete[] shrunkPixels;
  }
  watch.stop();
  LOG4CXX_INFO(_logger, "time for cooking: " << cookElapsed << " microsec, insertion: " << insertElapsed << " microsec");
  LOG4CXX_INFO(_logger, "Ran Q3: " << watch.getElapsed() << " microsec. ");
}
void Benchmark::createTempTableQ3(const Image &image) {
  MYSQL *mysql = _connections.getMainMysql();
  // write resulted image to local file
  string temptablename = "temp_shrunk_" + image.tablename;
  dropTableIfExists (mysql, temptablename, _logger);
  std::stringstream createsql;
  createsql << "CREATE TABLE " << temptablename << "("
    << " x INT NOT NULL,"
    << " y INT NOT NULL,"
    << " val_and INT NOT NULL,"
    << " pixavg REAL NOT NULL,"
    << " varavg REAL NOT NULL"
    << " ) ENGINE=InnoDB";
  execUpdateSQL (mysql, createsql.str(), _logger);
}
void Benchmark::outputToMysqlQ3Bulk(const Image &image, ShrunkPixel *shrunkPixels, int32_t shrankXSize, int32_t shrankYSize) {
  createTempTableQ3(image);
  LOG4CXX_INFO(_logger, "inserting Q3 results back to mysql in bulk...");
  CsvBuffer buffer (_logger, "temp_shrunk_" + image.tablename, 4 << 20);

  for (int32_t y = 0; y < shrankYSize; ++y) {
    for (int32_t x = 0; x < shrankXSize; ++x) {
      ShrunkPixel *thePixel = shrunkPixels + (y * shrankXSize + x);
      buffer.written(::sprintf (buffer.curbuf(), "%d,%d,%d,%f,%f\n",
          x, y, thePixel->valid_and, thePixel->pixAvg, thePixel->varAvg));
    }
  }
  buffer.close();
  buffer.loadToMysql(_connections.getMainMysql());
}
void Benchmark::runQ4 (int32_t cycle, const std::string &attribute, int xstart/*X4*/, int xlen/*U4*/, int ystart/*Y4*/, int ylen/*V4*/) {
  LOG4CXX_INFO(_logger, "Running Q4...");
  StopWatch watch;
  watch.init();
  stringstream sql;
  sql << "SELECT AVG(" << attribute << "),COUNT(*) FROM observations WHERE"
      << " cycle=" << cycle << " AND Contains(GeomFromText('Polygon(("
      << xstart << " " << ystart << ","
      << (xstart + xlen) << " " << ystart << ","
      << (xstart + xlen) << " " << (ystart + ylen) << ","
      << xstart << " " << (ystart + ylen) << ","
      << xstart << " " << ystart
      << "))'), center)";
  LOG4CXX_INFO(_logger, "Q4 query=: " << sql.str());
  MYSQL_RES *result = execSelectSQL(_connections.getMainMysql(), sql.str(), _logger);
  double globalAvg = 0.0;
  int count = 0;
  MYSQL_ROW row = ::mysql_fetch_row (result);
  if (row != NULL && row[0] != NULL) {
    globalAvg = ::atof(row[0]);
    count = ::atol(row[1]);
  }
  ::mysql_free_result (result);

  watch.stop();
  LOG4CXX_INFO(_logger, "Ran Q4: " << watch.getElapsed() << " microsec. result = " << globalAvg << ", count=" << count);
}

void Benchmark::runQ5 (int32_t cycle, const std::string &attribute, int xstart/*X5*/, int xlen/*U5*/, int ystart/*Y5*/, int ylen/*V5*/) {
  LOG4CXX_INFO(_logger, "Running Q5...");
  StopWatch watch;
  watch.init();
  stringstream sql;
  sql << "SELECT AVG(" << attribute << "),COUNT(*) FROM observations WHERE"
      << " cycle=" << cycle << " AND Intersects(GeomFromText('Polygon(("
      << xstart << " " << ystart << ","
      << (xstart + xlen) << " " << ystart << ","
      << (xstart + xlen) << " " << (ystart + ylen) << ","
      << xstart << " " << (ystart + ylen) << ","
      << xstart << " " << ystart
      << "))'), bbox)";
  LOG4CXX_INFO(_logger, "Q5 query=: " << sql.str());
  MYSQL_RES *result = execSelectSQL(_connections.getMainMysql(), sql.str(), _logger);
  double globalAvg = 0.0;
  int count = 0;
  MYSQL_ROW row = ::mysql_fetch_row (result);
  if (row != NULL && row[0] != NULL) {
    globalAvg = ::atof(row[0]);
    count = ::atol(row[1]);
  }
  ::mysql_free_result (result);

  watch.stop();
  LOG4CXX_INFO(_logger, "Ran Q5: " << watch.getElapsed() << " microsec. result = " << globalAvg << ", count=" << count);
}
void Benchmark::runQ6 (int32_t cycle, int boxSize/*D6*/, int threshold/*X*/) {
  LOG4CXX_INFO(_logger, "Running Q6...");
  StopWatch watch;
  watch.init();
  // current implementation aggregates per "box", boxSize x boxSize, not per pixel.
  // not sure Q6 requests it. (most likely not)
  stringstream sql;
  sql << "SELECT FLOOR(centerx/" << boxSize << "), FLOOR(centery/" << boxSize << "), COUNT(*) FROM observations"
      << " WHERE cycle=" << cycle
      << " GROUP BY FLOOR(centerx/" << boxSize << "), FLOOR(centery/" << boxSize << ") HAVING COUNT(*)>=" << threshold;
  MYSQL_RES *result = execSelectSQL(_connections.getMainMysql(), sql.str(), _logger);
  vector<Q6Result> results;
  for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
    Q6Result result;
    result.dx = ::atoi(row[0]);
    result.dy = ::atoi(row[1]);
    result.counts = ::atoi(row[2]);
    results.push_back(result);
    // you, too noisy! LOG4CXX_DEBUG(_logger, "Result: (" << (result.dx * boxSize) << "," << (result.dy * boxSize) << ") in " << boxSize <<" box. count=" << result.counts);
  }
  ::mysql_free_result (result);

  StopWatch watchInsert;
  watchInsert.init();
  outputToMysqlQ6Bulk (results);
  watchInsert.stop();

  watch.stop();
  LOG4CXX_INFO(_logger, "Ran Q6: " << watch.getElapsed() << " microsec (" << watchInsert.getElapsed() << " microsec for insertion). result counts = " << results.size());
}
// another version based on centers, not just boxes
void Benchmark::runQ6New (int32_t cycle, int boxSize/*D6*/, int threshold/*X*/, int xstart/*X6*/, int xlen/*U6*/, int ystart/*Y6*/, int ylen/*V6*/) {
  LOG4CXX_INFO(_logger, "Running Q6New...");
  StopWatch watch;
  watch.init();

  // first, retrieve all observations
  StopWatch watchScan;
  watchScan.init();
  stringstream sql;
  sql << "SELECT centerx,centery FROM observations WHERE cycle=" << cycle
      << " AND Contains(GeomFromText('Polygon(("
      << xstart << " " << ystart << ","
      << (xstart + xlen) << " " << ystart << ","
      << (xstart + xlen) << " " << (ystart + ylen) << ","
      << xstart << " " << (ystart + ylen) << ","
      << xstart << " " << ystart
      << "))'), center)";
  MYSQL_RES *result = execSelectSQL(_connections.getMainMysql(), sql.str(), _logger);
  vector<std::pair<int, int> > centers;
  centers.reserve (1 << 14);
  for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
    centers.push_back (std::pair<int, int> (::atol(row[0]), ::atol(row[1])));
  }
  ::mysql_free_result (result);
  watchScan.stop();
  LOG4CXX_INFO(_logger, "Observations: " << centers.size() );

  // next, cluster them to count density
  StopWatch watchCluster;
  watchCluster.init();
  int regionSize = boxSize + 1; // make sure matches could be found in +-1 region
  map <std::pair<int, int>, vector<int> > regions; // map< Region, entries(indexes of centers) >
  for (size_t i = 0; i < centers.size(); ++i) {
    const std::pair<int, int> &center = centers[i];
    std::pair<int, int> key (center.first / regionSize, center.second / regionSize);
    if (regions.find (key) == regions.end ()) {
      regions[key] = vector<int> ();
    }
    regions[key].push_back (i);
  }

  // finally, check density and answer centers that have enough density
  vector<Q6Result> results;
  for (size_t i = 0; i < centers.size(); ++i) {
    const std::pair<int, int> &center = centers[i];
    int regionX = center.first / regionSize;
    int regionY = center.second / regionSize;
    int count = 0;
    for (int x = regionX -1; x <= regionX + 1; ++x) {
      for (int y = regionY -1; y <= regionY + 1; ++y) {
        std::pair<int, int> key (x, y);
        std::map <std::pair<int, int>, std::vector<int> >::const_iterator it = regions.find (key);
        if (it == regions.end ()) continue;
        const std::vector<int> &entries = it->second;
        for (size_t j = 0; j < entries.size(); ++j) {
          const std::pair<int, int> &candidate = centers[entries[j]];
          if (std::abs(candidate.first - center.first) <= boxSize / 2
            && std::abs(candidate.second - center.second) <= boxSize / 2) {
            ++count;
          }
        }
      }
    }
    if (count >= threshold) {
      Q6Result result;
      result.dx = center.first;
      result.dy = center.second;
      result.counts = count;
      results.push_back (result);
    }
  }
  watchCluster.stop();

  StopWatch watchInsert;
  watchInsert.init();
  outputToMysqlQ6Bulk (results);
  watchInsert.stop();

  watch.stop();
  LOG4CXX_INFO(_logger, "Ran Q6New: " << watch.getElapsed() << " microsec (" << watchScan.getElapsed() << " microsec for scanning, " << watchCluster.getElapsed() << " microsec for generating answer, " << watchInsert.getElapsed() << " microsec for insertion). result counts = " << results.size());
}
void Benchmark::createTempTableQ6() {
  MYSQL *mysql = _connections.getMainMysql();
  dropTableIfExists (mysql, "temp_q6", _logger);
  execUpdateSQL (mysql, "CREATE TABLE temp_q6(dx INT NOT NULL,dy INT NOT NULL,counts INT NOT NULL) ENGINE=InnoDB", _logger);
}
void Benchmark::outputToMysqlQ6Bulk(const vector<Q6Result> &results) {
  createTempTableQ6();
  LOG4CXX_INFO(_logger, "inserting Q6 results back to mysql in bulk...");
  CsvBuffer buffer (_logger, "temp_q6", 4 << 20);
  for (size_t i = 0; i < results.size(); ++i) {
    const Q6Result &result = results[i];
    buffer.written(::sprintf (buffer.curbuf(), "%d,%d,%d\n",
        result.dx, result.dy, result.counts));
  }
  buffer.close();
  buffer.loadToMysql(_connections.getMainMysql());
}
void Benchmark::runQ7 (int xstart/*X7*/, int xlen/*U7*/, int ystart/*Y7*/, int ylen/*V7*/) {
  LOG4CXX_INFO(_logger, "Running Q7...");
  StopWatch watch;
  watch.init();
  stringstream sql;
  sql << "SELECT obsgroupid FROM obsgroup"
      << " WHERE Contains(GeomFromText('Polygon(("
      << xstart << " " << ystart << ","
      << (xstart + xlen) << " " << ystart << ","
      << (xstart + xlen) << " " << (ystart + ylen) << ","
      << xstart << " " << (ystart + ylen) << ","
      << xstart << " " << ystart
      << "))'), center_traj)";
  MYSQL_RES *result = execSelectSQL(_connections.getMainMysql(), sql.str(), _logger);
  int totalTuples = 0;
  for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
    ++totalTuples;
  }
  ::mysql_free_result (result);

  stringstream sqlObs;
  sqlObs << "SELECT obsid FROM observations"
      << " WHERE Contains(GeomFromText('Polygon(("
      << xstart << " " << ystart << ","
      << (xstart + xlen) << " " << ystart << ","
      << (xstart + xlen) << " " << (ystart + ylen) << ","
      << xstart << " " << (ystart + ylen) << ","
      << xstart << " " << ystart
      << "))'), center)";
  MYSQL_RES *result2 = execSelectSQL(_connections.getMainMysql(), sqlObs.str(), _logger);
  for (MYSQL_ROW row = ::mysql_fetch_row (result2); row != NULL; row = ::mysql_fetch_row (result2)) {
    ++totalTuples;
  }
  ::mysql_free_result (result2);

  watch.stop();
  LOG4CXX_INFO(_logger, "Ran Q7: " << watch.getElapsed() << " microsec. result counts = " << totalTuples);
}
// find images that intersect with this slab. should be few.
std::vector<Image> getIntersectingImages (MYSQL *mysql,
    int xstart, int xlen, int ystart, int ylen) {
  ImageTable imageTable;
  std::vector<Image> images = imageTable.getAllImages(mysql);
  std::vector<Image> intersectingImages;
  for (size_t i = 0; i < images.size(); ++i) {
    const Image &image = images[i];
    assert ((int) i == image.imageid);
    if (image.xend < xstart || xstart + xlen < image.xstart) continue;
    if (image.yend < ystart || ystart + ylen < image.ystart) continue;
    intersectingImages.push_back(image);
  }
  return intersectingImages;
}
void Benchmark::runQ8 (int xstart/*X8*/, int xlen/*U8*/, int ystart/*Y8*/, int ylen/*V8*/, int dist /*D8*/, bool distributed) {
  stringstream sql;
  sql << "SELECT obsgroupid FROM obsgroup"
      << " WHERE Contains(GeomFromText('Polygon(("
      << xstart << " " << ystart << ","
      << (xstart + xlen) << " " << ystart << ","
      << (xstart + xlen) << " " << (ystart + ylen) << ","
      << xstart << " " << (ystart + ylen) << ","
      << xstart << " " << ystart
      << "))'), center_traj)";
  stringstream sqlObs;
  sqlObs << "SELECT obsid FROM observations"
      << " WHERE Contains(GeomFromText('Polygon(("
      << xstart << " " << ystart << ","
      << (xstart + xlen) << " " << ystart << ","
      << (xstart + xlen) << " " << (ystart + ylen) << ","
      << xstart << " " << (ystart + ylen) << ","
      << xstart << " " << ystart
      << "))'), center)";
  _runQ89 ("Q8", sql.str(), sqlObs.str(), xstart, xlen, ystart, ylen, dist, distributed);
}

void Benchmark::runQ9 (int t, int xstart/*X9*/, int xlen/*U9*/, int ystart/*Y9*/, int ylen/*V9*/, int dist /*D9*/, bool distributed) {
  stringstream sql;
  sql << "SELECT obsgroupid FROM polygon_trajectory"
      << " WHERE fromtime<=" << t << " AND totime>=" << t <<" AND Intersects(GeomFromText('Polygon(("
      << xstart << " " << ystart << ","
      << (xstart + xlen) << " " << ystart << ","
      << (xstart + xlen) << " " << (ystart + ylen) << ","
      << xstart << " " << (ystart + ylen) << ","
      << xstart << " " << ystart
      << "))'), traj)";
  stringstream sqlObs;
  sqlObs << "SELECT obsid FROM observations"
      << " WHERE time=" << t << " AND Contains(GeomFromText('Polygon(("
      << xstart << " " << ystart << ","
      << (xstart + xlen) << " " << ystart << ","
      << (xstart + xlen) << " " << (ystart + ylen) << ","
      << xstart << " " << (ystart + ylen) << ","
      << xstart << " " << ystart
      << "))'), bbox)";
  _runQ89 ("Q9", sql.str(), sqlObs.str(), xstart, xlen, ystart, ylen, dist, distributed);
}

// boost thread's constructor doesn't allow too many parameters. so just pass this struct
struct PixelQ89ParallelParam {
  PixelQ89ParallelParam (
    size_t nodes,
    size_t nodeId,
    MYSQL *mysql,
    int dist,
    const std::vector<Image> &images,
    const std::vector<int> &imageStartX,
    const std::vector<int> &imageStartY,
    const std::vector<int> &imageEndX,
    const std::vector<int> &imageEndY,
    const std::vector<std::set<int> > &imageTiles,
    const std::vector<std::vector<size_t> > &imageCenters,
    const std::vector<int> &centerObsIds,
    const std::vector<int> &centerxs,
    const std::vector<int> &centerys) :
    _nodes (nodes), _nodeId(nodeId), _mysql(mysql), _dist(dist),
    _images (images), _imageStartX (imageStartX), _imageStartY (imageStartY),
    _imageEndX (imageEndX), _imageEndY(imageEndY),
    _imageTiles (imageTiles), _imageCenters(imageCenters),
    _centerObsIds (centerObsIds), _centerxs(centerxs), _centerys(centerys)
  {
  }
  size_t _nodes;
  size_t _nodeId;
  MYSQL *_mysql;
  int _dist;
  std::vector<Image> _images;
  std::vector<int> _imageStartX;
  std::vector<int> _imageStartY;
  std::vector<int> _imageEndX;
  std::vector<int> _imageEndY;
  std::vector<std::set<int> > _imageTiles;
  std::vector<std::vector<size_t> > _imageCenters;
  std::vector<int> _centerObsIds;
  std::vector<int> _centerxs;
  std::vector<int> _centerys;
};

void _pixelQ89Parallel (const PixelQ89ParallelParam &param, const MysqlHost &host) {
  stringstream nodeIdStr;
  nodeIdStr << param._nodeId;
  LoggerPtr logger(Logger::getLogger("Thread-" + nodeIdStr.str()));
  LOG4CXX_INFO(logger, "Started. nodeid=" << param._nodeId << " on " << host.host);
  StopWatch watchLocal;
  watchLocal.init();
  dropTableIfExists (param._mysql, "temp_result_q89", logger);
  execUpdateSQL (param._mysql, "CREATE TABLE temp_result_q89(obsid INT NOT NULL,x INT NOT NULL,y INT NOT NULL,pix INT NOT NULL) ENGINE=MyISAM", logger);
  // randomly change file name to make it unique

  int randomnum;
  {
    timeval t;
    ::gettimeofday(&t, NULL);
    int64_t wallClock = t.tv_sec * 1000000 + t.tv_usec;
    randomnum = (1103515245 * wallClock + 12345) & 0x3fffffff;
  }
  stringstream csvpath;
  csvpath << "/data/federated_mysql/csvs/temp_result_q89_" << param._nodeId << "_" << randomnum << ".csv";
  CsvBuffer resultCsv (logger, "temp_result_q89", 1 << 20, csvpath.str());
  int64_t writtenPixels = 0;
  size_t imageCnt = param._images.size();
  for (size_t i = 0; i < imageCnt; ++i) {
    const Image &image = param._images[i];
    if (image.imageid % param._nodes != param._nodeId) continue; // round robin distribution
    int width = image.getWidth();
    int height = image.getHeight();
    const set<int> &tiles = param._imageTiles[i];
    if (tiles.size() == 0) continue;
    const vector<size_t> &centers = param._imageCenters[i];
    LOG4CXX_INFO(logger, "image_" << image.imageid << "(x=" << image.xstart <<"-,y=" << image.ystart << "-). tiles to get:" << tiles.size() << ", centers in this image:" << centers.size());
    ImagePixels pixels = image.getPixels(param._mysql, logger, tiles);
    for (size_t j = 0; j < centers.size(); ++j) {
      size_t centerIndex = centers[j];
      int obsid = param._centerObsIds[centerIndex];
      int centerx = param._centerxs[centerIndex];
      int centery = param._centerys[centerIndex];
      assert (param._imageEndX[i] >= centerx && centerx >= param._imageStartX[i]);
      assert (param._imageEndY[i] >= centery && centery >= param._imageStartY[i]);
      // output pixels around this center.
      int endx = std::min(width, centerx - image.xstart + param._dist);
      int endy = std::min(height, centery - image.ystart + param._dist);
      for (int x = std::max(0, centerx - image.xstart - param._dist); x < endx; ++x) {
        for (int y = std::max(0, centery - image.ystart - param._dist); y < endy; ++y) {
          resultCsv.written(::sprintf (resultCsv.curbuf(), "%d,%d,%d,%d\n", obsid, x + image.xstart, y + image.ystart, pixels.getPixel(x,y)));
          ++writtenPixels;
        }
      }
    }
  }
  LOG4CXX_INFO(logger, "writtenPixels=" << writtenPixels);
  resultCsv.close ();
  stringstream loadsql;
  loadsql << "LOAD DATA LOCAL INFILE '" << csvpath.str() << "' INTO TABLE temp_result_q89 FIELDS TERMINATED BY ',' LINES TERMINATED BY '\\n'";
  execUpdateSQL (param._mysql, loadsql.str(), logger);
  watchLocal.stop();

  LOG4CXX_INFO(logger, "Thread Ended. elapsed=" << watchLocal.getElapsed() << " microsec");
}

void Benchmark::_runQ89 (const std::string &queryName, const std::string &groupSql, const std::string &obsSql, int xstart, int xlen, int ystart, int ylen, int dist, bool distributed) {
  LOG4CXX_INFO(_logger, "Running " << queryName << " (" << (distributed ? "distributed" : "standalone") << ")...");
  StopWatch watch;
  watch.init();

  // get list of groups that intersects with the slab
  // mysql is stupid. even FORCE INDEX doesn't use the primary key index when it's sub query
  // and the key is a part of the composite key. However, IN clause makes it use the index.
  // so, here we use a huge IN clause instead of temporary table.
  set<int> groupIds = execSelectIntSetSQL(_connections.getMainMysql(), groupSql, -1, _logger);
  set<int> obsIds;
  if (groupIds.size () > 0) {
    // get list of observation ids
    stringstream sql2;
    sql2 << "SELECT obsid FROM obsgroup_obs WHERE obsgroupid IN";
    appendInClause(sql2, groupIds);
    obsIds = execSelectIntSetSQL(_connections.getMainMysql(), sql2.str(), -1, _logger);
    LOG4CXX_INFO(_logger, "Observations from groups:" << obsIds.size());
  }
  // also mix the results from observation table (for singleton groups)
  set<int> singletonObsIds = execSelectIntSetSQL(_connections.getMainMysql(), obsSql, -1, _logger);
  LOG4CXX_INFO(_logger, "Observations from singletons:" << singletonObsIds.size());
  obsIds.insert(singletonObsIds.begin(), singletonObsIds.end());

  if (obsIds.size () == 0) {
    LOG4CXX_INFO(_logger, "No observation/group matched.");
    return;
  }

  // collect tiles to get for each image
  std::vector<Image> images = getIntersectingImages(_connections.getMainMysql(), xstart, xlen, ystart, ylen);
  size_t imageCnt = images.size ();
  std::vector<std::set<int> > imageTiles; // tiles to get for each intersecting image
  std::vector<std::vector<size_t> > imageCenters; // indexes of centers (observations) to get for each intersecting image
  std::vector<int> imageStartX, imageStartY, imageEndX, imageEndY;
  for (size_t i = 0; i < imageCnt; ++i) {
    imageTiles.push_back (std::set<int>());
    imageCenters.push_back (std::vector<size_t>());
    const Image &image = images[i];
    // "margin" for checking required pixels
    imageStartX.push_back (image.xstart - dist);
    imageStartY.push_back (image.ystart - dist);
    imageEndX.push_back (image.xend + dist);
    imageEndY.push_back (image.yend + dist);
  }
  assert (imageTiles.size() == imageCnt);
  assert (imageStartX.size() == imageCnt);
  assert (imageStartY.size() == imageCnt);
  assert (imageEndX.size() == imageCnt);
  assert (imageEndY.size() == imageCnt);
  LOG4CXX_INFO(_logger, "" << imageCnt << " intersecting images");

  // get centers of them
  stringstream sql3;
  sql3 << "SELECT obsid,centerx,centery FROM observations WHERE obsid IN";
  appendInClause(sql3, obsIds);
  MYSQL_RES *result = execSelectSQL(_connections.getMainMysql(), sql3.str(), _logger);
  std::vector<int> centerObsIds, centerxs, centerys;
  size_t cnt = 0, possibleCnt = 0;
  for (MYSQL_ROW row = ::mysql_fetch_row (result); row != NULL; row = ::mysql_fetch_row (result)) {
    int obsid = ::atol (row[0]);
    int centerx = ::atol (row[1]);
    int centery = ::atol (row[2]);
    centerObsIds.push_back(obsid);
    centerxs.push_back(centerx);
    centerys.push_back(centery);
    for (size_t i = 0; i < imageCnt; ++i) {
      if (imageEndX[i] < centerx || centerx < imageStartX[i]) continue;
      if (imageEndY[i] < centery || centery < imageStartY[i]) continue;
      ++possibleCnt;
      // okay, this observation is in this image. add tiles to get
      const Image &image = images[i];
      set<int> &tiles = imageTiles[i];
      vector<size_t> &centers = imageCenters[i];
      bool matched = false;
      for (int tilex = (centerx - image.xstart - dist) / TILE_SIZE; tilex <= (centerx - image.xstart + dist) / TILE_SIZE; ++tilex) {
        if (tilex < 0 || tilex > (image.xend - image.xstart) / TILE_SIZE) continue;
        for (int tiley = (centery - image.ystart - dist) / TILE_SIZE; tiley <= (centery - image.ystart + dist) / TILE_SIZE; ++tiley) {
          if (tiley < 0 || tiley > (image.yend - image.ystart) / TILE_SIZE) continue;
          int tile = (tiley * TILE_X_Y_RATIO) + tilex;
          matched = true;
          if (tiles.find (tile) == tiles.end()) tiles.insert (tile);
        }
      }
      if (matched) {
        centers.push_back (cnt);
      }
    }
    ++cnt;
  }
  ::mysql_free_result (result);
  LOG4CXX_INFO(_logger, "cnt=" << cnt << ",possibleCnt=" << possibleCnt);

  // output raw image around centers
  // we avoid accessing raw images over and over again by getting all pixels
  // from each image and then output related pixels for each observations.
  // also, this part is parallelized as images are distributed in round robin
  StopWatch watchInsert;
  watchInsert.init();
  if (distributed) {
    size_t nodes = _connections.getSubMysqlCount();
    LOG4CXX_INFO(_logger, "parallelizing pixel retrieval on " << nodes << " nodes...");

    boost::thread_group threadGroup;
    for (size_t i = 0; i < nodes; ++i) {
      boost::thread *threadPtr = new boost::thread(_pixelQ89Parallel, PixelQ89ParallelParam(nodes, i, _connections.getSubMysql(i), dist, images, imageStartX, imageStartY, imageEndX, imageEndY, imageTiles, imageCenters, centerObsIds, centerxs, centerys), _configs.getConnectionConfig().getSub()[i]);
      threadGroup.add_thread(threadPtr);
    }
    threadGroup.join_all();
  } else {
    dropTableIfExists (_connections.getMainMysql(), "temp_result_q89", _logger);
    execUpdateSQL (_connections.getMainMysql(), "CREATE TABLE temp_result_q89(obsid INT NOT NULL,x INT NOT NULL,y INT NOT NULL,pix INT NOT NULL) ENGINE=MyISAM", _logger);
    CsvBuffer resultCsv (_logger, "temp_result_q89", 1 << 22);
    int64_t writtenPixels = 0;
    for (size_t i = 0; i < imageCnt; ++i) {
      const Image& image = images[i];
      int width = image.getWidth();
      int height = image.getHeight();
      const set<int> &tiles = imageTiles[i];
      const vector<size_t> &centers = imageCenters[i];
      LOG4CXX_INFO(_logger, "image_" << image.imageid << "(x=" << image.xstart <<"-,y=" << image.ystart << "-). tiles to get:" << tiles.size() << ", centers in this image:" << centers.size());
      if (tiles.size() == 0) continue;
      ImagePixels pixels = image.getPixels(_connections.getMainMysql(), _logger, tiles);
      for (size_t j = 0; j < centers.size(); ++j) {
        size_t centerIndex = centers[j];
        int obsid = centerObsIds[centerIndex];
        int centerx = centerxs[centerIndex];
        int centery = centerys[centerIndex];
        assert (imageEndX[i] >= centerx && centerx >= imageStartX[i]);
        assert (imageEndY[i] >= centery && centery >= imageStartY[i]);
        // output pixels around this center.
        int endx = std::min(width, centerx - image.xstart + dist);
        int endy = std::min(height, centery - image.ystart + dist);
        for (int x = std::max(0, centerx - image.xstart - dist); x < endx; ++x) {
          for (int y = std::max(0, centery - image.ystart - dist); y < endy; ++y) {
            resultCsv.written(::sprintf (resultCsv.curbuf(), "%d,%d,%d,%d\n", obsid, x + image.xstart, y + image.ystart, pixels.getPixel(x,y)));
            ++writtenPixels;
          }
        }
      }
    }
    LOG4CXX_INFO(_logger, "writtenPixels=" << writtenPixels);
    resultCsv.close ();
    resultCsv.loadToMysql(_connections.getMainMysql());
  }
  watchInsert.stop();

  watch.stop();
  LOG4CXX_INFO(_logger, "Ran " << queryName << ": " << watch.getElapsed() << " microsec (" << watchInsert.getElapsed() << " microsec for insert).");
}
