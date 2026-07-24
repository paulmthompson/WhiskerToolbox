#include <catch2/catch_test_macros.hpp>

#include "fixtures/scenarios/neural/WhiskerContactScenario.hpp"
#include "fixtures/scenarios/neural/WhiskerContactScenarioAssertions.hpp"
#include "fixtures/whisker_contact_test_fixture.hpp"

TEST_CASE("WhiskerContactScenario generates expected structure", "[whisker_contact][scenario]") {
    WhiskerContactScenarioConfig cfg;
    cfg.duration_sec = 10.0;
    auto const scenario = WhiskerContactScenario::generate(cfg);

    REQUIRE(scenario.master_time->getTotalFrameCount() == 300000);
    REQUIRE(scenario.time_time->getTotalFrameCount() == 5000);
    REQUIRE(scenario.spikes != nullptr);
    REQUIRE(scenario.contact->size() > 0);
    REQUIRE(scenario.curvature->getAnalogTimeSeries().size() == 5000);
    REQUIRE(scenario.angle->getAnalogTimeSeries().size() == 5000);
}

TEST_CASE("WhiskerContactScenario is reproducible with fixed seed", "[whisker_contact][scenario]") {
    WhiskerContactScenarioConfig cfg;
    cfg.seed = 1234;

    auto const first = WhiskerContactScenario::generate(cfg);
    auto const second = WhiskerContactScenario::generate(cfg);

    REQUIRE(first.spikes->size() == second.spikes->size());
    std::vector<int64_t> first_times;
    std::vector<int64_t> second_times;
    for (auto event: first.spikes->view()) {
        first_times.push_back(event.time().getValue());
    }
    for (auto event: second.spikes->view()) {
        second_times.push_back(event.time().getValue());
    }
    REQUIRE(first_times == second_times);
}

TEST_CASE("WhiskerContactScenario maps master to time frames", "[whisker_contact][scenario]") {
    WhiskerContactScenarioConfig cfg;
    cfg.duration_sec = 1.0;
    auto const scenario = WhiskerContactScenario::generate(cfg);

    REQUIRE(scenario.time_time->getTimeAtIndex(TimeFrameIndex(0)) == 0);
    REQUIRE(scenario.time_time->getTimeAtIndex(TimeFrameIndex(1)) == 60);
    REQUIRE(scenario.time_time->getTimeAtIndex(TimeFrameIndex(499)) == 29940);
}

TEST_CASE("WhiskerContactTestFixture populates DataManager", "[whisker_contact][fixture]") {
    WhiskerContactTestFixture fixture;
    REQUIRE(whisker_contact_assertions::assertScenarioStructure(fixture.dm(), fixture.scenario()));
}

TEST_CASE("WhiskerContactScenario statistical properties", "[whisker_contact][statistics]") {
    WhiskerContactScenarioConfig cfg;
    cfg.duration_sec = 30.0;
    auto const scenario = WhiskerContactScenario::generate(cfg);

    REQUIRE(whisker_contact_assertions::assertContactDuration(
            *scenario.contact, 10.0f, 4.0f));
    REQUIRE(whisker_contact_assertions::assertAR1Autocorrelation(
            *scenario.curvature, cfg.ar1_phi, 0.08f));
    REQUIRE(whisker_contact_assertions::assertAR1Autocorrelation(
            *scenario.angle, cfg.ar1_phi, 0.08f));
    REQUIRE(whisker_contact_assertions::assertSpikeRateElevatedDuringContact(scenario, 3.0));
    REQUIRE(whisker_contact_assertions::assertCurvatureOnsetModulation(scenario, 0.05));
}
