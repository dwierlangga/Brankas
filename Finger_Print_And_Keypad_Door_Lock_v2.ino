#include <Keypad.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define OK 1
#define NOK 0
#define NA 2

#define ON 0
#define OFF 1

#define LOCK 13
#define BUZZER 12

SoftwareSerial FingerSerial(2, 3);

uint8_t getFingerprintEnroll(uint8_t id);

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&FingerSerial);

const byte ROWS = 4;
const byte COLS = 4;

char hexaKeys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {11, 10, 9, 8};
byte colPins[COLS] = {7, 6, 5, 4}; 
Keypad customKeypad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); 

LiquidCrystal_I2C lcd(0x27, 16, 2);

char gszCorrectPassword[10] = {'1','2','3','4','5','6','0','0','0','0'};

typedef enum enKeyPad 
{
  NO_KEY_PRESS=-1,
  RIGHT_KEY=0, 
  UP_KEY, 
  DOWN_KEY,
  LEFT_KEY,
  SELECT_KEY
};

unsigned short gusIsPasswordCorrect = 0;
short gusModeSelection = 0;
short gusModeSelection_Bak = 1;
unsigned short gus_Finger_ID_Memory[12];
unsigned short gusFinger_ID_Memory_Bakup[12];

enum enKeyPad enGet_Key(void);
enum enKeyPad enGet_Key_Function(unsigned short *usStatus);
int gibuttonState = 0;

unsigned long gusStart_LCD_Timer = 0;
short gsLCD_Index = 1;

void setup(){
  Serial.begin(9600);
  lcd.begin();
  lcd.backlight();

  delay(10);

  finger.begin(57600);

  pinMode(LOCK, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
  vClose_Door();

  vCheckFingerPrintStatus();
  vEEPROM_Read_Finger_ID_Memory();
  vEEPROM_Read_Password();

  Serial.println(gszCorrectPassword);

  gusStart_LCD_Timer = millis();
}

void loop(){
  //MAIN LCD
  if(millis() - gusStart_LCD_Timer >=1000){
    if(gsLCD_Index == 0){
      gsLCD_Index = 1;
    }
    else if(gsLCD_Index == 1){
      gsLCD_Index =0;
    }

    vLCD_Display_Init(gsLCD_Index);
    gusStart_LCD_Timer = millis();
  }

  //CHECK FINGER PRINT
  vFingerPrint_MainRoutine();

  //CHECK CORRECTNESS OF PASSWORD
  gusIsPasswordCorrect = usCheck_Correct_Password();

  if(gusIsPasswordCorrect == OK){
    digitalWrite(BUZZER, HIGH);
    delay(200);
    digitalWrite(BUZZER, LOW);
    //ENTER MODE SELECTION
    vMode_Selection();
  }

  else if(gusIsPasswordCorrect == NOK){
    vLCD_Display_Wrong_Password();
    digitalWrite(BUZZER, HIGH);
    delay(200);
    digitalWrite(BUZZER, LOW);
    delay(200);
    digitalWrite(BUZZER, HIGH);
    delay(200);
    digitalWrite(BUZZER, LOW);
    delay(2000);
  }
}

//CHECK FINGER PRINT STATUS
void vCheckFingerPrintStatus(void){
  if(finger.verifyPassword()){
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Finger Print");
    lcd.setCursor(6,1);
    lcd.print("READY");
    delay(2000);
  }
  else{
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Finger Print");
    lcd.setCursor(4,1);
    lcd.print("NOT FOUND");
    while (1);
  }
}

//READ FINGER PRINT MEMORY
void vEEPROM_Read_Finger_ID_Memory(void){
  unsigned short usAdress = 0;

  for(short i=1; i<11 ; i++){
    usAdress = i;
    gus_Finger_ID_Memory[i] = EEPROM.read(usAdress);
  }
}

//READ PASSWORD FROM EEPROM
void vEEPROM_Read_Password(void){
  unsigned short usAdress = 0;
  unsigned short usIsPasswordSetBefore = 0;

  usIsPasswordSetBefore = EEPROM.read(usAdress);

  if(usIsPasswordSetBefore != 1){
    vEEPROM_Write_Password(); //default for first time : 123456
    usIsPasswordSetBefore = 1;
    EEPROM.write(usAdress, usIsPasswordSetBefore);
  }
  for(short i=11; i<=16; i++){
    usAdress = i;
    gszCorrectPassword[i-11] = EEPROM.read(usAdress);
  }
}

//WROTE PASSWORD TO EEPROM
void vEEPROM_Write_Password(void){
  unsigned short usAdress = 0;

  for(short i=11; i<=16; i++){
    usAdress = i;
    EEPROM.write(usAdress, gszCorrectPassword[i-11]);
  }
}

//LCD DISPLAY WRONG PASSWORD
void vLCD_Display_Init(short sIndex){
  lcd.clear();
  if(sIndex == 0){
    lcd.setCursor(1,0);
    lcd.print("Keyin Password");
    lcd.setCursor(2,1);
    lcd.print("End With #");
  }
  else if(sIndex == 1){
    lcd.setCursor(1,0);
    lcd.print("Scan Ur Finger");
    lcd.setCursor(2,1);
    lcd.print("Open Door");
  }
}

//FINGER PRINT MAIN ROUTINE
void vFingerPrint_MainRoutine(void){
  short usFingerID = 0;

  usFingerID = usIsFingerPrintCorrect();

  //Serial.println(usFingerID);

  //FINGER MATCH
  if(usFingerID > 0){
    char szTemp[20];

    memset(szTemp,'\0', sizeof(szTemp));
    //DISPLAY DOOR ACCESS
    vLCD_Display_Access();
    vOpen_Door();
    digitalWrite(BUZZER, HIGH);
    delay(200);
    digitalWrite(BUZZER, LOW);
    delay(3000);
    vClose_Door();
  }
  else if(usFingerID == -1){
    vLCD_Display_Wrong_Finger();
    digitalWrite(BUZZER, HIGH);
    delay(200);
    digitalWrite(BUZZER, LOW);
    delay(200);
    digitalWrite(BUZZER, HIGH);
    delay(200);
    digitalWrite(BUZZER, LOW);

    do{
      usFingerID = usIsFingerPrintCorrect();
    }
    while(usFingerID == -1);
    delay(2000);
  }
}

//CHECK FINGER PRINT
unsigned short usIsFingerPrintCorrect(void){
  unsigned short p = finger.getImage();

  if(p != FINGERPRINT_NOFINGER){
    if(p != FINGERPRINT_OK)
    return -1;
  }
  else{
    return -2; //NO FINGER
  }

  p = finger.image2Tz();
  if(p != FINGERPRINT_OK)
  return -1;

  p = finger.fingerFastSearch();
  if(p != FINGERPRINT_OK)
  return -1;

  //FOUND A MATCH
  lcd.clear();
  lcd.print("FINGER MATCH");
  lcd.setCursor(0,1);
  lcd.print("ID: ");
  lcd.setCursor(4,1);
  lcd.print(finger.fingerID);

  delay(2000);

  return finger.fingerID;
}

//LCD DISPLAY DOOR ACCESS
void vLCD_Display_Access(void){
  lcd.clear();
  lcd.setCursor(6,0);
  lcd.print("Door");
  lcd.setCursor(5,1);
  lcd.print("Access");
}

//OPEN DOOR
void vOpen_Door(void){
  digitalWrite(LOCK, HIGH);
}

//CLOSE DOOR
void vClose_Door(void){
  digitalWrite(LOCK, LOW);
}

//DISPLAY WRONG FINGER
void vLCD_Display_Wrong_Finger(void){
  lcd.clear();
  lcd.setCursor(3,0);
  lcd.print("Finger Not");
  lcd.setCursor(5,1);
  lcd.print("Match!");
}

//CHECK KEYPAD
unsigned short usCheck_Correct_Password(void){
  char InKey = customKeypad.getKey();
  char cPassword[10] = {'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'};
  char cPassword_Disp[10] = {'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'};
  unsigned short i = 0;

  if((InKey != '\0') && (InKey != '#')){
    do{
      if((InKey != '\0') && (InKey != '#')){
        digitalWrite(BUZZER, HIGH);
        delay(30);
        digitalWrite(BUZZER, LOW);

        if(i<6){
          cPassword[i] = InKey;
          cPassword_Disp[i] = '*'; //BLIND PASSWORD
          i++;
        }
        vLCD_Dis_Password(cPassword_Disp);
      }
      InKey = customKeypad.getKey();
      //Serial.println(InKey);
    }
    while(InKey != '#');

    //CHECK PASSWORD
    if(usIs_Password_Correct(gszCorrectPassword, cPassword) == OK){
      //DOOR ACCESS
      return OK;
    }
    else{
      return NOK;
    }
  }
  else{
    return NA;
  }
}

//DISPLAY PASSWORD ON LCD
void vLCD_Dis_Password(char *cPassword){
  lcd.clear();
  lcd.print("Password :");
  lcd.setCursor(0,1);
  lcd.print(cPassword);
}

//COMPARE 2 PASSWORD
unsigned short usIs_Password_Correct(char *szCPW, char *cszKPW){
  if(strncmp(szCPW, cszKPW, 6) == 0){
    return OK;
  }
  else{
    return NOK;
  }
}

//MODE SELECTION
void vMode_Selection(void){
  enum enKeyPad enKeyPadValue = NO_KEY_PRESS;
  unsigned short usStatus = NOK;
  unsigned short usExit = 0;
  unsigned short usSelect = 0;

  gusModeSelection = 0;
  gusModeSelection_Bak = 1;

    do{
      enKeyPadValue = enGet_Key_Function(&usStatus);
      if(usStatus == OK){
        if(enKeyPadValue == UP_KEY){
          gusModeSelection--;
          if(gusModeSelection <= -1){
            gusModeSelection = 2;
          }
        }
        else if(enKeyPadValue == DOWN_KEY){
          gusModeSelection++;
        }
        else if(enKeyPadValue == SELECT_KEY){
          usSelect = 1;
        }
        else if((enKeyPadValue == LEFT_KEY) || (enKeyPadValue == RIGHT_KEY)){
          usExit = 1;
        }
      }
      
      if(gusModeSelection == 0){
        //DISPLAY ENROLLED MODE
        vLCD_Display_Enrolled_Mode();
        if(usSelect == 1){
          vFinger_Enrolled_Mode();
          usSelect = 0;
        }
      }
      else if(gusModeSelection == 1){
        //DISPLAY DELETE MODE
        vLCD_Display_Delete_Mode();
        if(usSelect == 1){
          vFinger_Delete_Mode();
          usSelect = 0;
        }
      }
      else if(gusModeSelection == 2){
      //DISPLAY CHANGE PASSWORD MODE
      vLCD_Display_Change_Password_Mode();
      if(usSelect == 1){
        vChange_Password_Mode();
        usSelect = 0;
      }
    }
    else{
      gusModeSelection = 0;
    }
  }
  while(usExit == 0);
}

  

//DISPLAY ENROLLED MODE
void vLCD_Display_Enrolled_Mode(void){
  if(gusModeSelection != gusModeSelection_Bak){
    lcd.clear();
    lcd.print("ENROLLED FINGER");
    //lcd.setCursor(0,1);
    //lcd.print("UP/DOWN");
    gusModeSelection_Bak = gusModeSelection;
  }
}

//FINGER ENROLLED MODE
void vFinger_Enrolled_Mode(void){
  enum enKeyPad enKeyPadValue = NO_KEY_PRESS;
  unsigned short usStatus = NOK;
  short usFinger_ID = 1;
  short usFinger_ID_Bak = 2;
  unsigned short usIsID_Available = 0;
  unsigned short usSelect = 0;

  do{
    enKeyPadValue = enGet_Key_Function(&usStatus);
    if(usStatus == OK){
      if((enKeyPadValue == UP_KEY) || (enKeyPadValue == DOWN_KEY)){
        if(enKeyPadValue == UP_KEY){
           usFinger_ID--;
        }
        else if(enKeyPadValue == DOWN_KEY){
          usFinger_ID++;
        }
        
        if(usFinger_ID < 1){
          usFinger_ID = 1;
        }
        else if(usFinger_ID > 10){
          usFinger_ID = 10;
        }
      }
      else if(enKeyPadValue == SELECT_KEY){
        usSelect = 1;
      }
      else if((enKeyPadValue == LEFT_KEY) || (enKeyPadValue == RIGHT_KEY)){
        gusModeSelection_Bak = gusModeSelection + 1;
        break;
      }
    }

    if(usFinger_ID != usFinger_ID_Bak){
      //CHECK WHETHER THIS ID AVAILABLE OR NOT
      if(usCheck_Finger_ID_Available(usFinger_ID) == 1){
        usIsID_Available = 1;
      }
      else{
        usIsID_Available = 0;
      }

      vLCD_Display_Select_Enrolled_ID(usFinger_ID, usIsID_Available);
      usFinger_ID_Bak = usFinger_ID;
    }

    //ENTER DELETE FUNCTION
    if(usSelect == 1){
      if(usDelete_Finger_Print(usFinger_ID) == OK){
        gus_Finger_ID_Memory[usFinger_ID] = 0;
        vEEPROM_Write_Finger_ID_Memory();
      }
      else{
        vLCD_Display_Error();
        delay(3000);
      }
      usSelect = 0;
      usFinger_ID_Bak = usFinger_ID + 1;
      gusModeSelection_Bak = gusModeSelection + 1;
    }
  }
  while(1);
}

//WAIT RELEASE AND READ KEYPAD
enum enKeyPad enGet_Key_Function(unsigned short *usStatus){
  enum enKeyPad enKeyPadTemp = NO_KEY_PRESS;
  enum enKeyPad enKeyPadValue = NO_KEY_PRESS;

  enKeyPadValue = enGet_Key();
  //Serial.println(enKeyPadValue);

  if(enKeyPadValue != NO_KEY_PRESS){
    //WAIT USER RELASE BUTTON
    do{
      enKeyPadTemp = enGet_Key();
    }
    while(enKeyPadTemp != NO_KEY_PRESS);
    *usStatus = OK;
  }
  else{
    *usStatus = NOK;
  }
  return enKeyPadValue;
}

//READ KEYPAD FUNCTION
enum enKeyPad enGet_Key(void){
  enum enKeyPad enKeyPadValue;
  char InKey = customKeypad.getKey();

  if(InKey == 'A'){
    digitalWrite(BUZZER, HIGH);
    delay(30);
    digitalWrite(BUZZER, LOW);
    enKeyPadValue = UP_KEY;
  }
  else if(InKey == 'B'){
    digitalWrite(BUZZER, HIGH);
    delay(30);
    digitalWrite(BUZZER, LOW);
    enKeyPadValue = DOWN_KEY;
  }
  else if(InKey == 'C'){
    digitalWrite(BUZZER, HIGH);
    delay(30);
    digitalWrite(BUZZER, LOW);
    enKeyPadValue = LEFT_KEY;
  }
  else if(InKey == '#'){
    digitalWrite(BUZZER, HIGH);
    delay(30);
    digitalWrite(BUZZER, LOW);
    enKeyPadValue = SELECT_KEY;
  }
  else{
    enKeyPadValue = NO_KEY_PRESS;
  }
  return enKeyPadValue;
}

//CHECK FINGER ID AVAILABLE
unsigned short usCheck_Finger_ID_Available(unsigned short usID){
  if(gus_Finger_ID_Memory[usID] == 1){
    return 0;
  }
  else{
    return 1;
  }
}

//DISPLAY SELECT ID IN ENROLLED MODE
void vLCD_Display_Select_Enrolled_ID(unsigned short usID, unsigned short usAvailability){
  char szTemp[20];

  lcd.clear();
  sprintf(szTemp, "Select ID: %d", usID);
  lcd.print(szTemp);
  lcd.setCursor(0,1);
  if(usAvailability == 1){
    lcd.print("(EMPTY)");
  }
  else{
    lcd.print("(IS USED)");
  }
}

//DELETE FINGER PRINT FUNCTION
unsigned short usDelete_Finger_Print(unsigned short usFingerID){
  short p = -1;

  p = finger.deleteModel(usFingerID);

  if(p == FINGERPRINT_OK){
    lcd.clear();
    lcd.print("DELETED !");
    delay(1000);
    return OK;
  }
  else{
    return OK;
  }
}

//WRITE FINGER PRINT MEMORY
void vEEPROM_Write_Finger_ID_Memory(void){
  unsigned short usAdress = 0;

  for(short i=1; i<11; i++){
    usAdress = i;
    EEPROM.write(usAdress, gus_Finger_ID_Memory[i]);
  }
}

//DISPLAY ERROR
void vLCD_Display_Error(void){
  lcd.clear();
  lcd.print("ERROR !");
}

//DISPLAY DELETE MODE
void vLCD_Display_Delete_Mode(void){
  lcd.clear();
  lcd.print("DELETE FINGER");
  //lcd.setCursor(0,1);
  //lcd.print("UP/DOWN");
  gusModeSelection_Bak = gusModeSelection;
}

//FINGER DELETE MODE
void vFinger_Delete_Mode(void){
  enum enKeyPad enKeyPadValue = NO_KEY_PRESS;
  unsigned short usStatus = NOK;
  short usFinger_ID = 1;
  short usFinger_ID_Bak = 2;
  unsigned short usIsID_Available = 0;
  unsigned short usSelect = 0;

  do{
    enKeyPadValue = enGet_Key_Function(&usStatus);
    if(usStatus == OK){
      if((enKeyPadValue == UP_KEY) || (enKeyPadValue == DOWN_KEY)){
        if(enKeyPadValue == UP_KEY){
          usFinger_ID--;
        }
        else if(enKeyPadValue == DOWN_KEY){
          usFinger_ID++;
        }
        
        if(usFinger_ID < 1){
          usFinger_ID = 1;
        }
        else if(usFinger_ID > 10){
          usFinger_ID = 10;
        }
      }
      else if(enKeyPadValue == SELECT_KEY){
        usSelect = 1;
      }
      else if((enKeyPadValue == LEFT_KEY) || (enKeyPadValue == RIGHT_KEY)){
        gusModeSelection_Bak = gusModeSelection + 1;
        break;
      }
    }

    if(usFinger_ID != usFinger_ID_Bak){
      //CHECK WHETHER THIS ID AVAILABLE OR NOT
      if(usCheck_Finger_ID_Available(usFinger_ID) == 1){
        usIsID_Available = 1;
      }
      else{
        usIsID_Available = 0;
      }

      vLCD_Display_Select_Enrolled_ID(usFinger_ID, usIsID_Available);
      usFinger_ID_Bak = usFinger_ID;
    }

    //ENTER DELETE FUNCTION
    if(usSelect == 1){
      if(usDelete_Finger_Print(usFinger_ID) == OK){
        gus_Finger_ID_Memory[usFinger_ID] = 0;
        vEEPROM_Write_Finger_ID_Memory();
      }
      else{
        vLCD_Display_Error();
        delay(3000);
      }
      usSelect = 0;
      usFinger_ID_Bak = usFinger_ID + 1;
      gusModeSelection_Bak = gusModeSelection + 1;
    }
  }
  while(1);
}

//DISPLAY CHANGE PASSWORD MODE
void vLCD_Display_Change_Password_Mode(void){
  if(gusModeSelection != gusModeSelection_Bak){
    lcd.clear();
    lcd.print("CHANGE PASSWORD");
    //lcd.setCursor(0,1);
    //lcd.print("UP/DOWN");
    gusModeSelection_Bak = gusModeSelection;
  }
}

//CHANGE PASSWORD FUNCTION
void vChange_Password_Mode(void){
  char InKey = '\0';
  char cPassword[10] = {'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'};
  unsigned short i = 0;
  unsigned short usStrLen = 0;
  enum enKeyPad enKeyPadValue = NO_KEY_PRESS;
  unsigned short usStatus = NOK;
  unsigned short usExit = 0;

  vLCD_Display_Keyin_New_Password();

  do{
    if((InKey != '\0') && (InKey != '#')){
      digitalWrite(BUZZER, HIGH);
      delay(30);
      digitalWrite(BUZZER, LOW);

      if(i < 6){
        cPassword[i] = InKey;
        i++;
      }
      vLCD_Dis_New_Password(cPassword);
    }

    InKey = customKeypad.getKey(); //NO KEY CODE

    if(InKey == 'C'){
      digitalWrite(BUZZER, HIGH);
      delay(30);
      digitalWrite(BUZZER, LOW);
      usExit = 1;
      break;
    }

    usStrLen = strlen(cPassword);
  } 
  while((InKey != '#') || (usStrLen<6));
  if(usExit == 0){
    digitalWrite(BUZZER, HIGH);
    delay(30);
    digitalWrite(BUZZER, LOW);
    strncpy(gszCorrectPassword, cPassword, 6); //REPLACE WITH THE NEW PASSWORD
    vEEPROM_Write_Password();

    vLCD_Dis_Complete_Password();
    delay(3000);
  }

  gusModeSelection_Bak = gusModeSelection + 1;
}

//LCD DISPLAY WRONG PASSWORD
void vLCD_Display_Keyin_New_Password(void){
  lcd.clear();
  lcd.setCursor(2,0);
  lcd.print("KEYIN NEW PW");
  lcd.setCursor(2,1);
  lcd.print("END WITH '#'");
}

//DISPLAY PASSWORD ON LCD
void vLCD_Dis_New_Password(char *cPassword){
  lcd.clear();
  lcd.print("New Password:");
  lcd.setCursor(0,1);
  lcd.print(cPassword);
}

//DISPLAY COMPLETE PASSWORD ON LCD
void vLCD_Dis_Complete_Password(void){
  lcd.clear();
  lcd.print("NEW PASSWORD SET");
  lcd.setCursor(4,1);
  lcd.print("COMPLETE !");
}

//LCD DISPLAY WRONG PASSWORD
void vLCD_Display_Wrong_Password(void){
  lcd.clear();
  lcd.setCursor(5,0);
  lcd.print("WRONG");
  lcd.setCursor(4,1);
  lcd.print("PASSWORD !");
  lcd.setCursor(4,2);  
}
