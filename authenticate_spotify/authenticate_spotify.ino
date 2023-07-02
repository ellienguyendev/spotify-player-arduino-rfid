#include <ArduinoJson.h> // needed for better memory management, check out https://arduinojson.org/
#include <ArduinoHttpClient.h>

#include "thingProperties.h"
#include "arduino_secrets.h"

const char SPOTIFY_CLIENT[] = SECRET_SPOTIFY_CLIENT;  // Client ID of your Spotify app
const char SPOTIFY_SECRET[] = SECRET_SPOTIFY_SECRET;  // Client secret of your Spotify app

WiFiServer server(80); // Server for Spotify authorization
WiFiSSLClient ssl; // Client for Spotify HTTPS requests
HttpClient authClient = HttpClient(ssl, "accounts.spotify.com", 443);
HttpClient apiClient = HttpClient(ssl, "api.spotify.com", 443);

String accessToken;
String refreshToken;
String ip_address;
bool authenticated = false;

// Returns the style HTML tag
const char* getStyle() {
  return "<style>html{height:100\%;display:grid;justify-content:center;align-content:center;"
         "background-color:#1ED760;font-size:60px;}</style>";
}

// Returns a simple HTML page
String getHTML(const char* message) {
  String html = "<!DOCTYPE html>\n";
  html += "<html><body>";
  html += getStyle();
  html += "<div>";
  html += message;
  html += "</div></body></html>";
  return html;
}

//######################################### AUTHORIZATION #######################################################
// Get the user authorization token
bool getAccessToken(String userCode) {
  String postData = "grant_type=authorization_code&code=" + userCode + "&redirect_uri="
                    "http://" + ip_address + "/redirect/";
  authClient.beginRequest();
  authClient.post("/api/token");
  authClient.sendHeader("Content-Type", "application/x-www-form-urlencoded");
  authClient.sendHeader("Content-Length", postData.length());
  authClient.sendBasicAuth(SPOTIFY_CLIENT, SPOTIFY_SECRET); // send the client id and secret for authentication
  authClient.beginBody();
  authClient.print(postData);
  authClient.endRequest();

  // If successful
  if (authClient.responseStatusCode() == 200) {
    DynamicJsonDocument json(512);
    deserializeJson(json, authClient.responseBody());
    accessToken = json["access_token"].as<String>();
    refreshToken = json["refresh_token"].as<String>();
    Serial.println("Refresh Token: " + refreshToken);
    return true;
  }
  return false;
}

//############################################ EVENTS ###########################################################
// Update IP address when connected
void onNetworkConnect() {
  IPAddress ip = WiFi.localIP();
  ip_address = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
  Serial.println("Please authenticate at " + ip_address);
}

//######################################### SETUP & LOOP ########################################################
void setup() {
  Serial.begin(9600);
  // Connect to Arduino IoT Cloud
  ArduinoCloud.begin(ArduinoIoTPreferredConnection, false); // disable watchdog restart

  // Add connection callback
  ArduinoIoTPreferredConnection.addCallback(NetworkConnectionEvent::CONNECTED, onNetworkConnect);

  // Begin the server
  server.begin();
}

void loop() {
  ArduinoCloud.update();

  if (!authenticated) {
    // Wait for an user to connect
    WiFiClient wifiClient = server.available();

    // If a user has connected
    if (wifiClient) {
      // Get the request
      String header = "";
      while (wifiClient.available()) {
        header += char(wifiClient.read());
      }

      // If authenticated, redirect
      if (header.indexOf("?code") >= 0) {
        // Parse the token
        String userCode = header.substring(header.indexOf("code=") + 5, header.indexOf("HTTP/1.1") - 1);
        authenticated = getAccessToken(userCode);
        if (authenticated) {
          wifiClient.print(getHTML("Authenticated!"));
        } else {
          wifiClient.print(getHTML("Authentication failed."));
        }
      } else if (header.indexOf("?error") >= 0) {
        wifiClient.print(getHTML("Cancelled."));
      }
      else {
        // Authenticate the user and get the code
        String webpage = "<!DOCTYPE html>\n";
        webpage += "<html><body>";
        webpage += getStyle();
        webpage += "<a href=\"https://accounts.spotify.com/authorize?client_id=";
        webpage += SPOTIFY_CLIENT;
        webpage += "&response_type=code&redirect_uri=http://";
        webpage += ip_address;
        webpage += "/redirect/&scope=user-read-playback-state "
                   "user-modify-playback-state\">Authenticate Spotify</a>\n";
        webpage += "</body></html>";
        wifiClient.print(webpage);
      }
      wifiClient.stop();
    }
  }
}