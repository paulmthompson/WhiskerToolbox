#include "ShaderManager.hpp"

#include "ShaderProgram.hpp"

#include <QDebug>
#include <QFileInfo>
#include <QOpenGLContext>

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
    QOpenGLContext * ctx = QOpenGLContext::currentContext();
    if (!ctx) {
        std::cerr << "[ShaderManager] No current OpenGL context when loading program: " << name << std::endl;
        return false;
    }

    auto & program_map = m_programs_by_context[ctx];
    auto it_existing = program_map.find(name);
    if (it_existing != program_map.end()) {
        // Already loaded for this context
        return true;
    }

    auto program = std::make_unique<ShaderProgram>(vertexPath, fragmentPath, geometryPath, sourceType);
    if (!program->reload()) {
        std::cerr << "[ShaderManager] Failed to compile shader program: " << name << std::endl;
        return false;
    }
    program_map[name] = std::move(program);
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

bool ShaderManager::loadComputeProgram(std::string const & name,
                                      std::string const & computePath,
                                      ShaderSourceType sourceType) {
    QOpenGLContext * ctx = QOpenGLContext::currentContext();
    if (!ctx) {
        std::cerr << "[ShaderManager] No current OpenGL context when loading compute program: " << name << std::endl;
        return false;
    }

    auto & program_map = m_programs_by_context[ctx];
    auto it_existing = program_map.find(name);
    if (it_existing != program_map.end()) {
        // Already loaded for this context
        return true;
    }

    auto program = std::make_unique<ShaderProgram>(computePath, sourceType);
    if (!program->reload()) {
        std::cerr << "[ShaderManager] Failed to compile compute shader program: " << name << std::endl;
        return false;
    }
    program_map[name] = std::move(program);
    m_programSourceType[name] = sourceType;

    // Only watch files for hot-reloading if using filesystem
    if (sourceType == ShaderSourceType::FileSystem) {
        if (!computePath.empty()) m_fileWatcher.addPath(QString::fromStdString(computePath));
        if (!computePath.empty()) m_pathToProgramName[computePath] = name;
    } else {
        // Print info if resource path (no hot-reload)
        std::cout << "[ShaderManager] Loaded compute shader program '" << name << "' from Qt resource. Hot-reloading is not available." << std::endl;
    }
    return true;
}

ShaderProgram * ShaderManager::getProgram(std::string const & name) {
    QOpenGLContext * ctx = QOpenGLContext::currentContext();
    if (!ctx) {
        return nullptr;
    }
    auto ctx_it = m_programs_by_context.find(ctx);
    if (ctx_it == m_programs_by_context.end()) {
        return nullptr;
    }
    auto & program_map = ctx_it->second;
    auto it = program_map.find(name);
    if (it != program_map.end()) {
        return it->second.get();
    }
    return nullptr;
}

void ShaderManager::onFileChanged(QString const & path) {
    std::string const pathStr = path.toStdString();
    auto it = m_pathToProgramName.find(pathStr);
    if (it != m_pathToProgramName.end()) {
        std::string const & programName = it->second;
        // Reload the program in all contexts that have it
        for (auto & [ctx, program_map]: m_programs_by_context) {
            (void) ctx;
            auto progIt = program_map.find(programName);
            if (progIt != program_map.end()) {
                std::cout << "[ShaderManager] Detected change in shader file: " << pathStr << ". Reloading program: " << programName << std::endl;
                if (progIt->second->reload()) {
                    std::cout << "[ShaderManager] Successfully reloaded shader program: " << programName << std::endl;
                    emit shaderReloaded(programName);
                } else {
                    std::cerr << "[ShaderManager] Failed to reload shader program: " << programName << ". Keeping previous version active." << std::endl;
                }
            }
        }
    }
    // Re-add the path to the watcher in case it was removed
    if (QFileInfo::exists(path)) {
        m_fileWatcher.addPath(path);
    }
}

void ShaderManager::cleanup() {
    // Clear all shader programs for all contexts
    // Let unique_ptr handle destruction automatically - this will call the safe destructor
    m_programs_by_context.clear();
    m_fileWatcher.removePaths(m_fileWatcher.files());
    m_fileWatcher.removePaths(m_fileWatcher.directories());
}

void ShaderManager::cleanupCurrentContext() {
    QOpenGLContext * ctx = QOpenGLContext::currentContext();
    if (!ctx) return;
    auto it = m_programs_by_context.find(ctx);
    if (it == m_programs_by_context.end()) return;
    // Let unique_ptr handle destruction automatically - this will call the safe destructor
    m_programs_by_context.erase(it);
}