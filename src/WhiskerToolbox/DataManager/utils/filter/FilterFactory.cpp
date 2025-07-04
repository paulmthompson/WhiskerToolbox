#include "FilterFactory.hpp"

std::unique_ptr<IFilter> FilterFactory::createFromOptions(FilterOptions const& options) {
    // Validate options first
    if (!options.isValid()) {
        throw std::invalid_argument("Invalid filter options: " + options.getValidationError());
    }

    // Now support all filter types
    if (options.type != FilterType::Butterworth && 
        options.type != FilterType::ChebyshevI && 
        options.type != FilterType::ChebyshevII && 
        options.type != FilterType::RBJ) {
        throw std::invalid_argument("Unsupported filter type in new interface");
    }

    // Create the appropriate filter based on type and response
    if (options.type == FilterType::Butterworth) {
        switch (options.response) {
            case FilterResponse::LowPass: {
                auto creator = [&]<int Order>() {
                    return createButterworthLowpass<Order>(
                        options.cutoff_frequency_hz,
                        options.sampling_rate_hz,
                        options.zero_phase
                    );
                };
                return createWithRuntimeOrder(options.order, creator);
            }

            case FilterResponse::HighPass: {
                auto creator = [&]<int Order>() {
                    return createButterworthHighpass<Order>(
                        options.cutoff_frequency_hz,
                        options.sampling_rate_hz,
                        options.zero_phase
                    );
                };
                return createWithRuntimeOrder(options.order, creator);
            }

            case FilterResponse::BandPass: {
                auto creator = [&]<int Order>() {
                    return createButterworthBandpass<Order>(
                        options.cutoff_frequency_hz,
                        options.high_cutoff_hz,
                        options.sampling_rate_hz,
                        options.zero_phase
                    );
                };
                return createWithRuntimeOrder(options.order, creator);
            }

            case FilterResponse::BandStop: {
                auto creator = [&]<int Order>() {
                    return createButterworthBandstop<Order>(
                        options.cutoff_frequency_hz,
                        options.high_cutoff_hz,
                        options.sampling_rate_hz,
                        options.zero_phase
                    );
                };
                return createWithRuntimeOrder(options.order, creator);
            }

            case FilterResponse::LowShelf:
            case FilterResponse::HighShelf:
                throw std::invalid_argument("Butterworth filter response type not yet supported in new interface");

            default:
                throw std::invalid_argument("Unknown filter response type");
        }
    } else if (options.type == FilterType::ChebyshevI) {
        switch (options.response) {
            case FilterResponse::LowPass: {
                auto creator = [&]<int Order>() {
                    return createChebyshevILowpass<Order>(
                        options.cutoff_frequency_hz,
                        options.sampling_rate_hz,
                        options.passband_ripple_db,
                        options.zero_phase
                    );
                };
                return createWithRuntimeOrder(options.order, creator);
            }

            case FilterResponse::HighPass: {
                auto creator = [&]<int Order>() {
                    return createChebyshevIHighpass<Order>(
                        options.cutoff_frequency_hz,
                        options.sampling_rate_hz,
                        options.passband_ripple_db,
                        options.zero_phase
                    );
                };
                return createWithRuntimeOrder(options.order, creator);
            }

            case FilterResponse::BandPass: {
                auto creator = [&]<int Order>() {
                    return createChebyshevIBandpass<Order>(
                        options.cutoff_frequency_hz,
                        options.high_cutoff_hz,
                        options.sampling_rate_hz,
                        options.passband_ripple_db,
                        options.zero_phase
                    );
                };
                return createWithRuntimeOrder(options.order, creator);
            }

            case FilterResponse::BandStop: {
                auto creator = [&]<int Order>() {
                    return createChebyshevIBandstop<Order>(
                        options.cutoff_frequency_hz,
                        options.high_cutoff_hz,
                        options.sampling_rate_hz,
                        options.passband_ripple_db,
                        options.zero_phase
                    );
                };
                return createWithRuntimeOrder(options.order, creator);
            }

            case FilterResponse::LowShelf:
            case FilterResponse::HighShelf:
                throw std::invalid_argument("Chebyshev I shelf filter response type not yet supported in new interface");

            default:
                throw std::invalid_argument("Unknown filter response type");
        }
    } else if (options.type == FilterType::ChebyshevII) {
        switch (options.response) {
            case FilterResponse::LowPass: {
                auto creator = [&]<int Order>() {
                    return createChebyshevIILowpass<Order>(
                        options.cutoff_frequency_hz,
                        options.sampling_rate_hz,
                        options.stopband_ripple_db,
                        options.zero_phase
                    );
                };
                return createWithRuntimeOrder(options.order, creator);
            }

            case FilterResponse::HighPass: {
                auto creator = [&]<int Order>() {
                    return createChebyshevIIHighpass<Order>(
                        options.cutoff_frequency_hz,
                        options.sampling_rate_hz,
                        options.stopband_ripple_db,
                        options.zero_phase
                    );
                };
                return createWithRuntimeOrder(options.order, creator);
            }

            case FilterResponse::BandPass: {
                auto creator = [&]<int Order>() {
                    return createChebyshevIIBandpass<Order>(
                        options.cutoff_frequency_hz,
                        options.high_cutoff_hz,
                        options.sampling_rate_hz,
                        options.stopband_ripple_db,
                        options.zero_phase
                    );
                };
                return createWithRuntimeOrder(options.order, creator);
            }

            case FilterResponse::BandStop: {
                auto creator = [&]<int Order>() {
                    return createChebyshevIIBandstop<Order>(
                        options.cutoff_frequency_hz,
                        options.high_cutoff_hz,
                        options.sampling_rate_hz,
                        options.stopband_ripple_db,
                        options.zero_phase
                    );
                };
                return createWithRuntimeOrder(options.order, creator);
            }

            case FilterResponse::LowShelf:
            case FilterResponse::HighShelf:
                throw std::invalid_argument("Chebyshev II shelf filter response type not yet supported in new interface");

            default:
                throw std::invalid_argument("Unknown filter response type");
        }
    } else if (options.type == FilterType::RBJ) {
        switch (options.response) {
            case FilterResponse::LowPass:
                return createRBJLowpass(
                    options.cutoff_frequency_hz,
                    options.sampling_rate_hz,
                    options.q_factor,
                    options.zero_phase
                );

            case FilterResponse::HighPass:
                return createRBJHighpass(
                    options.cutoff_frequency_hz,
                    options.sampling_rate_hz,
                    options.q_factor,
                    options.zero_phase
                );

            case FilterResponse::BandPass: {
                // For RBJ bandpass, we need center frequency and Q factor
                // Use cutoff_frequency_hz as center frequency, calculate Q from bandwidth if available
                double center_freq = options.cutoff_frequency_hz;
                double q_factor = options.q_factor;
                
                // If both cutoffs are specified, calculate center frequency and Q
                if (options.high_cutoff_hz > options.cutoff_frequency_hz) {
                    center_freq = (options.cutoff_frequency_hz + options.high_cutoff_hz) / 2.0;
                    double bandwidth = options.high_cutoff_hz - options.cutoff_frequency_hz;
                    q_factor = center_freq / bandwidth;  // Q = fc / BW
                }
                
                return createRBJBandpass(
                    center_freq,
                    options.sampling_rate_hz,
                    q_factor,
                    options.zero_phase
                );
            }

            case FilterResponse::BandStop:
                // For RBJ bandstop/notch, use cutoff_frequency_hz as center frequency
                return createRBJBandstop(
                    options.cutoff_frequency_hz,
                    options.sampling_rate_hz,
                    options.q_factor,
                    options.zero_phase
                );

            case FilterResponse::LowShelf:
            case FilterResponse::HighShelf:
                throw std::invalid_argument("RBJ shelf filter response type not supported");

            default:
                throw std::invalid_argument("Unknown filter response type");
        }
    }
    
    throw std::invalid_argument("Unsupported filter type");
}
