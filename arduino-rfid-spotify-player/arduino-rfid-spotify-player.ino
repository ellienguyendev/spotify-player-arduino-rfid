#include <ArduinoJson.h>  // needed for better memory management, check out https://arduinojson.org/
#include <ArduinoHttpClient.h>
#include <SPI.h>
#include <MFRC522.h>

#include "thingProperties.h"
#include "arduino_secrets.h"
#include "album_info.h"

#define SS_PIN 7   // Set the SS_PIN to the appropriate Arduino pin connected to the RC522 reader
#define RST_PIN 6  // Set the RST_PIN to the appropriate Arduino pin connected to the RC522 reader

const char SPOTIFY_CLIENT[] = SECRET_SPOTIFY_CLIENT;  // Client ID of your Spotify app
const char SPOTIFY_SECRET[] = SECRET_SPOTIFY_SECRET;  // Client secret of your Spotify app
String REFRESH_TOKEN = SECRET_REFRESH_TOKEN;          // Refresh token to obtain new access tokens
const unsigned long TOKEN_REFRESH_RATE = 2700000;     // Every 45 minutes

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Send SS & RST pins

WiFiSSLClient ssl;  // Client for Spotify HTTPS requests
HttpClient authClient = HttpClient(ssl, "accounts.spotify.com", 443);
HttpClient apiClient = HttpClient(ssl, "api.spotify.com", 443);

String accessToken;
unsigned long lastRefreshTime = millis() - TOKEN_REFRESH_RATE;  // Variable to keep track of last refresh token time


// ################## UTILITY ##################
// Creates a filter to only get required parameters
StaticJsonDocument<200> getFilter() {
  StaticJsonDocument<200> filter;

  // Set required data to true
  filter["device"]["volume_percent"] = true;
  return filter;
}

// Gets link to album tapped
String getAlbumURI(String cardUID) {
  int index = -1;

  for (int i = 0; i < sizeof(albumUIDs) / sizeof(albumUIDs[0]); i++) {
    if (albumUIDs[i] == cardUID) {
      index = i;
      break;
    }
  }

  if (index != -1) {
    String uri = albumURIs[index];
    Serial.println("Sending album URI: " + uri);
    return uri;
  }
}

// Gets action of controller card tapped
String getControlAction(String cardUID) {
  int index = -1;

  for (int i = 0; i < sizeof(controllerUIDs) / sizeof(controllerUIDs[0]); i++) {
    if (controllerUIDs[i] == cardUID) {
      index = i;
      break;
    }
  }

  if (index != -1) {
    String controlAction = controlActions[index];
    Serial.println("Control: " + controlAction);
    return controlAction;
  } else {
    return "false";
  }
}

// ################## AUTHORIZATION ##################
// Refresh the user authentication token
bool refreshAccessToken() {
  String postData = "grant_type=refresh_token&refresh_token=" + REFRESH_TOKEN;
  authClient.beginRequest();
  authClient.post("/api/token");
  authClient.sendHeader("Content-Type", "application/x-www-form-urlencoded");
  authClient.sendHeader("Content-Length", postData.length());
  authClient.sendBasicAuth(SPOTIFY_CLIENT, SPOTIFY_SECRET);  // send the client id and secret for authentication
  authClient.beginBody();
  authClient.print(postData);
  authClient.endRequest();

  // If successful
  if (authClient.responseStatusCode() == 200) {
    lastRefreshTime = millis();
    DynamicJsonDocument json(256);
    deserializeJson(json, authClient.responseBody());
    accessToken = json["access_token"].as<String>();
    Serial.println("Refresh Access Token Success");
    return true;
  } else {
    Serial.println("Refresh Access Token Failed");
    return false;
  }
}

/// ################## API REQUESTS ##################
// Send an album to play using the Spotify API
void sendToSpotify(String contextURI) {
  String requestData = "{ \"context_uri\": \"" + String(contextURI) + "\" }";

  apiClient.beginRequest();
  apiClient.put("/v1/me/player/play");
  apiClient.sendHeader("Content-Type", "application/json");
  apiClient.sendHeader("Authorization", "Bearer " + accessToken);
  apiClient.sendHeader("Content-Length", requestData.length());
  apiClient.beginBody();
  apiClient.print(requestData);
  apiClient.endRequest();

  int statusCode = apiClient.responseStatusCode();
  String statusMsg = apiClient.responseBody();

  // Print the status code
  Serial.print("Status Code: ");
  Serial.println(statusCode + statusMsg);


  // Check if the request was successful
  if (statusCode == 204) {
    Serial.println("Album sent to play successfully");
  } else {
    Serial.println("Failed to send album to play");
  }
}

// Skip a song towards a given direction
void controlSpotifyPlayer(String controlAction) {
  if (controlAction == "play" || controlAction == "pause") {
    playPause(controlAction);
  } else if (controlAction == "previous" || controlAction == "next") {
    skipSong(controlAction);
  } else if (controlAction == "shuffle on" || controlAction == "shuffle off") {
    shuffle(controlAction);
  } else if (controlAction == "volume up" || controlAction == "volume down") {
    setVolume(controlAction);
  }
}

// Toggle play/pause
void playPause(String controlAction) {
  apiClient.beginRequest();
  apiClient.put("/v1/me/player/" + controlAction);
  apiClient.sendHeader("Content-Length", 0);
  apiClient.sendHeader("Authorization", "Bearer " + accessToken);
  apiClient.endRequest();
}

// Skip a song towards a given direction
void skipSong(String controlAction) {
  apiClient.beginRequest();
  apiClient.post("/v1/me/player/" + controlAction);
  apiClient.sendHeader("Content-Length", 0);
  apiClient.sendHeader("Authorization", "Bearer " + accessToken);
  apiClient.endRequest();
}

// Toggle shuffle on/off
void shuffle(String controlAction) {
  String shuffle = controlAction == "shuffle on" ? "true" : "false";

  apiClient.beginRequest();
  apiClient.put("/v1/me/player/shuffle?state=" + shuffle);
  apiClient.sendHeader("Content-Length", 0);
  apiClient.sendHeader("Authorization", "Bearer " + accessToken);
  apiClient.endRequest();
}

// Get the current volume
int getVolume() {
  apiClient.beginRequest();
  apiClient.get("/v1/me/player");
  apiClient.sendHeader("Authorization", "Bearer " + accessToken);
  apiClient.endRequest();

  int statusCode = apiClient.responseStatusCode();
  // If successful and playing (we would've gotten status code 204 otherwise)
  if (statusCode == 200) {
    // Create a filter since the response is too large for our Arduino to handle
    StaticJsonDocument<200> filter = getFilter();
    DynamicJsonDocument json(4096);
    deserializeJson(json, apiClient.responseBody(), DeserializationOption::Filter(filter));

    int currentVolume = json["device"]["volume_percent"].as<int>();
    return currentVolume;
  }
}

// Set the player volume
void setVolume(String controlAction) {
  int currentVolume = getVolume();
  int newVolume = controlAction == "volume up" ? currentVolume + 5 : currentVolume - 5;

  apiClient.beginRequest();
  apiClient.put("/v1/me/player/volume?volume_percent=" + String(newVolume));
  apiClient.sendHeader("Content-Length", 0);
  apiClient.sendHeader("Authorization", "Bearer " + accessToken);
  apiClient.endRequest();
}

// ################## EVENTS ##################
// Update IP address when connected
void onNetworkConnect() {
  IPAddress ip = WiFi.localIP();
  String ip_address = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
  Serial.println("IP Address: " + ip_address);
}

// ################## ADRUINO SETUP & LOOP ##################
void setup() {
  Serial.begin(9600);

  // Connect to Arduino IoT Cloud
  ArduinoCloud.begin(ArduinoIoTPreferredConnection, false);  // disable watchdog restart
  ArduinoIoTPreferredConnection.addCallback(NetworkConnectionEvent::CONNECTED, onNetworkConnect);

  SPI.begin();         // Initialize SPI communication
  mfrc522.PCD_Init();  // Initialize the RC522 reader
}

void loop() {
  ArduinoCloud.update();

  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    // Get card UID when scanned
    String cardUID = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      cardUID += mfrc522.uid.uidByte[i] < 0x10 ? "0" : "";
      cardUID += mfrc522.uid.uidByte[i], HEX;
    }
    Serial.println("Card UID: " + cardUID);

    // Check to see if a new access token is needed
    if (millis() - lastRefreshTime >= TOKEN_REFRESH_RATE) {
      Serial.println("Obtaining new access token");
      refreshAccessToken();
    }

    // Check to see if card tapped is controller card
    String controlAction = getControlAction(cardUID);

    if (controlAction != "false") {
      // If controller card, control Spotify player
      controlSpotifyPlayer(controlAction);
    } else {
      // Else, send album to Spotify
      String uri = getAlbumURI(cardUID);
      sendToSpotify(uri);
    }

    // Halt PICC
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }
}