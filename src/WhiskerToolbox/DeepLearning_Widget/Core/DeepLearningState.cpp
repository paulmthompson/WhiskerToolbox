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
    _data = result.value();
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
    emit recurrentBindingsChanged();
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
        _data.recurrent_bindings.clear();
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

std::vector<RecurrentBindingData> const & DeepLearningState::recurrentBindings() const {
    return _data.recurrent_bindings;
}

void DeepLearningState::setRecurrentBindings(std::vector<RecurrentBindingData> bindings) {
    _data.recurrent_bindings = std::move(bindings);
    markDirty();
    emit recurrentBindingsChanged();
}

bool DeepLearningState::hasRecurrentBindings() const {
    return !_data.recurrent_bindings.empty();
}

std::string const & DeepLearningState::postEncoderModuleType() const {
    return _data.post_encoder_module_type;
}

void DeepLearningState::setPostEncoderModuleType(std::string const & type) {
    _data.post_encoder_module_type = type;
    markDirty();
    emit postEncoderModuleChanged();
}

std::string const & DeepLearningState::postEncoderPointKey() const {
    return _data.post_encoder_point_key;
}

void DeepLearningState::setPostEncoderPointKey(std::string const & key) {
    _data.post_encoder_point_key = key;
    markDirty();
    emit postEncoderModuleChanged();
}

// ── Encoder Shape Configuration ──

int DeepLearningState::encoderInputHeight() const {
    return _data.encoder_input_height;
}

void DeepLearningState::setEncoderInputHeight(int height) {
    if (_data.encoder_input_height != height) {
        _data.encoder_input_height = height;
        markDirty();
        emit encoderShapeChanged();
    }
}

int DeepLearningState::encoderInputWidth() const {
    return _data.encoder_input_width;
}

void DeepLearningState::setEncoderInputWidth(int width) {
    if (_data.encoder_input_width != width) {
        _data.encoder_input_width = width;
        markDirty();
        emit encoderShapeChanged();
    }
}

std::string const & DeepLearningState::encoderOutputShape() const {
    return _data.encoder_output_shape;
}

void DeepLearningState::setEncoderOutputShape(std::string const & shape) {
    if (_data.encoder_output_shape != shape) {
        _data.encoder_output_shape = shape;
        markDirty();
        emit encoderShapeChanged();
    }
}

bool DeepLearningState::shapeConfigured() const {
    // encoder_input_height/width start at 0 (default); EncoderShapeWidget
    // sets them to >= 1 (minimum 224) when the user clicks "Apply Shape".
    return _data.encoder_input_height > 0 || _data.encoder_input_width > 0;
}
