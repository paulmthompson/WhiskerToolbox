#ifndef SHADERMANAGER_HPP
#define SHADERMANAGER_HPP

#include "ShaderProgram.hpp"
#include "ShaderSourceType.hpp"

#include <QFileSystemWatcher>
#include <QObject>
#include <QOpenGLContext>

#include <filesystem>
#include <map>
#include <memory>
#include <string>

class ShaderManager : public QObject {
    Q_OBJECT
public:
    static ShaderManager & instance();

    // Load a program from source files/resources and assign it a friendly name.
    // Returns true on initial successful compilation.
    bool loadProgram(std::string const & name,
                     std::string const & vertexPath,
                     std::string const & fragmentPath,
                     std::string const & geometryPath = "",
                     ShaderSourceType sourceType = ShaderSourceType::FileSystem);

    // Load a compute shader program and assign it a friendly name.
    // Returns true on initial successful compilation.
    bool loadComputeProgram(std::string const & name,
                           std::string const & computePath,
                           ShaderSourceType sourceType = ShaderSourceType::FileSystem);

    // Retrieve a pointer to a loaded program for the current OpenGL context.
    ShaderProgram * getProgram(std::string const & name);

    ShaderManager(ShaderManager const &) = delete;
    void operator=(ShaderManager const &) = delete;

    void cleanup();
    void cleanupCurrentContext();

signals:
    void shaderReloaded(std::string const & name);

private slots:
    void onFileChanged(QString const & path);

private:
    ShaderManager();

    QFileSystemWatcher m_fileWatcher;
    // Programs keyed per OpenGL context to avoid cross-context invalidation
    std::map<QOpenGLContext *, std::map<std::string, std::unique_ptr<ShaderProgram>>> m_programs_by_context;
    std::map<std::string, std::string> m_pathToProgramName;     // file path -> program name
    std::map<std::string, ShaderSourceType> m_programSourceType;// program name -> source type (assumed consistent)
};

#endif// SHADERMANAGER_HPP