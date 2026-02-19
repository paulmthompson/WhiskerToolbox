#ifndef MLCORE_WIDGET_STATE_HPP
#define MLCORE_WIDGET_STATE_HPP

/**
 * @file MLCoreWidgetState.hpp
 * @brief EditorState subclass for the MLCore_Widget
 *
 * MLCoreWidgetState wraps MLCoreWidgetStateData and provides typed
 * accessors with Qt signal notifications. It supports JSON serialization
 * via reflect-cpp for workspace save/restore.
 *
 * ## Responsibilities
 *
 * - Expose feature tensor, region, label, model, and output configuration
 *   through typed getters/setters that emit change signals.
 * - Serialize/deserialize via `toJson()` / `fromJson()`.
 * - Track dirty state for unsaved-changes detection.
 *
 * ## Usage
 *
 * Created by the registration factory in MLCoreWidgetRegistration.cpp.
 * Shared between the main MLCoreWidget view and any future properties panel.
 *
 * @see MLCoreWidgetStateData for the underlying serializable struct
 * @see MLCoreWidgetRegistration for factory registration
 * @see MLCoreWidget for the view that consumes this state
 */

#include "MLCoreWidgetStateData.hpp"

#include "EditorState/EditorState.hpp"

#include <QString>

#include <cstdint>
#include <string>
#include <vector>

class MLCoreWidgetState : public EditorState {
    Q_OBJECT

public:
    explicit MLCoreWidgetState(QObject * parent = nullptr);
    ~MLCoreWidgetState() override = default;

    // === EditorState interface ===

    [[nodiscard]] QString getTypeName() const override {
        return QStringLiteral("MLCoreWidget");
    }

    [[nodiscard]] QString getDisplayName() const override;
    void setDisplayName(QString const & name) override;

    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

    // === Feature configuration ===

    void setFeatureTensorKey(std::string const & key);
    [[nodiscard]] std::string const & featureTensorKey() const;

    // === Region configuration ===

    void setTrainingRegionKey(std::string const & key);
    [[nodiscard]] std::string const & trainingRegionKey() const;

    void setPredictionRegionKey(std::string const & key);
    [[nodiscard]] std::string const & predictionRegionKey() const;

    // === Label configuration ===

    void setLabelSourceType(std::string const & type);
    [[nodiscard]] std::string const & labelSourceType() const;

    void setLabelIntervalKey(std::string const & key);
    [[nodiscard]] std::string const & labelIntervalKey() const;

    void setLabelGroupIds(std::vector<uint64_t> const & ids);
    [[nodiscard]] std::vector<uint64_t> const & labelGroupIds() const;

    // === Model configuration ===

    void setSelectedModelName(std::string const & name);
    [[nodiscard]] std::string const & selectedModelName() const;

    void setModelParametersJson(std::string const & json);
    [[nodiscard]] std::string const & modelParametersJson() const;

    // === Output configuration ===

    void setOutputPrefix(std::string const & prefix);
    [[nodiscard]] std::string const & outputPrefix() const;

    void setProbabilityThreshold(double threshold);
    [[nodiscard]] double probabilityThreshold() const;

    void setOutputProbabilities(bool enabled);
    [[nodiscard]] bool outputProbabilities() const;

    void setOutputPredictions(bool enabled);
    [[nodiscard]] bool outputPredictions() const;

    // === UI state ===

    void setActiveTab(int tab);
    [[nodiscard]] int activeTab() const;

signals:
    void featureTensorKeyChanged(QString const & key);
    void trainingRegionKeyChanged(QString const & key);
    void predictionRegionKeyChanged(QString const & key);
    void labelSourceTypeChanged(QString const & type);
    void labelIntervalKeyChanged(QString const & key);
    void labelGroupIdsChanged();
    void selectedModelNameChanged(QString const & name);
    void modelParametersJsonChanged();
    void outputPrefixChanged(QString const & prefix);
    void probabilityThresholdChanged(double threshold);
    void outputProbabilitiesChanged(bool enabled);
    void outputPredictionsChanged(bool enabled);
    void activeTabChanged(int tab);

private:
    MLCoreWidgetStateData _data;
};

#endif // MLCORE_WIDGET_STATE_HPP
