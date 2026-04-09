#include <N64Controller.h>

N64Controller N64_ctrl(2);
#define REQ_BYTE 'R'

// Corrected button bit order
uint16_t get_buttons() {
    uint16_t b = 0;
    N64_ctrl.update();

    if (N64_ctrl.A())        b |= (1 << 15);
    if (N64_ctrl.B())        b |= (1 << 14);
    if (N64_ctrl.Z())        b |= (1 << 13);
    if (N64_ctrl.Start())    b |= (1 << 12);
    if (N64_ctrl.D_up())     b |= (1 << 11);
    if (N64_ctrl.D_down())   b |= (1 << 10);
    if (N64_ctrl.D_left())   b |= (1 << 9);
    if (N64_ctrl.D_right())  b |= (1 << 8);
    if (N64_ctrl.L())        b |= (1 << 7);
    if (N64_ctrl.R())        b |= (1 << 6);
    if (N64_ctrl.C_up())     b |= (1 << 5);
    if (N64_ctrl.C_down())   b |= (1 << 4);
    if (N64_ctrl.C_left())   b |= (1 << 3);
    if (N64_ctrl.C_right())  b |= (1 << 2);

    return b;
}

// Cap stick to [-126, 126] and keep signedness
int8_t cap_stick(int8_t raw) {
    if (raw > 126)  return 126;
    if (raw < -126) return -126;
    return raw;
}

// Encode stick so that direction bit affects only the changing axis
int8_t encode_stick(int8_t raw, int8_t prev) {
    if (raw == prev) return prev;  // no change
    int8_t cap = cap_stick(raw);  // limit magnitude
    uint8_t dir_bit = (cap < 0) ? 0x80 : 0x00;
    uint8_t mag = (cap < 0) ? -cap : cap;
    return (int8_t)(dir_bit | (mag & 0x7F));
}

int8_t prev_x = 0;
int8_t prev_y = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial);
}

void loop() {
    if (Serial.available()) {
        char req = Serial.read();
        if (req == REQ_BYTE) {
            uint16_t buttons = get_buttons();
            int8_t x = N64_ctrl.axis_x();
            int8_t y = N64_ctrl.axis_y();
            int8_t enc_x = encode_stick(x, prev_x);
            int8_t enc_y = encode_stick(y, prev_y);
            prev_x = enc_x;
            prev_y = enc_y;

            Serial.write((uint8_t)(buttons >> 8));
            Serial.write((uint8_t)(buttons & 0xFF));
            Serial.write(enc_x);
            Serial.write(enc_y);
        }
    }
}