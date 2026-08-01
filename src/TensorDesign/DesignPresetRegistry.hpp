/**
 * @file DesignPresetRegistry.hpp
 * @brief Registry and built-in expansions for TensorDesign design authoring presets.
 */
#ifndef DESIGN_PRESET_REGISTRY_HPP
#define DESIGN_PRESET_REGISTRY_HPP

#include "ParameterSchema/ParameterSchema.hpp"
#include "TensorDesignSpec.hpp"

#include <nlohmann/json_fwd.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace Neuralyzer::TensorDesign {

enum class DesignPresetSource : std::uint8_t {
    BuiltIn,
    User
};

struct DesignPresetArgs {
    std::string row_source_key;
    std::string curvature_source_key;
    std::string spike_source_key;
    std::string angle_source_key;
    std::vector<std::string> keypoint_source_keys;
    int64_t onset_pre = 0;
    int64_t onset_post = 0;
};

struct DesignPresetExpansion {
    TensorDesignSpec spec;
};

struct DesignPresetDescriptor {
    std::string id;
    std::string display_name;
    std::string description;
    ParameterSchema parameters;
    DesignPresetSource source = DesignPresetSource::BuiltIn;
    std::function<std::optional<DesignPresetExpansion>(DesignPresetArgs const &)> expand;
};

class DesignPresetRegistry {
public:
    bool registerPreset(DesignPresetDescriptor descriptor);

    [[nodiscard]] DesignPresetDescriptor const * find(std::string const & id) const;
    [[nodiscard]] std::optional<DesignPresetExpansion> expand(
            std::string const & id,
            DesignPresetArgs const & args) const;
    [[nodiscard]] std::optional<DesignPresetExpansion> expandJson(
            std::string const & id,
            nlohmann::json const & parameters) const;
    [[nodiscard]] std::vector<DesignPresetDescriptor const *> descriptors() const;

private:
    std::vector<DesignPresetDescriptor> _descriptors;
};

[[nodiscard]] DesignPresetRegistry createBuiltInDesignPresetRegistry();
[[nodiscard]] std::optional<DesignPresetArgs> parseDesignPresetArgs(
        nlohmann::json const & parameters);

}// namespace Neuralyzer::TensorDesign

#endif// DESIGN_PRESET_REGISTRY_HPP
