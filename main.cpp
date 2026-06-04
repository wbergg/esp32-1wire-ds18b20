#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include "esp_log.h"

// GPIO where the DS18B20 is connected to
const int oneWireBus = 15;

// Setup a oneWire instance
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

// WiFi-settings
const char* ssid = "";
const char* password = "";

WiFiServer server(80);

// Reconnect backoff
static unsigned long lastReconnectAttempt = 0;
const unsigned long WIFI_RETRY_INTERVAL = 5000; // 5 seconds

void connectWiFi()
{
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
}

void ensureWiFi()
{
  if (WiFi.status() == WL_CONNECTED) return;

  if (millis() - lastReconnectAttempt >= WIFI_RETRY_INTERVAL) {
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect(false);   // keep credentials
    WiFi.begin(ssid, password);
    lastReconnectAttempt = millis();
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  esp_log_level_set("*", ESP_LOG_VERBOSE);

  sensors.begin();

  // Connect to Wi-Fi once
  connectWiFi();

  // Wait for connection, but don't block forever
  unsigned long connectTimeout = millis() + 15000;  // 15s
  while (WiFi.status() != WL_CONNECTED && millis() < connectTimeout) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi not connected — continuing, will retry in loop().");
    lastReconnectAttempt = millis(); // start retry timer
  }

  server.begin();
}

void loop() {
  // Non-blocking Wi-Fi reconnect
  ensureWiFi();

  WiFiClient client = server.available();

  if (client) {
    Serial.println("New Client.");
    unsigned long timeout = millis() + 3000;  // 3s client timeout
    String currentLine = "";

    // Request temperature data from all sensors on client connect
    sensors.requestTemperatures();
    int deviceCount = sensors.getDeviceCount();

    // Temp debug - print temp for sensor 0
    float temperatureC = sensors.getTempCByIndex(0);
    Serial.print(temperatureC);
    Serial.println(" ºC");

    bool responseSent = false;

    while (client.connected() && millis() < timeout) {
      while (client.available()) {
        char c = client.read();
        Serial.write(c);

        if (c == '\n') {
          // Blank line means end of HTTP headers
          if (currentLine.length() == 0 && !responseSent) {
            // HTTP headers with JSON response
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: application/json");
            client.println("Connection: close");
            client.println();

            // JSON object
            client.print("{");
            client.printf("\"sensor_count\":%d,", deviceCount);
            client.print("\"temperatures\":[");

            for (int i = 0; i < deviceCount; i++) {
              float tempC = sensors.getTempCByIndex(i);
              if (i > 0) {
                client.print(",");
              }
              client.printf("{\"sensor\":%d,\"temperature\":%.2f}", i, tempC);
              Serial.printf("Sensor %d: %.2f ºC\n", i, tempC);
            }

            client.print("]}");
            responseSent = true;
            break;
          }

          currentLine = "";
        } else if (c != '\r') {
          currentLine += c;
        }
      }

      if (responseSent) break;
      delay(1); // Yield to RTOS
    }

    client.stop();
    Serial.println("Client Disconnected.");
  }
}
