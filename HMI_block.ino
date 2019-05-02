#include <MCUFRIEND_kbv.h>
#include <Adafruit_GFX.h>
#include <EEPROM.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define TRUE  1
#define FALSE 0
#define ONE_ROW 25
#define SPACE 20
#define TEMP_SP_ROW_POS 5
#define HUMID_SP_ROW_POS 8
#define LIGHT_SP_ROW_POS 11
#define SOIL_SP_ROW_POS 14
#define CONFIRM_ROW_POS 16
#define PTR_POS_COL *(&ptr_pos_col)

#define C_TEMP_POS 5
#define C_HUMID_POS 8
#define C_LIGHT_POS 11
#define C_SOIL_POS 14
#define STOP_POS 16

#define TEMP_LENGTH_LIMIT 2
#define HUM_LENGTH_LIMIT 2
#define LIGHT_LENGTH_LIMIT 5
#define SOIL_LENGTH_LIMIT 4

#define LCD_RESET A4 // Can alternately just connect to Arduino's reset pin
#define LCD_CS A3   // Chip Select goes to Analog 3
#define LCD_CD A2  // Command/Data goes to Analog 2
#define LCD_WR A1  // LCD Write goes to Analog 1
#define LCD_RD A0 // LCD Read goes to Analog 0

// Assign human-readable names to some common 16-bit color values:
#define BLACK   0x0000
#define RED     0xF800
#define WHITE   0xFFFF
/*
  #define GREEN   0x07E0
  #define CYAN    0x07FF
  #define MAGENTA 0xF81F
  #define BLUE    0x001F
  #define YELLOW  0xFFE0
*/

RF24 radio(47, 46); //CE, CSN -- MOSI -> 51, MISO -> 50, SCK ->52
uint64_t address_1 = 0x1234;
uint64_t address_2 = 0x1233;


char key;
String temp_sp, humid_sp, light_sp, soil_sp; //variable to store the user input
int temp_sp_int, humid_sp_int, light_sp_int, soil_sp_int; //variable to sent wireless
float temperature, humidity;
int soil_moisture; //variable from the control block
int light_intensity;
//verification to the receiver
char temp[1] = "t";
char humid[1] = "h";
char light[1] = "l";
char soil[1] = "s";

MCUFRIEND_kbv tft;

byte controller_setting;
byte is_set, not_set;
int setting_flag[4] = {0, 0, 0, 0};
int NOR = 0;
int ptr_pos_col = 4;

//5 small loops for the screen working
byte temp_set = FALSE;
byte humid_set = FALSE;
byte light_set = FALSE;
byte soil_set = FALSE;
byte confirm_set = FALSE;

const int numRows = 4; // number of rows in the keypad
const int numCols = 4; // number of columns
const int debounceTime = 20; // number of milliseconds for switch to be stable
const char keymap[numRows][numCols] = {
  { '1', '2', '3', 'A' } ,
  { '4', '5', '6', 'B' } ,
  { '7', '8', '9', 'C' } ,
  { '*', '0', '#', 'D' }
};
// this array determines the pins used for rows and columns
const int rowPins[numRows] = { 36, 34, 32, 30 }; // Rows 0 through 3
const int colPins[numCols] = { 28, 26, 24, 22 }; // Columns 0 through 2

char getKey(void);
void print_custom(int col_pos, int row_pos, String content, byte textSize);
void print_custom_var(int col_pos, int row_pos, float var, byte textSize);

void setup() {
  Serial.begin(9600);

  radio.begin();
  radio.openReadingPipe(1, address_1);
  radio.openWritingPipe(address_2);
  radio.setPALevel(RF24_PA_MIN);
  radio.stopListening(); //set as transmitter

  tft.reset();
  uint16_t identifier = tft.readID();
  tft.begin(identifier);
  tft.fillScreen(BLACK);

  /*Keypay initialization*/
  for (int row = 0; row < numRows; row++)
  {
    pinMode(rowPins[row], INPUT); // Set row pins as input
    digitalWrite(rowPins[row], HIGH); // turn on Pull-ups
  }
  for (int column = 0; column < numCols; column++)
  {
    pinMode(colPins[column], OUTPUT); // Set column pins as outputs for writing
    digitalWrite(colPins[column], HIGH); // Make all columns inactive
  }

  //checking the user setting for the controller

  if (EEPROM.read(0) == 1) {
    controller_setting = TRUE; //user already set the parameter
  }
  else {
    controller_setting = FALSE;
  }
}

void loop() {
  if (!controller_setting) {//transmitter
    not_set = TRUE;
    radio.stopListening();
    tft.setCursor(0, 0);
    tft.setTextSize(4);
    tft.setTextColor(WHITE);
    tft.println("Parameters");
    tft.println("   setting");

    tft.setTextColor(RED);
    NOR = 4;
    print_custom(0, NOR * ONE_ROW, "Air temperature", 3);
    NOR += 3;
    print_custom(0, NOR * ONE_ROW, "Relative humidity", 3);
    NOR += 3;
    print_custom(0, NOR * ONE_ROW, "Light intensity", 3);
    NOR += 3;
    print_custom(0, NOR * ONE_ROW, "Soil moisture", 3);
    NOR += 2;
    print_custom(2 * SPACE, NOR * ONE_ROW, "press \'D\'", 3);
    print_custom(3 * SPACE, CONFIRM_ROW_POS * ONE_ROW, "to finish", 3);

    //clear set-point
    for (int i = 0; i < temp_sp.length(); i++) {
      temp_sp.remove(i);
    }
    for (int i = 0; i < humid_sp.length(); i++) {
      humid_sp.remove(i);
    }
    for (int i = 0; i < light_sp.length(); i++) {
      light_sp.remove(i);
    }
    for (int i = 0; i < soil_sp.length(); i++) {
      soil_sp.remove(i);
    }
    for (byte i = 0; i < 4; i++) {
      setting_flag[i] = 0;
    }

    //initialize set point
    temp_sp_int = 0;
    humid_sp_int = 0;
    light_sp_int = 0;
    soil_sp_int = 0;

    temp_set = FALSE;
    light_set = FALSE;
    humid_set = FALSE;
    soil_set =  FALSE;
    confirm_set = FALSE;

    while (not_set) {

      while (!temp_set) {

        key = getKey();
        print_custom(PTR_POS_COL * SPACE, TEMP_SP_ROW_POS * ONE_ROW, "|", 3);
        if (key != 0) {
          if (key == '0' || key == '1' || key == '2' || key == '3' ||
              key == '4' || key == '5' || key == '6' || key == '7' ||
              key == '8' || key == '9' )
          {
            tft.setTextColor(BLACK);
            print_custom(PTR_POS_COL * SPACE, TEMP_SP_ROW_POS * ONE_ROW, "|", 3);
            tft.setTextColor(RED);

            tft.setCursor(PTR_POS_COL * SPACE, TEMP_SP_ROW_POS * ONE_ROW);
            tft.print(key);
            temp_sp += key;
            PTR_POS_COL += 1;
          }
        }
        if (key == 'C') {
          if (PTR_POS_COL > 4) {
            tft.setTextColor(BLACK);
            print_custom(PTR_POS_COL * SPACE, TEMP_SP_ROW_POS * ONE_ROW, "|", 3);
            PTR_POS_COL -= 1;
            tft.setCursor(PTR_POS_COL * SPACE, TEMP_SP_ROW_POS * ONE_ROW);
            tft.print(temp_sp.charAt(temp_sp.length() - 1));
            temp_sp.remove(temp_sp.length() - 1);
            tft.setTextColor(RED);
          }
          else;
        }
        if (key == '#') {
          setting_flag[0] = 1;
          temp_sp_int = temp_sp.toInt();
          tft.setTextColor(BLACK);
          print_custom(PTR_POS_COL * SPACE, TEMP_SP_ROW_POS * ONE_ROW, "|", 3);
          tft.setTextColor(RED);

          //go to next row
          if (humid_sp.length() != 0) {
            PTR_POS_COL = 4 + humid_sp.length();
          }
          else {
            PTR_POS_COL = 4;
          }

          temp_set = TRUE; //escape while loop and indicate temp_set done
          light_set = TRUE;
          soil_set =  TRUE;
          confirm_set = TRUE;
          humid_set = FALSE;
        }
        if (key == 'B') { //go to humid_set row
          tft.setTextColor(BLACK);
          print_custom(PTR_POS_COL * SPACE, TEMP_SP_ROW_POS * ONE_ROW, "|", 3);
          tft.setTextColor(RED);

          /*Go down to humidity setting row*/
          if (humid_sp.length() != 0) {
            PTR_POS_COL = 4 + humid_sp.length();
          }
          else {
            PTR_POS_COL = 4;
          }
          humid_set = FALSE;
          temp_set = TRUE;
          light_set = TRUE;
          soil_set =  TRUE;
          confirm_set = TRUE;
        }
        if  (key == 'D') { // go to confirm block
          tft.setTextColor(BLACK);
          print_custom(PTR_POS_COL * SPACE, TEMP_SP_ROW_POS * ONE_ROW, "|", 3);
          tft.setTextColor(RED);

          PTR_POS_COL = 4;
          confirm_set = FALSE;
          humid_set = TRUE;
          temp_set = TRUE;
          light_set = TRUE;
          soil_set =  TRUE;
        }
      }

      while (!humid_set) {

        key = getKey();
        print_custom(PTR_POS_COL * SPACE, HUMID_SP_ROW_POS * ONE_ROW, "|", 3);
        if (key != 0) {
          if (key == '0' || key == '1' || key == '2' || key == '3' ||
              key == '4' || key == '5' || key == '6' || key == '7' ||
              key == '8' || key == '9' )
          {
            tft.setTextColor(BLACK);
            print_custom(PTR_POS_COL * SPACE, HUMID_SP_ROW_POS * ONE_ROW, "|", 3);
            tft.setTextColor(RED);

            tft.setCursor(PTR_POS_COL * SPACE, HUMID_SP_ROW_POS * ONE_ROW);
            tft.print(key);
            humid_sp += key;
            PTR_POS_COL += 1;
          }
        }
        if (key == 'C') {
          if (PTR_POS_COL > 4) {
            tft.setTextColor(BLACK);
            print_custom(PTR_POS_COL * SPACE, HUMID_SP_ROW_POS * ONE_ROW, "|", 3);
            PTR_POS_COL -= 1;
            tft.setCursor(PTR_POS_COL * SPACE, HUMID_SP_ROW_POS * ONE_ROW);
            tft.print(humid_sp.charAt(humid_sp.length() - 1));
            humid_sp.remove(humid_sp.length() - 1);
            tft.setTextColor(RED);
          }
          else;
        }
        if (key == '#') {
          setting_flag[1] = 1;
          humid_sp_int = humid_sp.toInt();
          tft.setTextColor(BLACK);
          print_custom(PTR_POS_COL * SPACE, HUMID_SP_ROW_POS * ONE_ROW, "|", 3);
          tft.setTextColor(RED);

          if (light_sp.length() != 0) {
            PTR_POS_COL = 4 + light_sp.length();
          }
          else {
            PTR_POS_COL = 4;
          }

          light_set = FALSE;
          humid_set = TRUE; //go to light_set row
          temp_set = TRUE;
          soil_set =  TRUE;
          confirm_set = TRUE;
        }
        if (key == 'A') { //go up to temp_set row
          tft.setTextColor(BLACK);
          print_custom(PTR_POS_COL * SPACE, HUMID_SP_ROW_POS * ONE_ROW, "|", 3);
          tft.setTextColor(RED);

          if (temp_sp.length() != 0) {
            PTR_POS_COL = 4 + temp_sp.length();
          }
          else {
            PTR_POS_COL = 4;
          }
          temp_set = FALSE;
          humid_set = TRUE;
          light_set = TRUE;
          soil_set =  TRUE;
          confirm_set = TRUE;
        }
        if (key == 'B') { // go down to light_set row
          tft.setTextColor(BLACK);
          print_custom(PTR_POS_COL * SPACE, HUMID_SP_ROW_POS * ONE_ROW, "|", 3);
          tft.setTextColor(RED);

          if (light_sp.length() != 0) {
            PTR_POS_COL = 4 + light_sp.length();
          }
          else {
            PTR_POS_COL = 4;
          }
          light_set = FALSE;
          temp_set = TRUE;
          humid_set = TRUE;
          soil_set =  TRUE;
          confirm_set = TRUE;
        }
        if  (key == 'D') {
          tft.setTextColor(BLACK);
          print_custom(PTR_POS_COL * SPACE, HUMID_SP_ROW_POS * ONE_ROW, "|", 3);
          tft.setTextColor(RED);

          PTR_POS_COL = 4;
          confirm_set = FALSE;
          humid_set = TRUE;
          temp_set = TRUE;
          light_set = TRUE;
          soil_set =  TRUE;
        }
      }

      while (!light_set) {

        key = getKey();
        print_custom(PTR_POS_COL * SPACE, LIGHT_SP_ROW_POS * ONE_ROW, "|", 3);
        if (key != 0) {
          if (key == '0' || key == '1' || key == '2' || key == '3' ||
              key == '4' || key == '5' || key == '6' || key == '7' ||
              key == '8' || key == '9' )
          {
            tft.setTextColor(BLACK);
            print_custom(PTR_POS_COL * SPACE, LIGHT_SP_ROW_POS * ONE_ROW, "|", 3);
            tft.setTextColor(RED);

            tft.setCursor(PTR_POS_COL * SPACE, LIGHT_SP_ROW_POS * ONE_ROW);
            tft.print(key);
            light_sp += key;
            PTR_POS_COL += 1;
          }
        }
        if (key == 'C') {
          if (PTR_POS_COL > 4) {
            tft.setTextColor(BLACK);
            print_custom(PTR_POS_COL * SPACE, LIGHT_SP_ROW_POS * ONE_ROW, "|", 3);
            PTR_POS_COL -= 1;
            tft.setCursor(PTR_POS_COL * SPACE, LIGHT_SP_ROW_POS * ONE_ROW);
            tft.print(light_sp.charAt(light_sp.length() - 1));
            light_sp.remove(light_sp.length() - 1);
            tft.setTextColor(RED);
          }
          else;
        }
        if (key == '#') {
          setting_flag[2] = 1;
          light_sp_int = light_sp.toInt();
          tft.setTextColor(BLACK);
          print_custom(PTR_POS_COL * SPACE, LIGHT_SP_ROW_POS * ONE_ROW, "|", 3);
          tft.setTextColor(RED);

          if (soil_sp.length() != 0) {
            PTR_POS_COL = 4 + soil_sp.length();
          }
          else {
            PTR_POS_COL = 4;
          }

          soil_set =  FALSE;
          light_set = TRUE;
          temp_set = TRUE;
          humid_set = TRUE;
          confirm_set = TRUE;
        }
        if (key == 'A') { //go up to humid_set row
          tft.setTextColor(BLACK);
          print_custom(PTR_POS_COL * SPACE, LIGHT_SP_ROW_POS * ONE_ROW, "|", 3);
          tft.setTextColor(RED);

          if (humid_sp.length() != 0) {
            PTR_POS_COL = 4 + humid_sp.length();
          }
          else {
            PTR_POS_COL = 4;
          }

          humid_set = FALSE;
          soil_set =  TRUE;
          temp_set = TRUE;
          light_set = TRUE;
          confirm_set = TRUE;
        }
        if (key == 'B') { // go down to soil_set row
          tft.setTextColor(BLACK);
          print_custom(PTR_POS_COL * SPACE, LIGHT_SP_ROW_POS * ONE_ROW, "|", 3);
          tft.setTextColor(RED);

          if (soil_sp.length() != 0) {
            PTR_POS_COL = 4 + soil_sp.length();
          }
          else {
            PTR_POS_COL = 4;
          }

          soil_set =  FALSE;
          light_set = TRUE;
          temp_set = TRUE;
          humid_set = TRUE;
          confirm_set = TRUE;
        }
        if  (key == 'D') {
          tft.setTextColor(BLACK);
          print_custom(PTR_POS_COL * SPACE, LIGHT_SP_ROW_POS * ONE_ROW, "|", 3);
          tft.setTextColor(RED);

          PTR_POS_COL = 4;
          confirm_set = FALSE;
          humid_set = TRUE;
          temp_set = TRUE;
          light_set = TRUE;
          soil_set =  TRUE;
        }
      }

      while (!soil_set) {

        key = getKey();
        print_custom(PTR_POS_COL * SPACE, SOIL_SP_ROW_POS * ONE_ROW, "|", 3);
        if (key != 0) {
          if (key == '0' || key == '1' || key == '2' || key == '3' ||
              key == '4' || key == '5' || key == '6' || key == '7' ||
              key == '8' || key == '9' )
          {
            tft.setTextColor(BLACK);
            print_custom(PTR_POS_COL * SPACE, SOIL_SP_ROW_POS * ONE_ROW, "|", 3);
            tft.setTextColor(RED);

            tft.setCursor(PTR_POS_COL * SPACE, SOIL_SP_ROW_POS * ONE_ROW);
            tft.print(key);
            soil_sp += key;
            PTR_POS_COL += 1;
          }
        }
        if (key == 'C') {
          if (PTR_POS_COL > 4) {
            tft.setTextColor(BLACK);
            print_custom(PTR_POS_COL * SPACE, SOIL_SP_ROW_POS * ONE_ROW, "|", 3);
            PTR_POS_COL -= 1;
            tft.setCursor(PTR_POS_COL * SPACE, SOIL_SP_ROW_POS * ONE_ROW);
            tft.print(soil_sp.charAt(soil_sp.length() - 1));
            soil_sp.remove(soil_sp.length() - 1);
            tft.setTextColor(RED);
          }
        }
        if (key == '#') {
          setting_flag[3] = 1;
          soil_sp_int = soil_sp.toInt();
          tft.setTextColor(BLACK);
          print_custom(PTR_POS_COL * SPACE, SOIL_SP_ROW_POS * ONE_ROW, "|", 3);
          tft.setTextColor(RED);
          PTR_POS_COL = 4;

          confirm_set = FALSE;
          light_set = TRUE;
          humid_set = TRUE;
          soil_set =  TRUE;
          temp_set = TRUE;
        }
        if (key == 'A') { //go up to light_set row
          tft.setTextColor(BLACK);
          print_custom(PTR_POS_COL * SPACE, SOIL_SP_ROW_POS * ONE_ROW, "|", 3);
          tft.setTextColor(RED);
          Serial.println(light_sp.length());

          if (light_sp.length() != 0) {
            PTR_POS_COL = 4 + light_sp.length();
          }
          else {
            PTR_POS_COL = 4;
          }

          light_set = FALSE;
          humid_set = TRUE;
          soil_set =  TRUE;
          temp_set = TRUE;
          confirm_set = TRUE;
        }
        if  (key == 'D') {
          tft.setTextColor(BLACK);
          print_custom(PTR_POS_COL * SPACE, SOIL_SP_ROW_POS * ONE_ROW, "|", 3);
          tft.setTextColor(RED);

          PTR_POS_COL = 4;
          confirm_set = FALSE;
          humid_set = TRUE;
          temp_set = TRUE;
          light_set = TRUE;
          soil_set =  TRUE;
        }
      }

      while (!confirm_set) {
        byte check = 0;
        for (byte i = 0; i < 4; i++) {
          if (setting_flag[i] == 0) {
            check += 1;
          }
        }

        if (check != 0) {
          tft.setTextColor(BLACK);
          print_custom(2 * SPACE, NOR * ONE_ROW, "press \'D\'", 3);
          print_custom(3 * SPACE, CONFIRM_ROW_POS * ONE_ROW, "to finish", 3);
          tft.setTextColor(RED);

          print_custom(PTR_POS_COL * SPACE, CONFIRM_ROW_POS * ONE_ROW, "PARA NOT SET", 3);
          delay(3000);
          tft.setTextColor(BLACK);
          print_custom(PTR_POS_COL * SPACE, CONFIRM_ROW_POS * ONE_ROW, "PARA NOT SET", 3);
          tft.setTextColor(RED);
          print_custom(2 * SPACE, NOR * ONE_ROW, "press \'D\'", 3);
          print_custom(3 * SPACE, CONFIRM_ROW_POS * ONE_ROW, "to finish", 3);

          PTR_POS_COL = 4 + temp_sp.length();
          temp_set = FALSE;
          light_set = TRUE;
          humid_set = TRUE;
          soil_set =  TRUE;
          confirm_set = TRUE;
        }
        else {
          tft.setTextColor(BLACK);
          print_custom(2 * SPACE, NOR * ONE_ROW, "press \'D\'", 3);
          print_custom(3 * SPACE, CONFIRM_ROW_POS * ONE_ROW, "to finish", 3);
          tft.setTextColor(RED);

          EEPROM.write(0, 1); //write to this address to indicate user set the sp
          PTR_POS_COL = 4;

          //SEND SETTING TO control block
          print_custom(3 * SPACE, CONFIRM_ROW_POS * ONE_ROW, "Waiting...", 3);
          radio.write(&temp, sizeof(temp)); // send specifier
          delay(1000);
          radio.write(&temp_sp_int, sizeof(temp_sp_int)); //send set-point
          delay(1000);

          radio.write(&humid, sizeof(humid));
          delay(1000);
          radio.write(&humid_sp_int, sizeof(humid_sp_int));
          delay(1000);

          radio.write(&light, sizeof(light));
          delay(1000);
          radio.write(&light_sp_int, sizeof(light_sp_int));
          delay(1000);

          radio.write(&soil, sizeof(soil));
          delay(1000);
          radio.write(&soil_sp_int, sizeof(soil_sp_int));
          delay(1000);

          tft.setTextColor(BLACK);
          print_custom(3 * SPACE, CONFIRM_ROW_POS * ONE_ROW, "Waiting...", 3);
          tft.setTextColor(RED);
          print_custom(PTR_POS_COL * SPACE, CONFIRM_ROW_POS * ONE_ROW, "FINISHED", 3);
          delay(3000);

          temp_set = TRUE;
          light_set = TRUE;
          humid_set = TRUE;
          soil_set =  TRUE;
          confirm_set = TRUE;
          controller_setting = TRUE;
          not_set = FALSE;
        }
      }
    }
  }

  else { //receiver
    tft.reset();
    uint16_t identifier = tft.readID();
    tft.begin(identifier);
    tft.fillScreen(BLACK);

    is_set = TRUE;
    tft.setTextColor(WHITE);
    print_custom(0, 0, "Greenhouse", 4);
    print_custom(5 * SPACE, 10 + ONE_ROW, "climate", 4);
    radio.startListening();

    /*test value*/
    temperature = 0;
    humidity = 0;
    soil_moisture = 0;
    light_intensity = 0;

    NOR = 4;
    ptr_pos_col = 4;
    tft.setTextColor(RED);
    //LAY OUT THE DISPLAY
    print_custom(0, NOR * ONE_ROW, "Air temperature", 3);
    NOR += 3;
    print_custom(0, NOR * ONE_ROW, "Relative humidity", 3);
    NOR += 3;
    print_custom(0, NOR * ONE_ROW, "Light intensity", 3);
    NOR += 3;
    print_custom(0, NOR * ONE_ROW, "Soil moisture", 3);
    print_custom(0, STOP_POS * ONE_ROW, "Press \'#\' to stop", 3);

    while (is_set) {
      //haven't set the parameter, display the setting for controller
      key = getKey();

      if (radio.available()) { //reading value sent by control block
        char var[32] = "";
        radio.read(&var, sizeof(var));
        if (strcmp(var, "t") == 0) {
          Serial.println("temp ok");
          byte temp_reading = false;

          while (!temp_reading) {
            if (radio.available()) {
              float var1 = 0 ;
              radio.read(&var1, sizeof(var1));

              tft.setTextColor(BLACK);
              print_custom_var(ptr_pos_col * SPACE, C_TEMP_POS * ONE_ROW, temperature, 3);
              print_custom((ptr_pos_col + 5) * SPACE, C_TEMP_POS * ONE_ROW, "oC", 3);

              *(&temperature) = var1;

              tft.setTextColor(RED);
              print_custom_var(ptr_pos_col * SPACE, C_TEMP_POS * ONE_ROW, temperature, 3);
              print_custom((ptr_pos_col + 5) * SPACE, C_TEMP_POS * ONE_ROW, "oC", 3);


              Serial.println(temperature);
              temp_reading = true;
            }
          }

        }

        else if (strcmp(var, "h") == 0) {
          Serial.println("humid ok");
          byte humid_reading = false;

          while (!humid_reading) {
            if (radio.available()) {
              float var1 = 0 ;
              radio.read(&var1, sizeof(var1));

              tft.setTextColor(BLACK);
              print_custom_var(ptr_pos_col * SPACE, C_HUMID_POS * ONE_ROW, humidity, 3);
              print_custom((ptr_pos_col + 5) * SPACE, C_HUMID_POS * ONE_ROW, "%RH", 3);

              *(&humidity) = var1;

              tft.setTextColor(RED);
              print_custom_var(ptr_pos_col * SPACE, C_HUMID_POS * ONE_ROW, humidity, 3);
              print_custom((ptr_pos_col + 5) * SPACE, C_HUMID_POS * ONE_ROW, "%RH", 3);

              Serial.println(humidity);
              humid_reading = true;
            }
          }

        }

        else if (strcmp(var, "s") == 0) {
          Serial.println("soil ok");
          byte soil_reading = false;

          while (!soil_reading) {
            if (radio.available()) {
              int var1 = 0 ;
              radio.read(&var1, sizeof(var1));

              tft.setTextColor(BLACK);
              print_custom_var(ptr_pos_col * SPACE, C_SOIL_POS * ONE_ROW, soil_moisture, 3);

              *(&soil_moisture) = var1;

              tft.setTextColor(RED);
              print_custom_var(ptr_pos_col * SPACE, C_SOIL_POS * ONE_ROW, soil_moisture, 3);

              Serial.println(soil_moisture);
              soil_reading = true;
            }
          }
        }

        else if (strcmp(var, "l") == 0) {
          Serial.println("light ok");
          byte light_reading = false;

          while (!light_reading) {
            if (radio.available()) {
              int var1 = 0 ;
              radio.read(&var1, sizeof(var1));

              tft.setTextColor(BLACK);
              print_custom_var(ptr_pos_col * SPACE, C_LIGHT_POS * ONE_ROW, light_intensity, 3);
              print_custom((ptr_pos_col + 7) * SPACE, C_LIGHT_POS * ONE_ROW, "lux", 3);

              *(&light_intensity) = var1;

              tft.setTextColor(RED);
              print_custom_var(ptr_pos_col * SPACE, C_LIGHT_POS * ONE_ROW, light_intensity, 3);
              print_custom((ptr_pos_col + 7) * SPACE, C_LIGHT_POS * ONE_ROW, "lux", 3);

              Serial.println(light_intensity);
              light_reading = true;
            }
          }
        }
      }
      else;


      if (key == '#') {
        //send off command to control block
        radio.stopListening();
        char off_cmd[] = "off";
        radio.write(&off_cmd, sizeof(off_cmd));

        radio.startListening();
        byte confirm_turnoff = false;
        while (!confirm_turnoff) {
          if (radio.available()) {
            char var[32] = "";
            radio.read(&var, sizeof(var));
            if (strcmp(var, "ok") == 0) {
              confirm_turnoff = true;
            }
          }
        }

        EEPROM.write(0, 0);
        is_set = FALSE;
        controller_setting = FALSE;
        tft.reset();
        uint16_t identifier = tft.readID();
        tft.begin(identifier);
        tft.fillScreen(BLACK);
      }
    }
  }
}



/****************Functions**********************/
char getKey(void)
{
  char key = 0; // 0 indicates no key pressed
  for (int column = 0; column < numCols; column++)
  {
    digitalWrite(colPins[column], LOW); // Activate the current column.
    for (int row = 0; row < numRows; row++) // Scan all rows for a key press.
    {
      if (digitalRead(rowPins[row]) == LOW) // Is a key pressed?
      {
        delay(debounceTime); // debounce
        while (digitalRead(rowPins[row]) == LOW)
          ; // wait for key to be released
        key = keymap[row][column]; // Remember which key was pressed.
      }
    }
    digitalWrite(colPins[column], HIGH); // De-activate the current column.
  }
  return key; // returns the key pressed or 0 if none
}

void print_custom(int col_pos, int row_pos, String content, byte textSize) {
  tft.setCursor(col_pos, row_pos);
  tft.setTextSize(textSize);
  tft.print(content);
}

void print_custom_var(int col_pos, int row_pos, float var, byte textSize) {
  tft.setCursor(col_pos, row_pos);
  tft.setTextSize(textSize);
  tft.print(var);
}
