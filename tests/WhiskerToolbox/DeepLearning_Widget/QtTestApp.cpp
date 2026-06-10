/**
 * @file QtTestApp.cpp
 * @brief Ensures a single leaked QApplication exists for combined DeepLearning widget tests.
 *
 * When multiple widget test translation units are linked into one Catch2 executable,
 * per-file static QApplication guards can race during initialization and destroy Qt
 * during static teardown. This listener creates one intentionally leaked
 * QApplication at test-run start, matching the DataInspector widget test pattern.
 */

#include <QApplication>

#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <array>

namespace {

/**
 * @brief Create a process-wide QApplication if one does not already exist.
 */
void ensureQApplication() {
    if (QApplication::instance() != nullptr) {
        return;
    }

    static int argc = 1;
    static char app_name[] = "test";
    static std::array<char *, 1> argv = {app_name};
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): intentionally leaked for test lifetime
    new QApplication(argc, argv.data());
}

class QtAppListener : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(Catch::TestRunInfo const & /*info*/) override {
        ensureQApplication();
    }
};

}// namespace

CATCH_REGISTER_LISTENER(QtAppListener)
