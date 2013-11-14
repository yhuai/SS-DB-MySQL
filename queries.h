// Classes to implement the benchmark queries.

#ifndef QUERIES_H
#define QUERIES_H

#include "configs.h"
#include <log4cxx/logger.h>
using namespace log4cxx;
#include "mysqlwrapper.h"

#include <utility>
#include <vector>

struct Q6Result;
struct ShrunkPixel;
class Benchmark {
public:
  Benchmark (const Configs &configs) : _configs(configs), _logger(Logger::getLogger("Benchmark")), _connections (_configs) {};
  void runQ1 (int32_t cycle, const std::string &attribute,
    int xstart/*X1*/, int xlen/*U1*/, int ystart/*Y1*/, int ylen/*V1*/);
  void runQ1All (int32_t cycle, const std::string &attribute);
  void runQ1AllParallel (int32_t cycle, const std::string &attribute);
  void runQ3 (int32_t cycle, bool all, size_t nodes, size_t nodeId, int xstart/*X3*/, int xlen/*U3*/, int ystart/*Y3*/, int ylen/*V3*/);
  void runQ4 (int32_t cycle, const std::string &attribute, int xstart/*X4*/, int xlen/*U4*/, int ystart/*Y4*/, int ylen/*V4*/);
  void runQ5 (int32_t cycle, const std::string &attribute, int xstart/*X5*/, int xlen/*U5*/, int ystart/*Y5*/, int ylen/*V5*/);
  void runQ6 (int32_t cycle, int boxSize/*D6*/, int threshold/*X*/);
  void runQ6New (int32_t cycle, int boxSize/*D6*/, int threshold/*X*/, int xstart/*X6*/, int xlen/*U6*/, int ystart/*Y6*/, int ylen/*V6*/);
  void runQ7 (int xstart/*X7*/, int xlen/*U7*/, int ystart/*Y7*/, int ylen/*V7*/);
  void runQ8 (int xstart/*X8*/, int xlen/*U8*/, int ystart/*Y8*/, int ylen/*V8*/, int dist /*D8*/, bool distributed);
  void runQ9 (int t, int xstart/*X9*/, int xlen/*U9*/, int ystart/*Y9*/, int ylen/*V9*/, int dist /*D9*/, bool distributed);

private:
  void composeStripes (std::vector <std::pair<int32_t, int32_t> > &stripes /*out*/,
    int xstart/*X1*/, int xlen/*U1*/, int ystart/*Y1*/, int ylen/*V1*/);

  void createTempTableQ3(const Image &image);
  void outputToMysqlQ3Bulk(const Image &image, ShrunkPixel *shrunkPixels, int32_t shrankXSize, int32_t shrankYSize);

  void createTempTableQ6();
  void outputToMysqlQ6Bulk(const std::vector<Q6Result> &results);

  void _runQ89 (const std::string &queryName, const std::string &groupSql, const std::string &obsSql, int xstart, int xlen, int ystart, int ylen, int dist, bool distributed);

  const Configs &_configs;
  LoggerPtr _logger;
  MysqlConnections _connections;
}; 


#define Q3_X_MAX 2048
#define Q3_Y_MAX 4612

// one pixel in the shrunk image
// note that this should be properly initialized with memset (0)
struct ShrunkPixel {
  int16_t used; // used to check first or not (to take AND)
  float pixAvg; // weighted average
  float varAvg; // weighted average
  int16_t valid_and; // initially 0. take the first value, and then apply AND to later values.
//  int16_t v_and[7]; // initially 0. take the first value, and then apply AND to later values.
};

struct Q6Result {
  int32_t dx, dy, counts;
};

#endif // QUERIES_H
