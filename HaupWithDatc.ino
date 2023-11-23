#include <SPI.h>
#include <Ethernet.h>
#include <DHT.h>
#include <MQUnifiedsensor.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress serverIP(192, 168, 1, 100);
EthernetServer server(80);

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define MQ_SENSOR_TYPE "MQ7"
#define MQ_PIN A0
MQUnifiedsensor mq("Arduino", 5, 10, MQ_PIN, MQ_SENSOR_TYPE);

boolean windowsAreOpen = false;  // Начальное значение

// Подключение реле к пину 7
const int relayPin = 7;

void setup() {
  Serial.begin(9600);

  pinMode(relayPin, OUTPUT);

  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    for (;;);
  }

  server.begin();
  delay(1000);
}

void loop() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  float gasValue = mq.readSensor();

  sendSensorData(humidity, temperature, gasValue);

  handleGETRequests();

  delay(5000);
}

void sendSensorData(float humidity, float temperature, float gasValue) {
  byte mac[6];
  Ethernet.MACAddress(mac);

  String url = "/api/command";

  // Строка с данными в формате JSON
  String jsonData = "{";
  jsonData += "\"mac\":\"";
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 0x10) {
      jsonData += "0";
    }
    jsonData += String(mac[i], HEX);
    if (i < 5) {
      jsonData += ":";
    }
  }
  jsonData += "\",";
  jsonData += "\"humidity\":" + String(humidity) + ",";
  jsonData += "\"temperature\":" + String(temperature) + ",";
  jsonData += "\"gas\":" + String(gasValue);
  jsonData += "}";

  Serial.print("Connecting to server...");
  EthernetClient client = server.available();
  if (client) {
    Serial.println("Connected!");
    Serial.print("Sending HTTP request...");

    // Отправка POST-запроса
    client.print("POST " + url + " HTTP/1.1\r\n" +
                 "Host: " + serverIP + "\r\n" +
                 "Content-Type: application/json\r\n" +
                 "Content-Length: " + jsonData.length() + "\r\n" +
                 "Connection: close\r\n\r\n");
    client.print(jsonData);

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.print(c);
      }
    }
    Serial.println("Request sent.");
    client.stop();
  } else {
    Serial.println("Connection failed.");
  }
}

void handleGETRequests() {
  EthernetClient client = server.available();
  if (client) {
    Serial.println("New client");
    boolean currentLineIsBlank = true;
    String request = "";

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        request += c;

        if (c == '\n' && currentLineIsBlank) {
          // Конец HTTP-запроса, обработаем его
          if (request.indexOf("GET /api/windows") != -1) {
            // Запрос на состояние окон
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: application/json");
            client.println("Connection: close");
            client.println();

            String response = "{\"mac\":\"";
            for (int i = 0; i < 6; i++) {
              if (mac[i] < 0x10) {
                response += "0";
              }
              response += String(mac[i], HEX);
              if (i < 5) {
                response += ":";
              }
            }
            response += "\",\"windowsAreOpen\":" + String(windowsAreOpen) + "}";
            client.print(response);

            // Проверяем условия для открытия окна
            if (macAddressMatch(request) && windowsAreOpen) {
              openWindows();
            }

            break;
          }
        }
        if (c == '\n') {
          // Новая строка начинается
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // Новый символ в строке
          currentLineIsBlank = false;
        }
      }
    }

    // Закрытие соединения
    delay(1);
    client.stop();
    Serial.println("Client disconnected");
  }
}

boolean macAddressMatch(String request) {
  byte mac[6];
  Ethernet.MACAddress(mac);

  String macString = "";
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 0x10) {
      macString += "0";
    }
    macString += String(mac[i], HEX);
    if (i < 5) {
      macString += ":";
    }
  }

  return request.indexOf(macString) != -1;
}

void openWindows() {
  // Здесь вставьте код для управления окном
  // Например, для реле
  digitalWrite(relayPin, HIGH);
  Serial.println("Windows are open!");
}
