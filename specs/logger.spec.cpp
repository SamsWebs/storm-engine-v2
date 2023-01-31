#include <igloo/igloo_alt.h>

#include <string>

#include "../common/logger.h"

using namespace igloo;

Describe(LoggerSpec) {
  Logger logger;
  std::string log_message, err_message;

  void SetUp() {
    logger.SetLogCallback(
        [&](const std::string &message) { log_message = message; });
    logger.SetErrCallback(
        [&](const std::string &message) { err_message = message; });
  }

  It(Should_Log_Message) {
    const std::string expected_message = "This is a log message";
    logger.Log(expected_message);
    Assert::That(log_message, Equals(expected_message));
  }

  It(Should_Log_Error_Message) {
    const std::string expected_message = "This is an error message";
    logger.Err(expected_message);
    Assert::That(err_message, Equals(expected_message));
  }
};
