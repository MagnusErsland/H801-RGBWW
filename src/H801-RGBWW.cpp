#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <Ticker.h>

#define MQTT_VERSION MQTT_VERSION_3_1_1

//#define debug 1

// Wifi: SSID and password
const char*             WIFI_SSID         = "...";
const char*             WIFI_PASSWORD     = "...";

// WebUpdate
const char* host = "esp8266rgbww";
ESP8266WebServer server(80);
const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

// MQTT: ID, server IP, port, username and password
const PROGMEM char*     MQTT_CLIENT_ID    = "h801";
const PROGMEM char*     MQTT_SERVER_IP    = "192.168.1.32";
const PROGMEM uint16_t  MQTT_SERVER_PORT  = 1883;
const PROGMEM char*     MQTT_USER         = "...";
const PROGMEM char*     MQTT_PASSWORD     = "...";

// MQTT: topics
// state
const PROGMEM char*     MQTT_LIGHT_STATE_TOPIC                = "soverom/light";
const PROGMEM char*     MQTT_LIGHT_COMMAND_TOPIC              = "soverom/light/switch";

// brightness
const PROGMEM char*     MQTT_LIGHT_BRIGHTNESS_STATE_TOPIC     = "soverom/light/brightness";
const PROGMEM char*     MQTT_LIGHT_BRIGHTNESS_COMMAND_TOPIC   = "soverom/light/brightness/set";

// color temp
const PROGMEM char*     MQTT_LIGHT_COLOR_TEMP_STATE_TOPIC     = "soverom/light/colortemp";
const PROGMEM char*     MQTT_LIGHT_COLOR_TEMP_COMMAND_TOPIC   = "soverom/light/colortemp/set";

// brightness
const PROGMEM char*     MQTT_RGB_BRIGHTNESS_STATE_TOPIC       = "soverom/rgb1/brightness/status";
const PROGMEM char*     MQTT_RGB_BRIGHTNESS_COMMAND_TOPIC     = "soverom/rgb1/brightness/set";

// state
const PROGMEM char*     MQTT_RGB_STATE_TOPIC                  = "soverom/rgb1/light/status";
const PROGMEM char*     MQTT_RGB_COMMAND_TOPIC                = "soverom/rgb1/light/switch";

// colors (rgb)
const PROGMEM char*     MQTT_RGB_COLOR_STATE_TOPIC            = "soverom/rgb1/rgb/status";
const PROGMEM char*     MQTT_RGB_COLOR_COMMAND_TOPIC          = "soverom/rgb1/rgb/set";

// effect
const PROGMEM char*     MQTT_RGB_EFFECT_STATE_TOPIC           = "soverom/rgb1/effect/status";
const PROGMEM char*     MQTT_RGB_EFFECT_COMMAND_TOPIC         = "soverom/rgb1/effect/set";

// payloads by default (on/off)
const PROGMEM char*     LIGHT_ON          = "ON";
const PROGMEM char*     LIGHT_OFF         = "OFF";

// effect payloads
const PROGMEM char*     EFFECT_RGB          = "RGB";
const PROGMEM char*     EFFECT_Fade         = "FADE";

// variables used to store the statea and the brightness of the light
boolean    m_light_state       = false;
uint16_t   m_light_brightness  = 0;
uint16_t   m_light_colortemp   = 327;
uint16_t   m_w1_brightness     = 0;
uint16_t   m_w2_brightness     = 0;
uint16_t   m_w1_brightness_f   = 0;
uint16_t   m_w2_brightness_f   = 0;

boolean    m_rgb_state          = false;
uint16_t   m_rgb_brightness     = 1023;
uint8_t    m_rgb_red            = 0;
uint8_t    m_rgb_green          = 0;
uint8_t    m_rgb_blue           = 0;
uint16_t   m_red_brightness     = 0;
uint16_t   m_green_brightness   = 0;
uint16_t   m_blue_brightness    = 0;
uint16_t   m_red_brightness_f   = 0;
uint16_t   m_green_brightness_f = 0;
uint16_t   m_blue_brightness_f  = 0;

int redLevel = 0;
int greenLevel = 0;
int blueLevel = 0;

// pin used for the led (PWM)
const PROGMEM uint8_t RGB_LIGHT_RED_PIN =     15;
const PROGMEM uint8_t RGB_LIGHT_GREEN_PIN =   13;
const PROGMEM uint8_t RGB_LIGHT_BLUE_PIN =    12;
const PROGMEM uint8_t W1_PIN =                14;
const PROGMEM uint8_t W2_PIN =                4;

const PROGMEM uint8_t GREEN_PIN =     1;
const PROGMEM uint8_t RED_PIN =       5;

// buffer used to send/receive data with MQTT
const uint8_t MSG_BUFFER_SIZE = 20;
char m_msg_buffer[MSG_BUFFER_SIZE];

Ticker msTick;

WiFiClient wifiClient;
PubSubClient client(wifiClient);

uint8_t effectState = 2;
uint16_t fadeValue(uint16_t p_brightness_f, uint16_t p_brightness);
void rgbFade();
void brightnessFadeRGB();
void brightnessFadeWW();

float counter = 0;
float pi = 3.14159;

void setColor(uint8_t p_red, uint8_t p_green, uint8_t p_blue) {
  m_red_brightness = map(p_red, 0, 255, 0, m_rgb_brightness);
  m_green_brightness = map(p_green, 0, 255, 0, m_rgb_brightness);
  m_blue_brightness = map(p_blue, 0, 255, 0, m_rgb_brightness);
  //Serial1.println(map(p_red, 0, 255, 0, m_rgb_brightness));
  //Serial1.println(map(p_green, 0, 255, 0, m_rgb_brightness));
  //Serial1.println(map(p_blue, 0, 255, 0, m_rgb_brightness));
}

void effect() {
  if (effectState == 1) {
    brightnessFadeRGB();
  }
  else if (effectState == 2) {
    rgbFade();
  }
  brightnessFadeWW();
}

void rgbFade() {
  if (m_rgb_state) {
    counter = counter + 1;
    redLevel = sin(counter/100)*1000;
    greenLevel = sin(counter/100 + pi*2/3)*1000;
    blueLevel = sin(counter/100 + pi*4/3)*1000;
    redLevel = map(redLevel,-1000,1000,0,1023);
    greenLevel = map(greenLevel,-1000,1000,0,1023);
    blueLevel = map(blueLevel,-1000,1000,0,1023);
  }
  else {
    redLevel = 0;
    greenLevel = 0;
    blueLevel = 0;
  }
  analogWrite(RGB_LIGHT_RED_PIN,redLevel);
  analogWrite(RGB_LIGHT_GREEN_PIN,greenLevel);
  analogWrite(RGB_LIGHT_BLUE_PIN,blueLevel);
}

void brightnessFadeRGB() {
  m_red_brightness_f = fadeValue(m_red_brightness_f, m_red_brightness);
  m_green_brightness_f = fadeValue(m_green_brightness_f, m_green_brightness);
  m_blue_brightness_f = fadeValue(m_blue_brightness_f, m_blue_brightness);
  analogWrite(RGB_LIGHT_RED_PIN, m_red_brightness_f);
  analogWrite(RGB_LIGHT_GREEN_PIN, m_green_brightness_f);
  analogWrite(RGB_LIGHT_BLUE_PIN, m_blue_brightness_f);
}

void brightnessFadeWW() {
  m_w1_brightness_f = fadeValue(m_w1_brightness_f, m_w1_brightness);
  m_w2_brightness_f = fadeValue(m_w2_brightness_f, m_w2_brightness);;
  analogWrite(W1_PIN, m_w1_brightness_f);
  analogWrite(W2_PIN, m_w2_brightness_f);
}

uint16_t fadeValue(uint16_t p_brightness_f, uint16_t p_brightness) {
  if (p_brightness_f + 25 < p_brightness) {
    p_brightness_f = p_brightness_f + 25;
  }
  else if (p_brightness_f - 25 > p_brightness) {
    p_brightness_f = p_brightness_f - 25;
  }
  else {
    p_brightness_f = p_brightness;
  }
  return p_brightness_f;
}


// function called to adapt the brightness and the state of the led
void setLightState(uint16_t p_light_brightness) {
  if (m_light_colortemp < 327) {
    m_w1_brightness = p_light_brightness;
    m_w2_brightness = map(m_light_colortemp, 154, 327, 0, p_light_brightness);
#if debug
    {
      Serial1.println(p_light_brightness);
      Serial1.println(map(m_light_colortemp, 154, 327, 0, p_light_brightness));
    }
#endif
  } else if (m_light_colortemp > 327) {

      m_w1_brightness = map(m_light_colortemp, 500, 327, 0, p_light_brightness);
      m_w2_brightness = p_light_brightness;
#if debug
    {
      Serial1.println(map(m_light_colortemp, 500, 327, 0, p_light_brightness));
      Serial1.println(p_light_brightness);
    }
#endif
  }
  else {
      m_w1_brightness = p_light_brightness;
      m_w2_brightness = p_light_brightness;
#if debug
    {
      Serial1.println(p_light_brightness);
      Serial1.println(p_light_brightness);
    }
#endif
  }
}

//
//    Publish
//

// function called to publish the state of the led (on/off)
void publishLightState() {
  if (m_light_state) {
    client.publish(MQTT_LIGHT_STATE_TOPIC, LIGHT_ON, true);
  } else {
    client.publish(MQTT_LIGHT_STATE_TOPIC, LIGHT_OFF, true);
  }
}

// function called to publish the brightness of the led
void publishLightBrightness() {
  snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d", m_light_brightness);
  client.publish(MQTT_LIGHT_BRIGHTNESS_STATE_TOPIC, m_msg_buffer, true);
}

// function called to publish the color temp of the led
void publishLightColorTemp() {
  snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d", m_light_colortemp);
  client.publish(MQTT_LIGHT_COLOR_TEMP_STATE_TOPIC, m_msg_buffer, true);
}

// function called to publish the state of the led (on/off)
void publishRGBState() {
  if (m_rgb_state) {
    client.publish(MQTT_RGB_STATE_TOPIC, LIGHT_ON, true);
  } else {
    client.publish(MQTT_RGB_STATE_TOPIC, LIGHT_OFF, true);
  }
}

// function called to publish the brightness of the led (0-100)
void publishRGBBrightness() {
  snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d", m_rgb_brightness);
  client.publish(MQTT_RGB_BRIGHTNESS_STATE_TOPIC, m_msg_buffer, true);
}

// function called to publish the colors of the led (xx(x),xx(x),xx(x))
void publishRGBColor() {
  snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d,%d,%d", m_rgb_red, m_rgb_green, m_rgb_blue);
  client.publish(MQTT_RGB_COLOR_STATE_TOPIC, m_msg_buffer, true);
}

void publishEffectState() {
  if (effectState == 1) {
    client.publish(MQTT_RGB_EFFECT_STATE_TOPIC, EFFECT_RGB, true);
  } else if (effectState == 2) {
    client.publish(MQTT_RGB_EFFECT_STATE_TOPIC, EFFECT_Fade, true);
  }
}

//
//    function called when a MQTT message arrived
//
void callback(char* p_topic, byte* p_payload, unsigned int p_length) {
  // concat the payload into a string
  String payload;
  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }
  // handle message topic

  Serial1.println(p_topic);
  if (String(MQTT_LIGHT_COMMAND_TOPIC).equals(p_topic)) {
    // test if the payload is equal to "ON" or "OFF"
    if (payload.equals(String(LIGHT_ON))) {
      if (m_light_state != true) {
        m_light_state = true;
        setLightState(m_light_brightness);
        publishLightState();
      }
    } else if (payload.equals(String(LIGHT_OFF))) {
      if (m_light_state != false) {
        m_light_state = false;
        setLightState(0);
        publishLightState();
      }
    }
  } else if (String(MQTT_LIGHT_BRIGHTNESS_COMMAND_TOPIC).equals(p_topic)) {
    uint16_t brightness = payload.toInt();
    if (brightness < 0 || brightness > 1023) {
      // do nothing...
      return;
    } else {
      m_light_brightness = brightness;
      setLightState(m_light_brightness);
      publishLightColorTemp();
      publishLightBrightness();
    }
  } else if (String(MQTT_LIGHT_COLOR_TEMP_COMMAND_TOPIC).equals(p_topic)) {
    uint16_t colorTemp = payload.toInt();
    if (colorTemp < 154 || colorTemp > 500) {
      return;
    } else {
      m_light_colortemp = colorTemp;
      setLightState(m_light_brightness);
      publishLightColorTemp();
      publishLightBrightness();
    }
  } else if (String(MQTT_RGB_COMMAND_TOPIC).equals(p_topic)) {
    // test if the payload is equal to "ON" or "OFF"
    if (payload.equals(String(LIGHT_ON))) {
      if (m_rgb_state != true) {
        m_rgb_state = true;
        setColor(m_rgb_red, m_rgb_green, m_rgb_blue);
        publishRGBState();
      }
    } else if (payload.equals(String(LIGHT_OFF))) {
      if (m_rgb_state != false) {
        m_rgb_state = false;
        setColor(0, 0, 0);
        publishRGBState();
      }
    }
  } else if (String(MQTT_RGB_EFFECT_COMMAND_TOPIC).equals(p_topic)) {
    if (payload.equals(String(EFFECT_RGB))) {
        effectState = 1;
        publishEffectState();
    } else if (payload.equals(String(EFFECT_Fade))) {
        effectState = 2;
        publishEffectState();
    }
  } else if (String(MQTT_RGB_BRIGHTNESS_COMMAND_TOPIC).equals(p_topic)) {
    uint16_t brightness = payload.toInt();
    if (brightness < 0 || brightness > 1023) {
      // do nothing...
      return;
    } else {
      m_rgb_brightness = brightness;
      setColor(m_rgb_red, m_rgb_green, m_rgb_blue);
      publishRGBBrightness();
    }
  } else if (String(MQTT_RGB_COLOR_COMMAND_TOPIC).equals(p_topic)) {
    // get the position of the first and second commas
    uint8_t firstIndex = payload.indexOf(',');
    uint8_t lastIndex = payload.lastIndexOf(',');

    uint8_t rgb_red = payload.substring(0, firstIndex).toInt();
    if (rgb_red < 0 || rgb_red > 255) {
      return;
    } else {
      m_rgb_red = rgb_red;
    }

    uint8_t rgb_green = payload.substring(firstIndex + 1, lastIndex).toInt();
    if (rgb_green < 0 || rgb_green > 255) {
      return;
    } else {
      m_rgb_green = rgb_green;
    }

    uint8_t rgb_blue = payload.substring(lastIndex + 1).toInt();
    if (rgb_blue < 0 || rgb_blue > 255) {
      return;
    } else {
      m_rgb_blue = rgb_blue;
    }

    effectState = 1;
    publishEffectState();
    setColor(m_rgb_red, m_rgb_green, m_rgb_blue);
    publishRGBColor();
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial1.print("INFO: Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial1.println("\nINFO: connected");

      // Once connected, publish an announcement...
      // publish the initial values
      publishLightState();
      publishLightBrightness();
      publishLightColorTemp();

      publishRGBState();
      publishRGBBrightness();
      publishRGBColor();
      publishEffectState();

      // ... and resubscribe
      client.subscribe(MQTT_LIGHT_COMMAND_TOPIC);
      client.subscribe(MQTT_LIGHT_BRIGHTNESS_COMMAND_TOPIC);
      client.subscribe(MQTT_LIGHT_COLOR_TEMP_COMMAND_TOPIC);

      client.subscribe(MQTT_RGB_COMMAND_TOPIC);
      client.subscribe(MQTT_RGB_BRIGHTNESS_COMMAND_TOPIC);
      client.subscribe(MQTT_RGB_COLOR_COMMAND_TOPIC);
      client.subscribe(MQTT_RGB_EFFECT_COMMAND_TOPIC);
    } else {
      Serial1.print("ERROR: failed, rc=");
      Serial1.print(client.state());
      Serial1.println("DEBUG: try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(500);
    }
  }
}

void setup() {
  // init the Serial1
  Serial1.begin(115200);


  // init the led
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(W1_PIN, OUTPUT);
  pinMode(W2_PIN, OUTPUT);
  pinMode(RGB_LIGHT_BLUE_PIN, OUTPUT);
  pinMode(RGB_LIGHT_RED_PIN, OUTPUT);
  pinMode(RGB_LIGHT_GREEN_PIN, OUTPUT);
  setLightState(m_light_brightness);

  digitalWrite(RED_PIN, LOW);
  // init the WiFi connection
  Serial1.println();
  Serial1.println();
  WiFi.mode(WIFI_STA);
  Serial1.print("INFO: Connecting to ");
  Serial1.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial1.print(".");
  }

  if(WiFi.waitForConnectResult() == WL_CONNECTED){
    MDNS.begin(host);
    server.on("/", HTTP_GET, [](){
      server.sendHeader("Connection", "close");
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(200, "text/html", serverIndex);
    });
    server.on("/update", HTTP_POST, [](){
      server.sendHeader("Connection", "close");
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(200, "text/plain", (Update.hasError())?"FAIL":"OK");
      ESP.restart();
    },[](){
      HTTPUpload& upload = server.upload();
      if(upload.status == UPLOAD_FILE_START){
        Serial1.setDebugOutput(true);
        WiFiUDP::stopAll();
        Serial1.printf("Update: %s\n", upload.filename.c_str());
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if(!Update.begin(maxSketchSpace)){//start with max available size
          Update.printError(Serial1);
        }
      } else if(upload.status == UPLOAD_FILE_WRITE){
        if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
          Update.printError(Serial1);
        }
      } else if(upload.status == UPLOAD_FILE_END){
        if(Update.end(true)){ //true to set the size to the current progress
          Serial1.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
        Serial1.setDebugOutput(false);
      }
      yield();
    });
    server.begin();
    MDNS.addService("http", "tcp", 80);

    Serial1.printf("Ready! Open http://%s.local in your browser\n", host);

    digitalWrite(GREEN_PIN, HIGH);
  } else {
    Serial1.println("WiFi Failed");
  }

  Serial1.println("INFO: WiFi connected");
  Serial1.print("INFO: IP address: ");
  Serial1.println(WiFi.localIP());

  // init the MQTT connection
  client.setServer(MQTT_SERVER_IP, MQTT_SERVER_PORT);
  client.setCallback(callback);

  msTick.attach_ms(30, effect);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  server.handleClient();
  client.loop();
}
