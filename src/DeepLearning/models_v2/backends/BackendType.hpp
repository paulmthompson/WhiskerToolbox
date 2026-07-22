#ifndef NEURALYZER_BACKEND_TYPE_HPP
#define NEURALYZER_BACKEND_TYPE_HPP

#include <string>

namespace dl {

    /**
     * @brief Serialization format / inference backend for model execution.
     */
    enum class BackendType {
        /** .pt files via torch::jit::load() (deprecated but functional) */
        TorchScript,
        /** .pt2 files via AOTIModelPackageLoader (recommended) */
        AOTInductor,
        /** .pte files via ExecuTorch runtime (optional, edge deployment) */
        ExecuTorch,
        /** Auto-detect from file extension */
        Auto
    };
    
    /**
     * @brief Convert a BackendType to a human-readable string.
     */
    [[nodiscard]] inline std::string backendTypeToString(BackendType type) {
        switch (type) {
            case BackendType::TorchScript:
                return "TorchScript";
            case BackendType::AOTInductor:
                return "AOTInductor";
            case BackendType::ExecuTorch:
                return "ExecuTorch";
            case BackendType::Auto:
                return "Auto";
        }
        return "Unknown";
    }
    
    /**
     * @brief Parse a BackendType from a string (case-insensitive).
     *
     * @return BackendType::Auto if the string is not recognized.
     */
    [[nodiscard]] inline BackendType backendTypeFromString(std::string const & str) {
        // Simple case-insensitive compare via lowering
        auto lower = str;
        for (auto & c: lower) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    
        if (lower == "torchscript" || lower == "torch_script" || lower == "jit") {
            return BackendType::TorchScript;
        }
        if (lower == "aotinductor" || lower == "aot_inductor" || lower == "inductor" || lower == "aoti") {
            return BackendType::AOTInductor;
        }
        if (lower == "executorch" || lower == "exec_torch") {
            return BackendType::ExecuTorch;
        }
        return BackendType::Auto;
    }

}// namespace dl

#endif// NEURALYZER_BACKEND_TYPE_HPP