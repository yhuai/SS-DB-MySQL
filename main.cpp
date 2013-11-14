
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <cstdio>
#include <cstdlib>
#include <iostream>


#include <errno.h>
#include <mysql.h>

#include <log4cxx/logger.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/helpers/exception.h>

#include "configs.h"
#include "loader.h"
#include "queries.h"
#include "dummydata.h"

using namespace std;
using namespace log4cxx;
using namespace log4cxx::helpers;

int main(int argc, char *argv[])
{
  try {
    PropertyConfigurator::configure("log4cxx.properties");
  } catch(log4cxx::helpers::Exception &e) {
    cerr << "failed to initialize log4cxx in main(): " << e.what();
    return EXIT_FAILURE;
  }

  LoggerPtr logger(Logger::getLogger("main"));
  try {
    LOG4CXX_INFO(logger, "started.");

    // load configuration files. we will pass around the configurations
    Configs configs;
    configs.readConfigFiles();

    if (argc <= 1) {
      LOG4CXX_ERROR(logger, "usage: scidb_rdb <command>");
      return EXIT_FAILURE;
    }

    int mysqlInitRet = ::mysql_library_init (0, NULL, NULL);
    if (mysqlInitRet != 0) {
      LOG4CXX_ERROR(logger, "falied to init mysql library: " << errno);
      return EXIT_FAILURE;
    }
    LOG4CXX_INFO(logger, "initialized mysql library.");

    if (std::string("load") == argv[1]) {
      int threshold;
      if (argc >= 3) {
        threshold = ::atol(argv[2]);
      } else {
        // now parameters are required
        LOG4CXX_ERROR(logger, "usage: scidb_rdb load <threshold>");
        return EXIT_FAILURE;
      }
      Loader loader(configs);
      loader.load(threshold);
    } else if (std::string("loadParallelFinish") == argv[1]) {
      if (argc < 3) {
        LOG4CXX_ERROR(logger, "usage: scidb_rdb loadParallelFinish <nodes>");
        return EXIT_FAILURE;
      }
      Loader loader(configs);
      loader.loadParallelFinish(::atol(argv[2]));
    } else if (std::string("loadParallel") == argv[1]) {
      if (argc < 4) {
        LOG4CXX_ERROR(logger, "usage: scidb_rdb loadParallel <threshold> <startingObservationId>");
        return EXIT_FAILURE;
      }
      Loader loader(configs);
      loader.loadParallel(::atol(argv[2]), ::atol(argv[3]));
    } else if (std::string("loadGroup") == argv[1]) {
      Loader loader(configs);
      if (argc < 4) {
        LOG4CXX_ERROR(logger, "usage: scidb_rdb loadGroup <D2> <T>");
        return EXIT_FAILURE;
      }
      loader.loadGroup(::atof (argv[2]), ::atol (argv[3]));
    } else if (std::string("q1") == argv[1]) {
      if (argc < 7) {
        LOG4CXX_ERROR(logger, "usage: scidb_rdb q1 <cycle> <xstart> <xlen> <ystart> <ylen>");
        LOG4CXX_ERROR(logger, "ex: scidb_rdb q1 2 0 1000 0 1000");
        return EXIT_FAILURE;
      }
      Benchmark benchmark (configs);
      benchmark.runQ1(::atol(argv[2]), "pix", ::atol(argv[3]), ::atol(argv[4]), ::atol(argv[5]), ::atol(argv[6]));
    } else if (std::string("q1all") == argv[1]) {
      if (argc < 3) {
        LOG4CXX_ERROR(logger, "usage: scidb_rdb q1all <cycle>");
        LOG4CXX_ERROR(logger, "ex: scidb_rdb q1all 2");
        return EXIT_FAILURE;
      }
      Benchmark benchmark (configs);
      benchmark.runQ1All(::atol(argv[2]), "pix");
    } else if (std::string("q1allparallel") == argv[1]) {
      if (argc < 3) {
        LOG4CXX_ERROR(logger, "usage: scidb_rdb q1allparallel <cycle>");
        LOG4CXX_ERROR(logger, "ex: scidb_rdb q1allparallel 2");
        return EXIT_FAILURE;
      }
      Benchmark benchmark (configs);
      benchmark.runQ1AllParallel(::atol(argv[2]), "pix");
    } else if (std::string("q2") == argv[1]) {
      if (argc < 4) {
        LOG4CXX_ERROR(logger, "usage: scidb_rdb q2 <threshold> <imageid>");
        return EXIT_FAILURE;
      }
      Loader loader(configs);
      loader.loadOneImage(::atol(argv[2]), ::atol(argv[3]));
    } else if (std::string("q3") == argv[1]) {
      if (argc < 7) {
        LOG4CXX_ERROR(logger, "usage: scidb_rdb q3 <cycle> <xstart> <xlen> <ystart> <ylen>");
        LOG4CXX_ERROR(logger, "ex: scidb_rdb q3 2 0 1000 0 1000");
        return EXIT_FAILURE;
      }
      Benchmark benchmark (configs);
      benchmark.runQ3 (::atol(argv[2]), false, 0, 0, ::atol(argv[3]), ::atol(argv[4]), ::atol(argv[5]), ::atol(argv[6]));
    } else if (std::string("q3all") == argv[1]) {
      if (argc < 3) {
        LOG4CXX_ERROR(logger, "usage: scidb_rdb q3all <cycle>");
        LOG4CXX_ERROR(logger, "ex: scidb_rdb q3all 2");
        return EXIT_FAILURE;
      }
      Benchmark benchmark (configs);
      benchmark.runQ3 (::atol(argv[2]), true, 0, 0, 0, 0, 0, 0);
    } else if (std::string("q3allparallel") == argv[1]) {
      if (argc < 5) {
        LOG4CXX_ERROR(logger, "usage: scidb_rdb q3allparallel <cycle> <nodeCount> <nodeid>");
        LOG4CXX_ERROR(logger, "ex: scidb_rdb q3allparallel 2 10 0");
        return EXIT_FAILURE;
      }
      Benchmark benchmark (configs);
      benchmark.runQ3 (::atol(argv[2]), true, ::atol (argv[3]), ::atol (argv[4]), 0, 0, 0, 0);
    } else if (std::string("q4") == argv[1]) {
      if (argc < 7) {
        LOG4CXX_ERROR(logger, "usage: scidb_rdb q4 <cycle> <xstart> <xlen> <ystart> <ylen>");
        LOG4CXX_ERROR(logger, "ex: scidb_rdb q4 2 494000 10000 504000 10000");
        return EXIT_FAILURE;
      }
      Benchmark benchmark (configs);
      benchmark.runQ4(::atol(argv[2]), "pixelSum", ::atol(argv[3]), ::atol(argv[4]), ::atol(argv[5]), ::atol(argv[6]));
    } else if (std::string("q5") == argv[1]) {
      if (argc < 7) {
        LOG4CXX_ERROR(logger, "usage: scidb_rdb q5 <cycle> <xstart> <xlen> <ystart> <ylen>");
        LOG4CXX_ERROR(logger, "ex: scidb_rdb q5 2 494000 10000 504000 10000");
        return EXIT_FAILURE;
      }
      Benchmark benchmark (configs);
      benchmark.runQ5(::atol(argv[2]), "pixelSum", ::atol(argv[3]), ::atol(argv[4]), ::atol(argv[5]), ::atol(argv[6]));
    } else if (std::string("q6") == argv[1]) {
      if (argc < 5) {
        LOG4CXX_ERROR(logger, "usage: scidb_rdb q6 <cycle> <boxsize> <threshold>");
        LOG4CXX_ERROR(logger, "ex: scidb_rdb q6 2 100 10");
        return EXIT_FAILURE;
      }
      Benchmark benchmark (configs);
      benchmark.runQ6(::atol(argv[2]), ::atol(argv[3]), ::atol(argv[4]));
    } else if (std::string("q6new") == argv[1]) {
      if (argc < 9) {
        LOG4CXX_ERROR(logger, "usage: scidb_rdb q6new <cycle> <boxsize> <threshold> <xstart> <xlen> <ystart> <ylen>");
        LOG4CXX_ERROR(logger, "ex: scidb_rdb q6new 2 100 10 494000 10000 504000 10000");
        return EXIT_FAILURE;
      }
      Benchmark benchmark (configs);
      benchmark.runQ6New(::atol(argv[2]), ::atol(argv[3]), ::atol(argv[4]), ::atol(argv[5]), ::atol(argv[6]), ::atol(argv[7]), ::atol(argv[8]));
    } else if (std::string("q7") == argv[1]) {
      if (argc < 6) {
        LOG4CXX_ERROR(logger, "usage: scidb_rdb q7 <xstart> <xlen> <ystart> <ylen>");
        LOG4CXX_ERROR(logger, "ex: scidb_rdb q7 494000 10000 504000 10000");
        return EXIT_FAILURE;
      }
      Benchmark benchmark (configs);
      benchmark.runQ7(::atol(argv[2]), ::atol(argv[3]), ::atol(argv[4]), ::atol(argv[5]));
    } else if (std::string("q8") == argv[1] || std::string("q8parallel") == argv[1]) {
      if (argc < 7) {
        LOG4CXX_ERROR(logger, "usage: scidb_rdb " << argv[1] << " <xstart> <xlen> <ystart> <ylen> <dist>");
        LOG4CXX_ERROR(logger, "ex: scidb_rdb " << argv[1] << " 494000 10000 504000 10000 50");
        return EXIT_FAILURE;
      }
      Benchmark benchmark (configs);
      benchmark.runQ8(::atol(argv[2]), ::atol(argv[3]), ::atol(argv[4]), ::atol(argv[5]), ::atol(argv[6]), std::string("q8parallel") == argv[1]);
    } else if (std::string("q9") == argv[1] || std::string("q9parallel") == argv[1]) {
      if (argc < 8) {
        LOG4CXX_ERROR(logger, "usage: scidb_rdb " << argv[1] << " <time> <xstart> <xlen> <ystart> <ylen> <dist>");
        LOG4CXX_ERROR(logger, "ex: scidb_rdb " << argv[1] << " 60 494000 10000 504000 10000 50");
        return EXIT_FAILURE;
      }
      Benchmark benchmark (configs);
      benchmark.runQ9(::atol(argv[2]), ::atol(argv[3]), ::atol(argv[4]), ::atol(argv[5]), ::atol(argv[6]), ::atol(argv[7]), std::string("q9parallel") == argv[1]);
    } else {
      LOG4CXX_ERROR(logger, "unknown command " << argv[1]);
    }

    ::mysql_library_end ();
    LOG4CXX_INFO(logger, "terminated mysql library.");

    LOG4CXX_INFO(logger, "ended.");
  } catch(log4cxx::helpers::Exception &e) {
    LOG4CXX_ERROR(logger, "log4cxx exception caught in main(): " << e.what());
  } catch(std::exception &e) {
    LOG4CXX_ERROR(logger, "stdexception caught in main(): " << e.what());
  }

  return EXIT_SUCCESS;
}
