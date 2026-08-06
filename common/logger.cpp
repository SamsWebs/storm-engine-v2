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

  // localtime returns null if the value cannot be represented as local time.
  // Feeding that to strftime is undefined, and this runs on every log line.
  const std::tm *local = std::localtime(&now);
  if (local == nullptr) {
    return "(unknown time)";
  }

  std::string output(30, '\0');
  const std::size_t written =
      std::strftime(&output[0], output.size(), "%d-%b-%Y %H:%M:%S", local);

  // strftime reports the characters written, excluding the terminator, and
  // returns 0 if the buffer was too small. Without this resize the string
  // keeps its NUL padding and every log line carries ten trailing NUL bytes.
  output.resize(written);
  return output;
}

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

  // '\n' rather than std::endl: the engine logs on every entity and component
  // operation, and std::endl flushes on every line. Errors still flush, so a
  // diagnostic that precedes a crash is not lost in the buffer.
  std::cout << color << logEntry.message << RESET << '\n';
  if (logType == LogType::LOG_ERROR) {
    std::cout.flush();
  }

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