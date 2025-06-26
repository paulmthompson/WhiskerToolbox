#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrameV2.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

// Helper class to create temporary test files
class TempTestDirectory {
public:
    TempTestDirectory() {
        // Create unique temporary directory
        temp_path = std::filesystem::temp_directory_path() / ("whiskertoolbox_test_" + std::to_string(std::time(nullptr)));
        std::filesystem::create_directories(temp_path);
    }

    ~TempTestDirectory() {
        // Clean up temporary directory
        if (std::filesystem::exists(temp_path)) {
            std::filesystem::remove_all(temp_path);
        }
    }

    void createFile(std::string const & filename) {
        auto filepath = temp_path / filename;
        std::ofstream file(filepath);
        file << "dummy content";
        file.close();
    }

    std::string getPath() const {
        return temp_path.string();
    }

private:
    std::filesystem::path temp_path;
};

TEST_CASE("FilenameTimeFrame - Basic filename extraction", "[timeframe][filename]") {
    TempTestDirectory temp_dir;

    SECTION("Simple frame number pattern") {
        // Create test files with simple frame numbers
        temp_dir.createFile("frame_001.jpg");
        temp_dir.createFile("frame_042.jpg");
        temp_dir.createFile("frame_123.jpg");
        temp_dir.createFile("frame_500.jpg");

        FilenameTimeFrameOptions options;
        options.folder_path = temp_dir.getPath();
        options.file_extension = ".jpg";
        options.regex_pattern = R"(frame_(\d+)\.jpg)";
        options.mode = FilenameTimeFrameMode::FOUND_VALUES;
        options.sort_ascending = true;

        auto timeframe = createTimeFrameFromFilenames(options);

        REQUIRE(timeframe != nullptr);
        REQUIRE(timeframe->getTotalFrameCount() == 4);

        // Values should be sorted: 1, 42, 123, 500
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(0)) == 1);
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(1)) == 42);
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(2)) == 123);
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(3)) == 500);
    }

    SECTION("Mixed file types with extension filtering") {
        // Create files with different extensions
        temp_dir.createFile("image_100.png");
        temp_dir.createFile("image_200.jpg");// Should be ignored
        temp_dir.createFile("image_300.png");
        temp_dir.createFile("readme.txt");// Should be ignored

        FilenameTimeFrameOptions options;
        options.folder_path = temp_dir.getPath();
        options.file_extension = ".png";
        options.regex_pattern = R"(image_(\d+)\.png)";
        options.mode = FilenameTimeFrameMode::FOUND_VALUES;

        auto timeframe = createTimeFrameFromFilenames(options);

        REQUIRE(timeframe != nullptr);
        REQUIRE(timeframe->getTotalFrameCount() == 2);
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(0)) == 100);
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(1)) == 300);
    }

    SECTION("Complex filename pattern") {
        // Create files with more complex naming
        temp_dir.createFile("experiment_2_session_1_frame_1001.tiff");
        temp_dir.createFile("experiment_2_session_1_frame_1050.tiff");
        temp_dir.createFile("experiment_2_session_1_frame_2000.tiff");

        FilenameTimeFrameOptions options;
        options.folder_path = temp_dir.getPath();
        options.file_extension = ".tiff";
        options.regex_pattern = R"(experiment_\d+_session_\d+_frame_(\d+)\.tiff)";
        options.mode = FilenameTimeFrameMode::FOUND_VALUES;

        auto timeframe = createTimeFrameFromFilenames(options);

        REQUIRE(timeframe != nullptr);
        REQUIRE(timeframe->getTotalFrameCount() == 3);
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(0)) == 1001);
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(1)) == 1050);
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(2)) == 2000);
    }
}

TEST_CASE("FilenameTimeFrame - Different creation modes", "[timeframe][filename][modes]") {
    TempTestDirectory temp_dir;

    // Create test files with gaps in numbering
    temp_dir.createFile("frame_10.jpg");
    temp_dir.createFile("frame_25.jpg");
    temp_dir.createFile("frame_30.jpg");
    temp_dir.createFile("frame_50.jpg");

    FilenameTimeFrameOptions base_options;
    base_options.folder_path = temp_dir.getPath();
    base_options.file_extension = ".jpg";
    base_options.regex_pattern = R"(frame_(\d+)\.jpg)";

    SECTION("FOUND_VALUES mode") {
        base_options.mode = FilenameTimeFrameMode::FOUND_VALUES;
        auto timeframe = createTimeFrameFromFilenames(base_options);

        REQUIRE(timeframe != nullptr);
        REQUIRE(timeframe->getTotalFrameCount() == 4);
        // Should contain only the extracted values: 10, 25, 30, 50
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(0)) == 10);
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(3)) == 50);
    }

    SECTION("ZERO_TO_MAX mode") {
        base_options.mode = FilenameTimeFrameMode::ZERO_TO_MAX;
        auto timeframe = createTimeFrameFromFilenames(base_options);

        REQUIRE(timeframe != nullptr);
        REQUIRE(timeframe->getTotalFrameCount() == 51);// 0 to 50 inclusive
        // Should be a complete range from 0 to 50
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(0)) == 0);
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(10)) == 10);
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(50)) == 50);
    }

    SECTION("MIN_TO_MAX mode") {
        base_options.mode = FilenameTimeFrameMode::MIN_TO_MAX;
        auto timeframe = createTimeFrameFromFilenames(base_options);

        REQUIRE(timeframe != nullptr);
        REQUIRE(timeframe->getTotalFrameCount() == 41);// 10 to 50 inclusive
        // Should be a complete range from 10 to 50
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(0)) == 10);
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(15)) == 25);
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(40)) == 50);
    }
}

TEST_CASE("FilenameTimeFrame - Error handling", "[timeframe][filename][errors]") {
    TempTestDirectory temp_dir;

    SECTION("Non-existent directory") {
        FilenameTimeFrameOptions options;
        options.folder_path = "/path/that/does/not/exist";
        options.file_extension = ".jpg";
        options.regex_pattern = R"(frame_(\d+)\.jpg)";
        options.mode = FilenameTimeFrameMode::FOUND_VALUES;

        auto timeframe = createTimeFrameFromFilenames(options);
        REQUIRE(timeframe == nullptr);
    }

    SECTION("No matching files") {
        // Create files that don't match the pattern
        temp_dir.createFile("nopattern.jpg");
        temp_dir.createFile("alsonopattern.jpg");

        FilenameTimeFrameOptions options;
        options.folder_path = temp_dir.getPath();
        options.file_extension = ".jpg";
        options.regex_pattern = R"(frame_(\d+)\.jpg)";
        options.mode = FilenameTimeFrameMode::FOUND_VALUES;

        auto timeframe = createTimeFrameFromFilenames(options);
        REQUIRE(timeframe == nullptr);
    }

    SECTION("Invalid regex pattern") {
        temp_dir.createFile("frame_001.jpg");

        FilenameTimeFrameOptions options;
        options.folder_path = temp_dir.getPath();
        options.file_extension = ".jpg";
        options.regex_pattern = "invalid[regex(pattern";
        options.mode = FilenameTimeFrameMode::FOUND_VALUES;

        auto timeframe = createTimeFrameFromFilenames(options);
        REQUIRE(timeframe == nullptr);
    }

    SECTION("Files with no numerical content") {
        temp_dir.createFile("frame_abc.jpg");
        temp_dir.createFile("frame_xyz.jpg");

        FilenameTimeFrameOptions options;
        options.folder_path = temp_dir.getPath();
        options.file_extension = ".jpg";
        options.regex_pattern = R"(frame_(\w+)\.jpg)";// Captures letters, not numbers
        options.mode = FilenameTimeFrameMode::FOUND_VALUES;

        auto timeframe = createTimeFrameFromFilenames(options);
        REQUIRE(timeframe == nullptr);
    }
}

TEST_CASE("FilenameTimeFrame - Sorting behavior", "[timeframe][filename][sorting]") {
    TempTestDirectory temp_dir;

    // Create files in non-sequential order
    temp_dir.createFile("frame_300.jpg");
    temp_dir.createFile("frame_100.jpg");
    temp_dir.createFile("frame_500.jpg");
    temp_dir.createFile("frame_200.jpg");

    FilenameTimeFrameOptions options;
    options.folder_path = temp_dir.getPath();
    options.file_extension = ".jpg";
    options.regex_pattern = R"(frame_(\d+)\.jpg)";
    options.mode = FilenameTimeFrameMode::FOUND_VALUES;

    SECTION("Ascending sort (default)") {
        options.sort_ascending = true;
        auto timeframe = createTimeFrameFromFilenames(options);

        REQUIRE(timeframe != nullptr);
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(0)) == 100);
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(1)) == 200);
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(2)) == 300);
        REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex(3)) == 500);
    }

    SECTION("No sorting") {
        options.sort_ascending = false;
        auto timeframe = createTimeFrameFromFilenames(options);

        REQUIRE(timeframe != nullptr);
        // Order should be based on filesystem iteration (unpredictable)
        // Just verify all values are present
        std::vector<int> found_values;
        for (int i = 0; i < timeframe->getTotalFrameCount(); ++i) {
            found_values.push_back(timeframe->getTimeAtIndex(TimeFrameIndex(i)));
        }

        REQUIRE(std::find(found_values.begin(), found_values.end(), 100) != found_values.end());
        REQUIRE(std::find(found_values.begin(), found_values.end(), 200) != found_values.end());
        REQUIRE(std::find(found_values.begin(), found_values.end(), 300) != found_values.end());
        REQUIRE(std::find(found_values.begin(), found_values.end(), 500) != found_values.end());
    }
}

TEST_CASE("TimeFrameV2 - CameraTimeFrame from filenames", "[timeframev2][filename][camera]") {
    TempTestDirectory temp_dir;

    // Create test files
    temp_dir.createFile("cam_1001.png");
    temp_dir.createFile("cam_1050.png");
    temp_dir.createFile("cam_1100.png");

    FilenameTimeFrameOptions options;
    options.folder_path = temp_dir.getPath();
    options.file_extension = ".png";
    options.regex_pattern = R"(cam_(\d+)\.png)";

    SECTION("Sparse CameraTimeFrame") {
        options.mode = FilenameTimeFrameMode::FOUND_VALUES;
        auto camera_frame = TimeFrameUtils::createCameraTimeFrameFromFilenames(options);

        REQUIRE(camera_frame != nullptr);
        REQUIRE(camera_frame->getTotalFrameCount() == 3);
        REQUIRE(camera_frame->isSparse());
        REQUIRE_FALSE(camera_frame->isDense());

        // Test coordinate access
        CameraFrameIndex first_frame = camera_frame->getTimeAtIndex(TimeFrameIndex(0));
        REQUIRE(first_frame.getValue() == 1001);

        CameraFrameIndex last_frame = camera_frame->getTimeAtIndex(TimeFrameIndex(2));
        REQUIRE(last_frame.getValue() == 1100);

        // Test reverse lookup
        REQUIRE(camera_frame->getIndexAtTime(CameraFrameIndex(1001)) == TimeFrameIndex(0));
        REQUIRE(camera_frame->getIndexAtTime(CameraFrameIndex(1050)) == TimeFrameIndex(1));
    }

    SECTION("Dense CameraTimeFrame - ZERO_TO_MAX") {
        options.mode = FilenameTimeFrameMode::ZERO_TO_MAX;
        auto camera_frame = TimeFrameUtils::createCameraTimeFrameFromFilenames(options);

        REQUIRE(camera_frame != nullptr);
        REQUIRE(camera_frame->getTotalFrameCount() == 1101);// 0 to 1100 inclusive
        REQUIRE(camera_frame->isDense());
        REQUIRE_FALSE(camera_frame->isSparse());

        // Test coordinate access
        CameraFrameIndex first_frame = camera_frame->getTimeAtIndex(TimeFrameIndex(0));
        REQUIRE(first_frame.getValue() == 0);

        CameraFrameIndex middle_frame = camera_frame->getTimeAtIndex(TimeFrameIndex(500));
        REQUIRE(middle_frame.getValue() == 500);

        CameraFrameIndex last_frame = camera_frame->getTimeAtIndex(TimeFrameIndex(1100));
        REQUIRE(last_frame.getValue() == 1100);
    }

    SECTION("Dense CameraTimeFrame - MIN_TO_MAX") {
        options.mode = FilenameTimeFrameMode::MIN_TO_MAX;
        auto camera_frame = TimeFrameUtils::createCameraTimeFrameFromFilenames(options);

        REQUIRE(camera_frame != nullptr);
        REQUIRE(camera_frame->getTotalFrameCount() == 100);// 1001 to 1100 inclusive
        REQUIRE(camera_frame->isDense());

        // Test coordinate access
        CameraFrameIndex first_frame = camera_frame->getTimeAtIndex(TimeFrameIndex(0));
        REQUIRE(first_frame.getValue() == 1001);

        CameraFrameIndex last_frame = camera_frame->getTimeAtIndex(TimeFrameIndex(99));
        REQUIRE(last_frame.getValue() == 1100);
    }
}

TEST_CASE("TimeFrameV2 - UncalibratedTimeFrame from filenames", "[timeframev2][filename][uncalibrated]") {
    TempTestDirectory temp_dir;

    // Create test files with arbitrary indices
    temp_dir.createFile("data_12345.dat");
    temp_dir.createFile("data_67890.dat");
    temp_dir.createFile("data_99999.dat");

    FilenameTimeFrameOptions options;
    options.folder_path = temp_dir.getPath();
    options.file_extension = ".dat";
    options.regex_pattern = R"(data_(\d+)\.dat)";
    options.mode = FilenameTimeFrameMode::FOUND_VALUES;

    auto uncalib_frame = TimeFrameUtils::createUncalibratedTimeFrameFromFilenames(options);

    REQUIRE(uncalib_frame != nullptr);
    REQUIRE(uncalib_frame->getTotalFrameCount() == 3);
    REQUIRE(uncalib_frame->isSparse());

    // Test coordinate access
    UncalibratedIndex first_index = uncalib_frame->getTimeAtIndex(TimeFrameIndex(0));
    REQUIRE(first_index.getValue() == 12345);

    UncalibratedIndex last_index = uncalib_frame->getTimeAtIndex(TimeFrameIndex(2));
    REQUIRE(last_index.getValue() == 99999);

    // Test reverse lookup
    REQUIRE(uncalib_frame->getIndexAtTime(UncalibratedIndex(12345)) == TimeFrameIndex(0));
    REQUIRE(uncalib_frame->getIndexAtTime(UncalibratedIndex(67890)) == TimeFrameIndex(1));
}

TEST_CASE("TimeFrameV2 - Error handling", "[timeframev2][filename][errors]") {
    FilenameTimeFrameOptions invalid_options;
    invalid_options.folder_path = "/nonexistent/path";
    invalid_options.file_extension = ".jpg";
    invalid_options.regex_pattern = R"(frame_(\d+)\.jpg)";
    invalid_options.mode = FilenameTimeFrameMode::FOUND_VALUES;

    SECTION("CameraTimeFrame error handling") {
        auto camera_frame = TimeFrameUtils::createCameraTimeFrameFromFilenames(invalid_options);
        REQUIRE(camera_frame == nullptr);
    }

    SECTION("UncalibratedTimeFrame error handling") {
        auto uncalib_frame = TimeFrameUtils::createUncalibratedTimeFrameFromFilenames(invalid_options);
        REQUIRE(uncalib_frame == nullptr);
    }
}