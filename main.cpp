#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>

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

void setup() {
  sensors.begin();
  Serial.begin(115200);
  delay(100);

  // Connect to Wi-Fi
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);

  // Wait for connection, but don't block boot forever — loop() will keep
  // retrying via the reconnect logic if we time out here.
  unsigned long connectTimeout = millis() + 15000;  // 15s
  while (WiFi.status() != WL_CONNECTED && millis() < connectTimeout) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi not connected — continuing, will retry in loop().");
  }
  server.begin();
}

void loop() {
  // Non-blocking WiFi reconnect with backoff
  static unsigned long lastReconnectAttempt = 0;
  if (WiFi.status() != WL_CONNECTED && millis() - lastReconnectAttempt > 5000) {
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    lastReconnectAttempt = millis();
  }

  WiFiClient client = server.available();

  if (client) {
    Serial.println("New Client.");
    unsigned long timeout = millis() + 3000;  // 3s client timeout
    char buf[256];
    int bufPos = 0;

    // Request temperature data from all sensors on client connect
    sensors.requestTemperatures();
    int deviceCount = sensors.getDeviceCount();

    // Temp debug - print temp for sensor 0
    float temperatureC = sensors.getTempCByIndex(0);
    Serial.print(temperatureC);
    Serial.println("ºC");

    while (client.connected() && millis() < timeout) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);

        if (c == '\n') {
          if (bufPos == 0) {
            // HTTP headers with JSON response
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: application/json");
            client.println("Connection: close");
            client.println();

            // Start JSON object
            client.print("{");
            client.printf("\"sensor_count\":%d,", deviceCount);  // Add sensor count
            client.print("\"temperatures\":[");

            // Loop through all sensors and get their temperatures
            for (int i = 0; i < deviceCount; i++) {
              float tempC = sensors.getTempCByIndex(i);
              if (i > 0) {
                client.print(",");  // Add a comma between JSON objects
              }
              client.printf("{\"sensor\":%d,\"temperature\":%.2f}", i, tempC);
              Serial.printf("Sensor %d: %.2fºC\n", i, tempC);
            }

            // Close temperatures array
            client.print("]}");

            break;
          } else {  // If newline, then clear buffer
            bufPos = 0;
          }
        } else if (c != '\r') {  // If anything else but a carriage return character
          if (bufPos < (int)sizeof(buf) - 1) {
            buf[bufPos++] = c;
          }
        }
      } else {
        delay(1);  // Yield to RTOS to avoid watchdog reset
      }
    }
    // Close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  }
}
