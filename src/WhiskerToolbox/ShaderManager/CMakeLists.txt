add_library(ShaderManager
    ShaderManager.cpp
    ShaderManager.hpp
    ShaderProgram.cpp
    ShaderProgram.hpp
    ShaderSourceType.hpp
)

target_link_libraries(ShaderManager
    Qt6::Core
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Gui
    Qt6::Widgets
    glm::glm
)

target_include_directories(ShaderManager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}) 