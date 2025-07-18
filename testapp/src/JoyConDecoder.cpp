#include "JoyConDecoder.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>

#include <cstdint>
#include <vector>
#include <algorithm> // for clamp

int16_t to_signed_16(uint8_t lsb, uint8_t msb) {
    return static_cast<int16_t>((msb << 8) | lsb);
}

// Button masks for Joy-Con Right
constexpr uint32_t BUTTON_A_MASK_RIGHT = 0x000800;
constexpr uint32_t BUTTON_B_MASK_RIGHT = 0x000200;
constexpr uint32_t BUTTON_X_MASK_RIGHT = 0x000400;
constexpr uint32_t BUTTON_Y_MASK_RIGHT = 0x000100;
constexpr uint32_t BUTTON_PLUS_MASK_RIGHT = 0x000002;
constexpr uint32_t BUTTON_R_MASK_RIGHT = 0x004000;
constexpr uint32_t BUTTON_STICK_MASK_RIGHT = 0x000004;

// Button masks for Joy-Con Left
constexpr uint32_t BUTTON_UP_MASK_LEFT = 0x000002;
constexpr uint32_t BUTTON_DOWN_MASK_LEFT = 0x000001;
constexpr uint32_t BUTTON_LEFT_MASK_LEFT = 0x000008;
constexpr uint32_t BUTTON_RIGHT_MASK_LEFT = 0x000004;
constexpr uint32_t BUTTON_MINUS_MASK_LEFT = 0x000100;
constexpr uint32_t BUTTON_L_MASK_LEFT = 0x000040;
constexpr uint32_t BUTTON_STICK_MASK_LEFT = 0x000800;

// Pro Controller button masks
constexpr uint64_t BUTTON_A_MASK = 0x000800000000;
constexpr uint64_t BUTTON_B_MASK = 0x000400000000;
constexpr uint64_t BUTTON_X_MASK = 0x000200000000;
constexpr uint64_t BUTTON_Y_MASK = 0x000100000000;
constexpr uint64_t BUTTON_R_SHOULDER = 0x004000000000;
constexpr uint64_t BUTTON_L_SHOULDER = 0x000000400000;
constexpr uint64_t BUTTON_DPAD_UP = 0x000000020000;
constexpr uint64_t BUTTON_DPAD_RIGHT = 0x000000040000;
constexpr uint64_t BUTTON_DPAD_DOWN = 0x000000010000;
constexpr uint64_t BUTTON_DPAD_LEFT = 0x000000080000;
constexpr uint64_t BUTTON_GUIDE = 0x000010000000;
constexpr uint64_t BUTTON_BACK = 0x000001000000;
constexpr uint64_t BUTTON_START = 0x000002000000;
constexpr uint64_t BUTTON_R_THUMB = 0x000004000000;
constexpr uint64_t BUTTON_L_THUMB = 0x000008000000;

StickData DecodeJoystick(const std::vector<uint8_t>& buffer, bool isLeft, bool upright) {
    if (buffer.size() < 16) {
        return { 0, 0 };
    }

    const uint8_t* data = isLeft ? &buffer[10] : &buffer[13];

    int x_raw = ((data[1] & 0x0F) << 8) | data[0];
    int y_raw = (data[2] << 4) | ((data[1] & 0xF0) >> 4);

    float x = (x_raw - 2048) / 2048.0f;
    float y = (y_raw - 2048) / 2048.0f;

    if (!upright) {
        float tx = x, ty = y;
        x = isLeft ? -ty : ty;
        y = isLeft ? tx : -tx;
    }

    const float deadzone = 0.08f;
    if (std::abs(x) < deadzone && std::abs(y) < deadzone) {
        return { 0, 0 };
    }

    x = std::clamp(x * 1.7f, -1.0f, 1.0f);
    y = std::clamp(y * 1.7f, -1.0f, 1.0f);

    int16_t outX = static_cast<int16_t>(x * 32767);
    int16_t outY = static_cast<int16_t>(-y * 32767);

    return { outX, outY };
}

MouseData DecodeMouseCoords(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < 0x18) return { 960, 471 };

    // Direct integer arithmetic - much faster than floating point
    int16_t raw_x = to_signed_16(buffer[0x10], buffer[0x11]);
    int16_t raw_y = to_signed_16(buffer[0x12], buffer[0x13]);

    // Convert to screen coordinates using integer math
    // Scale from [-32767, 32767] to [0, 1920] and [0, 943]
    uint16_t x = static_cast<uint16_t>((raw_x + 32767) * 1920 / 65534);
    uint16_t y = static_cast<uint16_t>((32767 - raw_y) * 943 / 65534);

    return { x, y };
}

MotionData DecodeMotion(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < 0x3C) return { 0, 0, 0, 0, 0, 0 };

    MotionData motion;
    motion.accelX = to_signed_16(buffer[0x30], buffer[0x31]);
    motion.accelY = to_signed_16(buffer[0x32], buffer[0x33]);
    motion.accelZ = to_signed_16(buffer[0x34], buffer[0x35]);
    
    motion.gyroX = to_signed_16(buffer[0x36], buffer[0x37]);
    motion.gyroY = to_signed_16(buffer[0x38], buffer[0x39]);
    motion.gyroZ = to_signed_16(buffer[0x3A], buffer[0x3B]);

    return motion;
}

uint32_t ExtractButtonState(const std::vector<uint8_t>& buffer, bool isLeft) {
    if (buffer.size() < 7) return 0;

    int btnOffset = isLeft ? 4 : 3;
    return (buffer[btnOffset] << 16) | (buffer[btnOffset + 1] << 8) | buffer[btnOffset + 2];
}

void DisplayJoyConData(const std::vector<uint8_t>& buffer, JoyConSide side, JoyConOrientation orientation) {
    if (buffer.size() < 0x3C) {
        std::cout << "Buffer too small for Joy-Con data\n";
        return;
    }

    bool isLeft = (side == JoyConSide::Left);
    bool upright = (orientation == JoyConOrientation::Upright);

    // Extract mouse coordinates
    MouseData mouse = DecodeMouseCoords(buffer);
    
    // Extract joystick data
    StickData stick = DecodeJoystick(buffer, isLeft, upright);
    
    // Extract motion data
    MotionData motion = DecodeMotion(buffer);
    
    // Extract button state
    uint32_t buttonState = ExtractButtonState(buffer, isLeft);

    // Display data
    std::cout << "\r"; // Carriage return to overwrite line
    std::cout << "Joy-Con " << (isLeft ? "Left" : "Right") << " | ";
    std::cout << "Mouse: (" << std::setw(4) << mouse.x << ", " << std::setw(4) << mouse.y << ") | ";
    std::cout << "Stick: (" << std::setw(6) << stick.x << ", " << std::setw(6) << stick.y << ") | ";
    std::cout << "Gyro: (" << std::setw(6) << motion.gyroX << ", " << std::setw(6) << motion.gyroY << ", " << std::setw(6) << motion.gyroZ << ") | ";
    std::cout << "Accel: (" << std::setw(6) << motion.accelX << ", " << std::setw(6) << motion.accelY << ", " << std::setw(6) << motion.accelZ << ") | ";
    std::cout << "Buttons: 0x" << std::hex << std::setw(6) << std::setfill('0') << buttonState << std::dec << std::setfill(' ');
    std::cout.flush();
}

void DisplayProControllerData(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < 0x3C) {
        std::cout << "Buffer too small for Pro Controller data\n";
        return;
    }

    // Extract state for Pro Controller
    uint64_t state = 0;
    for (int i = 3; i <= 8; ++i) {
        state = (state << 8) | buffer[i];
    }

    // Extract mouse coordinates
    MouseData mouse = DecodeMouseCoords(buffer);
    
    // Extract joystick data (Pro Controller specific)
    auto decode_pro_stick = [](const uint8_t* data) -> StickData {
        if (!data) return { 0, 0 };
        
        int x_raw = ((data[1] & 0x0F) << 8) | data[0];
        int y_raw = (data[2] << 4) | ((data[1] & 0xF0) >> 4);

    float x = (x_raw - 2048) / 2048.0f;
    float y = (y_raw - 2048) / 2048.0f;

    constexpr float deadzone = 0.08f;
    if (std::abs(x) < deadzone && std::abs(y) < deadzone) {
        return { 0, 0 };
    }

    x = std::clamp(x * 1.7f, -1.0f, 1.0f);
    y = std::clamp(y * 1.7f, -1.0f, 1.0f);

    int16_t outX = static_cast<int16_t>(x * 32767);
    int16_t outY = static_cast<int16_t>(y * 32767);

        return { outX, outY };
    };

    StickData leftStick = decode_pro_stick(&buffer[10]);
    StickData rightStick = decode_pro_stick(&buffer[13]);
    
    // Extract motion data
    MotionData motion = DecodeMotion(buffer);

    // Display data
    std::cout << "\r"; // Carriage return to overwrite line
    std::cout << "Pro Controller | ";
    std::cout << "Mouse: (" << std::setw(4) << mouse.x << ", " << std::setw(4) << mouse.y << ") | ";
    std::cout << "L-Stick: (" << std::setw(6) << leftStick.x << ", " << std::setw(6) << leftStick.y << ") | ";
    std::cout << "R-Stick: (" << std::setw(6) << rightStick.x << ", " << std::setw(6) << rightStick.y << ") | ";
    std::cout << "Gyro: (" << std::setw(6) << motion.gyroX << ", " << std::setw(6) << motion.gyroY << ", " << std::setw(6) << motion.gyroZ << ") | ";
    std::cout << "Buttons: 0x" << std::hex << std::setw(12) << std::setfill('0') << state << std::dec << std::setfill(' ');
    std::cout.flush();
}

// Mouse control implementations
void SetMousePosition(uint16_t x, uint16_t y) {
    SetCursorPos(x, y);
}

// Cache screen metrics for performance
static int screenWidth = -1, screenHeight = -1;

void SendMouseMove(uint16_t x, uint16_t y) {
    // Cache screen metrics on first call
    if (screenWidth == -1) {
        screenWidth = GetSystemMetrics(SM_CXSCREEN);
        screenHeight = GetSystemMetrics(SM_CYSCREEN);
    }
    
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    input.mi.dx = (x * 65535) / screenWidth;
    input.mi.dy = (y * 65535) / screenHeight;
    SendInput(1, &input, sizeof(INPUT));
}

void SendMouseClick(bool leftButton, bool down) {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = leftButton ? 
        (down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP) :
        (down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
    SendInput(1, &input, sizeof(INPUT));
}

void SendMouseScroll(int delta) {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = delta;
    SendInput(1, &input, sizeof(INPUT));
}

void ControlMouseFromJoyCon(const std::vector<uint8_t>& buffer, JoyConSide side, JoyConOrientation orientation) {
    if (buffer.size() < 0x3C) return;

    bool isLeft = (side == JoyConSide::Left);
    
    // Only process right Joy-Con for mouse control (IR camera)
    if (isLeft) return;

    // Extract mouse coordinates from IR camera data - direct access for speed
    if (buffer.size() < 0x18) return;
    
    int16_t raw_x = to_signed_16(buffer[0x10], buffer[0x11]);
    int16_t raw_y = to_signed_16(buffer[0x12], buffer[0x13]);
    
    // Fast integer conversion to screen coordinates
    static int screenWidth = -1, screenHeight = -1;
    if (screenWidth == -1) {
        screenWidth = GetSystemMetrics(SM_CXSCREEN);
        screenHeight = GetSystemMetrics(SM_CYSCREEN);
    }
    
    uint16_t x = static_cast<uint16_t>((raw_x + 32767) * screenWidth / 65534);
    uint16_t y = static_cast<uint16_t>((32767 - raw_y) * screenHeight / 65534);
    
    // Move mouse directly
    SetCursorPos(x, y);
    
    // Handle button presses for mouse clicks
    static bool aPressed = false, bPressed = false;
    
    // Extract button state directly
    uint32_t buttonState = (buffer[3] << 16) | (buffer[4] << 8) | buffer[5];
    
    bool aDown = (buttonState & BUTTON_A_MASK_RIGHT) != 0;
    bool bDown = (buttonState & BUTTON_B_MASK_RIGHT) != 0;
    
    // A button = left click
    if (aDown && !aPressed) {
        SendMouseClick(true, true);   // Left click down
    } else if (!aDown && aPressed) {
        SendMouseClick(true, false);  // Left click up
    }
    
    // B button = right click
    if (bDown && !bPressed) {
        SendMouseClick(false, true);  // Right click down
    } else if (!bDown && bPressed) {
        SendMouseClick(false, false); // Right click up
    }
    
    aPressed = aDown;
    bPressed = bDown;
}