#include "web.h"

#include "editor/console.h"
#include "editor/editor.h"
#include "editor/panels/explorer.h"

// clang-format off

EM_JS(void, webPanic, (const char* file, int line, const char* message), {
    const fileName = UTF8ToString(file);
    const text = UTF8ToString(message);

    const nl = String.fromCharCode(10);
    alert('Fatal error at ' + fileName + ' : ' + line + nl + nl + text);

    window.location.reload();
})

EM_JS(void, webCopyText, (const char* data, int size), {
    if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(UTF8ToString(data, size));
    }
})

// clang-format on

void platformPanic(const char* file, int line, const char* message) {
    webPanic(file, line, message);
}

void platformCopyToSysClipboard(StrView text) {
    webCopyText(text.data, (int)text.size);
}

void platformSuspend(void) {
    editorMsg("Suspend is unavailable on the web.");
}

void platformRunShell(const char* shell_hint, const char* command) {
    UNUSED(shell_hint);
    UNUSED(command);
    editorMsg("Shell is unavailable on the web.");
}

EMSCRIPTEN_KEEPALIVE void webStart(void) {
    editorInit();
    webInit();
    editorLoadInitConfig();

#ifdef DEMO_WORKSPACE_PATH
    editorExplorerOpenDir(DEMO_WORKSPACE_PATH);
    if (gEditor.explorer_panel->node) {
        uiPanelSetFocused(&gEditor.ui, (Panel*)gEditor.explorer_panel);
    }
#endif

    gEditor.state = STATE_RUNNING;
}
