#ifndef SHADERPROGRAM_HPP
#define SHADERPROGRAM_HPP

#include "ShaderSourceType.hpp"

#include <glm/glm.hpp>
#include <QOpenGLShaderProgram> // GLUint, QOpenGLShaderProgram

#include <filesystem>
#include <map>
#include <string>

class ShaderProgram {
public:
    ShaderProgram(std::string const & vertexPath,
                  std::string const & fragmentPath,
                  std::string const & geometryPath = "",
                  ShaderSourceType sourceType = ShaderSourceType::FileSystem);

    // Constructor for compute shaders
    ShaderProgram(std::string const & computePath,
                  ShaderSourceType sourceType = ShaderSourceType::FileSystem);

    ~ShaderProgram();

    // Attempts to re-compile and re-link the program from its source files/resources.
    // Returns true on success. On failure, the previous program remains active.
    bool reload();

    // Binds the program for use.
    void use();

    // Uniform setter methods
    void setUniform(std::string const & name, int value);
    void setUniform(std::string const & name, float value);
    void setUniform(std::string const & name, glm::mat4 const & matrix);

    // Getters for file/resource paths
    [[nodiscard]] std::string const & getVertexPath() const { return m_vertexPath; }
    [[nodiscard]] std::string const & getFragmentPath() const { return m_fragmentPath; }
    [[nodiscard]] std::string const & getGeometryPath() const { return m_geometryPath; }
    [[nodiscard]] std::string const & getComputePath() const { return m_computePath; }

    [[nodiscard]] QOpenGLShaderProgram * getNativeProgram() const { return m_program.get(); }
    [[nodiscard]] GLuint getProgramId() const { return m_program ? m_program->programId() : 0; }

private:
    bool compileAndLink();
    bool loadShaderSource(std::string const & path, std::string & outSource) const;
    bool loadShaderSourceResource(std::string const & resourcePath, std::string & outSource) const;

    std::unique_ptr<QOpenGLShaderProgram> m_program;
    std::map<std::string, GLint> m_uniformLocations;

    std::string m_vertexPath, m_fragmentPath, m_geometryPath, m_computePath;
    ShaderSourceType m_sourceType;
    bool m_isComputeShader = false;

    // For hot-reloading (filesystem only)
    std::filesystem::file_time_type m_vertexTimestamp;
    std::filesystem::file_time_type m_fragmentTimestamp;
    std::filesystem::file_time_type m_geometryTimestamp;
    std::filesystem::file_time_type m_computeTimestamp;
};

#endif// SHADERPROGRAM_HPP