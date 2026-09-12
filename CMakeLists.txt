cmake_minimum_required(VERSION 3.15)

project(nino VERSION 0.2.1 LANGUAGES C)

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
    src/common.h

    src/editor/action.c
    src/editor/action.h
    src/editor/buildnum.c
    src/editor/buildnum.h
    src/editor/config.c
    src/editor/config.h
    src/editor/console.c
    src/editor/console.h
    src/editor/editor.c
    src/editor/editor.h
    src/editor/file_io.c
    src/editor/file_io.h
    src/editor/highlight.c
    src/editor/highlight.h
    src/editor/row.c
    src/editor/row.h
    src/editor/search.c
    src/editor/search.h
    src/editor/select.c
    src/editor/select.h

    src/editor/panels/edit.c
    src/editor/panels/edit.h
    src/editor/panels/explorer.c
    src/editor/panels/explorer.h
    src/editor/panels/prompt.c
    src/editor/panels/prompt.h
    src/editor/panels/welcome.c
    src/editor/panels/welcome.h

    src/ui/compositor.c
    src/ui/compositor.h
    src/ui/layout.c
    src/ui/layout.h
    src/ui/panel.h
    src/ui/rect.h
    src/ui/surface.c
    src/ui/surface.h

    src/utils/json.c
    src/utils/json.h
    src/utils/opt.h
    src/utils/os.h
    src/utils/unicode.c
    src/utils/unicode.h
    src/utils/utils.c
    src/utils/utils.h

    src/backend/platform.h
    src/backend/shared/event.c
    src/backend/shared/event.h
    src/backend/shared/frame_differ.c
    src/backend/shared/frame_differ.h
    src/backend/terminal/main.c
    src/backend/terminal/input.c
    src/backend/terminal/output.c
    src/backend/terminal/terminal.c
    src/backend/terminal/terminal.h
)

if (WIN32)
  list(APPEND CORE_SOURCES
        src/utils/os_win32.c
        src/utils/os_win32.h

        src/backend/terminal/os_win32.c
    )
else()
  list(APPEND CORE_SOURCES
        src/utils/os_unix.c
        src/utils/os_unix.h

        src/backend/terminal/os_unix.c
    )
endif()

add_executable(${PROJECT_NAME} ${CORE_SOURCES} ${BUNDLED_FILE})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_custom_target(file_toucher
    COMMAND ${CMAKE_COMMAND} -E touch_nocreate
            ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/buildnum.c
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
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic -Wshadow -include "${COMMON_HEADER}")
endif()

install(TARGETS ${PROJECT_NAME})
