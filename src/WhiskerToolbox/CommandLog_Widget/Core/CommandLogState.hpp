/**
 * @file CommandLogState.hpp
 * @brief EditorState subclass for the CommandLog widget
 */

#ifndef COMMAND_LOG_STATE_HPP
#define COMMAND_LOG_STATE_HPP

#include "CommandLogStateData.hpp"
#include "EditorState/EditorState.hpp"

#include <string>

/**
 * @brief Minimal state class for the Command Log widget
 *
 * The Command Log widget displays recorded commands from CommandRecorder.
 * It has no meaningful persistent state beyond its instance identity.
 */
class CommandLogState : public EditorState {
    Q_OBJECT

public:
    explicit CommandLogState(QObject * parent = nullptr);
    ~CommandLogState() override = default;

    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("CommandLogWidget"); }
    [[nodiscard]] QString getDisplayName() const override;
    void setDisplayName(QString const & name) override;

    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

private:
    CommandLogStateData _data;
};

#endif// COMMAND_LOG_STATE_HPP
