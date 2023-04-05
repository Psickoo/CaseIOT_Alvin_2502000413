#include <Arduino.h>
#include <Ticker.h>
#include <DHTesp.h>
#include <BH1750.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define LED_RED 5
#define LED_GREEN 18
#define LED_YELLOW 19
#define DHT_PIN 15
#define LED_COUNT 3
  const uint8_t arLed[LED_COUNT] = {LED_YELLOW, LED_GREEN, LED_RED};


#define WIFI_SSID "Galaxy A70651B"
#define WIFI_PASSWORD "Password"
#define MQTT_BROKER "broker.emqx.io"
#define MQTT_TOPIC_PUBLISH "esp32_dor/data"
#define MQTT_TOPIC_SUBSCRIBE "esp32_dor/cmd"

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
Ticker timerPublish, ledOff, sendLux, sendTemp, sendHumidity;
DHTesp dht;
BH1750 lightMeter;

char g_szDeviceId[30];
void WifiConnect();
boolean mqttConnect();
void onPublishMessage();
void pubLux();
void pubTemp();
void pubHumidity();
bool turnOn = false;
float fHumidity = 0;
float fTemperature = 0;
float lux = 0;
char szTopic[50];
char szData[10];

void setup() {
  Serial.begin(9600);
  delay(100);
  pinMode(LED_BUILTIN, OUTPUT);
  for (uint8_t i=0; i<LED_COUNT; i++)
    pinMode(arLed[i], OUTPUT);
  Wire.begin();
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire);
  Serial.printf("Free Memory: %d\n", ESP.getFreeHeap());
  WifiConnect();
  dht.setup(DHT_PIN, DHTesp::DHT11);
  mqttConnect();
  timerPublish.attach_ms(2000, onPublishMessage);
  sendTemp.attach(7000, [](){ pubTemp(); });
  sendHumidity.attach(5000, [](){ pubHumidity(); });
  sendLux.attach(4000, [](){ pubLux(); });
}

void loop() {
  mqtt.loop();
}

void pubTemp() {
  Serial.printf("Temperature: %.2f C\n", fTemperature);
  sprintf(szTopic, "%s/temp", MQTT_TOPIC_PUBLISH);
  sprintf(szData, "%.2f", fTemperature);
  mqtt.publish(szTopic, szData);
}

void pubHumidity() {
  Serial.printf("Humidity: %.2f\n", fHumidity);
  sprintf(szTopic, "%s/humidity", MQTT_TOPIC_PUBLISH);
  sprintf(szData, "%.2f", fHumidity);
  mqtt.publish(szTopic, szData);
}

void pubLux() {
  Serial.printf("Light: %.2f lx\n", lux);
  sprintf(szTopic, "%s/light", MQTT_TOPIC_PUBLISH);
  sprintf(szData, "%.2f", lux);
  mqtt.publish(szTopic, szData);
  ledOff.once_ms(100, [](){
  digitalWrite(LED_BUILTIN, LOW);
  });
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String strTopic = topic;
  int8_t idx = strTopic.lastIndexOf('/') + 1;
  String strDev = strTopic.substring(idx);
  Serial.printf("==> Recv [%s]: ", topic);
  Serial.write(payload, len);
  Serial.println();

  if (strcmp(topic, MQTT_TOPIC_SUBSCRIBE) == 0) {
    payload[len] = '\0';
    Serial.printf("==> Recv [%s]: ", payload);
    if (strcmp((char*)payload, "led-on") == 0) {
      turnOn = true;
      digitalWrite(LED_YELLOW, HIGH);
    }
    else if (strcmp((char*)payload, "led-off") == 0) {
      turnOn = false;
      digitalWrite(LED_YELLOW, LOW);
    }
  }
}

void onPublishMessage() {
szTopic[50];
szData[10];
digitalWrite(LED_BUILTIN, HIGH);
fHumidity = dht.getHumidity();
fTemperature = dht.getTemperature();
lux = lightMeter.readLightLevel();
Serial.printf("Temperature: %.2f C, Humidity: %.2f %%, light: %.2f\n",
fTemperature, fHumidity, lux);

if (lux > 400)
{
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, LOW);

  Serial.printf("Door is opened!\n");
}else
{
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED, LOW);
  Serial.printf("Door is closed!\n");
}
}

boolean mqttConnect() {
sprintf(g_szDeviceId, "esp32_%08X",(uint32_t)ESP.getEfuseMac());
mqtt.setServer(MQTT_BROKER, 1883);
mqtt.setCallback(mqttCallback);
Serial.printf("Connecting to %s clientId: %s\n", MQTT_BROKER, g_szDeviceId);
// Connect to MQTT Broker
// Or, if you want to authenticate MQTT:
boolean status = mqtt.connect(g_szDeviceId);
if (status == false) {
Serial.print(" fail, rc=");
Serial.print(mqtt.state());
return false;
}
Serial.println(" success");
mqtt.subscribe(MQTT_TOPIC_SUBSCRIBE);
Serial.printf("Subcribe topic: %s\n", MQTT_TOPIC_SUBSCRIBE);
onPublishMessage();
return mqtt.connected();
}

void WifiConnect()
{
WiFi.mode(WIFI_STA);
WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
while (WiFi.waitForConnectResult() != WL_CONNECTED) {
Serial.println("Connection Failed! Rebooting...");
delay(5000);
ESP.restart();
}
Serial.print("System connected with IP address: ");
Serial.println(WiFi.localIP());
Serial.printf("RSSI: %d\n", WiFi.RSSI());
}