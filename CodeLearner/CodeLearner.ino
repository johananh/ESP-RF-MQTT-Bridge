// rf_capture_unique_csv.ino
// This sketch captures unique 433MHz RF signals, assigns a button ID,
// and prints the decoded values to the Serial Monitor in CSV format.

#include "RCSwitchStretch.h"    // Include the  RC_SWITCH tweaked library

// Create an instance of the RCSwitch class
RCSwitch mySwitch = RCSwitch();

// Define the GPIO pin connected to the data pin of the 433MHz RF receiver.
const int RF_RECEIVE_PIN = 16; 

// --- Variables for storing unique codes ---
const int MAX_UNIQUE_CODES = 20; // Set a max number of unique buttons to learn
unsigned long capturedCodes[MAX_UNIQUE_CODES];
int uniqueCodeCount = 0; // Counter for unique codes found

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial) {
    // Wait for the serial port to connect
  }
  Serial.println("\n");
  Serial.println("ESP32 RF Receiver - Unique Code CSV Capture");
  Serial.println("-------------------------------------------------");
  Serial.println("Press buttons on your remote to learn their unique codes.");
  
  // Print the CSV header for logging
  Serial.println("ButtonID,Value,Bitlength,Protocol,Delay(us)");

  // Enable receiving on the specified GPIO pin
  mySwitch.enableReceive(RF_RECEIVE_PIN); 
}

void loop() {

  if (mySwitch.available()) {
    unsigned long receivedValue = mySwitch.getReceivedValue();
    bool isDuplicate = false;
  
    // Step 1: Check if the received code has already been captured
    for (int i = 0; i < uniqueCodeCount; i++) {
      if (capturedCodes[i] == receivedValue) {
        isDuplicate = true;
        break; // Found a duplicate, exit the loop
      }
    }

    // Step 2: If it's a new code and we have space, log it
    if (!isDuplicate && uniqueCodeCount < MAX_UNIQUE_CODES) {
      
      // Print the new entry with a unique ButtonID (1-based)
      Serial.print(uniqueCodeCount + 1);
      Serial.print(",");
      Serial.print(receivedValue);
      Serial.print(",");
      Serial.print(mySwitch.getReceivedBitlength());
      Serial.print(",");
      Serial.print(mySwitch.getReceivedProtocol());
      Serial.print(",");
      Serial.println(mySwitch.getReceivedDelay());
      
      // Store the new code
      capturedCodes[uniqueCodeCount] = receivedValue;
      // Increment the counter for the next unique button
      uniqueCodeCount++;
    }
    
    // Reset the receiver to be ready for the next signal
    mySwitch.resetAvailable();
  }
}
