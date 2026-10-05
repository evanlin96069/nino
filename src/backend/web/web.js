var Module = Module || {};

// Render

const canvas = document.getElementById("editor");
const ctx = canvas.getContext("2d");

const FONT_SIZE = 16;
const LINE_HEIGHT = 20;

const FONT = `${FONT_SIZE}px monospace`;

ctx.font = FONT;
ctx.textBaseline = "alphabetic";

const cellWidth = Math.round(ctx.measureText("M").width * (window.devicePixelRatio || 1)) / (window.devicePixelRatio || 1);
const cellHeight = LINE_HEIGHT;

const metrics = ctx.measureText("M");
const fontAscent = metrics.fontBoundingBoxAscent ?? FONT_SIZE * 0.8;
const fontDescent = metrics.fontBoundingBoxDescent ?? FONT_SIZE * 0.2;
const baselineOffset = Math.round(
    (cellHeight - (fontAscent + fontDescent)) / 2 + fontAscent
);

function resizeCanvas(columns, rows) {
    const dpr = window.devicePixelRatio || 1;
    canvas.width = Math.round(columns * cellWidth * dpr);
    canvas.height = Math.round(rows * cellHeight * dpr);
    canvas.style.width = `${canvas.width / dpr}px`;
    canvas.style.height = `${canvas.height / dpr}px`;
    canvas.getContext("2d").setTransform(dpr, 0, 0, dpr, 0, 0);

    ctx.font = FONT;
    ctx.textBaseline = "alphabetic";
}

function snap(v) {
    const dpr = window.devicePixelRatio || 1;
    return Math.round(v * dpr) / dpr;
}

function drawBackground(x, y, width, r, g, b, a) {
    ctx.fillStyle = `rgba(${r}, ${g}, ${b}, ${a / 255})`;

    const left = snap(x * cellWidth);
    const right = snap((x + width) * cellWidth);
    const top = snap(y * cellHeight);
    const bottom = snap((y + 1) * cellHeight);

    ctx.fillRect(left, top, right - left, bottom - top);
}

function drawGrapheme(x, y, grapheme, r, g, b, a) {
    ctx.fillStyle = `rgba(${r}, ${g}, ${b}, ${a / 255})`;

    const canvasY = y * cellHeight + baselineOffset;
    const canvasX = x * cellWidth;
    ctx.fillText(grapheme, canvasX, canvasY);
}

const cursor = document.getElementById("cursor");

function drawCursor(visible, x, y) {
    cursor.style.display = visible ? "block" : "none";

    if (visible) {
        cursor.style.left = `${x * cellWidth}px`;
        cursor.style.top = `${y * cellHeight}px`;
        cursor.style.width = `${cellWidth}px`;
        cursor.style.height = `${cellHeight}px`;
    }
}

// Event

let terminalColumns = 0;
let terminalRows = 0;

function resize() {
    const width = window.innerWidth;
    const height = window.innerHeight;

    const columns = Math.floor(width / cellWidth);
    const rows = Math.floor(height / cellHeight);

    if (columns === terminalColumns && rows === terminalRows) {
        return;
    }

    terminalColumns = columns;
    terminalRows = rows;

    resizeCanvas(columns, rows);
    Module._webEventResize(columns, rows);
}

const keyboard = document.getElementById("keyboard");

let dragging = false;
let dragButton = -1;
const BUTTON_OFFSET = [0, 2, 1]; // 0 left, 1 middle, 2 right
let composing = false;

const KEY = {
    BACKSPACE: 0, ENTER: 1, LEFT: 2, RIGHT: 3, UP: 4, DOWN: 5,
    HOME: 6, END: 7, PAGE_UP: 8, PAGE_DOWN: 9, TAB: 10, BACK_TAB: 11,
    DELETE: 12, INSERT: 13, F: 14, ESC: 15, CHAR: 16, TEXT: 17
};
const MOD = { SHIFT: 1, ALT: 2, CTRL: 4, META: 8 };
const MOUSE = {
    MOUSE1_PRESSED: 0, MOUSE2_PRESSED: 1, MOUSE3_PRESSED: 2,
    MOUSE1_RELEASED: 3, MOUSE2_RELEASED: 4, MOUSE3_RELEASED: 5,
    MOUSE1_DRAG: 6, MOUSE2_DRAG: 7, MOUSE3_DRAG: 8,
    MWHEEL_UP: 9, MWHEEL_DOWN: 10
};

function modifiers(event) {
    return (event.shiftKey ? MOD.SHIFT : 0) |
           (event.altKey ? MOD.ALT : 0) |
           (event.ctrlKey ? MOD.CTRL : 0) |
           (event.metaKey ? MOD.META : 0);
}

function keyEvent(event) {
    if (composing || event.isComposing) return;
    if (["Shift", "Control", "Alt", "Meta", "AltGraph"].includes(event.key))
        return;
    const mods = modifiers(event);
    const special = {
        Backspace: KEY.BACKSPACE, Enter: KEY.ENTER,
        ArrowLeft: KEY.LEFT, ArrowRight: KEY.RIGHT,
        ArrowUp: KEY.UP, ArrowDown: KEY.DOWN,
        Home: KEY.HOME, End: KEY.END,
        PageUp: KEY.PAGE_UP, PageDown: KEY.PAGE_DOWN,
        Tab: event.shiftKey ? KEY.BACK_TAB : KEY.TAB,
        Delete: KEY.DELETE, Insert: KEY.INSERT, Escape: KEY.ESC
    };

    let code;
    let id = 0;
    let unicode = 0;

    if (event.key in special) {
        code = special[event.key];
    } else if (/^F([1-9]|1[0-2])$/.test(event.key)) {
        code = KEY.F;
        id = Number(event.key.slice(1));
    } else if (event.key.length > 0 &&
               (mods & (MOD.CTRL | MOD.META | MOD.ALT)) &&
               !event.getModifierState("AltGraph")) {
        code = KEY.CHAR;
        id = event.key.toUpperCase().codePointAt(0);
    } else if (event.key && !["Dead", "Process", "Unidentified"].includes(event.key)) {
        event.preventDefault();
        for (const char of event.key) {
            Module._webEventKey(KEY.TEXT, 0, 0, char.codePointAt(0), 0);
        }
        return;
    } else {
        return;
    }

    event.preventDefault();
    Module._webEventKey(code, mods, id, unicode,  0);
}

function point(event) {
    const rect = canvas.getBoundingClientRect();
    return {
        x: Math.max(0, Math.floor((event.clientX - rect.left) / cellWidth)),
        y: Math.max(0, Math.floor((event.clientY - rect.top) / cellHeight))
    };
}

function sendMouse(type, event) {
    const p = point(event);
    Module._webEventMouse(type, p.x, p.y);
}

Module.preRun = Module.preRun || [];
Module.onRuntimeInitialized = function () {
    canvas.addEventListener("pointerdown", (event) => {
        canvas.setPointerCapture(event.pointerId);
        keyboard.focus({ preventScroll: true });
        if (dragging || event.button < 0 || event.button > 2) return;
        dragging = true;
        dragButton = event.button;
        sendMouse(MOUSE.MOUSE1_PRESSED + BUTTON_OFFSET[dragButton], event);
    });
    canvas.addEventListener("pointermove", (event) => {
        if (dragging) {
            sendMouse(MOUSE.MOUSE1_DRAG + BUTTON_OFFSET[dragButton], event);
        }
    });
    canvas.addEventListener("pointerup", (event) => {
        if (dragging && event.button === dragButton) {
            sendMouse(MOUSE.MOUSE1_RELEASED + BUTTON_OFFSET[dragButton], event);
            dragging = false;
            dragButton = -1;
        }
    });

    canvas.addEventListener("contextmenu", (event) => event.preventDefault());

    canvas.addEventListener("wheel", (event) => {
        event.preventDefault();
        sendMouse(event.deltaY < 0 ? MOUSE.MWHEEL_UP : MOUSE.MWHEEL_DOWN, event);
    },  { passive: false });

    document.addEventListener("keydown", keyEvent, true);
    keyboard.addEventListener("input", (event) => {
        const text = event.data || keyboard.value;
        for (const char of text) {
            Module._webEventKey(KEY.TEXT, 0, 0, char.codePointAt(0), 0);
        }
        keyboard.value = "";
    });

    keyboard.addEventListener("compositionstart", () => { composing = true; });
    keyboard.addEventListener("compositionend", () => { composing = false; });

    keyboard.addEventListener("paste", (event) => {
        const text = event.clipboardData.getData("text/plain");
        const bytes = Module.lengthBytesUTF8(text) + 1;
        const ptr = Module._malloc(bytes);
        Module.stringToUTF8(text, ptr, bytes);
        Module._webEventPaste(ptr, bytes - 1);
        Module._free(ptr);
        event.preventDefault();
    });

    window.addEventListener("resize", resize);

    window.addEventListener("focus", () => {
        Module._webEventFocus(1);
    });
    window.addEventListener("blur", () => {
        Module._webEventFocus(0)
    });

    // Start
    Module._webStart(1, 1);
    resize();
    keyboard.focus({ preventScroll: true });
};
