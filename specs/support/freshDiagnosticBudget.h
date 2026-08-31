#pragma once

#include <thread>
#include <utility>
using namespace storm;

// Every ECS diagnostic throttle is a static thread_local counter (see
// EcsShouldReport's comment in common/ecs.h), shared process-wide on
// whichever thread touches it and never reset. Plenty of specs, across two
// files, exercise the same call site's diagnostic more than once in the
// suite, so by the time a later case wants to assert on it, its budget of
// ECS_MAX_DIAGNOSTIC_REPORTS is normally already spent on the main thread.
//
// A test-only reset seam is the wrong shape for this: the counters are
// function-local and template-local statics scattered across common/ecs.h
// and common/ecs.cpp, so reaching them from a spec would mean restructuring
// every throttle into a keyed registry and putting a test-only entry point
// in a public header that games compile too. Running the triggering call on
// a fresh thread instead is genuinely robust — a new thread's TLS is zero
// regardless of what the suite has already run, and regardless of suite
// order — at the cost of nothing but a thread and a join.
template <typename F> void OnFreshDiagnosticBudget(F &&f) {
  std::thread(std::forward<F>(f)).join();
}
