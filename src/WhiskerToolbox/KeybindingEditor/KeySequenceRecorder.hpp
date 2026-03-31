/**
 * @file KeySequenceRecorder.hpp
 * @brief Widget that captures a key press and emits it as a QKeySequence
 *
 * When activated (via startRecording()), the widget grabs keyboard focus and
 * intercepts the next key event. The captured sequence is emitted via
 * keySequenceRecorded(). Pressing Escape cancels recording.
 *
 * @see KeybindingEditor for usage
 */

#ifndef KEY_SEQUENCE_RECORDER_HPP
#define KEY_SEQUENCE_RECORDER_HPP

#include <QKeySequence>
#include <QPushButton>
#include <QString>

/**
 * @brief Push button that captures a single key combination when recording
 *
 * Normal state: displays "Record" text.
 * Recording state: displays "Press a key..." and captures the next key event.
 */
class KeySequenceRecorder : public QPushButton {
    Q_OBJECT

public:
    explicit KeySequenceRecorder(QWidget * parent = nullptr);

    /// Start recording mode — grabs focus and waits for a key press
    void startRecording();

    /// Cancel recording without emitting a result
    void cancelRecording();

    /// Whether the recorder is currently waiting for a key press
    [[nodiscard]] bool isRecording() const { return _recording; }

signals:
    /// Emitted when a key sequence is successfully captured
    void keySequenceRecorded(QKeySequence const & seq);

    /// Emitted when recording is cancelled (Escape pressed)
    void recordingCancelled();

protected:
    void keyPressEvent(QKeyEvent * event) override;
    void focusOutEvent(QFocusEvent * event) override;

private:
    bool _recording = false;

    void _finishRecording(QKeySequence const & seq);
    void _resetAppearance();
};

#endif// KEY_SEQUENCE_RECORDER_HPP
