#include "logger.h"

std::vector<LogEntry> Logger::messages;

const std::string Logger::GREEN = "\033[32m";
const std::string Logger::RED = "\033[31m";
const std::string Logger::RESET = "\033[0m";

void Logger::Log(const std::string &message) {
  logHelper(message, LogType::LOG_INFO);
}

void Logger::Err(const std::string &message) {
  logHelper(message, LogType::LOG_ERROR);
}

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

void Logger::logHelper(const std::string &message, LogType logType) {
  LogEntry logEntry;
  logEntry.type = logType;
  std::string color;
  std::string logDesc;

  switch (logType) {
  case LogType::LOG_ERROR:
    color = RED;
    logDesc = "ERR";
    break;
  case LogType::LOG_INFO:
    color = GREEN;
    logDesc = "LOG";
    break;
  default:
    break;
  }

  logEntry.message =
      logDesc + " [ " + CurrentDateTimeToString() + "]: " + message;

  std::cout << color << logEntry.message << RESET << std::endl;

  // Keep the in-memory history bounded — the engine logs on every entity /
  // component operation, so an uncapped vector grows for the whole session.
  constexpr std::size_t MAX_LOG_ENTRIES = 1000;
  if (messages.size() >= MAX_LOG_ENTRIES) {
    messages.erase(messages.begin(), messages.begin() + MAX_LOG_ENTRIES / 2);
  }
  messages.push_back(logEntry);

  if (log_callback_) {
    log_callback_(logEntry);
  }
  if (logType == LogType::LOG_ERROR && err_callback_) {
    err_callback_(logEntry);
  }
}