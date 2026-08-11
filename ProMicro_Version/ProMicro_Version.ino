#include <Arduino.h>
#include <Keyboard.h>
#include <EEPROM.h>


#include "API_C3.h"         
#include "API_Hardware.h"
#include "API_HostBus.h"
#include "HID_Reports.h"
#include "I2C.h"
#include "GestureMacros.h"


bool printReportData = false;  // toggle for printing out report data each run


uint32_t measure = 0;

//======================Normal Keypress Globals===========================================

const uint8_t num_outputs = 27;   // outputs - shift register
const uint8_t num_inputs = 3;    // inputs- mcu pins

const uint8_t inputPins[num_inputs] = {
20, 23, 21
};

//shift register control pins
const uint8_t matrix_ser   = 7;  
const uint8_t matrix_srclk = 8;  // confirm these each mcu!!! i did
const uint8_t matrix_rclk  = 9;

const uint8_t key_pressed = HIGH; //

const uint16_t DEBOUNCE = 5; //ms

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
uint32_t key_last[num_outputs][num_inputs] = {};



//========================Key Matrix Functions ==================================
void init_key_matrix();
bool scan_key_matrix();

//=======================Gesture Key Globals =======================================


typedef struct //Struct to track whether a gesture is active and where it started
{
    bool active;
    uint16_t startX; 
    uint16_t startY;
    uint32_t startMillis; //Arduinio millis time when gesture starts
} GestureInfo; 

const uint8_t num_touchpads = 4; //number of connected touchpads/gesture keys

GestureInfo gestureKeyInfo[num_touchpads] = {};


uint8_t gesture_values[num_touchpads] = {0,0,0,0};


bool pad_connected[num_touchpads] = {};   // true for pads that initialized


bool multikeyGesture = false; //this defines the start of a new wait window for checking for multigestures
uint32_t multikeyGesture_startMillis = 0;
const uint32_t MKG_WAIT = 600; //Multikey gesture wait window (ms) <===========TUNE THIS



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
const int DX_DEADZONE = 400; //none region --> abs dx less than this value is ignored as no gesture and sign determines direction  <====TUNABLE
const int DY_DEADZONE = 350; // none region --> abs dy less than this value is ignored as no gesture and sign determines direction <==== TUNABLE

const uint32_t MIN_GESTURE_DURATION = 5;   // tune this min
const uint32_t MAX_GESTURE_DURATION = 700;  // tune this max



//============================Gesture Key Functions =======================================

bool init_pad_channel(uint8_t channel);
void process_ptp_report(uint8_t i2c_channel, HID_report_t* report);
void printPtpReport(uint8_t i2c_channel, HID_report_t * report);
uint8_t classify_swipe_direction(uint8_t i2c_channel, int32_t dx, int32_t dy, uint32_t gesture_duration);
uint8_t count_active_pads();
uint16_t compute_gesture_ID();
void finalize_gesture();
void send_computer_command(uint8_t gestureValue, uint8_t i2c_channel);

//============================Serial Control Functions=======================================
void check_serial();


// =====================Set Up=========================

void setup()
{
  Serial.ignoreFlowControl();
  Serial.begin(115200);
  //while (!Serial)
  delay(200);
  EEPROM.begin(4096);
  //delay(3000);
  //Serial.println("serial + eeprom up");

  //temp_flash_EEPROM(); //use when assigning EEPROM values locally instead of from the PC script


  API_Hardware_init();
  //Serial.println("hardware init done");

  delay(2);
  API_C3_init(PROJECT_I2C_FREQUENCY, ALPS_I2C_ADDR, PROJECT_I2C_FREQUENCY, ALPS_I2C_ADDR);
  //Serial.println("I2C init done");
  //EEPROM.begin() //figure out storage size??
  initialize_from_EEPROM();
  /*for (int i = 0; i < 120; i++){
    Serial.print("Index: ");
    Serial.print(i);
    Serial.print(" ID: ");
    Serial.print(macro_location[i][0]);
    Serial.print(" pos: ");
    Serial.print(macro_location[i][1]);
    Serial.println(" ");
  }
  */
  //Serial.println("eeprom init done");
  Keyboard.begin();
  init_key_matrix();
  //Serial.println("matrix done");

  
  //initailize all gesture key channels
  for (uint8_t i = 0; i < num_touchpads; i++) {
    bool success = init_pad_channel(i);
    pad_connected[i] = success;
    if (!success) { //check if initialization worked
      //Serial.print("channel: ");
      //Serial.print(i);
      //Serial.println(" failed initialization");
    }
    else { //print system info for eahc channel
    //Serial.println (i);
    //Serial.println ("  : gesture key initialization = success");
    //systemInfo_t sysInfo;
    //API_C3_readSystemInfo(i, &sysInfo);
    //printSystemInfo(i, &sysInfo);
    }
  }


 
}

// ==========================    MAIN LOOP    ===================================


void loop()
{

  check_serial(); //check for 1 character serial commands. 

  if(scan_key_matrix()){

    //reset all gestures if a non mod key is pressed
    for (uint8_t i = 0; i < num_touchpads; i++) {
      gestureKeyInfo[i].active = false;
      gestureKeyInfo[i].startMillis = 0;
      gesture_values[i] = GESTURE_NONE;
    }
    multikeyGesture = false; //reset mkg window
    
    return;
  }
  
  uint8_t dr_status = API_C3_DR_Asserted();

  if(pad_connected[0] && (dr_status & DR0_MASK))          // When Data is ready
  {
    HID_report_t report;
    API_C3_getReport(0, &report);    // read the report
    process_ptp_report(0, &report); //TEEEEEST
  }
  
  if(pad_connected[1] && (dr_status & DR1_MASK))    // When Data is ready
  {
    HID_report_t report;
    API_C3_getReport(1, &report);    // read the report
    process_ptp_report(1, &report);
  }

  if(pad_connected[2] && (dr_status & DR2_MASK))    // When Data is ready
  {
    HID_report_t report;
    API_C3_getReport(2, &report);    // read the report
    process_ptp_report(2, &report);
  }

  if(pad_connected[3] && (dr_status & DR3_MASK))    // When Data is ready
  {
    HID_report_t report;
    API_C3_getReport(3, &report);    // read the report
    process_ptp_report(3, &report);
  }



  if ( multikeyGesture && (millis() - multikeyGesture_startMillis >= MKG_WAIT)) {
    //Serial.println("multikey gesture timed out, forced gesture classification");
    finalize_gesture();
  }


}

// ================================== Gesture Key Functions =======================================================



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
    uint8_t rb = API_C3_readRegister(channel, REG_FEED_CONFIG4);
    Serial.print("setPtpMode FAIL ch ");
    Serial.print(channel);
    Serial.print(" readback 0x");
    Serial.println(rb, HEX);
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
  
  if (printReportData)//keep on if coordinates should print to serial overtime --> needed for gesture visualization
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

    //Serial.print("Channel ");
    //Serial.print(i2c_channel);
    //Serial.print(" Gesture started at x = ");
    //Serial.print(currentX);
    //Serial.print(" y = ");
    //Serial.println(currentY);

  }
  else if (gestureKeyInfo[i2c_channel].active && tip == 0)
  {
    int32_t dx = (int32_t)currentX - (int32_t)gestureKeyInfo[i2c_channel].startX;
    int32_t dy = (int32_t)currentY - (int32_t)gestureKeyInfo[i2c_channel].startY;

    uint32_t gesture_duration = millis() - gestureKeyInfo[i2c_channel].startMillis;

    //Serial.print("Channel ");
    //Serial.print(i2c_channel);
    //Serial.print(" Gesture ended at x= ");
    //Serial.print(currentX);
    //Serial.print(" y = ");
    //Serial.print(currentY);
    //Serial.print(" dx=");
    //Serial.print(dx);
    //Serial.print(" dy=");
    //Serial.println(dy);
    
    gestureKeyInfo[i2c_channel].active = false;

    uint8_t gestureValue = classify_swipe_direction(i2c_channel, dx, dy, gesture_duration);
    measure = micros();
    
    send_computer_command(gestureValue, i2c_channel);
  }
}

uint8_t classify_swipe_direction(uint8_t i2c_channel, int32_t dx, int32_t dy, uint32_t gesture_duration) //determine direction of swipe based on defined thresholds
{
  //time filters
  if (gesture_duration < MIN_GESTURE_DURATION || gesture_duration > MAX_GESTURE_DURATION) {
    //Serial.print("GESTURE,");
    //Serial.print(i2c_channel);
    //Serial.println(",NONE - duration");
    //Serial.print("Gesture Duration (ms): ");
    //Serial.println(gesture_duration);
    return GESTURE_NONE;
  }
  
  int32_t abs_dx = abs(dx);
  int32_t abs_dy = abs(dy);

  if (abs_dx <= DX_DEADZONE && abs_dy <= DY_DEADZONE){
    //Serial.print("GESTURE,");
    //Serial.print(i2c_channel);
    //Serial.println(",NONE");
    return GESTURE_NONE;
  }

  if (dx > DX_DEADZONE && abs(dx) >= abs(dy)) {
    //Serial.print("GESTURE,");
    //Serial.print(i2c_channel);
    //Serial.println(",RIGHT");
    
    return GESTURE_RIGHT;
  }

  else if (dx < -DX_DEADZONE && abs(dx) >= abs(dy)) {
    //Serial.print("GESTURE,");
    //Serial.print(i2c_channel);
    //Serial.println(",LEFT");
            
    return GESTURE_LEFT;
  }

  else if (dy > DY_DEADZONE && abs(dy) > abs(dx)) {
    //Serial.print("GESTURE,");
    //Serial.print(i2c_channel);
    //Serial.println(",DOWN");
    
    return GESTURE_DOWN;
  }

  else if (dy < -DY_DEADZONE && abs(dy) > abs(dx)) {
    //Serial.print("GESTURE,");
    //Serial.print(i2c_channel);
    //Serial.println(",UP");
    
    return GESTURE_UP;
  }
  else {
    //Serial.print("GESTURE,");
    //Serial.print(i2c_channel);
    //Serial.println(",NONE --else");
    
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

    //Serial.print("pad # ");
    //Serial.print(pad);
    //Serial.print(", value= ");
    //Serial.println(gesture_values[pad]);
    //Serial.print("placevalue: ");
    //Serial.print(placevalue);
    //Serial.print("term: ");
    //Serial.println(term);

    gestureID += term;
    placevalue*=6;
  }
  
  //Serial.print("gestureID: ");
  //Serial.println(gestureID);

  return gestureID;

  
}



void finalize_gesture(){

  uint16_t gestureID = compute_gesture_ID();
  //Serial.print("final gestureID: ");
  //Serial.println(gestureID);
 
  run_macro(gestureID);
  
  for (uint8_t i=0; i<num_touchpads; i++){
    gesture_values[i] = GESTURE_NONE;
    }

  multikeyGesture = false;

}



void send_computer_command(uint8_t gestureValue, uint8_t i2c_channel) //
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

  //Serial.print("Channel ");
  //Serial.print(i2c_channel);
  //Serial.print(" lifted. Pads still active: ");
  //Serial.println(stillActive);

  if (stillActive == 0) {
    finalize_gesture();
    uint32_t x = micros() - measure;
    Serial.println(x);
    //Serial.print("classify-> command single us: ");
  }
 
}




//=================NORMAL KEYPRESS FUNCTIONS========================

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


bool scan_key_matrix() 
{
  bool anyKeyPressed = false; 

  //bool nonModKeyPressed = false; 
  uint32_t currentMillis = millis();

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
      bool pressed = (digitalRead(inputPins[rowNum]) == key_pressed);

      if (pressed != key_state[colNum][rowNum] && (currentMillis - key_last[colNum][rowNum] >= DEBOUNCE)) {   // register if new press only (pressed now and not pressed before)
        key_state[colNum][rowNum] = pressed;
        key_last[colNum][rowNum]  = currentMillis;
        
        uint16_t code = keyMap[colNum][rowNum]; //lookup code from matrix
        //Serial.print("key press detected  col=");
        //Serial.print(colNum);
        //Serial.print(" row=");
        //Serial.print(rowNum);
        //Serial.print(" code=0x");
        //Serial.println(code, HEX);
        if (code != 0) {
          if(pressed) Keyboard.press(code);
          else Keyboard.release(code);
        }
      }

      if (key_state[colNum][rowNum]) anyKeyPressed = true;

      //ONLY want to reset gesture info in main loop for non mod keys --> if mod key then keep gestures processing while pressed
      //if (key_state[colNum][rowNum] && (keyMap[colNum][rowNum] < 0x80 || keyMap[colNum][rowNum] > 0x87)) { 
        //nonModKeyPressed = true;
      //} 
          
    }
    //walk the bit
    digitalWrite(matrix_rclk, LOW); 
    digitalWrite(matrix_ser, LOW);    
    digitalWrite(matrix_srclk, HIGH); //delay needed?
    digitalWrite(matrix_srclk, LOW);
    digitalWrite(matrix_rclk, HIGH);
  }
  
  return anyKeyPressed;
  //return nonModKeyPressed;
}







/**************************************************************/
/*
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

*/

uint8_t i2cPing(uint8_t channel, uint8_t i2cAddr)
{
  uint8_t error;

  //i2c_scanner uses the return value of
  //the Write.endTransmisstion to see if
  //a device did acknowledge being addressed.
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




//============================Check for serial commands=======================================

void check_serial(){
  
  if(Serial.available()){
    //Serial.print("AHHHH");
    int check = Serial.read();
    if(check == -1) return; //stop checking if nothing was read

    if(check == 'r') { // r -> begin remapping
      read_remapping();
      initialize_from_EEPROM();
      Serial.end();
      delay(500);
      Serial.begin(9600);
    }


    if(check == 'd'){ // d -> output the list of macro IDs and EEPROM locations
      for(int i = 0; i < 255; i++){
        if(macro_location[i][0] == 0) break;
        Serial.print("Macro : ");
        Serial.print(i);
        Serial.print(" , ID : ");
        Serial.print(macro_location[i][0]);
        Serial.print(" , Location : ");
        Serial.println(macro_location[i][1]);
      }
    }
  }
}