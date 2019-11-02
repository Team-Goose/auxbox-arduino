//includes useful libraries
#include <ESP8266WiFi.h>
#include <Arduino_JSON.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiClientSecureBearSSL.h>

//declaration of variables
HTTPClient http;
ESP8266WebServer server(80);

bool set_up = false;
bool onWiFi = false;

IPAddress ip;

//gonna need some client stuff aren't we?
const String CLIENT_ID = "";
const String CLIENT_SEC = "";

int numNetworks = WiFi.scanNetworks();
String token = "";
String userID = "";
String playListID = "";


//declaration of functions
void handleRoot();
void handleNotFound();
void handleGetWiFi();
void handleSetWiFi();
void handleCallback();
void handleAddToPlaylist();
void handleGetPlaylist();

void setup() {
  Serial.begin(115200);
  Serial.println();

  //set up softAP on esp8266 (basically a LAN, no internet access)
  Serial.print("Setting up soft access point... ");
  boolean result = WiFi.softAP("AuxBox");
  if (result == true)
  {
    Serial.println("Ready");
    set_up = true;
  }
  else
  {
    Serial.println("Failed!");
  }

  //show how to handle different http requests
  server.on("/", HTTP_GET, handleRoot);
  server.on("/getwifi", HTTP_GET, handleGetWiFi);
  server.on("/setwifi", HTTP_POST, handleSetWiFi);
  server.on("/callback", HTTP_GET, handleCallback);
  server.on("/addtoplaylist", HTTP_POST, handleAddToPlaylist);
  server.on("/getplaylist", HTTP_GET, handleGetPlaylist);
  server.onNotFound(handleNotFound);

  //start server and indicate useful stuff in serial monitor
  server.begin();
  ip = WiFi.softAPIP();
  Serial.println("HTTP server started!");
  Serial.print("Device IP: ");
  Serial.println(ip);
}

void loop() {
  //if in setup stage with softAP active
  if (set_up) {
    Serial.printf("Devices connected: %d\n", WiFi.softAPgetStationNum());
    server.handleClient();
    delay(3000);
  }
  //if past setup and actually connected to wifi
  else if(onWiFi) {
    server.handleClient();
  }
}

//basically just to make sure nothing breaks when the IP is accessed without a url
void handleRoot() {
  Serial.print("root");
  server.send(200, "text/plain", "root");   // Send HTTP status 200 (Ok) and send some text to the browser/client
}

//when the mobile app asks for a list of accessible wifi networks
void handleGetWiFi() {
  //point back to root
  server.sendHeader("Location", "/");

  numNetworks = WiFi.scanNetworks();
  String networks[numNetworks];
  int index = 0;
  bool in = false;

  //use some basic scanning and concatenation to create an easily-readable
  //json text to send to the mobile app
  if (numNetworks > 0) {
    String networksJson = "{\"wifi\": [";
    
    for (int i = 0; i < numNetworks; i++) {
      for(int j = 0; j < numNetworks; j++) {
        if(WiFi.SSID(i).equals(networks[j])){
          in = true;
        }
      }

      if(!in) {
        networksJson = networksJson + "{\"ssid\": \"" + WiFi.SSID(i) + "\", ";
        networksJson = networksJson + "\"rssi\": \"" + WiFi.RSSI(i) + "\"}";
        if (i != numNetworks - 1) {
          networksJson = networksJson + ",\n";
        }
        networks[index] = WiFi.SSID(i);
        index++;
      }
      if(i == numNetworks - 1) {
        networksJson = networksJson.substring(0, networksJson.length() - 2) + "]}";
      }

      in = false;
    }
    //send generated json to mobile app
    server.send(200, "text/plain", networksJson);
  }
  else {
    //inform app of lack of wifi (such a tragedy)
    server.send(200, "text/plain", "No WiFi networks available...");
  }
}

//when the mobile app tells the esp8266 which wifi to connect to
void handleSetWiFi() {
  if (server.hasArg("plain")== false){ //Check if body received
    server.send(200, "text/plain", "Body not received");
    return;
  }

  String ssid;
  String pass;
  int index;

  //receives body in form of ssid,password
  //parses body for index of comma^^
  for(int i = 0; i < server.arg("plain").length(); i++) {
    if(server.arg("plain").substring(i, i + 1).equals(",")){
      index = i;
      break;
    }
  }

  //uses found index to parse body for ssid and password
  ssid = server.arg("plain").substring(0, index);
  pass = server.arg("plain").substring(index + 1, server.arg("plain").length());

  WiFi.disconnect();

  IPAddress staticIP(192, 168, 43, 112); //ESP static ip
  IPAddress gateway(192, 168, 43, 46);   //IP Address of your WiFi Router (Gateway)
  IPAddress subnet(255, 255, 255, 0);  //Subnet mask
  IPAddress dns(8, 8, 8, 8);  //DNS

  WiFi.config(staticIP, subnet, gateway, dns);
  WiFi.mode(WIFI_STA);

  //if password is empty (unprotected network), only start with ssid
  if(pass.equals("")){
    WiFi.begin(ssid);
  }
  //if there is a password, start with ssid and pw
  else{
    WiFi.begin(ssid, pass);
  }

  //basically just informing the user of connection status in serial window
  Serial.print("Connecting to " + ssid + "'s WiFi...");
  while(WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.print(".");
  }
  ip = WiFi.localIP();
  Serial.println("\n");
  Serial.println("Successfully connected to " + ssid + ".");
  Serial.print("New IP Address:\t");
  Serial.println(ip);
  Serial.println("");

//  //sending new IP to mobile app while it's still connected to the softAP
//  server.send(200, "text/plain", ipToString(ip));
//  delay(500);

  server.send(200, "text/plain", "Connected.");

  //turning off softAP and exiting setup stage
  Serial.println("Shutting down Soft Access Point...");
  WiFi.softAPdisconnect(true);
  set_up = false;
  onWiFi = true;
  Serial.println("Successfully shut down SoftAP.");
}

void handleCallback() {
  if(server.hasArg("code") == false) {
    server.send(200, "text/plain", "Auth code not received");
    return;
  }

  String code = server.arg("code");
  const uint8_t fingerprint[20] = {0x40, 0xaf, 0x00, 0x6b, 0xec, 0x90, 0x22, 0x41, 0x8e, 0xa3, 0xad, 0xfa, 0x1a, 0xe8, 0x25, 0x41, 0x1d, 0x1a, 0x54, 0xb3};

  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);

  client->setFingerprint(fingerprint);
  client->setInsecure();
  
  http.begin(*client ,"https://accounts.spotify.com/api/token/");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  int httpcode = http.POST("grant_type=authorization_code&code=" + code + "&redirect_uri=http%3A%2F%2F192.168.43.112%2Fcallback&client_id=" + CLIENT_ID + "&client_secret=" + CLIENT_SEC);
  String payload = http.getString();
  JSONVar output = JSON.parse(payload);
  token = output["access_token"];
  //http.end();

  http.begin(*client ,"https://api.spotify.com/v1/me");
  http.addHeader("Authorization", "Bearer "+token);
  httpcode = http.GET();
  payload = http.getString();
  JSONVar user = JSON.parse(payload);
  userID = user["id"];
  
//  http.end();
  //check for or create playlist
  Serial.println("Checking AuxBox playlist status...");
  http.begin(*client ,"https://api.spotify.com/v1/users/"+userID+"/playlists");
  http.addHeader("Authorization","Bearer "+token);
  http.addHeader("Content-Type", "application/json");
  httpcode = http.POST("{\"name\": \"AuxBox\", \"public\": true }");
  payload = http.getString();
  server.send(200, "text/plain", code + " " + http.header("location"));
  JSONVar playlistData = JSON.parse(payload);
  playListID = playlistData["id"];

  http.end();
  Serial.println("AuxBox playlist created.");

//  http.begin(*client, "https://api.spotify.com/v1/me/player/play");
//  http.addHeader("Authorization", "Bearer " + token);
//  httpcode = http.PUT("{\"context_uri\": \"spotify:playlist:" + playListID + "\"}");
//  payload = http.getString();
//  Serial.println(payload);
//
//  http.end();
}

void handleAddToPlaylist() {
//  server.send(200, "text/plain", "Request received.");
//  Serial.println("Request received.");

  const uint8_t fingerprint[20] = {0x40, 0xaf, 0x00, 0x6b, 0xec, 0x90, 0x22, 0x41, 0x8e, 0xa3, 0xad, 0xfa, 0x1a, 0xe8, 0x25, 0x41, 0x1d, 0x1a, 0x54, 0xb3};

  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);

  client->setFingerprint(fingerprint);
  client->setInsecure();

  if (server.hasArg("plain")== false){ //Check if body received
    server.send(200, "text/plain", "Body not received");
    return;
  }

  String songID = server.arg("plain");

  http.begin(*client ,"https://api.spotify.com/v1/playlists/" + playListID + "/tracks");
  http.addHeader("Authorization","Bearer "+token);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Content-Length", "100");
  int httpcode = http.POST("{\"uris\": [\"spotify:track:" + songID + "\"]}");
  String payload = http.getString();
  Serial.println(songID);
  Serial.println(payload);

  http.end();
  server.send(200, "text/plain", "Received.");
}

void handleGetPlaylist() {
  const uint8_t fingerprint[20] = {0x40, 0xaf, 0x00, 0x6b, 0xec, 0x90, 0x22, 0x41, 0x8e, 0xa3, 0xad, 0xfa, 0x1a, 0xe8, 0x25, 0x41, 0x1d, 0x1a, 0x54, 0xb3};

  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);

  client->setFingerprint(fingerprint);
  client->setInsecure();
  http.begin(*client ,"https://api.spotify.com/v1/playlists/"+playListID+"/tracks?limit=100");
  http.addHeader("Authorization", "Bearer "+token);
  http.GET();
  String payload = http.getString();
  
  server.send(200, "text/plain", payload);
}

///////////////////////

void handleNotFound() {
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}