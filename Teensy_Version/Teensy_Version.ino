#include <Arduino.h>
#include <Keyboard.h>
#include <EEPROM.h>


#include "API_C3.h"         
#include "API_Hardware.h"
#include "API_HostBus.h"
#include "HID_Reports.h"
#include "I2C.h"
#include "GestureMacros.h"

#define USE_DR_I2C 0 // Reads out if data is ready through I2C instead of the interrupt pin


bool dataPrint_mode_g = true;  /** < toggle for printing out data > */ //remove later

//======================Key Matrix Setup===========================================

const uint8_t num_outputs = 27;   // outputs - shift register
const uint8_t num_inputs = 3;    // inputs- pins

const uint8_t inputPins[num_inputs] = {
14, 16, 10 
};

//shift register control pins
const uint8_t matrix_ser   = 2;  
const uint8_t matrix_srclk = 3;  // confirm these with schematic
const uint8_t matrix_rclk  = 4;

const uint8_t key_pressed = LOW;

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




//Struct to track whether a gesture is active and where it started
typedef struct
{
    bool active;
    uint16_t startX; 
    uint16_t startY;
    uint32_t startMillis; //Arduinio millis time when gesture starts
} GestureInfo; 

const uint8_t num_touchpads = 4; //number of connected touchpads/gesture keys

GestureInfo gestureKeyInfo[num_touchpads] = {};


uint8_t gesture_values[num_touchpads] = {0,0,0,0};
bool multikeyGesture = false;
uint32_t multikeyGesture_startMillis = 0;
const uint32_t MKG_WAIT = 400; //Multikey gesture wait window (ms)



/*enum Direction {

    NONE,
    RIGHT,
    LEFT,
    DOWN,
    UP

}; no longer using this*/


const uint8_t GESTURE_NONE  = 0;
const uint8_t GESTURE_UP    = 1;
const uint8_t GESTURE_RIGHT = 2;
const uint8_t GESTURE_DOWN  = 3;
const uint8_t GESTURE_LEFT  = 4;
const uint8_t GESTURE_TAP   = 5;

/*hard coded thresholds that will need to be validated with real data and specific to the direction of swipe
const int RIGHT_SWIPE_MIN_DX = 430; // min x delta for right swipe
const int RIGHT_SWIPE_MAX_ADY = 350;   //max abs y delta allowed for a right swipe 
const int LEFT_SWIPE_MAX_DX  = -430; // max x delta for a left swipe
const int LEFT_SWIPE_MAX_ADY  = 350;   // max abs y delta allowed for a left swipe
const int DOWN_SWIPE_MIN_DY  = 350; // min y delta for a down swipe
const int DOWN_SWIPE_MAX_ADX  = 400;   // max abs x delta allowed for a down swipe
const int UP_SWIPE_MAX_DY    = -350; // max y delta for an up swipe
const int UP_SWIPE_MAX_ADX    = 400;   // max abs x delta allowed for an up swipe
*/
const int X_DEADZONE = 250; //none region
const int Y_DEADZONE = 200;

const uint32_t MIN_GESTURE_DURATION = 10;   // tune this min
const uint32_t MAX_GESTURE_DURATION = 500;  // tune this max

bool init_pad_channel(uint8_t channel);
void process_ptp_report(uint8_t i2c_channel, HID_report_t* report);
uint8_t classify_swipe_direction(uint8_t i2c_channel, int32_t dx, int32_t dy, uint32_t gesture_duration);

uint16_t compute_gesture_ID();
void send_computer_command(uint8_t gestureValue, uint8_t i2c_channel);


void setup()
{
  Serial.begin(9600);
  while(!Serial);
  
  API_Hardware_init();       //Initialize board hardware
  delay(2);                  //delay for power up
  
  // initialize i2c connection at 400kHz 
  API_C3_init(PROJECT_I2C_FREQUENCY, ALPS_I2C_ADDR, PROJECT_I2C_FREQUENCY, ALPS_I2C_ADDR); 

  Serial.println(F("I2C initialized"));

  initialize_from_EEPROM(); //load flagsa and macro locations
  Keyboard.begin();

  //initailize all gesture key channels
  for (uint8_t i = 0; i < num_touchpads; i++) {
    bool success = init_pad_channel(i);
    if (!success) { //check if initialization worked
      Serial.print("channel: ");
      Serial.print(i);
      Serial.println(" failed initialization");
    }
    else { //print system info for eahc channel
    systemInfo_t sysInfo;
    API_C3_readSystemInfo(i, &sysInfo);
    printSystemInfo(i, &sysInfo);
    }
  }
 
}


void loop()
{
  uint8_t dr_status = API_C3_DR_Asserted();

  if(dr_status & DR0_MASK)          // When Data is ready
  {
    HID_report_t report;
    API_C3_getReport(0, &report);    // read the report
    process_ptp_report(0, &report); //TEEEEEST
  }
  
  if(dr_status & DR1_MASK)          // When Data is ready
  {
    HID_report_t report;
    API_C3_getReport(1, &report);    // read the report
    process_ptp_report(1, &report);
  }

  if ( multikeyGesture && (millis() - multikeyGesture_startMillis >= MKG_WAIT)) {
    Serial.println("multikey gesture timed out, forced gesture classification");
    finalize_gesture();
  }


}


//Functions

void printPtpReport(uint8_t i2c_channel, HID_report_t * report) //print PTP report to serial
{
  Serial.print(report->ptp.tip);
  Serial.print(",");
  Serial.print(report->ptp.x);
  Serial.print(",");
  Serial.println(report->ptp.y);
}


bool init_pad_channel(uint8_t channel){

  uint8_t i2c_error = i2cPing(channel, ALPS_I2C_ADDR);

  Serial.print("channel ");
  Serial.print(channel);
  Serial.print(" ping response: 0x");
  Serial.println(i2c_error, HEX);

  if (i2c_error != 0){
    Serial.println("ping failed");
    return false;
  }
  Serial.println("ping OK");
  bool set_mode = API_C3_setPtpMode(channel);
  if (!set_mode) {
    Serial.println("failed to set to ptp mode");
    return false;
  }
  bool enable_tracking = API_C3_enableTracking(channel);
  if (!enable_tracking) {
    Serial.println("failed to enable tracking");
    return false;
  }
  bool enable_feed = API_C3_enableFeed(channel);
  if (!enable_feed) {
    Serial.println("failed to enable feed");
    return false;
  }
  /*bool enable_comp = API_C3_enableComp(channel);
  if (!enable_comp) {
    Serial.println("failed to enable compensation");
    return false;
  }*/ //this fails currently anyways

return true;

}

//process ptp report determines the coordinates that a gesture starts and ends at and prints out the deltas

void process_ptp_report(uint8_t i2c_channel, HID_report_t* report)
{
  /*if (!using_trackpad[i2c_channel]) {
    Serial.println("channel disabled in EEPROM channel ");
    Serial.print(i2c_channel);
  }*/
  if (report == NULL)
  {
    Serial.print("report is null on channel ");
    Serial.println(i2c_channel);
  }
  if (dataPrint_mode_g)//keep on if coordinates should print to serial overtime --> for gesture visualization
  {
    printPtpReport(i2c_channel, report);
  } 

  uint8_t tip = report->ptp.tip; //tip = 0 when nothing is touching the sensor, can be used to track the start and end of a gesture
  uint16_t currentX = report->ptp.x;
  uint16_t currentY = report->ptp.y;

  if (!gestureKeyInfo[i2c_channel].active && tip == 1)
  {
    gestureKeyInfo[i2c_channel].active = true;
    gestureKeyInfo[i2c_channel].startX = currentX;
    gestureKeyInfo[i2c_channel].startY = currentY;
    gestureKeyInfo[i2c_channel].startMillis = millis(); //start of gesture clock

    Serial.print("Channel ");
    Serial.print(i2c_channel);
    Serial.print(" Gesture started at x = ");
    Serial.print(currentX);
    Serial.print(" y = ");
    Serial.println(currentY);

  }
  else if (gestureKeyInfo[i2c_channel].active && tip == 0)
  {
    int32_t dx = (int32_t)currentX - (int32_t)gestureKeyInfo[i2c_channel].startX;
    int32_t dy = (int32_t)currentY - (int32_t)gestureKeyInfo[i2c_channel].startY;

    uint32_t gesture_duration = millis() - gestureKeyInfo[i2c_channel].startMillis;

    Serial.print("Channel ");
    Serial.print(i2c_channel);
    Serial.print(" Gesture ended at x= ");
    Serial.print(currentX);
    Serial.print(" y = ");
    Serial.print(currentY);
    Serial.print(" dx=");
    Serial.print(dx);
    Serial.print(" dy=");
    Serial.println(dy);
    
    gestureKeyInfo[i2c_channel].active = false;

   uint8_t gestureValue = classify_swipe_direction(i2c_channel, dx, dy, gesture_duration);
    send_computer_command(gestureValue, i2c_channel);
  }
}

uint8_t classify_swipe_direction(uint8_t i2c_channel, int32_t dx, int32_t dy, uint32_t gesture_duration) //determine direction of swipe based on defined thresholds
{
  //time filters
  if (gesture_duration < MIN_GESTURE_DURATION || gesture_duration > MAX_GESTURE_DURATION) {
    Serial.print("GESTURE,");
    Serial.print(i2c_channel);
    Serial.println(",NONE");
    Serial.print("Gesture Duration (ms): ");
    Serial.println(gesture_duration);
    return GESTURE_NONE;
  }
  
  int32_t abs_dx = abs(dx);
  int32_t abs_dy = abs(dy);

  if (abs_dx <= X_DEADZONE && abs_dy <= Y_DEADZONE){
    Serial.print("GESTURE,");
    Serial.print(i2c_channel);
    Serial.println(",NONE");
    return GESTURE_NONE;
  }

  if (dx > X_DEADZONE && abs(dx) >= abs(dy)) {
    Serial.print("GESTURE,");
    Serial.print(i2c_channel);
    Serial.println(",RIGHT");
    
    return GESTURE_RIGHT;
  }

  else if (dx < -X_DEADZONE && abs(dx) >= abs(dy)) {
    Serial.print("GESTURE,");
    Serial.print(i2c_channel);
    Serial.println(",LEFT");
            
    return GESTURE_LEFT;
  }

  else if (dy > Y_DEADZONE && abs(dy) > abs(dx)) {
    Serial.print("GESTURE,");
    Serial.print(i2c_channel);
    Serial.println(",DOWN");
    
    return GESTURE_DOWN;
  }

  else if (dy < -Y_DEADZONE && abs(dy) > abs(dx)) {
    Serial.print("GESTURE,");
    Serial.print(i2c_channel);
    Serial.println(",UP");
    
    return GESTURE_UP;
  }
  else {
    Serial.print("GESTURE,");
    Serial.print(i2c_channel);
    Serial.println(",NONE");
    
    return GESTURE_NONE;
  }
}


uint8_t count_active_pads()
{
  uint8_t activePads = 0;
  for (uint8_t i = 0; i < num_touchpads; i++) {
    if (gestureKeyInfo[i].active) {
      activePads++;
    }
  }
  return activePads;
}

uint16_t compute_gesture_ID() {

  uint16_t gestureID = 0;
  uint16_t placevalue=1;

  for (uint8_t pad=0; pad< num_touchpads; pad++) {
    
    uint16_t term= (uint16_t)gesture_values[pad] * placevalue;

    Serial.print("pad # ");
    Serial.print(pad);
    Serial.print(", value= ");
    Serial.print(gesture_values[pad]);
    Serial.print("placevalue: ");
    Serial.print(placevalue);
    Serial.print("term: ");
    Serial.println(term);

    gestureID += term;
    placevalue*=6;
  }
  
  Serial.print("gestureID: ");
  Serial.println(gestureID);

  return gestureID;

  
}

void finalize_gesture(){

  uint16_t gestureID = compute_gesture_ID();
  Serial.print("final gestureID: ");
  Serial.println(gestureID);
 
  if (!does_macro_exist(gestureID)) {

    Serial.println("No macro stored");
    Serial.print("gestureID in send_computer_command function: ");
    Serial.println(gestureID);
   
    }
  else{
  run_macro(gestureID);
    }
  
  for (uint8_t i=0; i<num_touchpads; i++){
    gesture_values[i] = GESTURE_NONE;
    }

  multikeyGesture = false;

}

void send_computer_command(uint8_t gestureValue, uint8_t i2c_channel) //update with new method
{
  if (gestureValue == GESTURE_NONE){
    return;
  }

  gesture_values[i2c_channel] = gestureValue;
if (!multikeyGesture) {
    multikeyGesture = true;
    multikeyGesture_startMillis = millis();
  }

  uint8_t stillActive = count_active_pads();

  Serial.print("Channel ");
  Serial.print(i2c_channel);
  Serial.print(" lifted. Pads still active: ");
  Serial.println(stillActive);

  if (stillActive == 0) {
    finalize_gesture();
  }
 
  
}



void scan_key_matrix() {
  for (uint8_t colNum = 0; colNum < num_outputs; colNum++) {
    uint32_t current_column = 0;
    current_column |= (1UL << colNum);

    digitalWrite(matrix_rclk, LOW);
    for (int8_t bitNum = 31; bitNum >= 0; bitNum--) {
      uint8_t bitVal = (current_column & (1UL << bitNum)) ? HIGH : LOW;
      digitalWrite(matrix_ser, bitVal);
      digitalWrite(matrix_srclk, HIGH);
      digitalWrite(matrix_srclk, LOW);
    }
    digitalWrite(matrix_rclk, HIGH);

    delayMicroseconds(30);

    for (uint8_t rowNum = 0; rowNum < num_inputs; rowNum++) {
      uint8_t rowPin = inputPins[rowNum];
      int level = digitalRead(rowPin);
      if (level == key_pressed) {
        uint8_t code = keyMap[rowNum][colNum];
        if (code != 0) {
          Keyboard.press(code);
          Keyboard.releaseAll();
        }
      }
    }
  }
}











/**************************************************************/

void printSystemInfo(uint8_t channel, systemInfo_t* sysInfo)
{
  Serial.print(F("Channel "));
  Serial.print(channel, DEC);
  Serial.println(F(" System Information"));
  Serial.print(F("Hardware ID:\t0x"));
  Serial.println(sysInfo->hardwareId, HEX);
  Serial.print(F("Firmware ID:\t0x"));
  Serial.println(sysInfo->firmwareId, HEX);
  Serial.print(F("Vendor ID:\t0x"));
  Serial.println(sysInfo->vendorId, HEX);
  Serial.print(F("Product ID:\t0x"));
  Serial.println(sysInfo->productId, HEX);
  Serial.print(F("Version ID:\t0x"));
  Serial.println(sysInfo->versionId, HEX);
  Serial.println(F(""));
}



uint8_t i2cPing(uint8_t channel, uint8_t i2cAddr)
{
  uint8_t error;

  // The i2c_scanner uses the return value of
  // the Write.endTransmisstion to see if
  // a device did acknowledge being addressed.
  I2C_beginTransmission(channel, i2cAddr);
  error = I2C_endTransmission(channel, true);

  if (error == 0)
  {
    Serial.print("I2C device responded at address 0x");
    if (i2cAddr<16) 
      Serial.print("0");
    Serial.print(i2cAddr,HEX);
    Serial.println("  !");
  }
  else 
  {
    Serial.print("I2C error occurred at address 0x");
    if (i2cAddr<16) 
      Serial.print("0");
    Serial.print(i2cAddr,HEX);
    Serial.print(": ");
    Serial.println(error);
  }    

  return error;
}