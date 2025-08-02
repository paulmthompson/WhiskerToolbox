#ifndef SHADERMANAGER_HPP
#define SHADERMANAGER_HPP

#include "ShaderSourceType.hpp"
#include "ShaderProgram.hpp"

#include <QFileSystemWatcher>
#include <QObject>

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

    // Retrieve a pointer to a loaded program.
    ShaderProgram * getProgram(std::string const & name);

    ShaderManager(ShaderManager const &) = delete;
    void operator=(ShaderManager const &) = delete;

    void cleanup();

signals:
    void shaderReloaded(std::string const & name);

private slots:
    void onFileChanged(QString const & path);

private:
    ShaderManager();

    QFileSystemWatcher m_fileWatcher;
    std::map<std::string, std::unique_ptr<ShaderProgram>> m_programs;
    std::map<std::string, std::string> m_pathToProgramName;
    std::map<std::string, ShaderSourceType> m_programSourceType;
};

#endif// SHADERMANAGER_HPP