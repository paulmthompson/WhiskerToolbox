#ifndef FILTER_FACTORY_HPP
#define FILTER_FACTORY_HPP

#include "IFilter.hpp"
#include "FilterImplementations.hpp"
#include "ZeroPhaseDecorator.hpp"

#include <memory>
#include <stdexcept>

/**
 * @brief Factory for creating filters with the new interface
 * 
 * This factory provides a way to create filters from FilterOptions (for compatibility)
 * or directly with specific parameters.
 */
class FilterFactory {
public:
    /**
     * @brief Create a Butterworth lowpass filter
     * 
     * @tparam Order Filter order (1-8)
     * @param cutoff_hz Cutoff frequency in Hz
     * @param sampling_rate_hz Sampling rate in Hz
     * @param zero_phase Whether to apply zero-phase filtering
     * @return Unique pointer to the created filter
     */
    template<int Order>
    static std::unique_ptr<IFilter> createButterworthLowpass(
            double cutoff_hz, 
            double sampling_rate_hz, 
            bool zero_phase = false) {
        static_assert(Order >= 1 && Order <= 8, "Filter order must be between 1 and 8");
        
        auto filter = std::make_unique<ButterworthLowpassFilter<Order>>(cutoff_hz, sampling_rate_hz);
        
        if (zero_phase) {
            return std::make_unique<ZeroPhaseDecorator>(std::move(filter));
        }
        
        return filter;
    }

    /**
     * @brief Create a Butterworth highpass filter
     * 
     * @tparam Order Filter order (1-8)
     * @param cutoff_hz Cutoff frequency in Hz
     * @param sampling_rate_hz Sampling rate in Hz
     * @param zero_phase Whether to apply zero-phase filtering
     * @return Unique pointer to the created filter
     */
    template<int Order>
    static std::unique_ptr<IFilter> createButterworthHighpass(
            double cutoff_hz, 
            double sampling_rate_hz, 
            bool zero_phase = false) {
        static_assert(Order >= 1 && Order <= 8, "Filter order must be between 1 and 8");
        
        auto filter = std::make_unique<ButterworthHighpassFilter<Order>>(cutoff_hz, sampling_rate_hz);
        
        if (zero_phase) {
            return std::make_unique<ZeroPhaseDecorator>(std::move(filter));
        }
        
        return filter;
    }

    /**
     * @brief Create a Butterworth bandpass filter
     * 
     * @tparam Order Filter order (1-8)
     * @param low_cutoff_hz Low cutoff frequency in Hz
     * @param high_cutoff_hz High cutoff frequency in Hz
     * @param sampling_rate_hz Sampling rate in Hz
     * @param zero_phase Whether to apply zero-phase filtering
     * @return Unique pointer to the created filter
     */
    template<int Order>
    static std::unique_ptr<IFilter> createButterworthBandpass(
            double low_cutoff_hz,
            double high_cutoff_hz,
            double sampling_rate_hz, 
            bool zero_phase = false) {
        static_assert(Order >= 1 && Order <= 8, "Filter order must be between 1 and 8");
        
        auto filter = std::make_unique<ButterworthBandpassFilter<Order>>(
            low_cutoff_hz, high_cutoff_hz, sampling_rate_hz);
        
        if (zero_phase) {
            return std::make_unique<ZeroPhaseDecorator>(std::move(filter));
        }
        
        return filter;
    }

    /**
     * @brief Create a Butterworth bandstop filter
     * 
     * @tparam Order Filter order (1-8)
     * @param low_cutoff_hz Low cutoff frequency in Hz
     * @param high_cutoff_hz High cutoff frequency in Hz
     * @param sampling_rate_hz Sampling rate in Hz
     * @param zero_phase Whether to apply zero-phase filtering
     * @return Unique pointer to the created filter
     */
    template<int Order>
    static std::unique_ptr<IFilter> createButterworthBandstop(
            double low_cutoff_hz,
            double high_cutoff_hz,
            double sampling_rate_hz, 
            bool zero_phase = false) {
        static_assert(Order >= 1 && Order <= 8, "Filter order must be between 1 and 8");
        
        auto filter = std::make_unique<ButterworthBandstopFilter<Order>>(
            low_cutoff_hz, high_cutoff_hz, sampling_rate_hz);
        
        if (zero_phase) {
            return std::make_unique<ZeroPhaseDecorator>(std::move(filter));
        }
        
        return filter;
    }

    /**
     * @brief Create a Chebyshev I lowpass filter
     * 
     * @tparam Order Filter order (1-8)
     * @param cutoff_hz Cutoff frequency in Hz
     * @param sampling_rate_hz Sampling rate in Hz
     * @param ripple_db Passband ripple in dB
     * @param zero_phase Whether to apply zero-phase filtering
     * @return Unique pointer to the created filter
     */
    template<int Order>
    static std::unique_ptr<IFilter> createChebyshevILowpass(
            double cutoff_hz, 
            double sampling_rate_hz, 
            double ripple_db,
            bool zero_phase = false) {
        static_assert(Order >= 1 && Order <= 8, "Filter order must be between 1 and 8");
        
        auto filter = std::make_unique<ChebyshevILowpassFilter<Order>>(cutoff_hz, sampling_rate_hz, ripple_db);
        
        if (zero_phase) {
            return std::make_unique<ZeroPhaseDecorator>(std::move(filter));
        }
        
        return filter;
    }

    /**
     * @brief Create a Chebyshev I highpass filter
     * 
     * @tparam Order Filter order (1-8)
     * @param cutoff_hz Cutoff frequency in Hz
     * @param sampling_rate_hz Sampling rate in Hz
     * @param ripple_db Passband ripple in dB
     * @param zero_phase Whether to apply zero-phase filtering
     * @return Unique pointer to the created filter
     */
    template<int Order>
    static std::unique_ptr<IFilter> createChebyshevIHighpass(
            double cutoff_hz, 
            double sampling_rate_hz, 
            double ripple_db,
            bool zero_phase = false) {
        static_assert(Order >= 1 && Order <= 8, "Filter order must be between 1 and 8");
        
        auto filter = std::make_unique<ChebyshevIHighpassFilter<Order>>(cutoff_hz, sampling_rate_hz, ripple_db);
        
        if (zero_phase) {
            return std::make_unique<ZeroPhaseDecorator>(std::move(filter));
        }
        
        return filter;
    }

    /**
     * @brief Create a Chebyshev I bandpass filter
     * 
     * @tparam Order Filter order (1-8)
     * @param low_cutoff_hz Low cutoff frequency in Hz
     * @param high_cutoff_hz High cutoff frequency in Hz
     * @param sampling_rate_hz Sampling rate in Hz
     * @param ripple_db Passband ripple in dB
     * @param zero_phase Whether to apply zero-phase filtering
     * @return Unique pointer to the created filter
     */
    template<int Order>
    static std::unique_ptr<IFilter> createChebyshevIBandpass(
            double low_cutoff_hz,
            double high_cutoff_hz,
            double sampling_rate_hz, 
            double ripple_db,
            bool zero_phase = false) {
        static_assert(Order >= 1 && Order <= 8, "Filter order must be between 1 and 8");
        
        auto filter = std::make_unique<ChebyshevIBandpassFilter<Order>>(
            low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db);
        
        if (zero_phase) {
            return std::make_unique<ZeroPhaseDecorator>(std::move(filter));
        }
        
        return filter;
    }

    /**
     * @brief Create a Chebyshev I bandstop filter
     * 
     * @tparam Order Filter order (1-8)
     * @param low_cutoff_hz Low cutoff frequency in Hz
     * @param high_cutoff_hz High cutoff frequency in Hz
     * @param ripple_db Passband ripple in dB
     * @param zero_phase Whether to apply zero-phase filtering
     * @return Unique pointer to the created filter
     */
    template<int Order>
    static std::unique_ptr<IFilter> createChebyshevIBandstop(
            double low_cutoff_hz,
            double high_cutoff_hz,
            double sampling_rate_hz, 
            double ripple_db,
            bool zero_phase = false) {
        static_assert(Order >= 1 && Order <= 8, "Filter order must be between 1 and 8");
        
        auto filter = std::make_unique<ChebyshevIBandstopFilter<Order>>(
            low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db);
        
        if (zero_phase) {
            return std::make_unique<ZeroPhaseDecorator>(std::move(filter));
        }
        
        return filter;
    }

    /**
     * @brief Create a Chebyshev II lowpass filter
     * 
     * @tparam Order Filter order (1-8)
     * @param cutoff_hz Cutoff frequency in Hz
     * @param sampling_rate_hz Sampling rate in Hz
     * @param stopband_ripple_db Stopband ripple in dB
     * @param zero_phase Whether to apply zero-phase filtering
     * @return Unique pointer to the created filter
     */
    template<int Order>
    static std::unique_ptr<IFilter> createChebyshevIILowpass(
            double cutoff_hz, 
            double sampling_rate_hz, 
            double stopband_ripple_db,
            bool zero_phase = false) {
        static_assert(Order >= 1 && Order <= 8, "Filter order must be between 1 and 8");
        
        auto filter = std::make_unique<ChebyshevIILowpassFilter<Order>>(cutoff_hz, sampling_rate_hz, stopband_ripple_db);
        
        if (zero_phase) {
            return std::make_unique<ZeroPhaseDecorator>(std::move(filter));
        }
        
        return filter;
    }

    /**
     * @brief Create a Chebyshev II highpass filter
     * 
     * @tparam Order Filter order (1-8)
     * @param cutoff_hz Cutoff frequency in Hz
     * @param sampling_rate_hz Sampling rate in Hz
     * @param stopband_ripple_db Stopband ripple in dB
     * @param zero_phase Whether to apply zero-phase filtering
     * @return Unique pointer to the created filter
     */
    template<int Order>
    static std::unique_ptr<IFilter> createChebyshevIIHighpass(
            double cutoff_hz, 
            double sampling_rate_hz, 
            double stopband_ripple_db,
            bool zero_phase = false) {
        static_assert(Order >= 1 && Order <= 8, "Filter order must be between 1 and 8");
        
        auto filter = std::make_unique<ChebyshevIIHighpassFilter<Order>>(cutoff_hz, sampling_rate_hz, stopband_ripple_db);
        
        if (zero_phase) {
            return std::make_unique<ZeroPhaseDecorator>(std::move(filter));
        }
        
        return filter;
    }

    /**
     * @brief Create a Chebyshev II bandpass filter
     * 
     * @tparam Order Filter order (1-8)
     * @param low_cutoff_hz Low cutoff frequency in Hz
     * @param high_cutoff_hz High cutoff frequency in Hz
     * @param sampling_rate_hz Sampling rate in Hz
     * @param stopband_ripple_db Stopband ripple in dB
     * @param zero_phase Whether to apply zero-phase filtering
     * @return Unique pointer to the created filter
     */
    template<int Order>
    static std::unique_ptr<IFilter> createChebyshevIIBandpass(
            double low_cutoff_hz,
            double high_cutoff_hz,
            double sampling_rate_hz, 
            double stopband_ripple_db,
            bool zero_phase = false) {
        static_assert(Order >= 1 && Order <= 8, "Filter order must be between 1 and 8");
        
        auto filter = std::make_unique<ChebyshevIIBandpassFilter<Order>>(
            low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, stopband_ripple_db);
        
        if (zero_phase) {
            return std::make_unique<ZeroPhaseDecorator>(std::move(filter));
        }
        
        return filter;
    }

    /**
     * @brief Create a Chebyshev II bandstop filter
     * 
     * @tparam Order Filter order (1-8)
     * @param low_cutoff_hz Low cutoff frequency in Hz
     * @param high_cutoff_hz High cutoff frequency in Hz
     * @param sampling_rate_hz Sampling rate in Hz
     * @param stopband_ripple_db Stopband ripple in dB
     * @param zero_phase Whether to apply zero-phase filtering
     * @return Unique pointer to the created filter
     */
    template<int Order>
    static std::unique_ptr<IFilter> createChebyshevIIBandstop(
            double low_cutoff_hz,
            double high_cutoff_hz,
            double sampling_rate_hz, 
            double stopband_ripple_db,
            bool zero_phase = false) {
        static_assert(Order >= 1 && Order <= 8, "Filter order must be between 1 and 8");
        
        auto filter = std::make_unique<ChebyshevIIBandstopFilter<Order>>(
            low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, stopband_ripple_db);
        
        if (zero_phase) {
            return std::make_unique<ZeroPhaseDecorator>(std::move(filter));
        }
        
        return filter;
    }

    /**
     * @brief Create an RBJ lowpass filter (always 2nd order)
     * 
     * @param cutoff_hz Cutoff frequency in Hz
     * @param sampling_rate_hz Sampling rate in Hz
     * @param q_factor Q factor for the filter
     * @param zero_phase Whether to apply zero-phase filtering
     * @return Unique pointer to the created filter
     */
    static std::unique_ptr<IFilter> createRBJLowpass(
            double cutoff_hz, 
            double sampling_rate_hz, 
            double q_factor = 0.707,
            bool zero_phase = false) {
        
        auto filter = std::make_unique<RBJLowpassFilter>(cutoff_hz, sampling_rate_hz, q_factor);
        
        if (zero_phase) {
            return std::make_unique<ZeroPhaseDecorator>(std::move(filter));
        }
        
        return filter;
    }

    /**
     * @brief Create an RBJ highpass filter (always 2nd order)
     * 
     * @param cutoff_hz Cutoff frequency in Hz
     * @param sampling_rate_hz Sampling rate in Hz
     * @param q_factor Q factor for the filter
     * @param zero_phase Whether to apply zero-phase filtering
     * @return Unique pointer to the created filter
     */
    static std::unique_ptr<IFilter> createRBJHighpass(
            double cutoff_hz, 
            double sampling_rate_hz, 
            double q_factor = 0.707,
            bool zero_phase = false) {
        
        auto filter = std::make_unique<RBJHighpassFilter>(cutoff_hz, sampling_rate_hz, q_factor);
        
        if (zero_phase) {
            return std::make_unique<ZeroPhaseDecorator>(std::move(filter));
        }
        
        return filter;
    }

    /**
     * @brief Create an RBJ bandpass filter (always 2nd order)
     * 
     * @param center_freq_hz Center frequency in Hz
     * @param sampling_rate_hz Sampling rate in Hz
     * @param q_factor Q factor for the filter
     * @param zero_phase Whether to apply zero-phase filtering
     * @return Unique pointer to the created filter
     */
    static std::unique_ptr<IFilter> createRBJBandpass(
            double center_freq_hz,
            double sampling_rate_hz, 
            double q_factor = 1.0,
            bool zero_phase = false) {
        
        auto filter = std::make_unique<RBJBandpassFilter>(center_freq_hz, sampling_rate_hz, q_factor);
        
        if (zero_phase) {
            return std::make_unique<ZeroPhaseDecorator>(std::move(filter));
        }
        
        return filter;
    }

    /**
     * @brief Create an RBJ bandstop/notch filter (always 2nd order)
     * 
     * @param center_freq_hz Center frequency in Hz for notch filter
     * @param sampling_rate_hz Sampling rate in Hz
     * @param q_factor Q factor for notch filter
     * @param zero_phase Whether to apply zero-phase filtering
     * @return Unique pointer to the created filter
     */
    static std::unique_ptr<IFilter> createRBJBandstop(
            double center_freq_hz,
            double sampling_rate_hz, 
            double q_factor = 10.0,
            bool zero_phase = false) {
        
        auto filter = std::make_unique<RBJBandstopFilter>(center_freq_hz, sampling_rate_hz, q_factor);
        
        if (zero_phase) {
            return std::make_unique<ZeroPhaseDecorator>(std::move(filter));
        }
        
        return filter;
    }

private:
    // Helper template to dispatch filter creation based on runtime order
    template<typename FilterCreator>
    static std::unique_ptr<IFilter> createWithRuntimeOrder(int order, FilterCreator creator) {
        switch (order) {
            case 1: return creator.template operator()<1>();
            case 2: return creator.template operator()<2>();
            case 3: return creator.template operator()<3>();
            case 4: return creator.template operator()<4>();
            case 5: return creator.template operator()<5>();
            case 6: return creator.template operator()<6>();
            case 7: return creator.template operator()<7>();
            case 8: return creator.template operator()<8>();
            default:
                throw std::invalid_argument("Unsupported filter order: " + std::to_string(order));
        }
    }
};

#endif // FILTER_FACTORY_HPP
