#include "os.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>

#include "terminal.h"

#include "utils/os.h"

#define SIGWINCH_BYTE 0x01
#define SIGTSTP_BYTE 0x02
#define SIGCONT_BYTE 0x03

static int sig_rd = -1, sig_wr = -1;
static volatile sig_atomic_t winch_queued = 0;
static volatile sig_atomic_t tstp_queued = 0;

static int tty_fd = -1;

static void SIGWINCH_handler(int sig) {
    UNUSED(sig);
    if (!winch_queued) {
        winch_queued = 1;
        const uint8_t b = SIGWINCH_BYTE;
        UNUSED(write(sig_wr, &b, 1));
    }
}

static void SIGTSTP_handler(int sig) {
    UNUSED(sig);
    if (!tstp_queued) {
        tstp_queued = 1;
        const uint8_t b = SIGTSTP_BYTE;
        UNUSED(write(sig_wr, &b, 1));
    }
}

static void SIGCONT_handler(int sig) {
    UNUSED(sig);
    if (tstp_queued) {
        const uint8_t b = SIGCONT_BYTE;
        UNUSED(write(sig_wr, &b, 1));
    }
}

static int installSIGTSTPHandler(void) {
    struct sigaction tstp_action = {
        .sa_handler = SIGTSTP_handler,
    };
    sigemptyset(&tstp_action.sa_mask);
    return sigaction(SIGTSTP, &tstp_action, NULL);
}

void terminalOsDeinit(void) {
    if (tty_fd != -1) {
        close(tty_fd);
        tty_fd = -1;
    }
    if (sig_rd != -1) {
        close(sig_rd);
        sig_rd = -1;
    }
    if (sig_wr != -1) {
        close(sig_wr);
        sig_wr = -1;
    }
}

void terminalOsInit(void) {
    tty_fd = open("/dev/tty", O_RDWR);
    if (tty_fd == -1)
        PANIC("Failed to open /dev/tty");

    int p[2];
    if (pipe(p) == -1) {
        PANIC("Failed to create pipe for signal handling");
    }
    sig_rd = p[0];
    sig_wr = p[1];

    struct sigaction winch_action = {
        .sa_handler = SIGWINCH_handler,
    };
    sigemptyset(&winch_action.sa_mask);
    if (sigaction(SIGWINCH, &winch_action, NULL) == -1) {
        PANIC("Failed to install SIGWINCH handler");
    }

    if (installSIGTSTPHandler() == -1) {
        PANIC("Failed to install SIGTSTP handler");
    }

    struct sigaction cont_action = {
        .sa_handler = SIGCONT_handler,
    };
    sigemptyset(&cont_action.sa_mask);
    if (sigaction(SIGCONT, &cont_action, NULL) == -1) {
        PANIC("Failed to install SIGCONT handler");
    }
}

static struct termios orig_termios;

bool isStdinTty(void) {
    return isatty(STDIN_FILENO);
}

void enableRawMode(void) {
    if (tcgetattr(tty_fd, &orig_termios) == -1)
        PANIC("Unable to read terminal attributes");

    struct termios raw = orig_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(tty_fd, TCSAFLUSH, &raw) == -1)
        PANIC("Unable to enable raw terminal mode");
}

void disableRawMode(void) {
    UNUSED(tcsetattr(tty_fd, TCSAFLUSH, &orig_termios));
}

static bool has_pending_resize = false;

static bool readConsoleByte(uint8_t* out, int timeout_ms) {
    struct pollfd fds[2] = {
        {.fd = tty_fd, .events = POLLIN},
        {.fd = sig_rd, .events = POLLIN},
    };

    while (true) {
        int ret = poll(fds, 2, timeout_ms);
        if (ret <= 0)
            return false;

        if (fds[0].revents & POLLIN)
            return read(tty_fd, out, 1) == 1;

        if (fds[1].revents & POLLIN) {
            uint8_t buf[64];
            ssize_t n = read(sig_rd, buf, sizeof(buf));
            for (ssize_t i = 0; i < n; i++) {
                switch (buf[i]) {
                    case SIGWINCH_BYTE:
                        has_pending_resize = true;
                        winch_queued = 0;
                        break;
                    case SIGTSTP_BYTE: {
                        struct sigaction sa = {
                            .sa_handler = SIG_DFL,
                        };
                        sigemptyset(&sa.sa_mask);
                        sigaction(SIGTSTP, &sa, NULL);

                        terminalExit();
                        raise(SIGTSTP);
                    } break;
                    case SIGCONT_BYTE:
                        installSIGTSTPHandler();
                        // Only restore if we're not in background
                        if (tcgetpgrp(tty_fd) == getpgrp()) {
                            terminalStart();
                            tstp_queued = 0;
                        }
                        break;
                    default:
                        break;
                }
            }
            if (has_pending_resize)
                return false;
        }
    }
}

ConsoleEvent readConsoleEvent(int timeout_ms) {
    ConsoleEvent ev = {.type = CONSOLE_EVENT_ERROR};

    if (has_pending_resize) {
        has_pending_resize = false;
        int width, height;
        if (getWindowSize(&width, &height) == 0) {
            ev.type = CONSOLE_EVENT_RESIZE;
            ev.data.resize.width = width;
            ev.data.resize.height = height;
        }
        return ev;
    }

    uint8_t first_byte;
    if (!readConsoleByte(&first_byte, timeout_ms)) {
        if (has_pending_resize) {
            has_pending_resize = false;
            int width, height;
            if (getWindowSize(&width, &height) == 0) {
                ev.type = CONSOLE_EVENT_RESIZE;
                ev.data.resize.width = width;
                ev.data.resize.height = height;
                return ev;
            }
        }
        ev.type = CONSOLE_EVENT_TIMEOUT;
        return ev;
    }

    ev.type = CONSOLE_EVENT_KEY;

    // ASCII fast-path
    if ((first_byte & 0x80) == 0x00) {
        ev.data.unicode = (uint32_t)first_byte;
        return ev;
    }

    // Decode UTF-8
    int bytes;
    if ((first_byte & 0xE0) == 0xC0) {
        ev.data.unicode = (first_byte & 0x1F) << 6;
        bytes = 1;
    } else if ((first_byte & 0xF0) == 0xE0) {
        ev.data.unicode = (first_byte & 0x0F) << 12;
        bytes = 2;
    } else if ((first_byte & 0xF8) == 0xF0) {
        ev.data.unicode = (first_byte & 0x07) << 18;
        bytes = 3;
    } else {
        ev.type = CONSOLE_EVENT_ERROR;
        return ev;
    }

    int shift = (bytes - 1) * 6;
    for (int i = 0; i < bytes; i++) {
        uint8_t byte;
        if (!readConsoleByte(&byte, READ_GRACE_MS)) {
            ev.type = CONSOLE_EVENT_ERROR;
            return ev;
        }
        if ((byte & 0xC0) != 0x80) {
            ev.type = CONSOLE_EVENT_ERROR;
            return ev;
        }

        ev.data.unicode |= (byte & 0x3F) << shift;
        shift -= 6;
    }

    return ev;
}

int writeConsole(const void* buf, size_t count) {
    return write(tty_fd, buf, count);
}

int getWindowSize(int* width, int* height) {
    struct winsize ws;
    if (ioctl(tty_fd, TIOCGWINSZ, &ws) != -1 && ws.ws_col != 0) {
        *width = ws.ws_col;
        *height = ws.ws_row;
        return 0;
    }
    return -1;
}

void platformSuspend(void) {
    kill(0, SIGTSTP);
}

static void setSignalHandlerDefaults(void) {
    struct sigaction sa = {
        .sa_handler = SIG_DFL,
    };
    sigemptyset(&sa.sa_mask);

    sigaction(SIGTSTP, &sa, NULL);
    sigaction(SIGWINCH, &sa, NULL);
    sigaction(SIGCONT, &sa, NULL);
}

static void installSignalHandlers(void) {
    struct sigaction winch_action = {
        .sa_handler = SIGWINCH_handler,
    };
    sigemptyset(&winch_action.sa_mask);
    sigaction(SIGWINCH, &winch_action, NULL);

    installSIGTSTPHandler();

    struct sigaction cont_action = {
        .sa_handler = SIGCONT_handler,
    };
    sigemptyset(&cont_action.sa_mask);
    sigaction(SIGCONT, &cont_action, NULL);
}

void platformRunShell(const char* shell_hint, const char* cmd) {
    const char* shell = NULL;
    if (shell_hint && shell_hint[0] && access(shell_hint, X_OK) == 0) {
        shell = shell_hint;
    }
    if (!shell) {
        const char* env_shell = getenv("SHELL");
        if (env_shell && env_shell[0] && access(env_shell, X_OK) == 0) {
            shell = env_shell;
        }
    }
    if (!shell) {
        shell = "/bin/sh";
    }

    terminalExit();
    setSignalHandlerDefaults();

    pid_t pid = fork();
    if (pid < 0) {
        // Fork failed
        terminalStart();
        return;
    }

    if (pid == 0) {
        // Child
        dup2(tty_fd, STDIN_FILENO);
        dup2(tty_fd, STDOUT_FILENO);
        dup2(tty_fd, STDERR_FILENO);
        if (tty_fd > STDERR_FILENO) {
            close(tty_fd);
        }
        close(sig_rd);
        close(sig_wr);
        const char* args[] = {shell, "-c", cmd, NULL};
        execv(shell, (char* const*)args);
        _exit(127);
    }

    // Parent
    int status;
    while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {
    }

    const char* press_key_msg = "\r\nPress any key to continue...";
    UNUSED(write(tty_fd, press_key_msg, strlen(press_key_msg)));

    // echo off
    struct termios cbreak = orig_termios;
    cbreak.c_lflag &= ~(ECHO | ICANON);
    cbreak.c_cc[VMIN] = 1;
    cbreak.c_cc[VTIME] = 0;
    UNUSED(tcsetattr(tty_fd, TCSAFLUSH, &cbreak));

    uint8_t dummy;
    UNUSED(read(tty_fd, &dummy, 1));
    UNUSED(write(tty_fd, "\r\n", 2));

    UNUSED(tcsetattr(tty_fd, TCSAFLUSH, &orig_termios));

    // Drain queued signals
    struct pollfd pfd = {.fd = sig_rd, .events = POLLIN};
    while (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
        uint8_t buf[64];
        ssize_t n = read(sig_rd, buf, sizeof(buf));
        if (n <= 0)
            break;
    }
    winch_queued = 0;
    tstp_queued = 0;
    installSignalHandlers();

    terminalStart();
}
