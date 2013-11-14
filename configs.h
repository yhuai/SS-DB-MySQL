#ifndef CONFIGS_H
#define CONFIGS_H

#include <vector>
#include <sstream>

struct MysqlHost {
  std::string host;
  int port;
  std::string db;
  std::string user;
  std::string pass;
  void load(std::string line) {
    std::stringstream str(line);
    str >> host >> port >> db >> user >> pass;
  }
  std::string toString() const {
    std::stringstream str;
    str << "host=" << host << ", port=" << port << ", db=" << db << ", user=" << user;
    return str.str();
  }
};

// ConnectionConfig
// Class to read connection.config file.
class ConnectionConfig {
public:
  ConnectionConfig () {};
  void readConfigFile ();
  const MysqlHost& getMain() const {return _main; }
  const std::vector<MysqlHost>& getSub() const {return _sub; }
private:
  MysqlHost _main;
  std::vector<MysqlHost> _sub;
};


// Configs
// Class to retain all configurations for the benchmark program.
class Configs {
public:
  Configs() {};
  void readConfigFiles ();
  const ConnectionConfig& getConnectionConfig() const { return _connectionConfig; }

private:
  ConnectionConfig _connectionConfig;
};

#endif // CONFIGS_H
