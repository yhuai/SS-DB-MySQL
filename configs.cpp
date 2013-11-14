#include "configs.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include <log4cxx/logger.h>
using namespace log4cxx;

using namespace std;

void ConnectionConfig::readConfigFile () {
  LoggerPtr logger(Logger::getLogger("ConnectionConfig"));
  LOG4CXX_INFO(logger, "reading connection config..");
  std::ifstream file("connection.config", std::ios::in);
  if (!file) {
    LOG4CXX_ERROR(logger, "could not find connection.config. Have you renamed from connection.config.template?");
    throw std::exception();
  }

  bool first = true;
  std::string line;
  while (true) {
    std::getline(file, line);
    if (line == "") break;
    if (line[0] == '#') continue;
    MysqlHost host;
    host.load(line);
    if (first) {
      LOG4CXX_INFO(logger, "main server:" << host.toString());
      first = false;
      _main = host;
    } else {
      LOG4CXX_INFO(logger, "sub server:" << host.toString());
      _sub.push_back(host);
    }
  }
  LOG4CXX_INFO(logger, "read connection config.");
}

void Configs::readConfigFiles () {
  _connectionConfig.readConfigFile();
}