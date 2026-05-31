#pragma once

#include <chrono>
#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

enum LogType { LOG_INFO, LOG_WARNING, LOG_ERROR };

struct LogEntry {
  LogType type;
  std::string message;
};

class Logger {
public:
  using LogCallback = std::function<void(const LogEntry &)>;

  static std::vector<LogEntry> messages;
  void Log(const std::string &message);
  void Err(const std::string &message);
  void SetLogCallback(const LogCallback &callback);
  void SetErrCallback(const LogCallback &callback);

private:
  LogCallback log_callback_;
  LogCallback err_callback_;
  void logHelper(const std::string &msg, LogType logType);
  static const std::string GREEN;
  static const std::string RED;
  static const std::string RESET;
};

typedef std::unique_ptr<Logger> Logger_Ptr;