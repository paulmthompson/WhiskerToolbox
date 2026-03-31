/**
 * @file KeySequenceRecorder.cpp
 * @brief Implementation of the key sequence recording widget
 */

#include "KeySequenceRecorder.hpp"

#include <QFocusEvent>
#include <QKeyEvent>

KeySequenceRecorder::KeySequenceRecorder(QWidget * parent)
    : QPushButton(parent) {
    setText(QStringLiteral("Record"));
    setToolTip(QStringLiteral("Click to record a new key binding"));
    setFocusPolicy(Qt::StrongFocus);

    connect(this, &QPushButton::clicked, this, &KeySequenceRecorder::startRecording);
}

void KeySequenceRecorder::startRecording() {
    _recording = true;
    setText(QStringLiteral("Press a key..."));
    setStyleSheet(QStringLiteral("QPushButton { background-color: #fff3cd; border: 2px solid #ffc107; }"));
    setFocus(Qt::OtherFocusReason);
    grabKeyboard();
}

void KeySequenceRecorder::cancelRecording() {
    if (!_recording) {
        return;
    }
    _recording = false;
    _resetAppearance();
    releaseKeyboard();
    emit recordingCancelled();
}

void KeySequenceRecorder::keyPressEvent(QKeyEvent * event) {
    if (!_recording) {
        QPushButton::keyPressEvent(event);
        return;
    }

    int const key = event->key();

    // Ignore bare modifier keys — wait for a real key
    if (key == Qt::Key_Shift || key == Qt::Key_Control ||
        key == Qt::Key_Alt || key == Qt::Key_Meta) {
        return;
    }

    // Escape cancels recording
    if (key == Qt::Key_Escape && event->modifiers() == Qt::NoModifier) {
        cancelRecording();
        return;
    }

    int const key_with_modifiers = key | static_cast<int>(event->modifiers());
    QKeySequence const seq(key_with_modifiers);
    _finishRecording(seq);
}

void KeySequenceRecorder::focusOutEvent(QFocusEvent * event) {
    if (_recording) {
        cancelRecording();
    }
    QPushButton::focusOutEvent(event);
}

void KeySequenceRecorder::_finishRecording(QKeySequence const & seq) {
    _recording = false;
    _resetAppearance();
    releaseKeyboard();
    emit keySequenceRecorded(seq);
}

void KeySequenceRecorder::_resetAppearance() {
    setText(QStringLiteral("Record"));
    setStyleSheet(QString{});
}
