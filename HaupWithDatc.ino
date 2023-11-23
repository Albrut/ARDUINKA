#include <SPI.h>
#include <Ethernet.h>
#include <DHT.h>
#include <MQUnifiedsensor.h>
#include <Servo.h>

char serverName[] = "makarovv25.pythonanywhere.com";
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED}; // Change this to your Ethernet shield's MAC address
EthernetServer server(80);

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define MQ_SENSOR_TYPE "MQ7"
#define MQ_PIN A0
MQUnifiedsensor mq("Arduino", 5, 10, MQ_PIN, MQ_SENSOR_TYPE);

boolean windowsAreOpen = false;  // Initial value

// Connect the relay to pin 7
const int relayPin = 7;
Servo windowServo;  // Create a Servo object to control the servo motor
const int servoPin = 9;  // Pin to which the servo motor is connected

void setup() {
  Serial.begin(9600);
  windowServo.attach(servoPin);  // Attach the servo motor to the corresponding pin
  pinMode(relayPin, OUTPUT);

  // Initialize Ethernet with obtaining IP and MAC address via DHCP
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    while (true) {
      delay(1);
    }
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

// Other functions remain unchanged...


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
                 "Host: " + serverName + "\r\n" +
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

            byte mac[6];
            Ethernet.MACAddress(mac);

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
  windowServo.write(180);  // Поворачиваем сервопривод на 180 градусов (или другое подходящее значение)
  delay(1000);  // Даем времени для полного открытия окна
  Serial.println("Windows are open!");
}
