#pragma once
#include <vector>
#include <utility>
#include <cstdint>
#include <Windows.h>

enum class JoyConSide { Left, Right };
enum class JoyConOrientation { Upright, Sideways };

struct MouseData {
    uint16_t x;
    uint16_t y;
};

struct StickData {
    int16_t x;
    int16_t y;
};

struct MotionData {
    int16_t gyroX, gyroY, gyroZ;
    int16_t accelX, accelY, accelZ;
};

// Simplified functions focused on data extraction
MouseData DecodeMouseCoords(const std::vector<uint8_t>& buffer);
StickData DecodeJoystick(const std::vector<uint8_t>& buffer, bool isLeft, bool upright);
MotionData DecodeMotion(const std::vector<uint8_t>& buffer);
uint32_t ExtractButtonState(const std::vector<uint8_t>& buffer, bool isLeft);

// Display functions
void DisplayJoyConData(const std::vector<uint8_t>& buffer, JoyConSide side, JoyConOrientation orientation);
void DisplayProControllerData(const std::vector<uint8_t>& buffer);

// Mouse control functions
void SetMousePosition(uint16_t x, uint16_t y);
void SendMouseMove(uint16_t x, uint16_t y);
void SendMouseClick(bool leftButton, bool down);
void SendMouseScroll(int delta);
void ControlMouseFromJoyCon(const std::vector<uint8_t>& buffer, JoyConSide side, JoyConOrientation orientation);
