/*************CONTROL BLOCK**********************
  BH1750 connection: SDA - 20, SCL - 21, ADDR - GND
  THERMISTOR: A1
  HUMIDITY: d2
  Soil moisture: A2
  NRF24L01:  - MOSI,  - MISO,  - SCK
*/
/******************************VARIABLE DECLARATION********************************************/
#include <Wire.h>
#include <BH1750.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <EEPROM.h>
#include <TimerOne.h>
#include "fuzzy.h"
#include "DHT.h"

/****PIN DECLERATION***/
#define THERMISTOR 1
#define SOIL 2
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define FAN 13
#define HUMIDIFIER 12
#define DEHUMIDIFIER 11
#define WATER_PUMP 10
#define ARTI_LIGHTINH 9
#define SHADING 8

/***THERMISTOR MACRO**/
#define b0 0.0008983f //10 Cel degree - 17960
#define b1 0.0002499f //20 - 12090
#define b3 0.0000002f //35 - 6940
#define R_REF 10030
#define V_REF 5


float temp_calculate(float adc_val);

BH1750 lightMeter;
RF24 radio(47, 46); // CE, CSN

uint64_t address_1 = 0x1234;
uint64_t address_2 = 0x1233;
byte wireless_setting = false;

int temp_sp, humid_sp, light_sp, soil_sp;
byte receive_sp_count = 0;
byte is_system_set; //will work in receiver mode -  true will work in transmitter

//measuring variables
int lux;
float humid_m;
float temp_m;
int moisture;
char temp[1] = "t";
char humid[1] = "h";
char light[1] = "l";
char soil[1] = "s";

/******************************PROGRAM SETUP********************************************/
/****temperature input*******/
L_mf i_FLC_temp_NL;
trapezoidal_mf i_FLC_temp_NS;
triangular_mf i_FLC_temp_Z;
trapezoidal_mf i_FLC_temp_PS;
half_T_mf i_FLC_temp_PL;
/********temperature output*******/
L_mf o_FLC_temp_VL;
trapezoidal_mf o_FLC_temp_L;
trapezoidal_mf o_FLC_temp_M;
trapezoidal_mf o_FLC_temp_H;
half_T_mf o_FLC_temp_VH;

/****humidity input*******/
L_mf i_FLC_humid_NL;
trapezoidal_mf i_FLC_humid_NS;
triangular_mf i_FLC_humid_Z;
trapezoidal_mf i_FLC_humid_PS;
half_T_mf i_FLC_humid_PL;
/********humidifier output*******/
L_mf o_FLC_hum_VL;
trapezoidal_mf o_FLC_hum_L;
trapezoidal_mf o_FLC_hum_M;
trapezoidal_mf o_FLC_hum_H;
half_T_mf o_FLC_hum_VH;
/**********dehumidifier**********/
L_mf o_FLC_dehum_VL;
trapezoidal_mf o_FLC_dehum_L;
trapezoidal_mf o_FLC_dehum_M;
trapezoidal_mf o_FLC_dehum_H;
half_T_mf o_FLC_dehum_VH;

/************Soil moisture****************/
L_mf i_FLC_soil_NL;
trapezoidal_mf i_FLC_soil_NS;
triangular_mf i_FLC_soil_Z;
trapezoidal_mf i_FLC_soil_PS;
half_T_mf i_FLC_soil_PL;
/************output soil moisture***********/
triangular_mf o_FLC_soil_off;
trapezoidal_mf o_FLC_soil_L;
trapezoidal_mf o_FLC_soil_M;
trapezoidal_mf o_FLC_soil_H;
half_T_mf o_FLC_soil_VH;

/*****************Lighting**************************/
L_mf i_FLC_light_VL;
triangular_mf i_FLC_light_L;
triangular_mf i_FLC_light_M;
triangular_mf i_FLC_light_H;
half_T_mf i_FLC_light_VH;
/*****************Artificial light***************************/
triangular_mf o_FLC_light_off;
trapezoidal_mf o_FLC_light_L;
trapezoidal_mf o_FLC_light_M;
trapezoidal_mf o_FLC_light_H;
half_T_mf o_FLC_light_VH;
/*******************SHADING**************************/
L_mf o_FLC_shading_opened;
triangular_mf o_FLC_shading_half;
half_T_mf o_FLC_shading_closed;

void input_FLC_temp_init(void);
void output_FLC_temp_init(void);
float FLC_temp(float error);

void input_FLC_humid_init(void);
void output_FLC_hum_init(void);
void output_FLC_dehum_init(void);
float FLC_humidifier(float error);
float FLC_dehumidifier(float error);

void input_FLC_soil_init(void);
void output_FLC_soil_init(void);
float FLC_soil(float error);

void input_FLC_light_init(void);
void output_FLC_light_init(void);
void output_FLC_shading_init(void);
float FLC_artificial_light(float error);
float FLC_shading_light(float error);


void setup()
{
  pinMode(THERMISTOR, INPUT);
  pinMode(SOIL, INPUT);
  Serial.begin(9600);
  Serial.println("Ready");
  Wire.begin();
  lightMeter.begin();
  dht.begin();

  radio.begin();
  radio.openWritingPipe(address_1);
  radio.openReadingPipe(1, address_2);
  radio.setPALevel(RF24_PA_MIN);

  Timer1.initialize(10000000);
  Timer1.attachInterrupt(send_data);
  //Fuzzy initialization
  input_FLC_temp_init();
  output_FLC_temp_init();
  input_FLC_humid_init();
  output_FLC_hum_init();
  output_FLC_dehum_init();
  input_FLC_soil_init();
  output_FLC_soil_init();
  input_FLC_light_init();
  output_FLC_light_init();
  output_FLC_shading_init();

  if (EEPROM.read(0)) {
    is_system_set = true;
    Serial.println("System is already set");
  }
  else {
    is_system_set = false;
    Serial.println("System is not set");
  }
}


/***********************************INFINITE LOOP******************************************************/

void loop()
{

  if (!is_system_set) {
    Timer1.detachInterrupt();
    radio.startListening();
    Serial.println("listening");
    wireless_setting = false;

    while (!wireless_setting) {

      if (radio.available()) {
        char var[32] = "";
        radio.read(&var, sizeof(var));
        Serial.println(var);

        if (strcmp(var, "t") == 0) {
          Serial.println("temp ok");
          byte temp_reading = false;

          while (!temp_reading) {
            if (radio.available()) {
              int var1 = 0 ;
              radio.read(&var1, sizeof(var1));
              temp_sp = var1;
              Serial.println(temp_sp);
              receive_sp_count += 1;
              temp_reading = true;
            }
          }

        }

        else if (strcmp(var, "h") == 0) {
          Serial.println("humid ok");
          byte humid_reading = false;

          while (!humid_reading) {
            if (radio.available()) {
              int var1 = 0 ;
              radio.read(&var1, sizeof(var1));
              humid_sp = var1;
              Serial.println(humid_sp);
              receive_sp_count += 1;
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
              soil_sp = var1;
              Serial.println(soil_sp);
              receive_sp_count += 1;
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
              light_sp = var1;
              Serial.println(light_sp);
              receive_sp_count += 1;
              light_reading = true;
            }
          }

        }
        else;

      }
      if (receive_sp_count == 4) {
        //EEPROM.write(0, 1); //already setting
        wireless_setting = true;
        is_system_set = true;
        receive_sp_count = 0;
      }
    }
  }
  else {
    Serial.println("listening with Interrupts");
    wireless_setting = true;
    Timer1.attachInterrupt(send_data);
    radio.startListening();

    while (wireless_setting) {
      float adc_temp = analogRead(THERMISTOR);
      moisture = analogRead(SOIL);
      temp_m = temp_calculate(adc_temp);
      lux = lightMeter.readLightLevel();
      humid_m = dht.readHumidity();

      // Check if any reads failed and exit early (to try again).
      if (isnan(humid_m)) {
        Serial.println(F("Failed to read from DHT sensor!"));
        return;
      }

      if (radio.available())
      {
        noInterrupts();
        char var[32] = "";
        radio.read(&var, sizeof(var));
        if (strcmp(var, "off") == 0) {
          radio.stopListening();
          delay(500);
          char ack[32] = "ok";
          radio.write(&ack, sizeof(ack));
          wireless_setting = false;
          is_system_set = false;
          Serial.println("stop controlling");
          interrupts();
        }
      }

      if (temp_m > temp_sp) {
        float error = temp_m - temp_sp;
        Serial.println(FLC_temp(error));
        Serial.println("activated FLC temp");
      }
      if (humid_m > humid_sp) {
        float error = humid_m - humid_sp;
        Serial.println(FLC_temp(error));
        Serial.println("activated FLC humid");
      }
      if (lux > light_sp) {
        float error = lux - light_sp;
        Serial.println(FLC_temp(error));
        Serial.println("activated FLC light");
      }
      if (moisture > soil_sp) {
        float error = moisture - soil_sp;
        Serial.println(FLC_temp(error));
        Serial.println("activated FLC soil");
      }
    }
  }
}

/***************************APPLICATION FUNCTIONS************************************************/

float temp_calculate(float adc_value) {
  float R_thermistor, E_out, c_temp, temperature, ln3, ln;

  E_out = adc_value * 5 / 1023; //output voltage
  R_thermistor = (E_out * R_REF) / (V_REF - E_out); //determine thermistor resistance
  ln = log(R_thermistor);
  ln3 = pow(ln, 3);
  temperature = 1 / (b0 + b1 * ln + b3 * ln3);
  c_temp = temperature - 273.15f;

  return c_temp;
}

void send_data(void) {
  radio.stopListening();
  Serial.println("abc");
  radio.write(&temp, sizeof(temp)); // send specifier
  delay(400);
  radio.write(&temp_m, sizeof(temp_m)); //send measured data
  Serial.println(temp_m);
  delay(400);

  radio.write(&humid, sizeof(humid));
  delay(400);
  radio.write(&humid_m, sizeof(humid_m));
  Serial.println(humid_m);
  delay(400);

  radio.write(&soil, sizeof(soil));
  delay(400);
  radio.write(&moisture, sizeof(moisture));
  Serial.println(moisture);
  delay(400);

  radio.write(&light, sizeof(light));
  delay(400);
  radio.write(&lux, sizeof(lux));
  Serial.println(lux);
  delay(400);

  radio.startListening();
}

/********************temperature CONTROLLER***************************/

void input_FLC_temp_init(void) {
  i_FLC_temp_NL.a1 = -3;
  i_FLC_temp_NL.a2 = -2;

  i_FLC_temp_NS.a1 = -3;
  i_FLC_temp_NS.a1_1 = -2;
  i_FLC_temp_NS.a2_1 = -1;
  i_FLC_temp_NS.a2 = -0;

  i_FLC_temp_Z.a1 = -1;
  i_FLC_temp_Z.a2 = 1;
  i_FLC_temp_Z.a_m = 0;

  i_FLC_temp_PS.a1 = 0;
  i_FLC_temp_PS.a1_1 = 1;
  i_FLC_temp_PS.a2_1 = 2;
  i_FLC_temp_PS.a2 = 3;

  i_FLC_temp_PL.a1 = 2;
  i_FLC_temp_PL.a2 = 3;
}

void output_FLC_temp_init(void) {
  o_FLC_temp_VL.a1 = 20;
  o_FLC_temp_VL.a2 = 38;

  o_FLC_temp_L.a1 = 20;
  o_FLC_temp_L.a1_1 = 38;
  o_FLC_temp_L.a2_1 = 77;
  o_FLC_temp_L.a2 = 102;

  o_FLC_temp_M.a1 = 77;
  o_FLC_temp_M.a1_1 = 102;
  o_FLC_temp_M.a2_1 = 140;
  o_FLC_temp_M.a2 = 166;

  o_FLC_temp_H.a1 = 140;
  o_FLC_temp_H.a1_1 = 166;
  o_FLC_temp_H.a2_1 = 204;
  o_FLC_temp_H.a2 = 230;

  o_FLC_temp_VH.a1 = 204;
  o_FLC_temp_VH.a2 = 230;
}

float FLC_temp(float error) {

  float mf_deg[5];
  float new_out1 = 0, new_out2 = 0;
  float trash1, trash2;
  byte index[2] = {0, 0};
  byte extreme = false;

  mf_deg[0] = i_FLC_temp_NL.get_L_mf_value(i_FLC_temp_NL, error);
  mf_deg[1] = i_FLC_temp_NS.get_trap_mf_value(i_FLC_temp_NS, error);
  mf_deg[2] = i_FLC_temp_Z.get_tri_mf_value(i_FLC_temp_Z, error);
  mf_deg[3] = i_FLC_temp_PS.get_trap_mf_value(i_FLC_temp_PS, error);
  mf_deg[4] = i_FLC_temp_PL.get_half_T_mf_value(i_FLC_temp_PL, error);

  byte count = 0;
  for (byte j = 0; j < 5; j++) {

    if (mf_deg[j] != 0) {
      index[count] = j;
      count++;
    }
  }

  for (byte i = 0; i < 5; i++) {
    if (mf_deg[i] == 1) {
      extreme = true;
      break;
    }
  }

  if (!extreme) {
    if (mf_deg[index[0]] < mf_deg[index[1]]) {
      if (index[1] == 0) {
        o_FLC_temp_VH.get_output_half_T_mf_value(o_FLC_temp_VH, mf_deg[index[1]], new_out1);
        new_out2 = 255;
      }
      else if (index[1] == 1) {
        o_FLC_temp_H.get_output_trap_mf_values(o_FLC_temp_H, mf_deg[index[1]], new_out1, new_out2);
      }
      else if (index[1] == 2) {
        o_FLC_temp_M.get_output_trap_mf_values(o_FLC_temp_M, mf_deg[index[1]], new_out1, new_out2);
      }
      else if (index[1] == 3) {
        o_FLC_temp_L.get_output_trap_mf_values(o_FLC_temp_L, mf_deg[index[1]], new_out1, new_out2);
      }
      else if (index[1] == 4) {
        o_FLC_temp_VL.get_output_L_mf_value(o_FLC_temp_VL, mf_deg[index[1]], new_out1);
        new_out2 = 0;
      }
      else;
    }
    else if (mf_deg[index[0]] > mf_deg[index[1]]) {
      if (index[0] == 0) { //firing output VH
        o_FLC_temp_VH.get_output_half_T_mf_value(o_FLC_temp_VH, mf_deg[index[0]], new_out1);
        new_out2 = 255;
      }
      else if (index[0] == 1) {//H
        o_FLC_temp_H.get_output_trap_mf_values(o_FLC_temp_H, mf_deg[index[0]], new_out1, new_out2);
      }
      else if (index[0] == 2) {//M
        o_FLC_temp_M.get_output_trap_mf_values(o_FLC_temp_M, mf_deg[index[0]], new_out1, new_out2);
      }
      else if (index[0] == 3) {//L
        o_FLC_temp_L.get_output_trap_mf_values(o_FLC_temp_L, mf_deg[index[0]], new_out1, new_out2);
      }
      else if (index[0] == 4) {//VL
        o_FLC_temp_VL.get_output_L_mf_value(o_FLC_temp_VL, mf_deg[index[0]], new_out1);
        new_out2 = 0;
      }
      else;
    }
    else {
      if (index[0] == 0) { //firing output VH and H
        o_FLC_temp_H.get_output_trap_mf_values(o_FLC_temp_H, mf_deg[index[1]], new_out1, new_out2);
        new_out2 = 255;
      }
      else if (index[0] == 1) {//H and M
        o_FLC_temp_H.get_output_trap_mf_values(o_FLC_temp_H, mf_deg[index[0]], trash1, new_out2);
        o_FLC_temp_M.get_output_trap_mf_values(o_FLC_temp_M, mf_deg[index[1]], new_out1, trash2);
      }
      else if (index[0] == 2) {//M and L
        o_FLC_temp_M.get_output_trap_mf_values(o_FLC_temp_M, mf_deg[index[0]], trash1, new_out2);
        o_FLC_temp_L.get_output_trap_mf_values(o_FLC_temp_L, mf_deg[index[1]], new_out1, trash2);
      }
      else if (index[0] == 3) {//L and VL
        o_FLC_temp_L.get_output_trap_mf_values(o_FLC_temp_L, mf_deg[index[0]], new_out1, new_out2);
        new_out1 = 0;
      }
      else;
    }
  }
  else {
    if (index[0] == 0) { //firing output VH
      o_FLC_temp_VH.get_output_half_T_mf_value(o_FLC_temp_VH, mf_deg[index[0]], new_out1);
      new_out2 = 255;
    }
    else if (index[0] == 1) {//H
      o_FLC_temp_H.get_output_trap_mf_values(o_FLC_temp_H, mf_deg[index[0]], new_out1, new_out2);
    }
    else if (index[0] == 2) {//M
      o_FLC_temp_M.get_output_trap_mf_values(o_FLC_temp_M, mf_deg[index[0]], new_out1, new_out2);
    }
    else if (index[0] == 3) {//L
      o_FLC_temp_L.get_output_trap_mf_values(o_FLC_temp_L, mf_deg[index[0]], new_out1, new_out2);
    }
    else if (index[0] == 4) {//VL
      o_FLC_temp_VL.get_output_L_mf_value(o_FLC_temp_VL, mf_deg[index[0]], new_out1);
      new_out2 = 0;
    }
    else;
  }

  return round((new_out1 + new_out2) / 2);
}

/************************HUMIDITY CONTROLLER***********************/
void input_FLC_humid_init(void) {
  i_FLC_humid_NL.a1 = -20;
  i_FLC_humid_NL.a2 = -15;

  i_FLC_humid_NS.a1 = -20;
  i_FLC_humid_NS.a1_1 = -15;
  i_FLC_humid_NS.a2_1 = -5;
  i_FLC_humid_NS.a2 = 0;

  i_FLC_humid_Z.a1 = -5;
  i_FLC_humid_Z.a_m = 0;
  i_FLC_humid_Z.a2 = 5;

  i_FLC_humid_PS.a1 = 0;
  i_FLC_humid_PS.a1_1 = 5;
  i_FLC_humid_PS.a2_1 = 15;
  i_FLC_humid_PS.a2 = 20;

  i_FLC_humid_PL.a1 = 15;
  i_FLC_humid_PL.a2 = 20;
}

void output_FLC_hum_init(void) {
  o_FLC_hum_VL.a1 = 25;
  o_FLC_hum_VL.a2 = 51;

  o_FLC_hum_L.a1 = 25;
  o_FLC_hum_L.a1_1 = 51;
  o_FLC_hum_L.a2_1 = 77;
  o_FLC_hum_L.a2 = 102;

  o_FLC_hum_M.a1 = 77;
  o_FLC_hum_M.a1_1 = 102;
  o_FLC_hum_M.a2_1 = 153;
  o_FLC_hum_M.a2 = 179;

  o_FLC_hum_H.a1 = 153;
  o_FLC_hum_H.a1_1 = 179;
  o_FLC_hum_H.a2_1 = 204;
  o_FLC_hum_H.a2 = 230;

  o_FLC_hum_VH.a1 = 204;
  o_FLC_hum_VH.a2 = 230;
}

void output_FLC_dehum_init(void) {
  o_FLC_dehum_VL.a1 = 25;
  o_FLC_dehum_VL.a2 = 51;

  o_FLC_dehum_L.a1 = 25;
  o_FLC_dehum_L.a1_1 = 51;
  o_FLC_dehum_L.a2_1 = 77;
  o_FLC_dehum_L.a2 = 102;

  o_FLC_dehum_M.a1 = 77;
  o_FLC_dehum_M.a1_1 = 102;
  o_FLC_dehum_M.a2_1 = 153;
  o_FLC_dehum_M.a2 = 179;

  o_FLC_dehum_H.a1 = 153;
  o_FLC_dehum_H.a1_1 = 179;
  o_FLC_dehum_H.a2_1 = 204;
  o_FLC_dehum_H.a2 = 230;

  o_FLC_dehum_VH.a1 = 204;
  o_FLC_dehum_VH.a2 = 230;
}

float FLC_humidifier(float error) {
  float mf_deg[5];
  float new_out1 = 0, new_out2 = 0;
  float trash1, trash2;
  byte index[2] = {0, 0};
  byte extreme = false;

  mf_deg[0] = i_FLC_humid_NL.get_L_mf_value(i_FLC_humid_NL, error);
  mf_deg[1] = i_FLC_humid_NS.get_trap_mf_value(i_FLC_humid_NS, error);
  mf_deg[2] = i_FLC_humid_Z.get_tri_mf_value(i_FLC_humid_Z, error);
  mf_deg[3] = i_FLC_humid_PS.get_trap_mf_value(i_FLC_humid_PS, error);
  mf_deg[4] = i_FLC_humid_PL.get_half_T_mf_value(i_FLC_humid_PL, error);

  for (byte i = 0; i < 5; i++) {
    if (mf_deg[i] == 1) {
      extreme = true;
      break;
    }
  }

  byte count = 0;
  for (byte j = 0; j < 5; j++) {

    if (mf_deg[j] != 0) {
      index[count] = j;
      count++;
    }
  }

  if (!extreme) {
    if (mf_deg[index[0]] < mf_deg[index[1]]) {
      if (index[1] == 0) {
        o_FLC_hum_VL.get_output_L_mf_value(o_FLC_hum_VL, mf_deg[index[1]], new_out1);
        new_out2 = 0;
      }
      else if (index[1] == 1) {
        o_FLC_hum_L.get_output_trap_mf_values(o_FLC_hum_L, mf_deg[index[1]], new_out1, new_out2);
      }
      else if (index[1] == 2) {
        o_FLC_hum_M.get_output_trap_mf_values(o_FLC_hum_M, mf_deg[index[1]], new_out1, new_out2);
      }
      else if (index[1] == 3) {
        o_FLC_hum_H.get_output_trap_mf_values(o_FLC_hum_H, mf_deg[index[1]], new_out1, new_out2);
      }
      else if (index[1] == 4) {
        o_FLC_hum_VH.get_output_half_T_mf_value(o_FLC_hum_VH, mf_deg[index[1]], new_out1);
        new_out2 = 255;
      }
      else;
    }
    else if (mf_deg[index[0]] > mf_deg[index[1]]) {
      if (index[0] == 0) { //firing output VH
        o_FLC_hum_VL.get_output_L_mf_value(o_FLC_hum_VL, mf_deg[index[0]], new_out1);
        new_out2 = 0;
      }
      else if (index[0] == 1) {//H
        o_FLC_hum_L.get_output_trap_mf_values(o_FLC_hum_L, mf_deg[index[0]], new_out1, new_out2);
      }
      else if (index[0] == 2) {//M
        o_FLC_hum_M.get_output_trap_mf_values(o_FLC_hum_M, mf_deg[index[0]], new_out1, new_out2);
      }
      else if (index[0] == 3) {//L
        o_FLC_hum_H.get_output_trap_mf_values(o_FLC_hum_H, mf_deg[index[0]], new_out1, new_out2);
      }
      else if (index[0] == 4) {//VL
        o_FLC_hum_VH.get_output_half_T_mf_value(o_FLC_hum_VH, mf_deg[index[0]], new_out1);
        new_out2 = 255;
      }
      else;
    }
    else {//mf_degree equal to each other
      if (index[0] == 0) { //L and VL
        o_FLC_hum_L.get_output_trap_mf_values(o_FLC_hum_L, mf_deg[index[0]], new_out1, new_out2);
        new_out1 = 0;
      }
      else if (index[0] == 1) {//L and M
        o_FLC_temp_M.get_output_trap_mf_values(o_FLC_temp_M, mf_deg[index[0]], trash1, new_out2);
        o_FLC_temp_L.get_output_trap_mf_values(o_FLC_temp_L, mf_deg[index[1]], new_out1, trash2);
      }
      else if (index[0] == 2) {//M and L
        o_FLC_temp_H.get_output_trap_mf_values(o_FLC_temp_H, mf_deg[index[0]], trash1, new_out2);
        o_FLC_temp_M.get_output_trap_mf_values(o_FLC_temp_M, mf_deg[index[1]], new_out1, trash2);
      }
      else if (index[0] == 3) {
        o_FLC_temp_H.get_output_trap_mf_values(o_FLC_temp_H, mf_deg[index[1]], new_out1, new_out2);
        new_out2 = 255;
      }
      else;
    }
  }
  else { //extreme case
    if (index[0] == 0) {
      o_FLC_hum_VL.get_output_L_mf_value(o_FLC_hum_VL, mf_deg[index[0]], new_out1);
      new_out2 = 0;
    }
    else if (index[0] == 1) {
      o_FLC_hum_L.get_output_trap_mf_values(o_FLC_hum_L, mf_deg[index[0]], new_out1, new_out2);
    }
    else if (index[0] == 2) {
      o_FLC_hum_M.get_output_trap_mf_values(o_FLC_hum_M, mf_deg[index[0]], new_out1, new_out2);
    }
    else if (index[0] == 3) {
      o_FLC_hum_H.get_output_trap_mf_values(o_FLC_hum_H, mf_deg[index[0]], new_out1, new_out2);
    }
    else if (index[0] == 4) {
      o_FLC_hum_VH.get_output_half_T_mf_value(o_FLC_hum_VH, mf_deg[index[0]], new_out1);
      new_out2 = 255;
    }
    else;
  }

  return round((new_out1 + new_out2) / 2);
}

float FLC_dehumidifier(float error) {
  float mf_deg[5];
  float new_out1 = 0, new_out2 = 0;
  float trash1, trash2;
  byte index[2] = {0, 0};
  byte extreme = false;

  mf_deg[0] = i_FLC_humid_NL.get_L_mf_value(i_FLC_humid_NL, error);
  mf_deg[1] = i_FLC_humid_NS.get_trap_mf_value(i_FLC_humid_NS, error);
  mf_deg[2] = i_FLC_humid_Z.get_tri_mf_value(i_FLC_humid_Z, error);
  mf_deg[3] = i_FLC_humid_PS.get_trap_mf_value(i_FLC_humid_PS, error);
  mf_deg[4] = i_FLC_humid_PL.get_half_T_mf_value(i_FLC_humid_PL, error);

  for (byte i = 0; i < 5; i++) {
    if (mf_deg[i] == 1) {
      extreme = true;
      break;
    }
  }

  byte count = 0;
  for (byte j = 0; j < 5; j++) {

    if (mf_deg[j] != 0) {
      index[count] = j;
      count++;
    }
  }

  if (!extreme) {
    if (mf_deg[index[0]] < mf_deg[index[1]]) {
      if (index[1] == 0) {
        o_FLC_dehum_VH.get_output_half_T_mf_value(o_FLC_dehum_VH, mf_deg[index[1]], new_out1);
        new_out2 = 255;
      }
      else if (index[1] == 1) {
        o_FLC_dehum_H.get_output_trap_mf_values(o_FLC_dehum_H, mf_deg[index[1]], new_out1, new_out2);
      }
      else if (index[1] == 2) {
        o_FLC_dehum_M.get_output_trap_mf_values(o_FLC_dehum_M, mf_deg[index[1]], new_out1, new_out2);
      }
      else if (index[1] == 3) {
        o_FLC_dehum_L.get_output_trap_mf_values(o_FLC_dehum_L, mf_deg[index[1]], new_out1, new_out2);
      }
      else if (index[1] == 4) {
        o_FLC_dehum_VL.get_output_L_mf_value(o_FLC_dehum_VL, mf_deg[index[1]], new_out1);
        new_out2 = 0;
      }
      else;
    }
    else if (mf_deg[index[0]] > mf_deg[index[1]]) {
      if (index[0] == 0) { //firing output VH
        o_FLC_dehum_VH.get_output_half_T_mf_value(o_FLC_dehum_VH, mf_deg[index[0]], new_out1);
        new_out2 = 255;
      }
      else if (index[0] == 1) {//H
        o_FLC_dehum_H.get_output_trap_mf_values(o_FLC_dehum_H, mf_deg[index[0]], new_out1, new_out2);
      }
      else if (index[0] == 2) {//M
        o_FLC_dehum_M.get_output_trap_mf_values(o_FLC_dehum_M, mf_deg[index[0]], new_out1, new_out2);
      }
      else if (index[0] == 3) {//L
        o_FLC_dehum_L.get_output_trap_mf_values(o_FLC_dehum_L, mf_deg[index[0]], new_out1, new_out2);
      }
      else if (index[0] == 4) {//VL
        o_FLC_dehum_VL.get_output_L_mf_value(o_FLC_dehum_VL, mf_deg[index[0]], new_out1);
        new_out2 = 0;
      }
      else;
    }
    else {
      if (index[0] == 0) { //firing output VH and H
        o_FLC_dehum_H.get_output_trap_mf_values(o_FLC_dehum_H, mf_deg[index[1]], new_out1, new_out2);
        new_out2 = 255;
      }
      else if (index[0] == 1) {//H and M
        o_FLC_dehum_H.get_output_trap_mf_values(o_FLC_dehum_H, mf_deg[index[0]], trash1, new_out2);
        o_FLC_dehum_M.get_output_trap_mf_values(o_FLC_dehum_M, mf_deg[index[1]], new_out1, trash2);
      }
      else if (index[0] == 2) {//M and L
        o_FLC_dehum_M.get_output_trap_mf_values(o_FLC_dehum_M, mf_deg[index[0]], trash1, new_out2);
        o_FLC_dehum_L.get_output_trap_mf_values(o_FLC_dehum_L, mf_deg[index[1]], new_out1, trash2);
      }
      else if (index[0] == 3) {//L and VL
        o_FLC_dehum_L.get_output_trap_mf_values(o_FLC_dehum_L, mf_deg[index[0]], new_out1, new_out2);
        new_out1 = 0;
      }
      else;
    }
  }
  else {
    if (index[0] == 0) { //firing output VH
      o_FLC_dehum_VH.get_output_half_T_mf_value(o_FLC_dehum_VH, mf_deg[index[0]], new_out1);
      new_out2 = 255;
    }
    else if (index[0] == 1) {//H
      o_FLC_dehum_H.get_output_trap_mf_values(o_FLC_dehum_H, mf_deg[index[0]], new_out1, new_out2);
    }
    else if (index[0] == 2) {//M
      o_FLC_dehum_M.get_output_trap_mf_values(o_FLC_dehum_M, mf_deg[index[0]], new_out1, new_out2);
    }
    else if (index[0] == 3) {//L
      o_FLC_dehum_L.get_output_trap_mf_values(o_FLC_dehum_L, mf_deg[index[0]], new_out1, new_out2);
    }
    else if (index[0] == 4) {//VL
      o_FLC_dehum_VL.get_output_L_mf_value(o_FLC_dehum_VL, mf_deg[index[0]], new_out1);
      new_out2 = 0;
    }
    else;
  }

  return round((new_out1 + new_out2) / 2);
}

/**************************soil moisture**************************************/

void input_FLC_soil_init(void) {
  i_FLC_soil_NL.a1 = -150;
  i_FLC_soil_NL.a2 = -100;

  i_FLC_soil_NS.a1 = -150;
  i_FLC_soil_NS.a1_1 = -100;
  i_FLC_soil_NS.a2_1 = -50;
  i_FLC_soil_NS.a2 = 0;

  i_FLC_soil_Z.a1 = -50;
  i_FLC_soil_Z.a_m = 0;
  i_FLC_soil_Z.a2 = 50;

  i_FLC_soil_PS.a1 = 0;
  i_FLC_soil_PS.a1_1 = 50;
  i_FLC_soil_PS.a2_1 = 100;
  i_FLC_soil_PS.a2 = 150;

  i_FLC_soil_PL.a1 = 100;
  i_FLC_soil_PL.a2 = 150;
}

void output_FLC_soil_init(void) {
  o_FLC_soil_off.a1 = 0;
  o_FLC_soil_off.a_m = 0;
  o_FLC_soil_off.a2 = 25;

  o_FLC_soil_L.a1 = 0;
  o_FLC_soil_L.a1_1 = 25;
  o_FLC_soil_L.a2_1 = 64;
  o_FLC_soil_L.a2 = 89;

  o_FLC_soil_M.a1 = 64;
  o_FLC_soil_M.a1_1 = 89;
  o_FLC_soil_M.a2_1 = 127;
  o_FLC_soil_M.a2 = 153;

  o_FLC_soil_H.a1 = 127;
  o_FLC_soil_H.a1_1 = 153;
  o_FLC_soil_H.a2_1 = 191;
  o_FLC_soil_H.a2 = 217;

  o_FLC_soil_VH.a1 = 191;
  o_FLC_soil_VH.a2 = 217;
}

float FLC_soil(float error) {
  float mf_deg[5];
  float new_out1 = 0, new_out2 = 0;
  float trash1, trash2;
  byte index[2] = {0, 0};
  byte extreme = false;

  mf_deg[0] = i_FLC_soil_NL.get_L_mf_value(i_FLC_soil_NL, error);
  mf_deg[1] = i_FLC_soil_NS.get_trap_mf_value(i_FLC_soil_NS, error);
  mf_deg[2] = i_FLC_soil_Z.get_tri_mf_value(i_FLC_soil_Z, error);
  mf_deg[3] = i_FLC_soil_PS.get_trap_mf_value(i_FLC_soil_PS, error);
  mf_deg[4] = i_FLC_soil_PL.get_half_T_mf_value(i_FLC_soil_PL, error);

  for (byte i = 0; i < 5; i++) {
    if (mf_deg[i] == 1) {
      extreme = true;
      break;
    }
  }

  byte count = 0;
  for (byte j = 0; j < 5; j++) {

    if (mf_deg[j] != 0) {
      index[count] = j;
      count++;
    }
  }

  if (!extreme) {
    if (mf_deg[index[0]] < mf_deg[index[1]]) {
      if (index[1] == 0) {
        o_FLC_soil_VH.get_output_half_T_mf_value(o_FLC_soil_VH, mf_deg[index[1]], new_out1);
        new_out2 = 255;
      }
      else if (index[1] == 1) {
        o_FLC_soil_H.get_output_trap_mf_values(o_FLC_soil_H, mf_deg[index[1]], new_out1, new_out2);
      }
      else if (index[1] == 2) {
        o_FLC_soil_M.get_output_trap_mf_values(o_FLC_soil_M, mf_deg[index[1]], new_out1, new_out2);
      }
      else if (index[1] == 3) {
        o_FLC_soil_L.get_output_trap_mf_values(o_FLC_soil_L, mf_deg[index[1]], new_out1, new_out2);
      }
      else if (index[1] == 4) {
        new_out1 = 0;
        new_out2 = 0;
      }
      else;
    }
    else if (mf_deg[index[0]] > mf_deg[index[1]]) {
      if (index[0] == 0) { //firing output VH
        o_FLC_soil_VH.get_output_half_T_mf_value(o_FLC_soil_VH, mf_deg[index[0]], new_out1);
        new_out2 = 255;
      }
      else if (index[0] == 1) {//H
        o_FLC_soil_H.get_output_trap_mf_values(o_FLC_soil_H, mf_deg[index[0]], new_out1, new_out2);
      }
      else if (index[0] == 2) {//M
        o_FLC_soil_M.get_output_trap_mf_values(o_FLC_soil_M, mf_deg[index[0]], new_out1, new_out2);
      }
      else if (index[0] == 3) {//L
        o_FLC_soil_L.get_output_trap_mf_values(o_FLC_soil_L, mf_deg[index[0]], new_out1, new_out2);
      }
      else if (index[0] == 4) {//off
        new_out1 = 0;
        new_out2 = 0;
      }
      else;
    }
    else {
      if (index[0] == 0) { //firing output VH and H
        o_FLC_soil_H.get_output_trap_mf_values(o_FLC_soil_H, mf_deg[index[1]], new_out1, new_out2);
        new_out2 = 255;
      }
      else if (index[0] == 1) {//H and M
        o_FLC_soil_H.get_output_trap_mf_values(o_FLC_soil_H, mf_deg[index[0]], trash1, new_out2);
        o_FLC_soil_M.get_output_trap_mf_values(o_FLC_soil_M, mf_deg[index[1]], new_out1, trash2);
      }
      else if (index[0] == 2) {//M and L
        o_FLC_soil_M.get_output_trap_mf_values(o_FLC_soil_M, mf_deg[index[0]], trash1, new_out2);
        o_FLC_soil_L.get_output_trap_mf_values(o_FLC_soil_L, mf_deg[index[1]], new_out1, trash2);
      }
      else if (index[0] == 3) {//L and VL
        o_FLC_soil_L.get_output_trap_mf_values(o_FLC_soil_L, mf_deg[index[0]], new_out1, new_out2);
        new_out1 = 0;
      }
      else;
    }
  }
  else {
    if (index[0] == 0) { //firing output VH
      o_FLC_soil_VH.get_output_half_T_mf_value(o_FLC_soil_VH, mf_deg[index[0]], new_out1);
      new_out2 = 255;
    }
    else if (index[0] == 1) {//H
      o_FLC_soil_H.get_output_trap_mf_values(o_FLC_soil_H, mf_deg[index[0]], new_out1, new_out2);
    }
    else if (index[0] == 2) {//M
      o_FLC_soil_M.get_output_trap_mf_values(o_FLC_soil_M, mf_deg[index[0]], new_out1, new_out2);
    }
    else if (index[0] == 3) {//L
      o_FLC_soil_L.get_output_trap_mf_values(o_FLC_soil_L, mf_deg[index[0]], new_out1, new_out2);
    }
    else if (index[0] == 4) {//off
      new_out1 = 0;
      new_out2 = 0;
    }
    else;
  }

  return round((new_out1 + new_out2) / 2);
}


void input_FLC_light_init(void) {
  i_FLC_light_VL.a1 = 2500;
  i_FLC_light_VL.a2 = 5000;

  i_FLC_light_L.a1 = 4000;
  i_FLC_light_L.a_m = 6500;
  i_FLC_light_L.a2 = 9000;

  i_FLC_light_M.a1 = 8000;
  i_FLC_light_M.a_m = 10000;
  i_FLC_light_M.a2 = 12000;

  i_FLC_light_H.a1 = 11000;
  i_FLC_light_H.a_m = 13500;
  i_FLC_light_H.a2 = 16000;

  i_FLC_light_VH.a1 = 15000;
  i_FLC_light_VH.a2 = 17500;
}
void output_FLC_light_init(void) {
  o_FLC_light_off.a1 = 0;
  o_FLC_light_off.a_m = 0;
  o_FLC_light_off.a2 = 25;

  o_FLC_light_L.a1 = 0;
  o_FLC_light_L.a1_1 = 25;
  o_FLC_light_L.a2_1 = 64;
  o_FLC_light_L.a2 = 89;

  o_FLC_light_M.a1 = 64;
  o_FLC_light_M.a1_1 = 89;
  o_FLC_light_M.a2_1 = 127;
  o_FLC_light_M.a2 = 153;

  o_FLC_light_H.a1 = 127;
  o_FLC_light_H.a1_1 = 153;
  o_FLC_light_H.a2_1 = 191;
  o_FLC_light_H.a2 = 217;

  o_FLC_light_VH.a1 = 191;
  o_FLC_light_VH.a2 = 217;
}
void output_FLC_shading_init(void) {
  o_FLC_shading_opened.a1 = 65;
  o_FLC_shading_opened.a2 = 127;

  o_FLC_shading_half.a1 = 65;
  o_FLC_shading_half.a_m = 127;
  o_FLC_shading_half.a2 = 189;

  o_FLC_shading_closed.a1 = 127;
  o_FLC_shading_closed.a2 = 189;
}
float FLC_artificial_light(float error) {
  float mf_deg[5];
  float new_out1 = 0, new_out2 = 0;
  float trash1, trash2;
  byte index[2] = {0, 0};
  byte extreme = false;

  mf_deg[0] = i_FLC_light_VL.get_L_mf_value(i_FLC_light_VL, error);
  mf_deg[1] = i_FLC_light_L.get_tri_mf_value(i_FLC_light_L, error);
  mf_deg[2] = i_FLC_light_M.get_tri_mf_value(i_FLC_light_M, error);
  mf_deg[3] = i_FLC_light_H.get_tri_mf_value(i_FLC_light_H, error);
  mf_deg[4] = i_FLC_light_VH.get_half_T_mf_value(i_FLC_light_VH, error);

  for (byte i = 0; i < 5; i++) {
    if (mf_deg[i] == 1) {
      extreme = true;
      break;
    }
  }

  byte count = 0;
  for (byte j = 0; j < 5; j++) {

    if (mf_deg[j] != 0) {
      index[count] = j;
      count++;
    }
  }

  if (!extreme) {
    if (mf_deg[index[0]] < mf_deg[index[1]]) {
      if (index[1] == 0) {
        o_FLC_light_VH.get_output_half_T_mf_value(o_FLC_light_VH, mf_deg[index[1]], new_out1);
        new_out2 = 255;
      }
      else if (index[1] == 1) {
        o_FLC_light_H.get_output_trap_mf_values(o_FLC_light_H, mf_deg[index[1]], new_out1, new_out2);
      }
      else if (index[1] == 2) {
        o_FLC_light_M.get_output_trap_mf_values(o_FLC_light_M, mf_deg[index[1]], new_out1, new_out2);
      }
      else if (index[1] == 3) {
        o_FLC_light_L.get_output_trap_mf_values(o_FLC_light_L, mf_deg[index[1]], new_out1, new_out2);
      }
      else if (index[1] == 4) {
        new_out1 = 0;
        new_out2 = 0;
      }
      else;
    }
    else if (mf_deg[index[0]] > mf_deg[index[1]]) {
      if (index[0] == 0) { //firing output VH
        o_FLC_light_VH.get_output_half_T_mf_value(o_FLC_light_VH, mf_deg[index[0]], new_out1);
        new_out2 = 255;
      }
      else if (index[0] == 1) {//H
        o_FLC_light_H.get_output_trap_mf_values(o_FLC_light_H, mf_deg[index[0]], new_out1, new_out2);
      }
      else if (index[0] == 2) {//M
        o_FLC_light_M.get_output_trap_mf_values(o_FLC_light_M, mf_deg[index[0]], new_out1, new_out2);
      }
      else if (index[0] == 3) {//L
        o_FLC_light_L.get_output_trap_mf_values(o_FLC_light_L, mf_deg[index[0]], new_out1, new_out2);
      }
      else if (index[0] == 4) {//off
        new_out1 = 0;
        new_out2 = 0;
      }
      else;
    }
    else {
      if (index[0] == 0) { //firing output VH and H
        o_FLC_light_H.get_output_trap_mf_values(o_FLC_light_H, mf_deg[index[1]], new_out1, new_out2);
        new_out2 = 255;
      }
      else if (index[0] == 1) {//H and M
        o_FLC_light_H.get_output_trap_mf_values(o_FLC_light_H, mf_deg[index[0]], trash1, new_out2);
        o_FLC_light_M.get_output_trap_mf_values(o_FLC_light_M, mf_deg[index[1]], new_out1, trash2);
      }
      else if (index[0] == 2) {//M and L
        o_FLC_light_M.get_output_trap_mf_values(o_FLC_light_M, mf_deg[index[0]], trash1, new_out2);
        o_FLC_light_L.get_output_trap_mf_values(o_FLC_light_L, mf_deg[index[1]], new_out1, trash2);
      }
      else if (index[0] == 3) {//L and VL
        o_FLC_light_L.get_output_trap_mf_values(o_FLC_light_L, mf_deg[index[0]], new_out1, new_out2);
        new_out1 = 0;
      }
      else;
    }
  }
  else {
    if (index[0] == 0) { //firing output VH
      o_FLC_light_VH.get_output_half_T_mf_value(o_FLC_light_VH, mf_deg[index[0]], new_out1);
      new_out2 = 255;
    }
    else if (index[0] == 1) {//H
      o_FLC_light_H.get_output_trap_mf_values(o_FLC_light_H, mf_deg[index[0]], new_out1, new_out2);
    }
    else if (index[0] == 2) {//M
      o_FLC_light_M.get_output_trap_mf_values(o_FLC_light_M, mf_deg[index[0]], new_out1, new_out2);
    }
    else if (index[0] == 3) {//L
      o_FLC_light_L.get_output_trap_mf_values(o_FLC_light_L, mf_deg[index[0]], new_out1, new_out2);
    }
    else if (index[0] == 4) {//off
      new_out1 = 0;
      new_out2 = 0;
    }
    else;
  }

  return round((new_out1 + new_out2) / 2);
}
float FLC_shading_light(float error) {
  float mf_deg[5];
  float new_out1 = 0, new_out2 = 0;
  float trash1, trash2;
  byte index[2] = {0, 0};
  byte extreme = false;

  mf_deg[0] = i_FLC_light_VL.get_L_mf_value(i_FLC_light_VL, error);
  mf_deg[1] = i_FLC_light_L.get_tri_mf_value(i_FLC_light_L, error);
  mf_deg[2] = i_FLC_light_M.get_tri_mf_value(i_FLC_light_M, error);
  mf_deg[3] = i_FLC_light_H.get_tri_mf_value(i_FLC_light_H, error);
  mf_deg[4] = i_FLC_light_VH.get_half_T_mf_value(i_FLC_light_VH, error);

  for (byte i = 0; i < 5; i++) {
    if (mf_deg[i] == 1) {
      extreme = true;
      break;
    }
  }

  byte count = 0;
  for (byte j = 0; j < 5; j++) {

    if (mf_deg[j] != 0) {
      index[count] = j;
      count++;
    }
  }

  if (!extreme) {
    if (mf_deg[index[0]] < mf_deg[index[1]]) {
      if (index[1] == 0) {
        o_FLC_shading_opened.get_output_L_mf_value(o_FLC_shading_opened, mf_deg[index[1]], new_out1);
        new_out2 = 0;
      }
      else if (index[1] == 1) {
        o_FLC_shading_opened.get_output_L_mf_value(o_FLC_shading_opened, mf_deg[index[1]], new_out1);
        new_out2 = 0;
      }
      else if (index[1] == 2) {
        o_FLC_shading_half.get_output_tri_mf_values(o_FLC_shading_half, mf_deg[index[1]], new_out1, new_out2);
      }
      else if (index[1] == 3) {
        o_FLC_shading_closed.get_output_half_T_mf_value(o_FLC_shading_closed, mf_deg[index[1]], new_out1);
        new_out2 = 255;
      }
      else if (index[1] == 4) {
        o_FLC_shading_closed.get_output_half_T_mf_value(o_FLC_shading_closed, mf_deg[index[1]], new_out1);
        new_out2 = 255;
      }
      else;
    }
    else if (mf_deg[index[0]] > mf_deg[index[1]]) {
      if (index[0] == 0) {
        o_FLC_shading_opened.get_output_L_mf_value(o_FLC_shading_opened, mf_deg[index[0]], new_out1);
        new_out2 = 0;
      }
      else if (index[0] == 1) {
        o_FLC_shading_opened.get_output_L_mf_value(o_FLC_shading_opened, mf_deg[index[0]], new_out1);
        new_out2 = 0;
      }
      else if (index[0] == 2) {
        o_FLC_shading_half.get_output_tri_mf_values(o_FLC_shading_half, mf_deg[index[0]], new_out1, new_out2);
      }
      else if (index[0] == 3) {
        o_FLC_shading_closed.get_output_half_T_mf_value(o_FLC_shading_closed, mf_deg[index[0]], new_out1);
        new_out2 = 255;
      }
      else if (index[0] == 4) {
        o_FLC_shading_closed.get_output_half_T_mf_value(o_FLC_shading_closed, mf_deg[index[0]], new_out1);
        new_out2 = 255;
      }
      else;
    }
    else {
      if (index[0] == 0) { //firing output opened and half
        new_out1 = 0;
        o_FLC_shading_half.get_output_tri_mf_values(o_FLC_shading_half, mf_deg[index[1]], trash1, new_out2);
      }
      else if (index[0] == 1) {//firing output opened and half
        new_out1 = 0;
        o_FLC_shading_half.get_output_tri_mf_values(o_FLC_shading_half, mf_deg[index[1]], trash1, new_out2);
      }
      else if (index[0] == 2) {//M and L
        new_out2 = 255;
        o_FLC_shading_half.get_output_tri_mf_values(o_FLC_shading_half, mf_deg[index[1]], new_out1, trash2);
      }
      else;
    }
  }
  else {
    if (index[0] == 0) { //opened
      new_out1 = o_FLC_shading_opened.a1;
      new_out2 = 0;
    }
    else if (index[0] == 1) {//H
      new_out1 = o_FLC_shading_opened.a1;
      new_out2 = 0;
    }
    else if (index[0] == 2) {//M
      new_out1 = o_FLC_shading_half.a_m;
      new_out2 = 0;
    }
    else if (index[0] == 3) {//L
      new_out1 = o_FLC_shading_closed.a2;
      new_out2 = 255;
    }
    else if (index[0] == 4) {//off
      new_out1 = o_FLC_shading_closed.a2;
      new_out2 = 255;
    }
    else;
  }

  return round((new_out1 + new_out2) / 2);
}
