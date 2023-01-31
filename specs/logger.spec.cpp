#include <igloo/igloo_alt.h>

#include <string>

#include "../common/logger.h"

using namespace igloo;

Describe(LoggerTest) {
  void SetUp() { Logger::messages.clear(); }

  It(TestLog) {
    Logger logger;
    logger.Log("This is a log message");
    Assert::That(Logger::messages.size(), Equals(1));
    Assert::That(Logger::messages[0].type, Equals(LOG_INFO));
    // Assert::That(Logger::messages[0].message, Equals("This is a log
    // message"));
  };

  It(TestErr) {
    Logger logger;
    logger.Err("This is an error message");
    Assert::That(Logger::messages.size(), Equals(1));
    Assert::That(Logger::messages[0].type, Equals(LOG_ERROR));
    // Assert::That(Logger::messages[0].message,
    //             Equals("This is an error message"));
  };

  It(TestSetLogCallback) {
    Logger logger;
    std::vector<LogEntry> callback_messages;
    logger.SetLogCallback(
        [&](const LogEntry &entry) { callback_messages.push_back(entry); });
    logger.Log("This is a log message");
    Assert::That(callback_messages.size(), Equals(1));
    Assert::That(callback_messages[0].type, Equals(LOG_INFO));
    // Assert::That(callback_messages[0].message, Equals("This is a log
    // message"));
  };

  It(TestSetErrCallback) {
    Logger logger;
    std::vector<LogEntry> callback_messages;
    logger.SetErrCallback(
        [&](const LogEntry &entry) { callback_messages.push_back(entry); });
    logger.Err("This is an error message");
    Assert::That(callback_messages.size(), Equals(1));
    Assert::That(callback_messages[0].type, Equals(LOG_ERROR));
    // Assert::That(callback_messages[0].message,
    //              Equals("This is an error message"));
  };
};
