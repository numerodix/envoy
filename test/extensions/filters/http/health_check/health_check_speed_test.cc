#include <chrono>
#include <memory>

#include "envoy/config/route/v3/route_components.pb.h"

#include "common/buffer/buffer_impl.h"
#include "common/http/header_utility.h"
#include "common/upstream/upstream_impl.h"

#include "extensions/filters/http/health_check/health_check.h"

#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/cluster_info.h"
#include "test/test_common/printers.h"
#include "test/test_common/utility.h"

#include "benchmark/benchmark.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace HealthCheck {
namespace {

bool is_healthy_orig(uint64_t healthy, uint64_t degraded, uint64_t total, double min_healthy_perc) {
  return ((healthy + degraded) < total * min_healthy_perc / 100.0);
}

bool is_healthy_opt(uint64_t healthy, uint64_t degraded, uint64_t total, double min_healthy_perc) {
  return ((100UL * (healthy + degraded)) < total * static_cast<uint64_t>(min_healthy_perc));
}

static void BM_IsHealthyOrig(benchmark::State& state) {
  for (auto _ : state) {
    is_healthy_orig(2, 1, 10, 50.0);
  }
}

static void BM_IsHealthyOpt(benchmark::State& state) {
  for (auto _ : state) {
    is_healthy_opt(2, 1, 10, 50.0);
  }
}
BENCHMARK(BM_IsHealthyOpt);
BENCHMARK(BM_IsHealthyOrig);

} // namespace
} // namespace HealthCheck
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy

BENCHMARK_MAIN();