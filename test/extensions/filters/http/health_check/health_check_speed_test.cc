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

class HealthCheckFilterTest {
public:
  HealthCheckFilterTest(bool pass_through, bool caching)
      : request_headers_{{":path", "/healthcheck"}}, request_headers_no_hc_{{":path", "/foo"}} {

    if (caching) {
      cache_manager_ =
          std::make_shared<HealthCheckCacheManager>(dispatcher_, std::chrono::milliseconds(1));
    }

    prepareFilter(pass_through);
  }

  void prepareFilter(
      bool pass_through,
      ClusterMinHealthyPercentagesConstSharedPtr cluster_min_healthy_percentages = nullptr) {
    header_data_ = std::make_shared<std::vector<Http::HeaderUtility::HeaderDataPtr>>();
    envoy::config::route::v3::HeaderMatcher matcher;
    matcher.set_name(":path");
    matcher.set_exact_match("/healthcheck");
    header_data_->emplace_back(std::make_unique<Http::HeaderUtility::HeaderData>(matcher));
    filter_ = std::make_unique<HealthCheckFilter>(context_, pass_through, cache_manager_,
                                                  header_data_, cluster_min_healthy_percentages);
    filter_->setDecoderFilterCallbacks(callbacks_);
  }

  void runFilter() {
    EXPECT_CALL(context_, healthCheckFailed()).WillOnce(Return(false));
    EXPECT_CALL(context_, clusterManager());
    EXPECT_CALL(context_.cluster_manager_, get(Eq("www1"))).WillRepeatedly(Return(&cluster_www1_));
    EXPECT_CALL(context_.cluster_manager_, get(Eq("www2"))).WillRepeatedly(Return(&cluster_www2_));
    EXPECT_CALL(callbacks_, encodeHeaders_(HeaderMapEqualRef(&health_check_response_), true));
    filter_->decodeHeaders(request_headers_, true);
    EXPECT_EQ("health_check_ok_cluster_healthy", callbacks_.details_);
  }

  class MockHealthCheckCluster : public NiceMock<Upstream::MockThreadLocalCluster> {
  public:
    MockHealthCheckCluster(uint64_t membership_total, uint64_t membership_healthy,
                           uint64_t membership_degraded = 0) {
      info()->stats().membership_total_.set(membership_total);
      info()->stats().membership_healthy_.set(membership_healthy);
      info()->stats().membership_degraded_.set(membership_degraded);
    }
  };

  Http::TestResponseHeaderMapImpl health_check_response_{{":status", "200"}};
  MockHealthCheckCluster cluster_www1_{100, 50};
  MockHealthCheckCluster cluster_www2_{1000, 800};
  NiceMock<Server::Configuration::MockFactoryContext> context_;
  Event::MockTimer* cache_timer_{};
  Event::MockDispatcher dispatcher_;
  HealthCheckCacheManagerSharedPtr cache_manager_;
  std::unique_ptr<HealthCheckFilter> filter_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks_;
  Http::TestRequestHeaderMapImpl request_headers_;
  Http::TestRequestHeaderMapImpl request_headers_no_hc_;
  HeaderDataVectorSharedPtr header_data_;
};

static void BM_DecodeHeaders(benchmark::State& state) {
  HealthCheckFilterTest fixture{false, false};
  fixture.prepareFilter(false,
                        ClusterMinHealthyPercentagesConstSharedPtr(
                            new ClusterMinHealthyPercentages{{"www1", 50.0}, {"www2", 75.0}}));

  for (auto _ : state) {
    fixture.runFilter();
  }
}
BENCHMARK(BM_DecodeHeaders);

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
BENCHMARK(BM_IsHealthyOrig);

static void BM_IsHealthyOpt(benchmark::State& state) {
  for (auto _ : state) {
    is_healthy_opt(2, 1, 10, 50.0);
  }
}
BENCHMARK(BM_IsHealthyOpt);

} // namespace
} // namespace HealthCheck
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy

BENCHMARK_MAIN();