#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 7  // Set the SS_PIN to the appropriate Arduino pin connected to the RC522 reader
#define RST_PIN 6  // Set the RST_PIN to the appropriate Arduino pin connected to the RC522 reader

MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600); // Initialize the Serial Monitor
  SPI.begin();        // Initialize SPI communication
  mfrc522.PCD_Init(); // Initialize the RC522 reader
}

void loop() {
  // Check for new cards
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    // Print card UID
    String cardUID = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      cardUID += mfrc522.uid.uidByte[i] < 0x10 ? "0" : "";
      cardUID += mfrc522.uid.uidByte[i], HEX;
    }
    Serial.println(cardUID);

    // Halt PICC
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }
}
