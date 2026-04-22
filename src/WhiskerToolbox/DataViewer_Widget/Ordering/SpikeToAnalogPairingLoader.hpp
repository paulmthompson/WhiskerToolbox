/**
 * @file SpikeToAnalogPairingLoader.hpp
 * @brief Parser for CSV spike-to-analog channel pairing files.
 *
 * Parses a CSV file where each row records a spike event. The column positions
 * for the digital and analog channel indices, the field delimiter, and whether
 * indices are 1-based or 0-based are all configurable via
 * @c SpikeToAnalogParseConfig.
 *
 * For each unique digital_channel, the analog_channel that appears most often
 * (mode) is returned as the pairing.  Channel numbers are converted to 0-based
 * in the output according to the configured indexing policy.
 *
 * ## Placement Modes
 *
 * Three placement policies are supported when applying pairings to the
 * DataViewer lane stack:
 *
 * - **AdjacentBelow**: each `spikes_N` is placed in its own lane immediately
 *   below its paired `voltage_M`.
 * - **AdjacentAbove**: each `spikes_N` is placed immediately above its paired
 *   `voltage_M`.
 * - **Overlay**: each `spikes_N` shares the `lane_id` of its paired `voltage_M`
 *   (rendered in the same lane band with `overlay_mode = Overlay`).
 */

#ifndef DATAVIEWER_SPIKE_TO_ANALOG_PAIRING_LOADER_HPP
#define DATAVIEWER_SPIKE_TO_ANALOG_PAIRING_LOADER_HPP

#include <string>
#include <vector>

/**
 * @brief Field delimiter used in the spike-to-analog pairing CSV file.
 */
enum class SpikeToAnalogCsvDelimiter {
    Comma,     ///< Comma (',') — default
    Tab,       ///< Tab ('\t')
    Semicolon, ///< Semicolon (';')
    Whitespace,///< Any whitespace (space or tab)
    Pipe,      ///< Pipe ('|')
};

/**
 * @brief Parse options for @c parseSpikeToAnalogCSV.
 *
 * All column indices are 1-based (column 1 = first field in each row).
 */
struct SpikeToAnalogParseConfig {
    SpikeToAnalogCsvDelimiter delimiter{SpikeToAnalogCsvDelimiter::Comma};
    int digital_column{2};            ///< 1-based column index for the digital channel number
    int analog_column{3};             ///< 1-based column index for the analog channel number
    bool digital_one_based{true};     ///< true = subtract 1 from digital channel (CSV uses 1-based)
    bool analog_one_based{true};      ///< true = subtract 1 from analog channel (CSV uses 1-based)
    bool digital_key_one_based{true}; ///< true = destination series key suffix is 1-based (e.g. "spikes_1" = ch 0)
    bool analog_key_one_based{true};  ///< true = destination series key suffix is 1-based (e.g. "voltage_1" = ch 0)
};

/**
 * @brief One pairing between a digital spike channel and an analog LFP channel.
 *
 * Both channel numbers are 0-based.
 */
struct SpikeToAnalogPairing {
    int digital_channel{0};///< 0-based digital (spike) channel index
    int analog_channel{0}; ///< 0-based analog (LFP) channel index (modal pairing)
};

/**
 * @brief Placement policy for spike-to-analog pairing loader.
 */
enum class SpikeToAnalogPlacementMode {
    AdjacentBelow = 0,///< Place spike series in own lane immediately below the analog lane
    AdjacentAbove = 1,///< Place spike series in own lane immediately above the analog lane
    Overlay = 2,      ///< Spike series shares the analog series' lane band
};

/**
 * @brief Parse a spike-to-analog pairing CSV and compute the modal analog channel
 *        for each unique digital channel.
 *
 * Each row is split by the delimiter specified in @p config. The digital and
 * analog channel numbers are read from the configured column positions
 * (1-based column indices). Rows that cannot be parsed are silently skipped.
 *
 * When multiple analog channels appear for the same digital channel the most
 * frequent one (mode) is chosen.  Ties are broken by choosing the smallest
 * analog channel index.
 *
 * @param text   Raw CSV text content
 * @param config Parse options (delimiter, column positions, index base)
 * @return One @c SpikeToAnalogPairing per unique digital channel (0-based indices).
 *         Returns an empty vector if no valid rows are found.
 */
[[nodiscard]] std::vector<SpikeToAnalogPairing> parseSpikeToAnalogCSV(
        std::string const & text,
        SpikeToAnalogParseConfig const & config = SpikeToAnalogParseConfig{});

#endif// DATAVIEWER_SPIKE_TO_ANALOG_PAIRING_LOADER_HPP
