#include <igloo/igloo_alt.h>

#include <string>

#include "../common/logger.h"

using namespace igloo;
using namespace storm;

// Logger::messages is a process-global buffer, so every case clears it first
// and the callback vectors are local to each case.
Describe(LoggerSpec) {
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
  };

  It(should_call_custom_log_callback) {
    // Clear any existing log messages
    Logger::messages.clear();

    // Arrange
    std::string message = "Test log message";

    // Create an instance of Logger
    Logger logger;

    // Set custom log callback
    std::vector<LogEntry> callback_messages;
    logger.SetLogCallback(
        [&](const LogEntry &entry) { callback_messages.push_back(entry); });

    // Act
    logger.Log(message);

    // Assert
    Assert::That(Logger::messages.size(), Equals(1));
    Assert::That(Logger::messages[0].type, Equals(LogType::LOG_INFO));

    // The callback fired exactly once, and received the same entry that was
    // buffered — the decorated message, not the bare one the caller passed.
    Assert::That(callback_messages.size(), Equals(1u));
    Assert::That(callback_messages[0].type, Equals(LogType::LOG_INFO));
    Assert::That(callback_messages[0].message,
                 Equals(Logger::messages[0].message));
    Assert::That(callback_messages[0].message.find(message) !=
                     std::string::npos,
                 Equals(true));
  };

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

    Assert::That(callback_messages.size(), Equals(1u));
    Assert::That(callback_messages[0].type, Equals(LogType::LOG_ERROR));
    Assert::That(callback_messages[0].message,
                 Equals(Logger::messages[0].message));
    Assert::That(callback_messages[0].message.find(errorMessage) !=
                     std::string::npos,
                 Equals(true));
  };

  It(should_not_call_the_error_callback_for_an_info_message) {
    Logger::messages.clear();

    Logger logger;
    std::vector<LogEntry> err_messages;
    logger.SetErrCallback(
        [&](const LogEntry &entry) { err_messages.push_back(entry); });

    logger.Log("an ordinary message");

    Assert::That(Logger::messages.size(), Equals(1u));
    Assert::That(err_messages.size(), Equals(0u));
  };

  // The log callback is not info-only: logHelper invokes it for every entry,
  // and the error callback then fires as well. A game routing
  // diagnostics into its own sink through SetLogCallback receives errors
  // there too, and receives them twice if it also sets an error callback.
  It(should_deliver_an_error_to_the_log_callback_as_well) {
    Logger::messages.clear();

    Logger logger;
    std::vector<LogEntry> log_messages;
    std::vector<LogEntry> err_messages;
    logger.SetLogCallback(
        [&](const LogEntry &entry) { log_messages.push_back(entry); });
    logger.SetErrCallback(
        [&](const LogEntry &entry) { err_messages.push_back(entry); });

    logger.Err("something broke");

    Assert::That(log_messages.size(), Equals(1u));
    Assert::That(log_messages[0].type, Equals(LogType::LOG_ERROR));
    Assert::That(err_messages.size(), Equals(1u));
    Assert::That(err_messages[0].message, Equals(log_messages[0].message));
  };

  // Callbacks are per-Logger instance state, not static like messages.
  It(should_not_share_a_callback_between_logger_instances) {
    Logger::messages.clear();

    Logger withCallback;
    Logger withoutCallback;
    std::vector<LogEntry> callback_messages;
    withCallback.SetLogCallback(
        [&](const LogEntry &entry) { callback_messages.push_back(entry); });

    withoutCallback.Log("routed nowhere");

    Assert::That(Logger::messages.size(), Equals(1u));
    Assert::That(callback_messages.size(), Equals(0u));

    withCallback.Log("routed to the callback");

    Assert::That(Logger::messages.size(), Equals(2u));
    Assert::That(callback_messages.size(), Equals(1u));
  };

  // A replaced callback supersedes the previous one rather than adding to it.
  It(should_replace_a_previously_set_log_callback) {
    Logger::messages.clear();

    Logger logger;
    std::vector<LogEntry> first;
    std::vector<LogEntry> second;
    logger.SetLogCallback(
        [&](const LogEntry &entry) { first.push_back(entry); });
    logger.SetLogCallback(
        [&](const LogEntry &entry) { second.push_back(entry); });

    logger.Log("only the second callback sees this");

    Assert::That(first.size(), Equals(0u));
    Assert::That(second.size(), Equals(1u));
  };
};
