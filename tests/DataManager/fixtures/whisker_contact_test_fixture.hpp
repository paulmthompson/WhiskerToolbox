#ifndef WHISKER_CONTACT_TEST_FIXTURE_HPP
#define WHISKER_CONTACT_TEST_FIXTURE_HPP

/**
 * @file whisker_contact_test_fixture.hpp
 * @brief Catch2 fixture that populates DataManager with the whisker-contact scenario.
 */

#include "fixtures/scenarios/neural/WhiskerContactScenario.hpp"

#include "DataManager.hpp"

#include <memory>

/**
 * @brief Catch2 fixture providing a populated whisker-contact DataManager.
 */
class WhiskerContactTestFixture {
public:
    explicit WhiskerContactTestFixture(WhiskerContactScenarioConfig cfg = {})
        : _config(std::move(cfg)),
          _scenario(WhiskerContactScenario::generate(_config)),
          _data_manager(std::make_unique<DataManager>()) {
        _scenario.populate(*_data_manager);
    }

    DataManager & dm() { return *_data_manager; }
    DataManager const & dm() const { return *_data_manager; }

    WhiskerContactScenarioConfig const & config() const { return _config; }
    WhiskerContactScenario const & scenario() const { return _scenario; }

private:
    WhiskerContactScenarioConfig _config;
    WhiskerContactScenario _scenario;
    std::unique_ptr<DataManager> _data_manager;
};

#endif// WHISKER_CONTACT_TEST_FIXTURE_HPP
