// This is the keypress code to test with 
#include <Arduino.h>
#include <Keyboard.h>



const uint8_t num_outputs = 27;   // outputs - shift register
const uint8_t num_inputs = 3;    // inputs- pins

const uint8_t inputPins[num_inputs] = {
20, 23, 21 
};


//shift register control pins
const uint8_t matrix_ser   = 7;  
const uint8_t matrix_srclk = 8;  // confirm these!!! i did
const uint8_t matrix_rclk  = 9;

const uint8_t key_pressed = HIGH; //active high

const uint16_t keyMap[num_outputs][num_inputs] = {
  {KEY_ESC,           '`',              KEY_TAB},
  {KEY_CAPS_LOCK,     KEY_LEFT_SHIFT,   KEY_LEFT_CTRL},
  {0x00,              '1',              'q'},
  {'a',               0x00,             KEY_LEFT_GUI},
  {KEY_F1,            '2',              'w'},
  {'s',               'z',              KEY_LEFT_ALT},
  {KEY_F2,            '3',              'e'},
  {'d',               'x',              0x00},
  {KEY_F3,            '4',              'r'},
  {'f',               'c',              0x00},
  {KEY_F4,            '5',              't'},
  {'g',               'v',              0x00},
  {KEY_F5,            '6',              'y'},
  {'h',               'b',              ' '},
  {KEY_F6,            '7',              'u'},
  {'j',               'n',              0x00},
  {KEY_F7,            '8',              'i'},
  {'k',               'm',              0x00},
  {KEY_F8,            '9',              'o'},
  {'l',               ',',              KEY_RIGHT_ALT},
  {KEY_F9,            '0',              'p'},
  {';',               '.',              0x00},
  {KEY_F10,           '-',              '['},
  {'\'',              '/',              KEY_MENU},
  {KEY_F11,           '=',              ']'},
  {KEY_RETURN,        KEY_RIGHT_SHIFT,  KEY_RIGHT_CTRL},
  {KEY_F12,           KEY_BACKSPACE,    '\\'}
};


// previous pressed/not-pressed state for each key --> only register new press
bool key_state[num_outputs][num_inputs] = {};

void init_key_matrix();
void scan_key_matrix();
void scan_key_matrix_1();


void setup()
{
  Serial.begin(9600);
  delay(2000);
  Keyboard.begin();
  delay(2000);

  init_key_matrix(); 
}

void loop()
{
  unsigned long t0 = micros();
  scan_key_matrix_1();
  unsigned long dt = micros() - t0;

  static unsigned long lastReport = 0;
  if (millis() - lastReport >= 500) {   // print at most twice a second
    lastReport = millis();
    Serial.print("scan time (us): ");
    Serial.println(dt);
  }
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


void scan_key_matrix_1()
{

  digitalWrite(matrix_rclk, LOW);
  for (int8_t bitNum = 31; bitNum >= 0; bitNum--) {
    
    //for first 31 shifts low, last one high (bitNum==0)
    if (bitNum == 0) {                            
      digitalWrite(matrix_ser, HIGH);    
      } else {
      digitalWrite(matrix_ser, LOW); 
    }
    digitalWrite(matrix_srclk, HIGH);
    digitalWrite(matrix_srclk, LOW);
  }
  digitalWrite(matrix_rclk, HIGH);
 
  //read each column and move bit up
  for (uint8_t colNum = 0; colNum < num_outputs; colNum++) { 
  
    delayMicroseconds(10); //adjust with testing

    for (uint8_t rowNum = 0; rowNum < num_inputs; rowNum++) {
      uint8_t rowPin = inputPins[rowNum];
      bool pressed = (digitalRead(rowPin) == key_pressed);

      if (pressed && !key_state[colNum][rowNum]) {   // register if new press only (pressed now and not pressed before)
        uint16_t code = keyMap[colNum][rowNum]; //lookup code from matrix
        //Serial.print("key press detected  col=");
        //Serial.print(colNum);
        //Serial.print(" row=");
        //Serial.print(rowNum);
        //Serial.print(" code=0x");
        //Serial.println(code, HEX);
        if (code != 0) {
          Keyboard.press(code);
        }
      }
      else if (!pressed && key_state[colNum][rowNum]) {   // just released key
        uint16_t code = keyMap[colNum][rowNum];
        if (code != 0) {
          Keyboard.release(code);
        }
      }

      key_state[colNum][rowNum] = pressed;   // remember key state for next scan run
    }
    //walk the bit
    digitalWrite(matrix_rclk, LOW); 
    digitalWrite(matrix_ser, LOW);    
    digitalWrite(matrix_srclk, HIGH); //delay needed?
    digitalWrite(matrix_srclk, LOW);
    digitalWrite(matrix_rclk, HIGH);
  }

}

