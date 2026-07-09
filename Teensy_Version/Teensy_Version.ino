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

//hard coded thresholds that will need to be validated with real data and specific to the direction of swipe
const int RIGHT_SWIPE_MIN_DX = 430; // min x delta for right swipe
const int RIGHT_SWIPE_MAX_ADY = 350;   //max abs y delta allowed for a right swipe */
const int LEFT_SWIPE_MAX_DX  = -430; // max x delta for a left swipe
const int LEFT_SWIPE_MAX_ADY  = 350;   // max abs y delta allowed for a left swipe
const int DOWN_SWIPE_MIN_DY  = 350; // min y delta for a down swipe
const int DOWN_SWIPE_MAX_ADX  = 400;   // max abs x delta allowed for a down swipe
const int UP_SWIPE_MAX_DY    = -350; // max y delta for an up swipe
const int UP_SWIPE_MAX_ADX    = 400;   // max abs x delta allowed for an up swipe

const uint32_t MIN_GESTURE_MS = 50;   // tune this min
const uint32_t MAX_GESTURE_MS = 500;  // tune this max

bool init_pad_channel(uint8_t channel);
void process_ptp_report(uint8_t i2c_channel, HID_report_t* report);
uint8_t classify_swipe_direction(uint8_t i2c_channel, int32_t dx, int32_t dy, uint32_t gesture_duration);

uint16_t compute_gesture_ID(uint8_t gestureValue, uint8_t i2c_channel);
void send_computer_command(uint8_t gestureValue, uint8_t i2c_channel);


void setup()
{
  Serial.begin(115200);
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

/*
    Wait for the Data Ready line to assert. When it does, read the data (which clears DR) and analyze the data.
    The rest is just a user interface to change various settings.
    */
void loop()
{
  /* Handle incoming messages from modules */
  #if USE_DR_I2C
  uint8_t dr_status = API_C3_DR_Asserted_ViaI2C();
  #else
  uint8_t dr_status = API_C3_DR_Asserted();
  #endif

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
    Serial.println("EEPROM set to disable channel ");
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

   uint8_t gestureValue = classify_swipe_direction(i2c_channel, dx, dy, gesture_duration);
    send_computer_command(gestureValue, i2c_channel);
    gestureKeyInfo[i2c_channel].active = false;
  }
}

uint8_t classify_swipe_direction(uint8_t i2c_channel, int32_t dx, int32_t dy, uint32_t gesture_duration) //determine direction of swipe based on defined thresholds
{
  //time filters
  if (gesture_duration < MIN_GESTURE_MS || gesture_duration > MAX_GESTURE_MS) {
    Serial.print("GESTURE,");
    Serial.print(i2c_channel);
    Serial.print(",NONE");
    Serial.println(gesture_duration);
    return GESTURE_NONE;
  }
  
  int32_t abs_dx = abs(dx);
  int32_t abs_dy = abs(dy);

  if (dx >= RIGHT_SWIPE_MIN_DX && abs_dy <= RIGHT_SWIPE_MAX_ADY) 
  {
    Serial.print("GESTURE,");
    Serial.print(i2c_channel);
    Serial.println(",RIGHT");
    
    return GESTURE_RIGHT;
  }

  else if (dx <= LEFT_SWIPE_MAX_DX && abs_dy <= LEFT_SWIPE_MAX_ADY) {
    Serial.print("GESTURE,");
    Serial.print(i2c_channel);
    Serial.println(",LEFT");
            
    return GESTURE_LEFT;
  }

  else if (dy >= DOWN_SWIPE_MIN_DY && abs_dx <= DOWN_SWIPE_MAX_ADX) {
    Serial.print("GESTURE,");
    Serial.print(i2c_channel);
    Serial.println(",DOWN");
    
    return GESTURE_DOWN;
  }

  else if (dy <= UP_SWIPE_MAX_DY && abs_dx <= UP_SWIPE_MAX_ADX){
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




uint16_t compute_gesture_ID(uint8_t gestureValue, uint8_t i2c_channel) {
  uint8_t gesture_values[num_touchpads]= {0,0,0,0};
  gesture_values[i2c_channel] = gestureValue;

  uint16_t gestureID = 0;
  uint16_t place=1;

  for (uint8_t pad=0; pad< num_touchpads; pad++) {
    
    uint16_t term= (uint16_t)gesture_values[pad] * place;

    Serial.print("pad # ");
    Serial.print(pad);
    Serial.print(": value= ");
    Serial.print(gesture_values[pad]);
    Serial.print("place");
    Serial.print(place);
    Serial.print("term");
    Serial.println(term);

    gestureID += term;
    place*=6;
  }
  
  Serial.print("gestureID: ");
  Serial.println(gestureID);

  return gestureID;

  
}


/*void send_computer_command(uint8_t gestureValue, uint8_t i2c_channel) //update with new method
{
  if (gestureValue == GESTURE_NONE){
    return;
  }

  if (i2c_channel == 0) //commands are specific to each channel/key --> this logic will have to be changed for multikey gestures to
  {
    if (gestureValue == GESTURE_RIGHT)
    {
      Keyboard.press(KEY_RIGHT);
      Keyboard.releaseAll();
    }
    else if (gestureValue == GESTURE_LEFT)
    {
      Keyboard.press(KEY_LEFT);
      Keyboard.releaseAll();
    }
    else if (gestureValue == GESTURE_UP)
    {
      Keyboard.press(KEY_UP);
      Keyboard.releaseAll();
    }
    else if (gestureValue == GESTURE_DOWN)
    {
      Keyboard.press(KEY_DOWN);
      Keyboard.releaseAll();
    }
  }
  else if (i2c_channel == 1)
  {
    if (gestureValue == GESTURE_RIGHT)
    {
      Keyboard.press(KEY_RIGHT);
      delay(20);
      Keyboard.releaseAll();
    }
    else if (gestureValue == GESTURE_LEFT)
    {
      Keyboard.press(KEY_LEFT);
      delay(20);
      Keyboard.releaseAll();
    }
    else if (gestureValue == GESTURE_UP)
    {
      Keyboard.press(KEY_UP);
      delay(20);
      Keyboard.releaseAll();
    }
    else if (gestureValue == GESTURE_DOWN)
    {
      Keyboard.press(KEY_DOWN);
      delay(20);
      Keyboard.releaseAll();
    }
  }
}*/
//TEEEEEST-- old command for dual pads

void send_computer_command(uint8_t gestureValue, uint8_t i2c_channel) //update with new method
{
  if (gestureValue == GESTURE_NONE){
    return;
  }

  uint16_t gestureID = compute_gesture_ID(gestureValue,i2c_channel);

  if (!does_macro_exist(gestureID)) {

    Serial.print("No macro stored");
    Serial.println(gestureID);
   
  }

  run_macro(gestureID);
  
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

void printHidDescriptor(HIDDescriptor_t * hd)
{
  Serial.print(F("Hid Descriptor Length: \t"));
  Serial.println(hd->wHIDDescLength);
  Serial.print(F("bcd Version\t\t0x"));
  Serial.println(hd->bcdVersion, HEX);
  Serial.print(F("Report Desc Length\t"));
  Serial.println(hd->wReportDescLength);
  Serial.print(F("Report Desc Register\t"));
  Serial.println(hd->wReportDescRegister);
  Serial.print(F("Input Register\t\t"));
  Serial.println(hd->wInputRegister);
  Serial.print(F("Max Input Length\t"));
  Serial.println(hd->wMaxInputLength);
  Serial.print(F("Output Register\t\t"));
  Serial.println(hd->wOutputRegister);
  Serial.print(F("Max Output Length\t"));
  Serial.println(hd->wMaxOutputLength);
  Serial.print(F("Command Register\t"));
  Serial.println(hd->wCommandRegister);
  Serial.print(F("Data Register\t\t"));
  Serial.println(hd->wDataRegister);
  Serial.print(F("VID\t\t0x"));
  Serial.println(hd->wVendorID, HEX);
  Serial.print(F("PID\t\t0x"));
  Serial.println(hd->wProductID, HEX);
  Serial.print(F("Version\t\t0x"));
  Serial.println(hd->wVersionID, HEX);
}




void printCompMatrix(uint8_t i2c_channel)
{
  // This shows how to read the "compensation matrix" from the touchpad
  uint8_t sizeX, sizeY;
  uint16_t compNumberBytes;
  // allocate the largest possible comp image, once you know the 
  // comp size this could be made a smaller size (or dynamically allocated if your
  // language supports it)
  int16_t compImage[30 * 16]; 
  uint16_t index;
  // all of the sensorSize information will be fixed/consistent for a given project
  // once you know the sizeX, sizeY, and compNumberBytes you could just hard-code
  // all this and not bother to call this function
  API_C3_sensorSize(i2c_channel, &sizeX, &sizeY, &compNumberBytes);
  Serial.print(F("I2C Channel = "));
  Serial.print(i2c_channel);
  Serial.print(", ");
  Serial.print(F("Sensor X,Y = "));
  Serial.print(sizeX);
  Serial.print(",");
  Serial.print(sizeY);
  Serial.print(" bytes = ");
  Serial.println(compNumberBytes);
  // The compImage is a 1D array (row major ordered) that can be easily broken out into a 2D array
  API_C3_readComp(i2c_channel, compImage, compNumberBytes, 64);
  index = 0;
  while (index < (compNumberBytes / 2))
  {
    for (int x = 0; x < sizeX; x++)
    {
      Serial.print(compImage[index++]);
      Serial.print(",");
    }
    Serial.println();
  }
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