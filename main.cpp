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

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

void loop() {
  // Check if WiFi is connected, reconnect if not
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
  }

  WiFiClient client = server.available();

  // Temp debug - print temp for sensor 0
  float temperatureC = sensors.getTempCByIndex(0);
  Serial.print(temperatureC);
  Serial.println("ºC");

  // Request temperature data from all sensors
  sensors.requestTemperatures();
  int deviceCount = sensors.getDeviceCount();  // Get the number of sensors on the bus
  //Serial.printf("Found %d DS18B20 sensors\n", deviceCount);

  if (client) {
    Serial.println("New Client.");
    String currentLine = "";

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);

        if (c == '\n') {
          if (currentLine.length() == 0) { 
            // HTTP headers with JSON response
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: application/json");
            client.println();

            // Start JSON object
            client.print("{");
            client.printf("\"sensor_count\":%d,", deviceCount);  // Add sensor count
            client.print("\"temperatures\":[");

            // Loop through all sensors and get their temperatures
            for (int i = 0; i < deviceCount; i++) {
              float temperatureC = sensors.getTempCByIndex(i);
              if (i > 0) {
                client.print(",");  // Add a comma between JSON objects
              }
              client.printf("{\"sensor\":%d,\"temperature\":%.2f}", i, temperatureC);
              Serial.printf("Sensor %d: %.2fºC\n", i, temperatureC);
            }

            // Close temperatures array
            client.print("]}");

            break;
          } else {  // If newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // If anything else but a carriage return character
          currentLine += c;     
        }
      }
    }
    // Close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  }
}
