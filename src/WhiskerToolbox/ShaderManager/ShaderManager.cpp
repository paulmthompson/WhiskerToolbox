#include "ShaderManager.hpp"

#include "ShaderProgram.hpp"

#include <QDebug>
#include <QFileInfo>

#include <iostream>

ShaderManager & ShaderManager::instance() {
    static ShaderManager instance;
    return instance;
}

ShaderManager::ShaderManager() {
    connect(&m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &ShaderManager::onFileChanged);
}

bool ShaderManager::loadProgram(std::string const & name,
                                std::string const & vertexPath,
                                std::string const & fragmentPath,
                                std::string const & geometryPath,
                                ShaderSourceType sourceType) {
    auto program = std::make_unique<ShaderProgram>(vertexPath, fragmentPath, geometryPath, sourceType);
    if (!program->reload()) {
        std::cerr << "[ShaderManager] Failed to compile shader program: " << name << std::endl;
        return false;
    }
    m_programs[name] = std::move(program);
    m_programSourceType[name] = sourceType;

    // Only watch files for hot-reloading if using filesystem
    if (sourceType == ShaderSourceType::FileSystem) {
        if (!vertexPath.empty()) m_fileWatcher.addPath(QString::fromStdString(vertexPath));
        if (!fragmentPath.empty()) m_fileWatcher.addPath(QString::fromStdString(fragmentPath));
        if (!geometryPath.empty()) m_fileWatcher.addPath(QString::fromStdString(geometryPath));
        if (!vertexPath.empty()) m_pathToProgramName[vertexPath] = name;
        if (!fragmentPath.empty()) m_pathToProgramName[fragmentPath] = name;
        if (!geometryPath.empty()) m_pathToProgramName[geometryPath] = name;
    } else {
        // Print info if resource path (no hot-reload)
        std::cout << "[ShaderManager] Loaded shader program '" << name << "' from Qt resource. Hot-reloading is not available." << std::endl;
    }
    return true;
}

ShaderProgram * ShaderManager::getProgram(std::string const & name) {
    auto it = m_programs.find(name);
    if (it != m_programs.end()) {
        return it->second.get();
    }
    return nullptr;
}

void ShaderManager::onFileChanged(QString const & path) {
    std::string pathStr = path.toStdString();
    auto it = m_pathToProgramName.find(pathStr);
    if (it != m_pathToProgramName.end()) {
        std::string const & programName = it->second;
        auto progIt = m_programs.find(programName);
        if (progIt != m_programs.end()) {
            std::cout << "[ShaderManager] Detected change in shader file: " << pathStr << ". Reloading program: " << programName << std::endl;
            if (progIt->second->reload()) {
                std::cout << "[ShaderManager] Successfully reloaded shader program: " << programName << std::endl;
                emit shaderReloaded(programName);
            } else {
                std::cerr << "[ShaderManager] Failed to reload shader program: " << programName << ". Keeping previous version active." << std::endl;
            }
        }
    }
    // Re-add the path to the watcher in case it was removed
    if (QFileInfo::exists(path)) {
        m_fileWatcher.addPath(path);
    }
}

void ShaderManager::cleanup() {
    // Clear all shader programs
    for (auto& [name, program] : m_programs) {
        if (program) {
            delete program.release();
        }
    }
    m_programs.clear();
    m_fileWatcher.removePaths(m_fileWatcher.files());
    m_fileWatcher.removePaths(m_fileWatcher.directories());
}