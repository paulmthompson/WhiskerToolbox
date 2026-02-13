#include "DeepLearningState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <iostream>

DeepLearningState::DeepLearningState(QObject * parent)
    : EditorState(parent) {
}

QString DeepLearningState::getTypeName() const {
    return QStringLiteral("DeepLearningWidget");
}

std::string DeepLearningState::toJson() const {
    auto data = _data;
    data.instance_id = getInstanceId().toStdString();
    data.display_name = getDisplayName().toStdString();
    return rfl::json::write(data);
}

bool DeepLearningState::fromJson(std::string const & json) {
    auto const result = rfl::json::read<DeepLearningStateData>(json);
    if (!result) {
        std::cerr << "DeepLearningState::fromJson: parse error\n";
        return false;
    }
    _data = std::move(result.value());
    if (!_data.instance_id.empty()) {
        setInstanceId(QString::fromStdString(_data.instance_id));
    }
    if (!_data.display_name.empty()) {
        setDisplayName(QString::fromStdString(_data.display_name));
    }
    emit modelChanged();
    emit weightsPathChanged();
    emit inputBindingsChanged();
    emit outputBindingsChanged();
    emit staticInputsChanged();
    return true;
}

// ── Accessors / Mutators ──

std::string const & DeepLearningState::selectedModelId() const {
    return _data.selected_model_id;
}

void DeepLearningState::setSelectedModelId(std::string const & id) {
    if (_data.selected_model_id != id) {
        _data.selected_model_id = id;
        // Clear bindings when model changes — they are model-specific
        _data.input_bindings.clear();
        _data.output_bindings.clear();
        _data.static_inputs.clear();
        markDirty();
        emit modelChanged();
    }
}

std::string const & DeepLearningState::weightsPath() const {
    return _data.weights_path;
}

void DeepLearningState::setWeightsPath(std::string const & path) {
    if (_data.weights_path != path) {
        _data.weights_path = path;
        markDirty();
        emit weightsPathChanged();
    }
}

int DeepLearningState::batchSize() const {
    return _data.batch_size;
}

void DeepLearningState::setBatchSize(int size) {
    if (_data.batch_size != size) {
        _data.batch_size = size;
        markDirty();
        emit batchSizeChanged(size);
    }
}

int DeepLearningState::currentFrame() const {
    return _data.current_frame;
}

void DeepLearningState::setCurrentFrame(int frame) {
    if (_data.current_frame != frame) {
        _data.current_frame = frame;
        markDirty();
        emit currentFrameChanged(frame);
    }
}

std::vector<SlotBindingData> const & DeepLearningState::inputBindings() const {
    return _data.input_bindings;
}

void DeepLearningState::setInputBindings(std::vector<SlotBindingData> bindings) {
    _data.input_bindings = std::move(bindings);
    markDirty();
    emit inputBindingsChanged();
}

std::vector<OutputBindingData> const & DeepLearningState::outputBindings() const {
    return _data.output_bindings;
}

void DeepLearningState::setOutputBindings(std::vector<OutputBindingData> bindings) {
    _data.output_bindings = std::move(bindings);
    markDirty();
    emit outputBindingsChanged();
}

std::vector<StaticInputData> const & DeepLearningState::staticInputs() const {
    return _data.static_inputs;
}

void DeepLearningState::setStaticInputs(std::vector<StaticInputData> inputs) {
    _data.static_inputs = std::move(inputs);
    markDirty();
    emit staticInputsChanged();
}
