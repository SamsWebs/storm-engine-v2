#pragma once

#include <functional>
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
};