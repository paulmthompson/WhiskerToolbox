/**
 * @file CsvDatasetPreviewModel.test.cpp
 * @brief Unit tests for CsvDatasetPreviewModel
 *
 * Tests cover:
 * - Basic CSV parsing with comma delimiter
 * - Tab-separated values
 * - Semicolon-separated values
 * - Custom delimiters
 * - Header line handling (0, 1, multiple)
 * - Column name extraction from header
 * - Quote character handling
 * - Whitespace trimming
 * - Empty files and edge cases
 * - Chunk-based lazy loading
 * - Row/column count limits
 */

#include "CsvDatasetPreviewModel.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <catch2/catch_test_macros.hpp>

namespace {

// Helper to create a temporary CSV file and return its path
QString createTempCsvFile(QTemporaryDir & temp_dir, QString const & filename,
                          QString const & content) {
    QString file_path = temp_dir.path() + "/" + filename;
    QFile file(file_path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << content;
        file.close();
    }
    return file_path;
}

// Ensure QCoreApplication exists for Qt tests
struct AppGuard {
    AppGuard() {
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char const * argv[] = {"test"};
            static QCoreApplication app(argc, const_cast<char **>(argv));
        }
    }
};

static AppGuard const app_guard;

} // anonymous namespace

TEST_CASE("CsvDatasetPreviewModel - Basic comma-separated", "[CsvExplorer]") {
    QTemporaryDir temp_dir;
    REQUIRE(temp_dir.isValid());

    QString content = "name,age,city\nAlice,30,NYC\nBob,25,LA\nCharlie,35,Chicago\n";
    auto file_path = createTempCsvFile(temp_dir, "basic.csv", content);

    CsvDatasetPreviewModel model;
    CsvParseConfig config;
    config.column_delimiter = ",";
    config.header_lines = 1;
    config.use_first_header_as_names = true;

    REQUIRE(model.loadFile(file_path, config));
    REQUIRE(model.hasData());

    CHECK(model.rowCount() == 3);
    CHECK(model.columnCount() == 3);

    // Check column headers
    CHECK(model.headerData(0, Qt::Horizontal).toString() == "name");
    CHECK(model.headerData(1, Qt::Horizontal).toString() == "age");
    CHECK(model.headerData(2, Qt::Horizontal).toString() == "city");

    // Check data
    CHECK(model.data(model.index(0, 0)).toString() == "Alice");
    CHECK(model.data(model.index(0, 1)).toString() == "30");
    CHECK(model.data(model.index(0, 2)).toString() == "NYC");
    CHECK(model.data(model.index(1, 0)).toString() == "Bob");
    CHECK(model.data(model.index(2, 0)).toString() == "Charlie");
}

TEST_CASE("CsvDatasetPreviewModel - Tab-separated", "[CsvExplorer]") {
    QTemporaryDir temp_dir;
    REQUIRE(temp_dir.isValid());

    QString content = "col1\tcol2\tcol3\n1\t2\t3\n4\t5\t6\n";
    auto file_path = createTempCsvFile(temp_dir, "tabs.tsv", content);

    CsvDatasetPreviewModel model;
    CsvParseConfig config;
    config.column_delimiter = "\t";
    config.header_lines = 1;
    config.use_first_header_as_names = true;

    REQUIRE(model.loadFile(file_path, config));

    CHECK(model.rowCount() == 2);
    CHECK(model.columnCount() == 3);
    CHECK(model.headerData(0, Qt::Horizontal).toString() == "col1");
    CHECK(model.data(model.index(0, 0)).toString() == "1");
    CHECK(model.data(model.index(1, 2)).toString() == "6");
}

TEST_CASE("CsvDatasetPreviewModel - Semicolon-separated", "[CsvExplorer]") {
    QTemporaryDir temp_dir;
    REQUIRE(temp_dir.isValid());

    QString content = "x;y;z\n1.5;2.5;3.5\n4.5;5.5;6.5\n";
    auto file_path = createTempCsvFile(temp_dir, "semicolons.csv", content);

    CsvDatasetPreviewModel model;
    CsvParseConfig config;
    config.column_delimiter = ";";
    config.header_lines = 1;
    config.use_first_header_as_names = true;

    REQUIRE(model.loadFile(file_path, config));

    CHECK(model.rowCount() == 2);
    CHECK(model.columnCount() == 3);
    CHECK(model.data(model.index(0, 0)).toString() == "1.5");
}

TEST_CASE("CsvDatasetPreviewModel - No header lines", "[CsvExplorer]") {
    QTemporaryDir temp_dir;
    REQUIRE(temp_dir.isValid());

    QString content = "1,2,3\n4,5,6\n7,8,9\n";
    auto file_path = createTempCsvFile(temp_dir, "no_header.csv", content);

    CsvDatasetPreviewModel model;
    CsvParseConfig config;
    config.column_delimiter = ",";
    config.header_lines = 0;
    config.use_first_header_as_names = false;

    REQUIRE(model.loadFile(file_path, config));

    CHECK(model.rowCount() == 3);
    CHECK(model.columnCount() == 3);

    // Column names should be auto-generated
    CHECK(model.headerData(0, Qt::Horizontal).toString() == "Col0");
    CHECK(model.headerData(1, Qt::Horizontal).toString() == "Col1");
    CHECK(model.headerData(2, Qt::Horizontal).toString() == "Col2");

    CHECK(model.data(model.index(0, 0)).toString() == "1");
    CHECK(model.data(model.index(2, 2)).toString() == "9");
}

TEST_CASE("CsvDatasetPreviewModel - Multiple header lines", "[CsvExplorer]") {
    QTemporaryDir temp_dir;
    REQUIRE(temp_dir.isValid());

    QString content = "# This is a comment\nname,value\nA,1\nB,2\n";
    auto file_path = createTempCsvFile(temp_dir, "multi_header.csv", content);

    CsvDatasetPreviewModel model;
    CsvParseConfig config;
    config.column_delimiter = ",";
    config.header_lines = 2;
    config.use_first_header_as_names = true;

    REQUIRE(model.loadFile(file_path, config));

    CHECK(model.rowCount() == 2);

    // First header line is the comment, used as column names
    CHECK(model.headerData(0, Qt::Horizontal).toString() == "# This is a comment");
}

TEST_CASE("CsvDatasetPreviewModel - Quoted fields", "[CsvExplorer]") {
    QTemporaryDir temp_dir;
    REQUIRE(temp_dir.isValid());

    QString content = "name,description,value\n"
                      "\"Smith, John\",\"He said \"\"hello\"\"\",42\n"
                      "Jane,Simple,99\n";
    auto file_path = createTempCsvFile(temp_dir, "quoted.csv", content);

    CsvDatasetPreviewModel model;
    CsvParseConfig config;
    config.column_delimiter = ",";
    config.header_lines = 1;
    config.use_first_header_as_names = true;
    config.quote_char = "\"";

    REQUIRE(model.loadFile(file_path, config));

    CHECK(model.rowCount() == 2);
    CHECK(model.columnCount() == 3);

    // Quoted field with comma inside
    CHECK(model.data(model.index(0, 0)).toString() == "Smith, John");
    // Escaped quotes
    CHECK(model.data(model.index(0, 1)).toString() == "He said \"hello\"");
    CHECK(model.data(model.index(0, 2)).toString() == "42");
}

TEST_CASE("CsvDatasetPreviewModel - Whitespace trimming", "[CsvExplorer]") {
    QTemporaryDir temp_dir;
    REQUIRE(temp_dir.isValid());

    QString content = " a , b , c \n 1 , 2 , 3 \n";
    auto file_path = createTempCsvFile(temp_dir, "whitespace.csv", content);

    CsvDatasetPreviewModel model;

    SECTION("With trimming enabled") {
        CsvParseConfig config;
        config.column_delimiter = ",";
        config.header_lines = 1;
        config.use_first_header_as_names = true;
        config.trim_whitespace = true;

        REQUIRE(model.loadFile(file_path, config));
        CHECK(model.headerData(0, Qt::Horizontal).toString() == "a");
        CHECK(model.data(model.index(0, 1)).toString() == "2");
    }

    SECTION("With trimming disabled") {
        CsvParseConfig config;
        config.column_delimiter = ",";
        config.header_lines = 1;
        config.use_first_header_as_names = true;
        config.trim_whitespace = false;

        REQUIRE(model.loadFile(file_path, config));
        CHECK(model.headerData(0, Qt::Horizontal).toString() == " a ");
        CHECK(model.data(model.index(0, 1)).toString() == " 2 ");
    }
}

TEST_CASE("CsvDatasetPreviewModel - File info", "[CsvExplorer]") {
    QTemporaryDir temp_dir;
    REQUIRE(temp_dir.isValid());

    QString content = "h1,h2\nr1c1,r1c2\nr2c1,r2c2\nr3c1,r3c2\n";
    auto file_path = createTempCsvFile(temp_dir, "info.csv", content);

    CsvDatasetPreviewModel model;
    CsvParseConfig config;
    config.column_delimiter = ",";
    config.header_lines = 1;

    REQUIRE(model.loadFile(file_path, config));

    auto const & info = model.fileInfo();
    CHECK(info.total_lines == 4);
    CHECK(info.data_rows == 3);
    CHECK(info.detected_columns == 2);
    CHECK(info.column_names.size() == 2);
    CHECK(info.file_size > 0);
}

TEST_CASE("CsvDatasetPreviewModel - Empty file", "[CsvExplorer]") {
    QTemporaryDir temp_dir;
    REQUIRE(temp_dir.isValid());

    auto file_path = createTempCsvFile(temp_dir, "empty.csv", "");

    CsvDatasetPreviewModel model;
    CsvParseConfig config;

    CHECK_FALSE(model.loadFile(file_path, config));
    CHECK_FALSE(model.hasData());
}

TEST_CASE("CsvDatasetPreviewModel - Nonexistent file", "[CsvExplorer]") {
    CsvDatasetPreviewModel model;
    CsvParseConfig config;

    CHECK_FALSE(model.loadFile("/nonexistent/path/file.csv", config));
    CHECK_FALSE(model.hasData());
}

TEST_CASE("CsvDatasetPreviewModel - Clear", "[CsvExplorer]") {
    QTemporaryDir temp_dir;
    REQUIRE(temp_dir.isValid());

    QString content = "a,b\n1,2\n";
    auto file_path = createTempCsvFile(temp_dir, "clear.csv", content);

    CsvDatasetPreviewModel model;
    CsvParseConfig config;
    config.header_lines = 1;

    REQUIRE(model.loadFile(file_path, config));
    REQUIRE(model.hasData());

    model.clear();
    CHECK_FALSE(model.hasData());
    CHECK(model.rowCount() == 0);
    CHECK(model.columnCount() == 0);
}

TEST_CASE("CsvDatasetPreviewModel - Reload with different config", "[CsvExplorer]") {
    QTemporaryDir temp_dir;
    REQUIRE(temp_dir.isValid());

    // File with data that can be interpreted with or without header
    QString content = "a,b,c\n1,2,3\n4,5,6\n";
    auto file_path = createTempCsvFile(temp_dir, "reload.csv", content);

    CsvDatasetPreviewModel model;

    // First load with header
    CsvParseConfig config1;
    config1.column_delimiter = ",";
    config1.header_lines = 1;
    config1.use_first_header_as_names = true;

    REQUIRE(model.loadFile(file_path, config1));
    CHECK(model.rowCount() == 2);
    CHECK(model.headerData(0, Qt::Horizontal).toString() == "a");

    // Reload without header
    CsvParseConfig config2;
    config2.column_delimiter = ",";
    config2.header_lines = 0;
    config2.use_first_header_as_names = false;

    REQUIRE(model.reloadWithConfig(config2));
    CHECK(model.rowCount() == 3);
    CHECK(model.headerData(0, Qt::Horizontal).toString() == "Col0");
    CHECK(model.data(model.index(0, 0)).toString() == "a");
}

TEST_CASE("CsvDatasetPreviewModel - Pipe delimiter", "[CsvExplorer]") {
    QTemporaryDir temp_dir;
    REQUIRE(temp_dir.isValid());

    QString content = "key|value\nfoo|bar\nbaz|qux\n";
    auto file_path = createTempCsvFile(temp_dir, "pipe.csv", content);

    CsvDatasetPreviewModel model;
    CsvParseConfig config;
    config.column_delimiter = "|";
    config.header_lines = 1;
    config.use_first_header_as_names = true;

    REQUIRE(model.loadFile(file_path, config));

    CHECK(model.rowCount() == 2);
    CHECK(model.columnCount() == 2);
    CHECK(model.headerData(0, Qt::Horizontal).toString() == "key");
    CHECK(model.data(model.index(0, 1)).toString() == "bar");
}

TEST_CASE("CsvDatasetPreviewModel - Single column", "[CsvExplorer]") {
    QTemporaryDir temp_dir;
    REQUIRE(temp_dir.isValid());

    QString content = "value\n10\n20\n30\n";
    auto file_path = createTempCsvFile(temp_dir, "single_col.csv", content);

    CsvDatasetPreviewModel model;
    CsvParseConfig config;
    config.column_delimiter = ",";
    config.header_lines = 1;
    config.use_first_header_as_names = true;

    REQUIRE(model.loadFile(file_path, config));

    CHECK(model.rowCount() == 3);
    CHECK(model.columnCount() == 1);
    CHECK(model.headerData(0, Qt::Horizontal).toString() == "value");
    CHECK(model.data(model.index(0, 0)).toString() == "10");
}

TEST_CASE("CsvDatasetPreviewModel - Ragged rows (uneven columns)", "[CsvExplorer]") {
    QTemporaryDir temp_dir;
    REQUIRE(temp_dir.isValid());

    // Second row has fewer columns, third has more
    QString content = "a,b,c\n1,2\n3,4,5,6\n";
    auto file_path = createTempCsvFile(temp_dir, "ragged.csv", content);

    CsvDatasetPreviewModel model;
    CsvParseConfig config;
    config.column_delimiter = ",";
    config.header_lines = 1;
    config.use_first_header_as_names = true;

    REQUIRE(model.loadFile(file_path, config));

    CHECK(model.rowCount() == 2);

    // Short row: missing columns return empty QVariant
    CHECK(model.data(model.index(0, 0)).toString() == "1");
    CHECK(model.data(model.index(0, 1)).toString() == "2");
}

TEST_CASE("CsvDatasetPreviewModel - Chunk size", "[CsvExplorer]") {
    CsvDatasetPreviewModel model;

    CHECK(model.chunkSize() == 100); // default
    model.setChunkSize(50);
    CHECK(model.chunkSize() == 50);
    model.setChunkSize(5);
    CHECK(model.chunkSize() == 10); // minimum enforced
}

TEST_CASE("CsvDatasetPreviewModel - Row header shows row number", "[CsvExplorer]") {
    QTemporaryDir temp_dir;
    REQUIRE(temp_dir.isValid());

    QString content = "h\n1\n2\n3\n";
    auto file_path = createTempCsvFile(temp_dir, "rowheader.csv", content);

    CsvDatasetPreviewModel model;
    CsvParseConfig config;
    config.header_lines = 1;

    REQUIRE(model.loadFile(file_path, config));

    CHECK(model.headerData(0, Qt::Vertical).toString() == "0");
    CHECK(model.headerData(1, Qt::Vertical).toString() == "1");
    CHECK(model.headerData(2, Qt::Vertical).toString() == "2");
}

TEST_CASE("CsvDatasetPreviewModel - Invalid index returns empty", "[CsvExplorer]") {
    QTemporaryDir temp_dir;
    REQUIRE(temp_dir.isValid());

    QString content = "a\n1\n";
    auto file_path = createTempCsvFile(temp_dir, "invalid_idx.csv", content);

    CsvDatasetPreviewModel model;
    CsvParseConfig config;
    config.header_lines = 1;

    REQUIRE(model.loadFile(file_path, config));

    // Invalid model index
    CHECK_FALSE(model.data(QModelIndex()).isValid());
    // Out of range column
    CHECK_FALSE(model.data(model.index(0, 99)).isValid());
}

TEST_CASE("CsvDatasetPreviewModel - Space delimiter", "[CsvExplorer]") {
    QTemporaryDir temp_dir;
    REQUIRE(temp_dir.isValid());

    QString content = "x y z\n1 2 3\n4 5 6\n";
    auto file_path = createTempCsvFile(temp_dir, "space.txt", content);

    CsvDatasetPreviewModel model;
    CsvParseConfig config;
    config.column_delimiter = " ";
    config.header_lines = 1;
    config.use_first_header_as_names = true;
    config.trim_whitespace = false; // Important when using space as delimiter

    REQUIRE(model.loadFile(file_path, config));

    CHECK(model.rowCount() == 2);
    CHECK(model.columnCount() == 3);
    CHECK(model.headerData(0, Qt::Horizontal).toString() == "x");
}

TEST_CASE("CsvDatasetPreviewModel - Only header, no data", "[CsvExplorer]") {
    QTemporaryDir temp_dir;
    REQUIRE(temp_dir.isValid());

    QString content = "header_only\n";
    auto file_path = createTempCsvFile(temp_dir, "header_only.csv", content);

    CsvDatasetPreviewModel model;
    CsvParseConfig config;
    config.header_lines = 1;

    // Should fail because there are no data rows
    CHECK_FALSE(model.loadFile(file_path, config));
}
