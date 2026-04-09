#include "gamepad_serial.h"

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <string.h>

// Exports; do NOT define GAMEPAD_SERIAL_EXPORTS here
#define GAMEPAD_SERIAL_API __declspec(dllexport)

// Safe string copy
#define STRNCPY_SAFE(dst, src, sz) \
    do { snprintf((dst), (sz), "%s", (src)); } while (0)

// Serial constants
#define REQ        ((BYTE)'R')
#define BAUD       115200
#define DATA_LEN   4

// Button names (same as N64Tester)
static const char* BUTTON_NAMES[16] = {
    "A", "B", "Z", "Start",
    "DUp", "DDown", "DLeft", "DRight",
    "L", "R", "CUp", "CDown", "CLeft", "CRight",
    NULL, NULL
};

// Exactly Python decode_stick
static int decode_stick(BYTE encoded) {
    int mag = (int)(encoded & 0x7F);
    if (encoded & 0x80) return -mag;
    return mag;
}

// Exactly Python decode_buttons
static int decode_buttons(WORD b, char (*out_names)[16]) {
    int cnt = 0;
    for (int i = 0; i < 16; ++i) {
        if ((b & (1U << (15 - i))) && BUTTON_NAMES[i]) {
            STRNCPY_SAFE(out_names[cnt], BUTTON_NAMES[i], 16);
            ++cnt;
        }
    }
    return cnt;
}

// Global state
static volatile LONG g_running = 0;   // 0/1 flag, not CV-qualified
static HANDLE g_thread = NULL;
static HANDLE g_port = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_cs;

static GamepadState g_state;
static int g_hertz = 60;

// Clamp 1..24This matches your Python `hz = max(1, min(240, config.hz))`.
static int clamped_hertz(int hz) {
    if (hz < 1) return 1;
    if (hz > 240) return 240;
    return hz;
}

// Thread-safe copy
static void copy_state(GamepadState* dst) {
    EnterCriticalSection(&g_cs);
    *dst = g_state;
    LeaveCriticalSection(&g_cs);
}

// Free helper (must be before `gamepad_get_ports`)
static void free_ports(char** ports, int count) {
    if (!ports) return;
    for (int i = 0; i < count; ++i) {
        if (ports[i]) free(ports[i]);
    }
    free(ports);
}

// Background worker (exactly your Python `read_loop` logic)
static DWORD WINAPI worker_thread(LPVOID unused) {
    (void)unused;

    // Clamped 1..240 Hz
    double dt_ms = 1000.0 / (double)g_hertz;
    BYTE buf[DATA_LEN];
    DWORD wr, rd;

    // One byte: it is fine to pass its address
    const BYTE req_byte = REQ;

    g_state.connected = 0;

    for (;;) {
        if (!g_running) break;

        // Write REQ ('R') – no C2101
        if (!WriteFile(g_port, &req_byte, 1, &wr, NULL) || wr != 1) {
            g_state.connected = 0;
            Sleep((DWORD)dt_ms);
            continue;
        }

        // Read 4 bytes
        if (!ReadFile(g_port, buf, DATA_LEN, &rd, NULL) || rd != DATA_LEN) {
            g_state.connected = 0;
            Sleep((DWORD)dt_ms);
            continue;
        }

        g_state.connected = 1;

        int sx = decode_stick(buf[2]);
        int sy = decode_stick(buf[3]);

        WORD b = (WORD)((buf[0] << 8) | buf[1]);
        char pressed_tmp[GAMEPAD_BTN_MAX][16];
        int pressed_cnt = decode_buttons(b, pressed_tmp);

        EnterCriticalSection(&g_cs);  // OK: taking addr of g_cs (not a constant)
        g_state.sx = sx;
        g_state.sy = sy;
        g_state.pressed_count = pressed_cnt;
        for (int i = 0; i < pressed_cnt; ++i) {
            STRNCPY_SAFE(g_state.pressed[i], pressed_tmp[i], 16);
        }
        LeaveCriticalSection(&g_cs);

        DWORD sleep_ms = (DWORD)(dt_ms + 0.5);
        Sleep(sleep_ms);
    }

    return 0;
}

// Exactly like your Python flow: configure serial, start thread
GAMEPAD_SERIAL_API int gamepad_init(GamepadConfig* config) {
    if (!config || !config->port[0]) {
        return 0;
    }

    // Close any old port
    if (g_port != INVALID_HANDLE_VALUE) {
        CloseHandle(g_port);
        g_port = INVALID_HANDLE_VALUE;
    }

    g_hertz = clamped_hertz(config->hz);

    // Open COM port
    g_port = CreateFileA(
        config->port,
        GENERIC_READ | GENERIC_WRITE,
        0, NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (g_port == INVALID_HANDLE_VALUE) {
        return 0;
    }

    // 115200, 8N1
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(g_port, &dcb)) {
        CloseHandle(g_port);
        g_port = INVALID_HANDLE_VALUE;
        return 0;
    }

    dcb.BaudRate = BAUD;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    if (!SetCommState(g_port, &dcb)) {
        CloseHandle(g_port);
        g_port = INVALID_HANDLE_VALUE;
        return 0;
    }

    // Timeouts like Python timeout=0.05
    COMMTIMEOUTS to = {0};
    to.ReadIntervalTimeout = 50;
    to.ReadTotalTimeoutMultiplier = 10;
    to.ReadTotalTimeoutConstant = 50;
    to.WriteTotalTimeoutMultiplier = 0;
    to.WriteTotalTimeoutConstant = 50;
    if (!SetCommTimeouts(g_port, &to)) {
        CloseHandle(g_port);
        g_port = INVALID_HANDLE_VALUE;
        return 0;
    }

    // Shared state
    InitializeCriticalSection(&g_cs);
    memset(&g_state, 0, sizeof(g_state));

    // Start background read loop
    g_running = 1;
    g_thread = CreateThread(NULL, 0, worker_thread, NULL, 0, NULL);
    if (!g_thread) {
        CloseHandle(g_port);
        g_port = INVALID_HANDLE_VALUE;
        DeleteCriticalSection(&g_cs);
        return 0;
    }

    return 1;
}

GAMEPAD_SERIAL_API void gamepad_shutdown(void) {
    g_running = 0;

    if (g_thread) {
        DWORD wait = WaitForSingleObject(g_thread, 2000);
        if (wait == WAIT_TIMEOUT) {
            TerminateThread(g_thread, 1);
        }
        CloseHandle(g_thread);
        g_thread = NULL;
    }

    if (g_port != INVALID_HANDLE_VALUE) {
        CloseHandle(g_port);
        g_port = INVALID_HANDLE_VALUE;
    }

    DeleteCriticalSection(&g_cs);
}

GAMEPAD_SERIAL_API int gamepad_update(GamepadState* out_state) {
    if (!out_state) return 0;
    copy_state(out_state);
    return 1;
}

GAMEPAD_SERIAL_API int gamepad_is_running(void) {
    return g_running != 0;
}

GAMEPAD_SERIAL_API char** gamepad_get_ports(int* count) {
    if (!count) return NULL;

    char** ports = NULL;
    int n = 0, cap = 0;

    for (int i = 1; i <= 256; ++i) {
        char path[16];
        _snprintf_s(path, sizeof(path), _TRUNCATE, "\\\\.\\COM%d", i);

        HANDLE h = CreateFileA(
            path,
            0, 0, NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);

            if (n >= cap) {
                cap += 16;
                char** tmp = (char**)realloc(ports, cap * sizeof(char*));
                if (!tmp) {
                    free_ports(ports, n);
                    return NULL;
                }
                ports = tmp;
            }

            char* buf = (char*)malloc(GAMEPAD_PORT_MAX);
            if (!buf) {
                free_ports(ports, n);
                return NULL;
            }

            snprintf(buf, GAMEPAD_PORT_MAX, "COM%d", i);
            ports[n++] = buf;
        }
    }

    // NULL‑terminate
    char** tmp = (char**)realloc(ports, (n + 1) * sizeof(char*));
    if (tmp) {
        ports = tmp;
        ports[n] = NULL;
    } else {
        if (ports) {
            for (int j = 0; j < n; ++j) free(ports[j]);
            free(ports);
        }
        *count = 0;
        return NULL;
    }

    *count = n;
    return ports;
}