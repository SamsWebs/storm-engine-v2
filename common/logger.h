#pragma once

#include <functional>
#include <string>

class Logger {
public:
  void Log(const std::string &message);
  void Err(const std::string &message);

  void SetLogCallback(std::function<void(const std::string &)> callback);
  void SetErrCallback(std::function<void(const std::string &)> callback);

private:
  std::function<void(const std::string &)> log_callback_;
  std::function<void(const std::string &)> err_callback_;
};
