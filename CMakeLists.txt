cmake_minimum_required(VERSION 3.15)

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
set (BUNDLED_FILE "${RESOURCE_DIR}/bundle.h")

if(CMAKE_CROSSCOMPILING)
    # bundle.h must be pre-generated before cross-compiling
    set_source_files_properties(${BUNDLED_FILE} PROPERTIES GENERATED TRUE)
else()
    add_executable(bundler ${BUNDLER_SOURCE})

    set (BUNDLER_BIN $<TARGET_FILE:bundler>)

    add_custom_command(
        OUTPUT ${BUNDLED_FILE}
        COMMAND ${BUNDLER_BIN} ${BUNDLED_FILE} ${SYNTAX_FILES}
        DEPENDS bundler ${SYNTAX_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    )
endif()

set (CORE_SOURCES
    src/action.c
    src/action.h
    src/buildnum.c
    src/buildnum.h
    src/config.c
    src/config.h
    src/common.h
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
    src/opt.h
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

add_custom_target(file_toucher
    COMMAND ${CMAKE_COMMAND} -E touch_nocreate
            ${CMAKE_CURRENT_SOURCE_DIR}/src/buildnum.c
)

add_dependencies(${PROJECT_NAME} file_toucher)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    EDITOR_NAME="${PROJECT_NAME}"
    EDITOR_VERSION="${CMAKE_PROJECT_VERSION}"
)

set(COMMON_HEADER "${CMAKE_SOURCE_DIR}/src/common.h")

if (MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /wd4244 /wd4267 /wd4996 /FI "${COMMON_HEADER}")
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic -include "${COMMON_HEADER}")
endif()

install(TARGETS ${PROJECT_NAME})
