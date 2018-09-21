// USE ADAFRUIT 32U4 FOR THIS CODE

// To get rid of "TIMEOUT!" serial print when no card is present go to Adafruit_PN532.cpp
// In the waitready function comment out the following:
// PN532DEBUGPRINT.println("TIMEOUT!");

////////////////////////// RFID Wiring Configuration //////////////////////////

#include <Wire.h>
#include <Adafruit_PN532.h>
#include <SPI.h>

// If using the breakout or shield with I2C, define just the pins connected
// to the IRQ and reset lines.  Use the values below (2, 3) for the shield!
#define PN532_IRQ   (3)
#define PN532_RESET (4)  // Not connected by default on the NFC Shield

// If using the breakout with SPI, define the pins for SPI communication.
#define PN532_SCK  (15)
#define PN532_MOSI (16)
#define PN532_SS   (13)
#define PN532_MISO (14)  

// Or use this line for a breakout or shield with an I2C connection:
//Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

// Use this line for a breakout with a SPI connection:
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

//////////////////////////////////////////////////////////////////////////////



int lenHelper(float x) 
{
    if(x>=1000000000) return 13;
    if(x>=100000000) return 12;
    if(x>=10000000) return 11;
    if(x>=1000000) return 10;
    if(x>=100000) return 9;
    if(x>=10000) return 8;
    if(x>=1000) return 7;
    if(x>=100) return 6;
    if(x>=10) return 5;
    return 4;
}

void checkServerMsg(String check)
{
  if(Serial1.available()>0)
  {
    while(Serial1.available()>0)
    {
      String msg = Serial1.readStringUntil('\n');
      Serial.println(msg);
      if(msg == check)
      {
        break;
      }
      break;
     }
  }
}

// Sensor Variables
volatile unsigned int counter;      // Drip count
volatile unsigned long t = 0;       // Time of current drip
volatile float t_diff = 0;          // Time between current drip and previous
float x;                    // Total code elapsed time
unsigned short stop_time = 10000;   // Threshold time bewteen drops to determine when IV bag is finished
float vol = 0, prevvol = 0;         // Current and previous volume
float flowrate = 0;                 // Flowrate of IV line
float drop_factor;                  // IV line drop factor 
float timeCount = 0;
int BuffSizeData = 0;
int BuffSizeEnd = 0;

// RFID Variables
boolean success;                                  // Store whether scan was successful or not
uint8_t uid_patient[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned patient UID
uint8_t uid_ivbag[] = { 0, 0, 0, 0, 0, 0, 0 };    // Buffer to store the returned IV bag UID
uint8_t uidLength;                                // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

uint8_t patient_tag[] = { 192, 79, 109, 163, 0, 0, 0 };  // Expected patient tag
uint8_t ivbag_tag[] = { 22, 216, 219, 217, 0, 0, 0 };    // Expected IV bag tag
boolean patient_success = true;                          // State of whether or not correct tag was scanned for patient info
boolean ivbag_success = true;                            // State of whether or not correct tag was scanned for IV bag info

String patientName;

// Other Variables
volatile boolean print_state = true;              // Print state to only print the final volume once

void setup() {
  
  // PIN SETUP
  DDRD &= 0b11111110;  // Make sure PD0 is input for sensor (which is INT0 Pin 3)
  PORTD |= 0b00000001; // Turn on pullup resistor for PD0 (THIS MAY NOT BE NEEDED IF EXTERNAL PULLUP IS USED)
  DDRB |= 0b11100000;  // Make PB5,6,7 output for RGB LED. RED -> Pin 9, Green -> Pin 10, Blue -> Pin 11

  // EXTERNAL INTERRUPT
  EICRA = 0b00000010;  // Faliing edge of INT0 will generate interrupt request
  EIMSK = 0b00000001;  // Enable external pin interrupt for INT0
  // EIFR is ths flag register for external interrupts
  
  Serial.begin(115200);

////////////////////////// RFID SETUP //////////////////////////

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  
  // Set the max number of retry attempts to read from a card
  // This prevents us from waiting forever for a card, which is
  // the default behaviour of the PN532.
  nfc.setPassiveActivationRetries(0xFF);
  
  // configure board to read RFID tags
  nfc.SAMConfig();

    //////////////////////////////// Patient Information Setup ////////////////////////////////
  
  // Read patient tag
  Serial.println("Please Scan Patient ID");
  
  while(patient_success)
  {
     // Turn on blue LED while waiting
     PORTB |= (1 << PB7);
     success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid_patient[0], &uidLength);
     
     if (success & (memcmp(uid_patient, patient_tag, uidLength) == 0)){
      
      PORTB &= ~(1 << PB7); // Turn off blue LED
      PORTB |= (1 << PB6);  // Turn on green LED

      patientName = "Michael Knox";
      Serial.print("Patient Name: ");
      Serial.println(patientName);
      patient_success = 0;
      delay(2000);

      PORTB &= ~(1 << PB6); // Turn off green LED
     }
     else{
      
      PORTB &= ~(1 << PB7); // Turn off blue LED
      PORTB |= (1 << PB5);  // Turn on red LED
      
      Serial.println("Incorrect tag, please scan the correct one");
      delay(2000);
      PORTB &= ~(1 << PB5);  // Turn off red LED
     }
  }

  // Read the IV bag tag
  Serial.println("Please Scan IV Bag ID");

  
  while(ivbag_success)
  {
     // Turn on blue LED while waiting
     PORTB |= (1 << PB7);
     
     success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid_ivbag[0], &uidLength);
     
     if (success & (memcmp(uid_ivbag, ivbag_tag, uidLength) == 0)){

      PORTB &= ~(1 << PB7); // Turn off blue LED
      PORTB |= (1 << PB6);  // Turn on green LED
      
      drop_factor  = 0.1;
      Serial.print("Using IV bag with a drop factor of ");
      Serial.println(drop_factor);
      ivbag_success = 0;
      delay(2000);

      PORTB &= ~(1 << PB6); // Turn off green LED
     }
     else{
      
      PORTB &= ~(1 << PB7); // Turn off blue LED
      PORTB |= (1 << PB5);  // Turn on red LED
      
      Serial.println("Incorrect tag, please scan the correct one");
      delay(2000);

      PORTB &= ~(1 << PB5);  // Turn off red LED
     }
  }
  ////////////////////////// WiFi SETUP ////////////////////////// 
  
  ///Initialize WiFi////
  Serial1.begin(9600);
  //Serial1.println(String("AT+RST"));
  //Serial1.println(String("AT+CWMODE=1"));
  //Serial1.println(String("AT+CWJAP=\"Abraham Linksys\",\"dziedzic"));
  delay(1000);
  Serial1.println(String("AT+CIPSTART=\"TCP\",\"192.168.1.108\",8888"));
  delay(1000);
  checkServerMsg("Pass?\n");
  delay(1000);
  Serial1.println("AT+CIPSEND=" + String("3"));
  delay(1000);
  Serial1.print("abc");
  delay(1000);
  checkServerMsg("Name?\n");
  delay(1000);
  String patientNameLen = String(patientName.length());
  Serial1.println("AT+CIPSEND=" + String(patientNameLen));
  delay(1000);
  Serial1.print(patientName);  

  // Purple light to demonstrate DATA mode
  PORTB |= (1 << PB5);  // Turn on red LED
  PORTB |= (1 << PB7);  // Turn on blue LED
}

void loop() {
  // Calculated volume
  vol = drop_factor * counter;

  // Calculated flowrate
  flowrate = (drop_factor / (t_diff/1000)) * 60;

  /////WiFi Buffer Setup/////
  BuffSizeEnd = 24+lenHelper(vol);  

  // Print results
  checkServerMsg("OK\n");
  if (vol == 0.1)
  {
    x = ((float)millis()/60000.0);
  }
  if (prevvol != vol)
  {
    timeCount = ((float)millis()/60000.0) - x;
    Serial.print(timeCount,6);
    Serial.print(",");
    Serial.print(vol, 2);
    Serial.print(",");
    Serial.println(flowrate, 2);

    BuffSizeData = lenHelper(timeCount)+ 4 + lenHelper(vol) + lenHelper(flowrate) + 2;
    Serial1.println("AT+CIPSEND=" + String(BuffSizeData));
    delay(500);
    Serial1.print(timeCount,6);
    Serial1.print(",");
    Serial1.print(vol,2);
    Serial1.print(",");
    Serial1.print(flowrate,2); 
   }
  // Update previous volume value
  prevvol = vol;
}


// Interrupt functions
ISR(INT0_vect) {
  
  if (millis() - t > 90){   // Debouncing
    t_diff = millis() - t;  // Time differnece from previous drip
    counter++;              // Increment drip counter
    t = millis();           // Time of current drip
  }

 print_state = true;        // Allow final volume printing again
}

