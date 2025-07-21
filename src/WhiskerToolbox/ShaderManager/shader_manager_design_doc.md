Design Document: Shader Management System
Version: 1.1
Date: July 21, 2025
Author: Gemini

1. Overview
This document specifies the design for a centralized shader management system for a C++/Qt/OpenGL visualization application. The primary goals are to move GLSL shaders from hard-coded C-style strings to external files, establish a robust system for compiling and managing shader programs, and implement a "hot-reloading" feature to allow for live shader editing during development, dramatically improving iteration speed and maintainability.

2. Core Components
The system is composed of two primary classes: ShaderManager and ShaderProgram.

2.1. The ShaderManager Class
The ShaderManager will be a singleton, serving as the single, application-wide authority for all shader programs. This ensures that GPU resources are not duplicated across different widgets that share an OpenGL context.

Responsibilities:

Centralized Access: Provide a global access point to the single instance of the manager.

Program Lifecycle: Manage the loading, retrieval, and eventual cleanup of all ShaderProgram objects.

Hot-Reloading Hub: Contain the QFileSystemWatcher instance and orchestrate the reloading process when shader source files are modified on disk.

Class Specification:

class ShaderManager : public QObject {
public:
    // Singleton access method
    static ShaderManager& instance();

    // Load a program from source files and assign it a friendly name.
    // Returns true on initial successful compilation.
    bool loadProgram(const std::string& name,
                     const std::string& vertexPath,
                     const std::string& fragmentPath,
                     const std::string& geometryPath = ""); // Optional geometry

    // Retrieve a pointer to a loaded program.
    ShaderProgram* getProgram(const std::string& name);

    // Disable copy/move to enforce singleton pattern
    ShaderManager(const ShaderManager&) = delete;
    void operator=(const ShaderManager&) = delete;

private slots:
    // Slot to be connected to the QFileSystemWatcher's signal
    void onFileChanged(const QString& path);

private:
    ShaderManager(); // Private constructor

    QFileSystemWatcher m_fileWatcher;
    std::map<std::string, std::unique_ptr<ShaderProgram>> m_programs;
    // Maps a file path back to the program name that uses it
    std::map<std::string, std::string> m_pathToProgramName;
};

2.2. The ShaderProgram Class
This class encapsulates a single, compiled and linked GLSL shader program. It is responsible for its own state, including its OpenGL ID, uniform locations, and the logic for reloading itself.

Responsibilities:

Encapsulation: Hold the GLuint program ID and file paths for its constituent shaders.

Compilation & Linking: Contain the logic to read source files, compile the individual shaders, link them into a program, and report detailed errors.

Uniform Management: Provide a convenient and efficient way to set uniform values by caching their locations.

Self-Reloading: Contain the core reload() logic that attempts to create a new program from modified files and, on success, replaces the old one.

Class Specification:

class ShaderProgram {
public:
    ShaderProgram(const std::string& vertexPath,
                  const std::string& fragmentPath,
                  const std::string& geometryPath = "");

    // Attempts to re-compile and re-link the program from its source files.
    // Returns true on success. On failure, the previous program remains active.
    bool reload();

    // Binds the program for use.
    void use();

    // Uniform setter methods (examples)
    void setUniform(const std::string& name, int value);
    void setUniform(const std::string& name, float value);
    void setUniform(const std::string& name, const glm::mat4& matrix);

    // Getters for file paths, needed by the manager for the file watcher.
    const std::string& getVertexPath() const;
    // ... other getters for frag/geom paths

private:
    GLuint m_programID = 0;
    std::map<std::string, GLint> m_uniformLocations;

    // File information
    std::string m_vertexPath, m_fragmentPath, m_geometryPath;
    // Timestamps to prevent redundant reloads
    std::filesystem::file_time_type m_vertexTimestamp;
    // ... other timestamps

    // Private helper for compilation logic
    bool compileAndLink();
};

3. Hot-Reloading Mechanism
A "Developer Mode," enabled via a compile-time flag or a runtime setting, will activate the hot-reloading feature.

Workflow:

Initialization: When ShaderManager::loadProgram is called, it registers the shader source file paths with its internal QFileSystemWatcher.

File Modification: A developer saves a change to a .glsl file.

Signal Emission: The QFileSystemWatcher detects the change and emits the fileChanged(path) signal.

Slot Execution: The ShaderManager::onFileChanged(path) slot is invoked. It looks up which ShaderProgram corresponds to the modified path.

Reload Attempt: The manager calls the reload() method on the identified ShaderProgram.

Outcome:

Success: The ShaderProgram compiles the new source, deletes the old GPU program, and replaces it with the new one. A success message is logged.

Failure (e.g., syntax error): The compilation fails. The reload() method immediately returns false, leaving the last working program active on the GPU. A detailed compilation/link error log from OpenGL is printed to the console. The application does not crash.

4. Specific Plotting Scenarios & Shader Design
This section outlines the initial set of shader programs required for common visualization tasks.

4.1. Primitive Line Renderer (Axes, Grids, Ticks)
A single, versatile program for drawing all simple lines with controllable thickness.

Program Name: "primitive_line"

Shaders: line.vert, line.geom, line.frag

Description: This program draws simple, colored lines. It uses a geometry shader to expand a line into a quad, allowing for arbitrary thickness control.

Vertex Shader (line.vert):

Takes a vec2 position.

Passes the position directly to the geometry shader (no MVP transformation yet).

Geometry Shader (line.geom):

Input: lines

Output: triangle_strip (a quad)

Uniforms: mat4 u_MVP, float u_thickness.

Logic: For each line, it calculates the line's direction and normal in screen space. It uses u_thickness to generate four vertices for a quad (a "thick line") and transforms each with u_MVP. Passes attributes like distance along the line to the fragment shader.

Fragment Shader (line.frag):

Uniforms: vec3 u_Color, bool u_isDashed.

If u_isDashed is true, it uses an incoming attribute from the geometry shader to discard fragments, creating a dashed effect.

Outputs u_Color.

4.2. 2D Scatter Plot Renderer
A program designed to render thousands of points, where each point can have unique properties.

Program Name: "scatter_2d"

Shaders: scatter.vert, scatter.frag

Description: Renders points using gl_PointSize.

Vertex Shader (scatter.vert):

Vertex Attributes: vec2 a_position, vec3 a_color, float a_size.

Transforms a_position with u_MVP.

Sets gl_PointSize = a_size.

Passes a_color to the fragment shader.

Hover Logic: Compares gl_VertexID with a int u_hoveredPointID uniform. If they match, it increases gl_PointSize and modifies the output color.

Fragment Shader (scatter.frag):

Simply outputs the interpolated color received from the vertex shader. Uses gl_PointCoord to draw circular points instead of square ones.

4.3. 2D Texture Renderer (Heatmaps)
A simple program for drawing a texture mapped to a quad, ideal for heatmaps or image displays.

Program Name: "texture_2d"

Shaders: texture.vert, texture.frag

Description: Renders a quad and samples a texture.

Vertex Shader (texture.vert):

Takes vec2 a_position and vec2 a_texCoord.

Passes texture coordinates through to the fragment shader.

Fragment Shader (texture.frag):

Uniforms: sampler2D u_dataTexture, sampler1D u_colormapTexture, float u_minVal, float u_maxVal.

Samples the u_dataTexture to get a raw data value (e.g., from a single red channel).

Normalizes this value using u_minVal and u_maxVal to get a [0, 1] range.

Uses the normalized value as a texture coordinate to sample the 1D u_colormapTexture, retrieving the final color.

5. Detailed Plotting Scenario: Themed Plot with Axes and Grids
This section describes the implementation of a standard plot view with light/dark modes and dynamic axes/grids, using the components defined above.

C++ Application Responsibilities:
Theme Management: The application will hold the current theme state (e.g., enum Theme { Light, Dark }).

When the theme is Dark, the glClearColor will be set to a dark gray (e.g., 0.1, 0.1, 0.1, 1.0). The color passed to the line shader's u_Color uniform will be white (1.0, 1.0, 1.0).

When the theme is Light, glClearColor will be white or light gray, and the u_Color for lines will be black.

View Calculation: The application is responsible for tracking the current visible world-space coordinates (the view extents) based on user panning and zooming.

Vertex Generation: On every frame or view change, the C++ code will dynamically generate the vertex data for the axes and grid lines.

"Infinite" Axes: To make an axis span the canvas, its vertices are generated based on the current view extents. For the Y-axis, the line vertices would be (0, view.minY) and (0, view.maxY).

Grid Lines: The application will calculate the positions for each grid line based on the view extents and a user-defined spacing. For example, it will loop from view.minX to view.maxX with a given step, generating a vertical line at each step that spans from view.minY to view.maxY.

Rendering Calls: The application orchestrates the drawing.

Set the appropriate glClearColor.

Clear the screen.

Get the "primitive_line" shader program from the ShaderManager.

Bind the shader.

Set uniforms for the axes: u_Color (white/black), u_thickness (e.g., 2.0), u_isDashed (false).

Bind the VBO with axis vertex data and call glDrawArrays.

If grids are enabled, set uniforms for the grid: u_thickness (e.g., 1.0), u_isDashed (true or false depending on desired style).

Bind the VBO with grid line vertex data and call glDrawArrays.

Shader Responsibilities (primitive_line program):
The shader's only job is to correctly render the line primitives it is given. It is completely stateless regarding the plot's theme or zoom level. It simply executes its rendering technique based on the vertex data and uniform values it receives for a single draw call. This creates a clean separation of concerns.