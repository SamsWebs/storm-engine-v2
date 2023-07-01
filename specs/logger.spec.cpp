#include <igloo/igloo_alt.h>

#include <string>

#include "../common/logger.h"
#include <sstream>

using namespace igloo;

Describe(LoggerSpec){
    void LogCallbackFunction(const LogEntry &entry){std::stringstream ss;
ss << "Custom log callback: Type=" << static_cast<int>(entry.type)
   << ", Message=" << entry.message;
Logger::messages.push_back({entry.type, ss.str()});
}

void ErrCallbackFunction(const LogEntry &entry) {
  std::stringstream ss;
  ss << "Custom error callback: Type=" << static_cast<int>(entry.type)
     << ", Message=" << entry.message;
  Logger::messages.push_back({entry.type, ss.str()});
}

It(should_log_messages_with_correct_type) {
  // Clear any existing log messages
  Logger::messages.clear();

  // Arrange
  std::string message = "Test log message";
  std::string errorMessage = "Test error message";

  // Create an instance of Logger
  Logger logger;

  // Act
  logger.Log(message);
  logger.Err(errorMessage);

  // Assert
  Assert::That(Logger::messages.size(), Equals(2));

  // Check the first log entry
  Assert::That(Logger::messages[0].type, Equals(LogType::LOG_INFO));

  // Check the second log entry
  Assert::That(Logger::messages[1].type, Equals(LogType::LOG_ERROR));
}

It(should_call_custom_log_callback) {
  // Clear any existing log messages
  Logger::messages.clear();

  // Arrange
  std::string message = "Test log message";

  // Create an instance of Logger
  Logger logger;

  // Set custom error callback
  std::vector<LogEntry> callback_messages;
  logger.SetErrCallback(
      [&](const LogEntry &entry) { callback_messages.push_back(entry); });

  // Act
  logger.Log(message);

  // Assert
  Assert::That(Logger::messages.size(), Equals(1));
  Assert::That(Logger::messages[0].type, Equals(LogType::LOG_INFO));
}

It(should_call_custom_error_callback) {
  // Clear any existing log messages
  Logger::messages.clear();

  // Arrange
  std::string errorMessage = "Test error message";

  // Create an instance of Logger
  Logger logger;

  // Set custom error callback
  std::vector<LogEntry> callback_messages;
  logger.SetErrCallback(
      [&](const LogEntry &entry) { callback_messages.push_back(entry); });

  // Act
  logger.Err(errorMessage);

  // Assert
  Assert::That(Logger::messages.size(), Equals(1));
  Assert::That(Logger::messages[0].type, Equals(LogType::LOG_ERROR));
}
}
;