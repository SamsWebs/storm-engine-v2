#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>

#include "logger.h"

std::vector<LogEntry> Logger::messages;

std::string CurrentDateTimeToString() {
  std::time_t now =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::string output(30, '\0');
  std::strftime(&output[0], output.size(), "%d-%b-%Y %H:%M:%S",
                std::localtime(&now));
  return output;
};

void Logger::SetLogCallback(const LogCallback &callback) {
  log_callback_ = callback;
}

void Logger::SetErrCallback(const LogCallback &callback) {
  err_callback_ = callback;
}

void Logger::Log(const std::string &message) {
  LogEntry log_entry;
  log_entry.type = LOG_INFO;
  log_entry.message = "LOG: [" + CurrentDateTimeToString() + "]: " + message;
  messages.push_back(log_entry);

  if (log_callback_) {
    log_callback_(log_entry);
  }
}

void Logger::Err(const std::string &message) {
  LogEntry log_entry;
  log_entry.type = LOG_ERROR;
  log_entry.message = "ERR: [" + CurrentDateTimeToString() + "]: " + message;
  messages.push_back(log_entry);

  if (err_callback_) {
    err_callback_(log_entry);
  }
}