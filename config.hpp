#pragma once

// Base Topics
String deviceId;
String baseTopic; 

// #define ESP32C6 // Uncomment if using a ESP32C6 otherwise leave it commented out for a ESP32 Wroom
#ifdef ESP32C6
    // --- PIN DEFINITIONS (Seeed Studio XIAO ESP32-C6) ---
    // UART (Shared)
    #define RX_PIN      D7 
    #define TX_PIN      D6 
    #define TMC_SERIAL_PORT Serial0 

    // BLIND 1 PINS (Left)
    #define DIR1_PIN    D1
    #define STEP1_PIN   D0
    #define EN1_PIN     D2
    #define DIAG1_PIN   D3

    // BLIND 2 PINS (Right)
    #define DIR2_PIN    D9
    #define STEP2_PIN   D8
    #define EN2_PIN     D10
    #define DIAG2_PIN   D5
#else
    // --- PIN DEFINITIONS (ESP32-WROOM-DA or uPesy ESP32 Wroom Devkit) ---
    // UART (Shared TMC2209)
    #define RX_PIN      16
    #define TX_PIN      17
    #define TMC_SERIAL_PORT Serial2 

    // BLIND 1 PINS (Left)
    #define DIR1_PIN    15
    #define STEP1_PIN   4
    #define EN1_PIN     19
    #define DIAG1_PIN   21

    // BLIND 2 PINS (Right) (MS1 3.3V)
    #define DIR2_PIN    32
    #define STEP2_PIN   33
    #define EN2_PIN     13
    #define DIAG2_PIN   22
#endif

// --- MOTOR SETTINGS ---
#define R_SENSE      0.11f 
#define ACCELERATION  1000    

// --- PHYSICAL DIMENSIONS & CONFIG ---
#define HOMEING_TIMEOUT_SECS 240

// --- CONFIG VARIABLES (Saved to Preferences) ---
float cfg_lat = 42.4414;
float cfg_lon = -70.9692;
int   cfg_win_az = 230;    
float cfg_win_top = 2.02;
float cfg_user_dist = 1.14;
float cfg_eye_h = 1.4;
float cfg_gmt_offset = -5;
bool  cfg_auto_mode = false;

// Independent Motor Heights
float cfg_m1_max_meters = 1.8;
float cfg_m2_max_meters = 2.2;
float cfg_steps_per_cm = 1500.0f;

// Independent Motor Runtime Configs
int   cfg_motor_count = 2;
int   cfg_speed = 4600;
int   cfg_m1_current = 1000;
int   cfg_m2_current = 800;
int   cfg_m1_stall = 1;
int   cfg_m2_stall = 1;

// --- GLOBALS ---
TMC2209Stepper driver1(&TMC_SERIAL_PORT, R_SENSE, 0b00);
TMC2209Stepper driver2(&TMC_SERIAL_PORT, R_SENSE, 0b01);

AccelStepper stepper1(AccelStepper::DRIVER, STEP1_PIN, DIR1_PIN);
AccelStepper stepper2(AccelStepper::DRIVER, STEP2_PIN, DIR2_PIN);

AsyncWebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);
Preferences preferences;

bool homeRequested = false;
bool moveRequested = false;
long moveTarget = 0; // Target in steps (always based on M1's scale)
bool motorsEnabled = false;

void configSetup()
{
  preferences.begin("BlindNanny", false);
  cfg_lat = preferences.getFloat("lat", cfg_lat);
  cfg_lon = preferences.getFloat("lon", cfg_lon);
  cfg_win_az = preferences.getInt("az", cfg_win_az);
  cfg_win_top = preferences.getFloat("top", cfg_win_top);
  cfg_user_dist = preferences.getFloat("dist", cfg_user_dist);
  cfg_eye_h = preferences.getFloat("eye", cfg_eye_h);
  cfg_gmt_offset = preferences.getFloat("gmt", cfg_gmt_offset);
  cfg_auto_mode = preferences.getBool("auto", cfg_auto_mode);
  cfg_steps_per_cm = preferences.getFloat("spcm", cfg_steps_per_cm);
  
  // Load Independent Dimensions
  cfg_m1_max_meters = preferences.getFloat("m1_max", cfg_m1_max_meters);
  cfg_m2_max_meters = preferences.getFloat("m2_max", cfg_m2_max_meters);
  
  // Load Independent Motor parameters
  cfg_motor_count = preferences.getInt("cnt", cfg_motor_count);
  cfg_speed = preferences.getInt("speed", cfg_speed);
  cfg_m1_current = preferences.getInt("m1_curr", cfg_m1_current);
  cfg_m2_current = preferences.getInt("m2_curr", cfg_m2_current);
  cfg_m1_stall = preferences.getInt("m1_stall", cfg_m1_stall);
  cfg_m2_stall = preferences.getInt("m2_stall", cfg_m2_stall);

  configTime(cfg_gmt_offset * 3600, 0, "pool.ntp.org");
}
