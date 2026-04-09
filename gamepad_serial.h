#ifndef GAMEPAD_SERIAL_H
#define GAMEPAD_SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef GAMEPAD_SERIAL_EXPORTS
    #define GAMEPAD_SERIAL_API __declspec(dllexport)
#else
    #define GAMEPAD_SERIAL_API __declspec(dllimport)
#endif

#define GAMEPAD_BTN_MAX   16
#define GAMEPAD_PORT_MAX  256

typedef struct {
    int sx, sy;
    char pressed[GAMEPAD_BTN_MAX][16];
    int pressed_count;
    int connected;
} GamepadState;

typedef struct {
    char port[GAMEPAD_PORT_MAX];
    int hz;
} GamepadConfig;

GAMEPAD_SERIAL_API int gamepad_init(GamepadConfig* config);
GAMEPAD_SERIAL_API void gamepad_shutdown(void);
GAMEPAD_SERIAL_API int gamepad_update(GamepadState* out_state);
GAMEPAD_SERIAL_API int gamepad_is_running(void);
GAMEPAD_SERIAL_API char** gamepad_get_ports(int* count);

#ifdef __cplusplus
}
#endif

#endif // GAMEPAD_SERIAL_H