// This is the keypress code to test with 
#include <Arduino.h>
#include <Keyboard.h>



const uint8_t num_outputs = 27;   // outputs - shift register
const uint8_t num_inputs = 3;    // inputs- pins

const uint8_t inputPins[num_inputs] = {
14, 16, 10 
};


//shift register control pins
const uint8_t matrix_ser   = 7;  
const uint8_t matrix_srclk = 8;  // confirm these!!! i did
const uint8_t matrix_rclk  = 9;

const uint16_t time_delay = 70;

const uint8_t key_pressed = HIGH; //active high

const uint8_t keyMap[num_outputs][num_inputs] = {
  {0x1B,   0xC0,   0x09},
  {0x14,   0xA0,   0xA2},
  {0x00,   0x31,   0x51},
  {0x41,   0x00,   0x5B},  
  {0x70,   0x32,   0x57},  
  {0x53,   0x5A,   0xA4},  
  {0x71,   0x33,   0x45},
  {0x44,   0x58,   0x00},
  {0x72,   0x34,   0x52},
  {0x46,   0x43,   0x00},
  {0x73,   0x35,   0x54},
  {0x47,   0x56,   0x00},
  {0x74,   0x36,   0x59},
  {0x48,   0x42,   0x20},
  {0x75,   0x37,   0x55},
  {0x4A,   0x4E,   0x00},
  {0x76,   0x38,   0x49},
  {0x4B,   0x4D,   0x00},
  {0x77,   0x39,   0x4F},
  {0x4C,   0xBC,   0xA5},
  {0x78,   0x30,   0x50},
  {0xBA,   0xBE,   0x00},
  {0x79,   0xBD,   0xDB},
  {0xDE,   0xBF,   0x5D},
  {0x7A,   0xBB,   0xDD},
  {0x0D,   0xA1,   0xA3},
  {0x7B,   0x08,   0xDC} 
};

// previous pressed/not-pressed state for each key --> only register new press
bool key_state[num_outputs][num_inputs] = {};



void init_key_matrix();
void scan_key_matrix();
void scan_key_matrix_1();


void setup()
{
  Serial.begin(9600);
  while(!Serial);
  Keyboard.begin();

  init_key_matrix(); 
}

void loop()
{
  scan_key_matrix();
  //scan_key_matrix_1();
}

void init_key_matrix()
{
  for (uint8_t rowNum = 0; rowNum < num_inputs; rowNum++) {
    pinMode(inputPins[rowNum], INPUT); //
  }

  pinMode(matrix_ser, OUTPUT);
  pinMode(matrix_srclk, OUTPUT);
  pinMode(matrix_rclk, OUTPUT);

  digitalWrite(matrix_ser, LOW);
  digitalWrite(matrix_srclk, LOW);
  digitalWrite(matrix_rclk, LOW);

  Serial.println("key matrix initialized");
}

void scan_key_matrix() 
{
  for (uint8_t colNum = 0; colNum < num_outputs; colNum++) { 
    digitalWrite(matrix_rclk, LOW); 
    for (int8_t bitNum = 31; bitNum >= 0; bitNum--) {
       if (bitNum == colNum) {                           
        digitalWrite(matrix_ser, HIGH);    
        } else {
        digitalWrite(matrix_ser, LOW); 
        }
      //delayMicroseconds(time_delay);
      digitalWrite(matrix_srclk, HIGH); //possibly set delay after testing
      delayMicroseconds(time_delay);
      digitalWrite(matrix_srclk, LOW);
      //delayMicroseconds(time_delay);
    }
    digitalWrite(matrix_rclk, HIGH);

    delayMicroseconds(time_delay);

    for (uint8_t rowNum = 0; rowNum < num_inputs; rowNum++) {
      uint8_t rowPin = inputPins[rowNum];
      bool pressed = (digitalRead(rowPin) == key_pressed);

      if (pressed && !key_state[colNum][rowNum]) {   // register if new press only (pressed now and not pressed before)
        uint8_t code = keyMap[colNum][rowNum]; //lookup code from matrix
        Serial.print("key press detected  col=");
        Serial.print(colNum);
        Serial.print(" row=");
        Serial.print(rowNum);
        Serial.print(" code=0x");
        Serial.println(code, HEX);
        if (code != 0) {
          Keyboard.press(code);
        }
      }
      else if (!pressed && key_state[colNum][rowNum]) {   // just released key
        uint8_t code = keyMap[colNum][rowNum];
        if (code != 0) {
          Keyboard.release(code);
        }
      }

      key_state[colNum][rowNum] = pressed;   // remember key state for next scan run
    }
  }
}

void scan_key_matrix_1()
{

  digitalWrite(matrix_rclk, LOW);
  for (int8_t bitNum = 31; bitNum >= 0; bitNum--) {
    if (bitNum == 0) {                            
      digitalWrite(matrix_ser, HIGH);    
      } else {
      digitalWrite(matrix_ser, LOW); 
      }
    digitalWrite(matrix_srclk, HIGH);
    delayMicroseconds(time_delay); //possibly set delay after testing
    digitalWrite(matrix_srclk, LOW);
    //delayMicroseconds(1000);
  }
  digitalWrite(matrix_rclk, HIGH);
  //delayMicroseconds(1000);

  for (uint8_t colNum = 0; colNum < num_outputs; colNum++) { 
  
    delayMicroseconds(50); //can adjust with testing

    for (uint8_t rowNum = 0; rowNum < num_inputs; rowNum++) {
      uint8_t rowPin = inputPins[rowNum];
      bool pressed = (digitalRead(rowPin) == key_pressed);

      if (pressed && !key_state[colNum][rowNum]) {   // register if new press only (pressed now and not pressed before)
        uint8_t code = keyMap[colNum][rowNum]; //lookup code from matrix
        Serial.print("key press detected  col=");
        Serial.print(colNum);
        Serial.print(" row=");
        Serial.print(rowNum);
        Serial.print(" code=0x");
        Serial.println(code, HEX);
        if (code != 0) {
          Keyboard.press(code);
        }
      }
      else if (!pressed && key_state[colNum][rowNum]) {   // just released key
        uint8_t code = keyMap[colNum][rowNum];
        if (code != 0) {
          Keyboard.release(code);
        }
      }

      key_state[colNum][rowNum] = pressed;   // remember key state for next scan run
    }

    digitalWrite(matrix_rclk, LOW); 
    digitalWrite(matrix_ser, LOW);    
    digitalWrite(matrix_srclk, HIGH); //delay needed?
    delayMicroseconds(time_delay);
    digitalWrite(matrix_srclk, LOW);
    digitalWrite(matrix_rclk, HIGH);
  }

}

