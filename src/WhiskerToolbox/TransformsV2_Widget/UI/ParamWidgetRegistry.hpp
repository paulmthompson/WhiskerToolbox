#ifndef WHISKERTOOLBOX_PARAM_WIDGET_REGISTRY_HPP
#define WHISKERTOOLBOX_PARAM_WIDGET_REGISTRY_HPP

/**
 * @file ParamWidgetRegistry.hpp
 * @brief Registry for custom parameter widget factories
 *
 * Allows registering a custom QWidget* factory for specific parameter types,
 * with fallback to AutoParamWidget if no custom widget is registered.
 *
 * Usage:
 * @code
 * // Register a custom widget for a specific params type:
 * ParamWidgetRegistry::instance().registerCustomWidget<LineAngleParams>(
 *     [](QWidget* parent) -> QWidget* {
 *         return new LineAngleCustomWidget(parent);
 *     });
 *
 * // Query (returns nullptr if no custom widget registered):
 * auto* widget = ParamWidgetRegistry::instance().createCustomWidget(
 *     typeid(LineAngleParams), parent);
 * @endcode
 */

#include <QWidget>

#include <functional>
#include <typeindex>
#include <unordered_map>

/**
 * @brief Singleton registry for custom parameter widget factories
 *
 * The pipeline builder checks this registry first. If a custom widget
 * is registered for a parameter type, it is used. Otherwise, the
 * AutoParamWidget is used as the default form generator.
 */
class ParamWidgetRegistry {
public:
    using WidgetFactory = std::function<QWidget *(QWidget * parent)>;

    /// Get global singleton instance
    static ParamWidgetRegistry & instance() {
        static ParamWidgetRegistry registry;
        return registry;
    }

    /**
     * @brief Register a custom widget factory for a parameter type
     *
     * @tparam Params The parameter struct type
     * @param factory Lambda/function that creates the custom widget
     */
    template<typename Params>
    void registerCustomWidget(WidgetFactory factory) {
        factories_[std::type_index(typeid(Params))] = std::move(factory);
    }

    /**
     * @brief Check if a custom widget is registered for a params type
     */
    [[nodiscard]] bool hasCustomWidget(std::type_index params_type) const {
        return factories_.find(params_type) != factories_.end();
    }

    /**
     * @brief Create a custom widget for a params type, or nullptr if none registered
     */
    QWidget * createCustomWidget(std::type_index params_type, QWidget * parent) const {
        auto it = factories_.find(params_type);
        if (it != factories_.end()) {
            return it->second(parent);
        }
        return nullptr;
    }

    /**
     * @brief Templated convenience for creating a custom widget
     */
    template<typename Params>
    QWidget * createCustomWidget(QWidget * parent) const {
        return createCustomWidget(std::type_index(typeid(Params)), parent);
    }

private:
    ParamWidgetRegistry() = default;

    std::unordered_map<std::type_index, WidgetFactory> factories_;
};

#endif// WHISKERTOOLBOX_PARAM_WIDGET_REGISTRY_HPP
