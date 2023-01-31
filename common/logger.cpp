#include "logger.h"

void Logger::Log(const std::string &message) { log_callback_(message); };

void Logger::Err(const std::string &message) { err_callback_(message); };

void Logger::SetLogCallback(std::function<void(const std::string &)> callback) {
  log_callback_ = callback;
};

void Logger::SetErrCallback(std::function<void(const std::string &)> callback) {
  err_callback_ = callback;
};
