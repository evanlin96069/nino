cmake_minimum_required(VERSION 3.22.0)

project(nino VERSION 0.0.6 LANGUAGES C)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)

set(RESOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/resources")

set (SYNTAX_FILES
    ${RESOURCE_DIR}/syntax/c.json
    ${RESOURCE_DIR}/syntax/cpp.json
    ${RESOURCE_DIR}/syntax/java.json
    ${RESOURCE_DIR}/syntax/json.json
    ${RESOURCE_DIR}/syntax/make.json
    ${RESOURCE_DIR}/syntax/python.json
    ${RESOURCE_DIR}/syntax/rust.json
    ${RESOURCE_DIR}/syntax/zig.json
)

set (BUNDLER_SOURCE "${RESOURCE_DIR}/bundler.c")

add_executable(bundler ${BUNDLER_SOURCE})

set (BUNDLER_BIN $<TARGET_FILE:bundler>)
set (BUNDLED_FILE "${RESOURCE_DIR}/bundle.h")

add_custom_command(
    OUTPUT ${BUNDLED_FILE}
    COMMAND ${BUNDLER_BIN} ${BUNDLED_FILE} ${SYNTAX_FILES}
    DEPENDS bundler ${SYNTAX_FILES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)

set (CORE_SOURCES
    src/action.c
    src/action.h
    src/config.c
    src/config.h
    src/defines.h
    src/editor.c
    src/editor.h
    src/file_io.c
    src/file_io.h
    src/highlight.c
    src/highlight.h
    src/input.c
    src/input.h
    src/json.h
    src/nino.c
    src/os.h
    src/output.c
    src/output.h
    src/prompt.c
    src/prompt.h
    src/row.c
    src/row.h
    src/select.c
    src/select.h
    src/terminal.c
    src/terminal.h
    src/unicode.c
    src/unicode.h
    src/utils.c
    src/utils.h
)

if (WIN32)
    list(APPEND CORE_SOURCES
        src/os_win32.c
        src/os_win32.h
    )
else()
    list(APPEND CORE_SOURCES
        src/os_unix.c
        src/os_unix.h
    )
endif()

add_executable(${PROJECT_NAME} ${CORE_SOURCES} ${BUNDLED_FILE})

target_compile_definitions(${PROJECT_NAME} PRIVATE
    EDITOR_VERSION="${CMAKE_PROJECT_VERSION}"
)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /wd4244 /wd4267 /wd4996)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
endif()

install(TARGETS ${PROJECT_NAME})
