#include "GestureMacros.h"


//global variables
bool using_trackpad[4] = {false,false,false,false};
bool allow_HID = false;
bool allow_serial_output = false;
uint16_t macro_location[255][2]; //{macro id, eeprom id}


void initialize_from_EEPROM(){

  int length = EEPROM.length();
  int pos = 0; //to keep track of where in EEPROM has been read to
  uint8_t read_8 = 0;
  uint16_t read_16 = 0;

  EEPROM.get(pos,read_8);
  pos += sizeof(read_8);
  for (int i = 0; i < 4; i++){
    using_trackpad[i] = (read_8 & 0x01)? true:false;
    read_8 >> 0x01;
  }
  allow_HID = (read_8 & 0x01)? true:false;
  read_8 >> 0x01;
  allow_serial_output = (read_8 & 0x01)? true:false;
  read_8 >> 0x01;

  bool end_loop = false;
  int i = 0;
  while(end_loop && pos < EEPROM.length()-2){
      EEPROM.get(pos,read_16);

      uint16_t ID = read_16 & 0x07FF; //mask for only the macro ID
      uint16_t keys_in_macro = read_16 >> 11; //shift away the macro ID, leaving the macro length
      if(read_16 != 0){
        macro_location[i][0] = ID;
        macro_location[i][1] = pos;

        pos += sizeof(read_16);
        pos += sizeof(read_8) * (2*keys_in_macro-1);
      } else {
        end_loop = true;
        pos += sizeof(read_16);
      }
  }

}

void run_macro(uint16_t ID){

  //find macro
  int pos = 0;
  for(int i = 0; i<255; i++){
    if(macro_location[i][0] == ID) {
      pos = macro_location[i][1];
      break;
    } else if(macro_location[i][0] == 0){
      break;
    }
  }
  if(pos == 0) return; //break if no code found
  
  uint16_t length;
  EEPROM.get(pos,length);
  pos += sizeof(length); //sizeof should return 2, but could matter if types are changed in the future
  length = length >> 11;


  //prepare macro to run
  uint8_t action_list[length][2];  //key code, delay length (+1 to avoid unsigned int related issues)
  for(int i = 0; i<length; i++){
    EEPROM.get(pos,action_list[i][0]);
    pos += sizeof(action_list[i][0]); //sizeof should return 1, but could matter if types are changed in the future
    EEPROM.get(pos,action_list[i][1]);
    if(i < length-1) { //the final value doesn't have a stored delay
      pos += sizeof(action_list[i][1]); //sizeof should return 1, but could matter if types are changed in the future. 
      action_list[i][1] = (action_list[i][1] & 0x1F) + 1; //currently the top 3 bits are not used; however, this ensures they remain available if required in future. the +1 is to allow the method to create a fake -1 on an unsigned int
    } else {
      action_list[i][1] = 1;
    }
  }

  //run macro
  for(int i = 0; i<length; i++){
    Keyboard.press(action_list[i][0]);
    for(int j = 0; j <= i; j++){
      if(action_list[j][1] == 1){
        Keyboard.release(action_list[i][0]);
      }
      action_list[j][1] -= (action_list[j][1] > 0)? 1:0;
    }
  }

  //Keyboard.releaseALL(); //in case any commands were dropped
}

bool does_macro_exist(uint16_t ID){
  for(int i = 0; i<255; i++){
    if(macro_location[i][0] == ID) return true; //true if id is found
    if(macro_location[i][0] == 0) return false; //false if no more values written in the list
  }
  return false; //false if not found in a full list
}

