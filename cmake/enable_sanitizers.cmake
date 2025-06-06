
if (enableAddressSanitizer)
    if (WIN32)
        if (MSVC)
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /fsanitize=address")
            add_compile_definitions(_DISABLE_STRING_ANNOTATION=1 _DISABLE_VECTOR_ANNOTATION=1)
            message(STATUS "This is a Debug build for MSVC, enabling address sanitizer")
        endif()
    endif()

    if (UNIX)
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            message(STATUS "This is a Debug build for Clang, enabling address sanitizer")
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fPIC")
            message(STATUS "This is a Debug build for GCC, enabling address sanitizer")
        endif()
    endif()
endif()
