#include "DeepLearningState.hpp"

DeepLearningState::DeepLearningState(QObject * parent)
    : EditorState(parent) {
}

QString DeepLearningState::getTypeName() const {
    return QStringLiteral("DeepLearningWidget");
}

std::string DeepLearningState::toJson() const {
    // Placeholder implementation: no custom fields to serialize yet.
    return std::string{"{}"};
}

bool DeepLearningState::fromJson(std::string const & /*json*/) {
    // Placeholder implementation: ignore input JSON, nothing to restore.
    return true;
}
