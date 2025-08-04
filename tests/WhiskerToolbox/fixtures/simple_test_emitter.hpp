#ifndef SIMPLE_TEST_EMITTER_HPP
#define SIMPLE_TEST_EMITTER_HPP

#include <QObject>

// Simple test class with Q_OBJECT macro
class SimpleTestEmitter : public QObject {
    Q_OBJECT

public:
    explicit SimpleTestEmitter(QObject* parent = nullptr) : QObject(parent) {}

    void emitTestSignal(QString message) {
        emit testSignal(message);
    }

signals:
    void testSignal(QString message);
};

#endif // SIMPLE_TEST_EMITTER_HPP 