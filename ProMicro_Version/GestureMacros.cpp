#include "GestureMacros.h"


//global variables
bool using_trackpad[4] = {false,false,false,false};
bool allow_HID = false;
bool allow_serial_output = false;
uint16_t macro_location[255][2]; //{macro id, eeprom id}


//functions

void initialize_from_EEPROM(){

  int pos = 0; //to keep track of where in EEPROM has been read to
  uint8_t read_8 = 0;
  uint16_t read_16 = 0;

  EEPROM.get(pos,read_8);
  pos += sizeof(read_8);
  for (int i = 0; i < 4; i++){
    using_trackpad[i] = (read_8 & 0x01)? true:false;
    read_8 = read_8 >> 1;
  }
  allow_HID = (read_8 & 0x01)? true:false;
  read_8 = read_8 >> 1;
  allow_serial_output = (read_8 & 0x01)? true:false;
  read_8 = read_8 >> 0x01;

  bool end_loop = false;
  int i = 0;
  while(!end_loop && pos < EEPROM.length()-2){
      //Serial.println(pos);


      EEPROM.get(pos,read_16);

      uint16_t ID = read_16 & 0x07FF; //mask for only the macro ID
      uint16_t keys_in_macro = read_16 >> 11; //shift away the macro ID, leaving the macro length
      if(read_16 != 0){
        macro_location[i][0] = ID;
        macro_location[i][1] = pos;
        i++; //itterate to the next open array value

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
  //Serial.println(ID);

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

  //Serial.println(ID);
  
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
    //Serial.print("press: ");
    //Serial.println(action_list[i][0],HEX);
    for(int j = 0; j <= i; j++){
      if(action_list[j][1] == 1){
        Keyboard.release(action_list[i][0]);
        //Serial.print("lift: ");
        //Serial.println(action_list[i][0],HEX);
      }
      action_list[j][1] -= (action_list[j][1] > 0)? 1:0;
    }
  }
}


bool does_macro_exist(uint16_t ID){
  for(int i = 0; i<255; i++){
    if(macro_location[i][0] == ID) return true; //true if id is found
    if(macro_location[i][0] == 0) return false; //false if no more values written in the list
  }
  return false; //false if not found in a full list
}



int EEPROM_WRITE_GESTURE_START (uint16_t ID, uint16_t len, int pos){
  EEPROM.put(pos,ID + (len << 11));
  pos += sizeof(ID);
  //Serial.print(pos);
  return pos;
}

int EEPROM_WRITE_BYTE (uint8_t x, int pos){
  EEPROM.put(pos,x);
  pos += sizeof(x);
  //Serial.print(pos);
  return pos;
}


void temp_flash_EEPROM(){
  int pos = 0;
  pos = EEPROM_WRITE_BYTE(0x2F,pos);
  pos = EEPROM_WRITE_GESTURE_START( 1 , 1 , pos );	pos = EEPROM_WRITE_BYTE ( 0x71 , pos );		
  pos = EEPROM_WRITE_GESTURE_START( 2 , 1 , pos );	pos = EEPROM_WRITE_BYTE ( 0x77 , pos );		
  pos = EEPROM_WRITE_GESTURE_START( 3 , 1 , pos );	pos = EEPROM_WRITE_BYTE ( 0x65 , pos );		
  pos = EEPROM_WRITE_GESTURE_START( 4 , 1 , pos );	pos = EEPROM_WRITE_BYTE ( 0x72 , pos );		
  pos = EEPROM_WRITE_GESTURE_START( 6 , 1 , pos );	pos = EEPROM_WRITE_BYTE ( 0xDA , pos );		
  pos = EEPROM_WRITE_GESTURE_START( 12 , 1 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD7 , pos );		
  pos = EEPROM_WRITE_GESTURE_START( 18 , 1 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD9 , pos );		
  pos = EEPROM_WRITE_GESTURE_START( 24 , 1 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD8 , pos );		
  pos = EEPROM_WRITE_GESTURE_START( 36 , 1 , pos );	pos = EEPROM_WRITE_BYTE ( 0x74 , pos );		
  pos = EEPROM_WRITE_GESTURE_START( 72 , 1 , pos );	pos = EEPROM_WRITE_BYTE ( 0x79 , pos );		
  EEPROM.commit();
  pos = EEPROM_WRITE_GESTURE_START( 108 , 1 , pos );	pos = EEPROM_WRITE_BYTE ( 0x75 , pos );		
  pos = EEPROM_WRITE_GESTURE_START( 144 , 1 , pos );	pos = EEPROM_WRITE_BYTE ( 0x69 , pos );		
  pos = EEPROM_WRITE_GESTURE_START( 216 , 1 , pos );	pos = EEPROM_WRITE_BYTE ( 0x7A , pos );		
  pos = EEPROM_WRITE_GESTURE_START( 432 , 1 , pos );	pos = EEPROM_WRITE_BYTE ( 0x78 , pos );		
  pos = EEPROM_WRITE_GESTURE_START( 648 , 1 , pos );	pos = EEPROM_WRITE_BYTE ( 0x63 , pos );		
  pos = EEPROM_WRITE_GESTURE_START( 864 , 1 , pos );	pos = EEPROM_WRITE_BYTE ( 0x76 , pos );		
    EEPROM.commit();
  pos = EEPROM_WRITE_GESTURE_START( 7 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x71 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0xDA , pos );
  pos = EEPROM_WRITE_GESTURE_START( 13 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x71 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD7 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 19 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x71 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD9 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 25 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x71 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD8 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 37 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x71 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x74 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 73 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x71 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x79 , pos );
    EEPROM.commit();
  pos = EEPROM_WRITE_GESTURE_START( 109 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x71 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x75 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 145 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x71 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x69 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 217 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x71 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x7A , pos );
  pos = EEPROM_WRITE_GESTURE_START( 433 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x71 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x78 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 649 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x71 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x63 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 865 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x71 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x76 , pos );
    EEPROM.commit();
  pos = EEPROM_WRITE_GESTURE_START( 8 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x77 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0xDA , pos );
  pos = EEPROM_WRITE_GESTURE_START( 14 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x77 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD7 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 20 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x77 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD9 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 26 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x77 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD8 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 38 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x77 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x74 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 74 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x77 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x79 , pos );
    EEPROM.commit();
  pos = EEPROM_WRITE_GESTURE_START( 110 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x77 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x75 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 146 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x77 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x69 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 218 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x77 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x7A , pos );
  pos = EEPROM_WRITE_GESTURE_START( 434 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x77 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x78 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 650 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x77 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x63 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 866 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x77 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x76 , pos );
    EEPROM.commit();
  pos = EEPROM_WRITE_GESTURE_START( 9 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x65 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0xDA , pos );
  pos = EEPROM_WRITE_GESTURE_START( 15 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x65 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD7 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 21 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x65 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD9 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 27 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x65 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD8 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 39 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x65 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x74 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 75 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x65 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x79 , pos );
    EEPROM.commit();
  pos = EEPROM_WRITE_GESTURE_START( 111 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x65 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x75 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 147 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x65 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x69 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 219 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x65 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x7A , pos );
  pos = EEPROM_WRITE_GESTURE_START( 435 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x65 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x78 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 651 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x65 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x63 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 867 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x65 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x76 , pos );
    EEPROM.commit();
  pos = EEPROM_WRITE_GESTURE_START( 10 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x72 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0xDA , pos );
  pos = EEPROM_WRITE_GESTURE_START( 16 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x72 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD7 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 22 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x72 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD9 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 28 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x72 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD8 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 40 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x72 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x74 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 76 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x72 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x79 , pos );
    EEPROM.commit();
  pos = EEPROM_WRITE_GESTURE_START( 112 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x72 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x75 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 148 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x72 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x69 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 220 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x72 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x7A , pos );
  pos = EEPROM_WRITE_GESTURE_START( 436 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x72 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x78 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 652 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x72 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x63 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 868 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x72 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x76 , pos );
    EEPROM.commit();
  pos = EEPROM_WRITE_GESTURE_START( 42 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xDA , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x74 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 78 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xDA , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x79 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 114 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xDA , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x75 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 150 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xDA , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x69 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 222 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xDA , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x7A , pos );
  pos = EEPROM_WRITE_GESTURE_START( 438 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xDA , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x78 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 654 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xDA , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x63 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 870 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xDA , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x76 , pos );
    EEPROM.commit();
  pos = EEPROM_WRITE_GESTURE_START( 48 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD7 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x74 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 84 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD7 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x79 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 120 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD7 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x75 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 156 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD7 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x69 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 228 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD7 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x7A , pos );
  pos = EEPROM_WRITE_GESTURE_START( 444 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD7 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x78 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 660 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD7 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x63 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 876 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD7 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x76 , pos );
    EEPROM.commit();
  pos = EEPROM_WRITE_GESTURE_START( 54 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD9 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x74 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 90 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD9 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x79 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 126 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD9 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x75 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 162 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD9 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x69 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 234 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD9 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x7A , pos );
  pos = EEPROM_WRITE_GESTURE_START( 450 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD9 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x78 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 666 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD9 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x63 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 882 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD9 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x76 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 60 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD8 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x74 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 96 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD8 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x79 , pos );
    EEPROM.commit();
  pos = EEPROM_WRITE_GESTURE_START( 132 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD8 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x75 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 168 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD8 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x69 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 240 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD8 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x7A , pos );
  pos = EEPROM_WRITE_GESTURE_START( 456 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD8 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x78 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 672 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD8 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x63 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 888 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0xD8 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x76 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 252 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x74 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x7A , pos );
  pos = EEPROM_WRITE_GESTURE_START( 468 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x74 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x78 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 684 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x74 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x63 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 900 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x74 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x76 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 288 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x79 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x7A , pos );
  pos = EEPROM_WRITE_GESTURE_START( 504 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x79 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x78 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 720 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x79 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x63 , pos );
    EEPROM.commit();
  pos = EEPROM_WRITE_GESTURE_START( 936 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x79 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x76 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 324 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x75 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x7A , pos );
  pos = EEPROM_WRITE_GESTURE_START( 540 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x75 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x78 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 756 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x75 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x63 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 972 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x75 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x76 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 360 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x69 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x7A , pos );
  pos = EEPROM_WRITE_GESTURE_START( 576 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x69 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x78 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 792 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x69 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x63 , pos );
  pos = EEPROM_WRITE_GESTURE_START( 1008 , 2 , pos );	pos = EEPROM_WRITE_BYTE ( 0x69 , pos );	pos = EEPROM_WRITE_BYTE ( 0 , pos );	pos = EEPROM_WRITE_BYTE ( 0x76 , pos );
  pos = EEPROM_WRITE_BYTE(0,pos);
  EEPROM.commit();
}



