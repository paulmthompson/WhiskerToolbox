
#include "OpenGLWidget.hpp"

OpenGLWidget::OpenGLWidget(QWidget *parent) : QOpenGLWidget(parent) {
    // Constructor implementation (if needed)
}

void OpenGLWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // Set the clear color to white
}

void OpenGLWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT); // Clear the screen with the clear color
}