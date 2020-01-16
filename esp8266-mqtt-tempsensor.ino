#define PIN_ALARM  2  //pin where locate Alarm input
#define PIN_SIGNAL 15 //pin where locate Signal LED
#define PIN_OWIRE  13 //pin where locate temperature sensor

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#define MQTT_KEEPALIVE 5

#include <timeouter.h>
timeouter waitPublish;

#include <OneWire.h>
#include <DallasTemperature.h>
OneWire oneWire(PIN_OWIRE);
DallasTemperature sensors(&oneWire);

#include "ext-cred.h"
// Update these with values suitable for your network.
const char* ssid     = ssid_ext;
const char* password = password_ext;
        int status   = WL_IDLE_STATUS;     // the Wifi radio's status

/*
const char* mqtt_server = "broker.hivemq.com";
const int   mqtt_port   = 1883;
const char* mqtt_user   = "";
const char* mqtt_passwd = "";
*/

const char* mqtt_server = "m10.cloudmqtt.com";
const int   mqtt_port   = 18993;
const char* mqtt_user   = mqtt_user_ext;
const char* mqtt_passwd = mqtt_passwd_ext;

void callback(char* topic, byte* payload, unsigned int length);

WiFiClient espClient;
PubSubClient client(mqtt_server, mqtt_port, callback, espClient);

    long lastMsg = 0;
    char msg[50];
     int value = 0;
    bool alarm_state = false;
uint32_t ms_button = 0;
    bool signalLEDstate = true;

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if (String(topic) == "tempsensor/led") {
    if ((char)payload[0] == '1') {
      digitalWrite(PIN_SIGNAL, HIGH);   // Turn the LED on (Note that LOW is the voltage level
      // but actually the LED is on; this is because
      // it is active low on the ESP-01)
    } else {
      digitalWrite(PIN_SIGNAL, LOW);  // Turn the LED off by making the voltage HIGH
    }
  }
  
  if (String(topic) == "tempsensor/reset") {
    // Switch on the LED if an 1 was received as first character
    if ((char)payload[0] == '1') {
      //ESP.restart();
      Serial.println("reset");
    }
  }
  if (String(topic) == "tempsensor/som") {
    // speed of measurment
    String value = "";
    for (int i = 0; i < length; i++) {
      value += (char)payload[i];
    }
    float som = value.toInt();
    waitPublish.setDelay((1/som)*120000);
  } 
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    //delay((MQTT_KEEPALIVE + 10)*1000);

    // задержка на
    int i = 0;
    while(i < (MQTT_KEEPALIVE + 10)){ // MQTT_KEEPALIVE + 10 секунд
      digitalWrite(PIN_SIGNAL, HIGH);
      delay(500);
      digitalWrite(PIN_SIGNAL, LOW);
      delay(500);
      i++;
    }
    
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_passwd, "tempsensor/status", 0, 1, "offline", true)) {
    //if (client.connect(clientId.c_str(), mqtt_user, mqtt_passwd)) {
      Serial.println("connected");
      digitalWrite(PIN_SIGNAL, HIGH); // Turn the LED on
      client.publish("tempsensor/status", "online", true);
      // Once connected, subscribe
      //client.subscribe("tempsensor/#");
      client.subscribe("tempsensor/som");
      client.subscribe("tempsensor/reset");
      client.subscribe("tempsensor/led");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  WiFi.mode(WIFI_STA);
  
  pinMode     (PIN_SIGNAL, OUTPUT);// Initialize the BUILTIN_LED pin as an output
  digitalWrite(PIN_SIGNAL, LOW);   // Turn the LED off 
  pinMode     (PIN_ALARM,  INPUT); // Initialize pin for boiler error readings
  pinMode     (A0,         INPUT); // Initialize ADC for voltage measurement
  
  Serial.begin(115200);
  
  waitPublish.setDelay(120000);
  waitPublish.start();
  randomSeed(micros());
}

void loop() {
  // attempt to connect to WiFi network
  while (WiFi.status() != WL_CONNECTED) {
    delay(10);
    Serial.print(F("Attempting to connect to WPA SSID: "));
    Serial.println(ssid);
    // Connect to WPA/WPA2 network
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      if (signalLEDstate) {
        digitalWrite(PIN_SIGNAL, LOW);
      }else{
        digitalWrite(PIN_SIGNAL, HIGH);
      }
      signalLEDstate = !signalLEDstate;
      Serial.print(".");
    }
    Serial.println(F("You're connected to the network"));
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());
  }

  // attempt to connect to MQTT broker
  if (!client.connected()) {
    digitalWrite(PIN_SIGNAL, LOW); // Turn the LED off
    reconnect();
  }
  client.loop();

  // work with all sensors and transmit data
  if (waitPublish.isOver()) {
    sensors.requestTemperatures(); // от датчика получаем значение температуры
    dtostrf(sensors.getTempCByIndex(0), 6, 2, msg);
    client.publish("tempsensor/temp",msg); // отправляем в топик для термодатчика значение температуры
    
    float volt = (analogRead(A0)/1023.0)*5;
    dtostrf(volt, 6, 2, msg);
    client.publish("tempsensor/supplyVolt",msg); // отправляем в топик значение напряжения
    if (volt < 3) {
      client.publish("tempsensor/supplyPres", "1");
    } else {
      client.publish("tempsensor/supplyPres", "0");
    }
    
    // Фиксируем наличие сигнала
    if(digitalRead(PIN_ALARM) == LOW && !alarm_state){
      alarm_state = true;
      client.publish("tempsensor/pushbutton", "1");
    }
    // Фиксируем отсутствие сигнала
    if(digitalRead(PIN_ALARM) == HIGH && alarm_state){
      alarm_state = false;     
      client.publish("tempsensor/pushbutton", "0");
    }

    waitPublish.start();
  }
 
}
