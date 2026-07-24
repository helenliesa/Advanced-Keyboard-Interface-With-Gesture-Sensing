#ifndef GESTURE_MACROS_H
#define GESTURE_MACROS_H

#include <Arduino.h>
#include <EEPROM.h>
#include <Keyboard.h>

// global variables in the GestureMacros.cpp
extern bool using_trackpad[4];
extern bool allow_HID;
extern bool allow_serial_output;

//function signatures
void initialize_from_EEPROM();
void run_macro(uint16_t ID);
bool does_macro_exist(uint16_t ID);

#endif
