#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

//================ MQTT ==================
#include <PubSubClient.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

//================ OLED =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//================ WIFI =================
const char* ssid = "Yuda";
const char* password = "joyzy296";

//================ TELEGRAM ==============
#define BOT_TOKEN "8995576622:AAGH2M4Vm3oJ1YE2GTTCFzCJCDwUQwRTgFo"
#define CHAT_ID "1156417283"

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

//================ MQTT ==================
const char* mqtt_server = "broker.mqtt-dashboard.com";
const int mqtt_port = 1883;

// MQTT topics
const char* mqtt_ph_topic = "smart-river/pH";
const char* mqtt_turbidity_topic = "smart-river/Turbidity";
const char* mqtt_status_topic = "smart-river/status";

WiFiClient mqttClient;
PubSubClient mqtt(mqttClient);

//================ LED ==================
#define GREEN_LED 25
#define YELLOW_LED 26
#define RED_LED 27

//================ SENSOR PINS ===========
#define PH_PIN 34
#define TURBIDITY_PIN 35

String lastStatus = "";

//================ TELEGRAM COMMAND ======
unsigned long lastTelegramCheck = 0;
const unsigned long telegramInterval = 1000;

//================================================
// MQTT CONNECT
//================================================

void reconnectMQTT() {

  while (!mqtt.connected()) {

    Serial.print("Connecting to MQTT...");

    // Client ID
    String clientID = "ESP32-SmartWater-";
    clientID += String(random(0xffff), HEX);

    if (mqtt.connect(clientID.c_str())) {

      Serial.println("CONNECTED!");

      // Publish connection message
      mqtt.publish(
        mqtt_status_topic,
        "ESP32 Smart Water Monitoring System ONLINE"
      );

    } else {

      Serial.print("FAILED, rc=");
      Serial.print(mqtt.state());
      Serial.println(" retrying in 5 seconds...");

      delay(5000);
    }
  }
}

//================================================
// TELEGRAM COMMAND
//================================================

void handleTelegramMessages() {

  // Check Telegram every 1 second
  if (millis() - lastTelegramCheck < telegramInterval) {
    return;
  }

  lastTelegramCheck = millis();

  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while (numNewMessages) {

    Serial.println("New Telegram message received!");

    for (int i = 0; i < numNewMessages; i++) {

      String chat_id = bot.messages[i].chat_id;
      String text = bot.messages[i].text;

      Serial.print("Telegram command: ");
      Serial.println(text);

      //================================================
      // STATUS COMMAND
      //================================================

      if (text == "status" || text == "/status") {

        // Read latest sensor values
        int phRaw = analogRead(PH_PIN);
        int turbRaw = analogRead(TURBIDITY_PIN);

        //================ pH ==================

        float voltage = phRaw * (3.3 / 4095.0);

        float pH = 7 + ((2.5 - voltage) * 3);

        //================ TURBIDITY ===========

        String turbidity;

        if (turbRaw < 1500)

          turbidity = "CLEAR";

        else if (turbRaw < 3000)

          turbidity = "MEDIUM";

        else

          turbidity = "DIRTY";

        //================ WATER STATUS ========

        String status;

        if (pH < 6 || pH > 8 || turbRaw > 3000) {

          status = "BAD";

        }

        else if (pH < 6.5 || pH > 7.5) {

          status = "WARNING";

        }

        else {

          status = "GOOD";
        }

        //================================================
        // CREATE TELEGRAM MESSAGE
        //================================================

        String message = "";

        message += "SMART WATER MONITORING SYSTEM\n";
        message += "------------------------------\n";
        message += "CURRENT STATUS\n\n";

        message += "pH : ";
        message += String(pH, 2);
        message += "\n";

        message += "Turbidity : ";
        message += turbidity;
        message += "\n";

        message += "Status : ";
        message += status;
        message += "\n\n";

        message += "pH Raw : ";
        message += String(phRaw);
        message += "\n";

        message += "Turbidity Raw : ";
        message += String(turbRaw);
        message += "\n";

        //================ SEND MESSAGE ========

        bot.sendMessage(
          chat_id,
          message,
          ""
        );

        Serial.println("Telegram status sent!");
      }

      //================================================
      // HELP COMMAND
      //================================================

      else if (text == "help" || text == "/help") {

        String message = "";

        message += "SMART WATER MONITORING SYSTEM\n";
        message += "------------------------------\n";
        message += "Available Commands:\n\n";

        message += "status\n";
        message += "Check current water status.\n\n";

        message += "/status\n";
        message += "Check current water status.\n\n";

        message += "help\n";
        message += "Show available commands.\n\n";

        message += "/help\n";
        message += "Show available commands.";

        bot.sendMessage(
          chat_id,
          message,
          ""
        );

        Serial.println("Telegram help message sent!");
      }
    }

    // Check if there are more messages
    numNewMessages = bot.getUpdates(
      bot.last_message_received + 1
    );
  }
}

//================================================
// SETUP
//================================================

void setup() {

  Serial.begin(115200);

  //================ LED ==================

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  //================ OLED =================

  if (!display.begin(0x3C, true)) {

    Serial.println("OLED not found");

    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();

  //================ WIFI =================

  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);

    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  //================ TELEGRAM =============

  client.setInsecure();

  // Startup Telegram message
  bot.sendMessage(
    CHAT_ID,
    "ESP32 Smart Water Monitoring System is ONLINE.",
    ""
  );

  //================ MQTT ==================

  mqtt.setServer(mqtt_server, mqtt_port);

  reconnectMQTT();

  //================ OLED ==================

  display.clearDisplay();

  display.setCursor(0, 0);
  display.println("WiFi Connected!");

  display.setCursor(0, 15);
  display.println("MQTT Connected!");

  display.display();

  delay(1500);
}

//================================================
// LOOP
//================================================

void loop() {

  //================================================
  // MQTT CONNECTION CHECK
  //================================================

  if (!mqtt.connected()) {

    reconnectMQTT();
  }

  mqtt.loop();

  //================================================
  // TELEGRAM COMMAND CHECK
  //================================================

  handleTelegramMessages();

  //================================================
  // READ SENSOR
  //================================================

  int phRaw = analogRead(PH_PIN);
  int turbRaw = analogRead(TURBIDITY_PIN);

  //================ pH ==================

  float voltage = phRaw * (3.3 / 4095.0);

  float pH = 7 + ((2.5 - voltage) * 3);

  //================ TURBIDITY ===========

  String turbidity;

  if (turbRaw < 1500)

    turbidity = "CLEAR";

  else if (turbRaw < 3000)

    turbidity = "MEDIUM";

  else

    turbidity = "DIRTY";

  //================================================
  // WATER STATUS
  //================================================

  String status;

  if (pH < 6 || pH > 8 || turbRaw > 3000) {

    status = "BAD";

    digitalWrite(RED_LED, HIGH);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(GREEN_LED, LOW);

  }

  else if (pH < 6.5 || pH > 7.5) {

    status = "WARNING";

    digitalWrite(RED_LED, LOW);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);

  }

  else {

    status = "GOOD";

    digitalWrite(RED_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
  }

  //================================================
  // SERIAL MONITOR
  //================================================

  Serial.println("==================================");
  Serial.println(" SMART WATER MONITORING SYSTEM");
  Serial.println("==================================");

  Serial.print("pH Value     : ");
  Serial.println(pH, 2);

  Serial.print("pH Raw       : ");
  Serial.println(phRaw);

  Serial.print("Turbidity Raw: ");
  Serial.println(turbRaw);

  Serial.print("Turbidity    : ");
  Serial.println(turbidity);

  Serial.print("Status       : ");
  Serial.println(status);

  Serial.println("==================================\n");

  //================================================
  // OLED
  //================================================

  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("Water Monitor");

  display.setCursor(0, 15);
  display.print("pH: ");
  display.println(pH, 2);

  display.setCursor(0, 30);
  display.print("Turb: ");
  display.println(turbidity);

  display.setCursor(0, 45);
  display.print("Status: ");
  display.println(status);

  display.display();

  //================================================
  // MQTT PUBLISH
  //================================================

  // pH
  char phMessage[10];

  dtostrf(pH, 1, 2, phMessage);

  mqtt.publish(
    mqtt_ph_topic,
    phMessage
  );

  // Turbidity
  mqtt.publish(
    mqtt_turbidity_topic,
    turbidity.c_str()
  );

  // Status
  mqtt.publish(
    mqtt_status_topic,
    status.c_str()
  );

  Serial.println("MQTT Data Published!");

  //================================================
  // TELEGRAM AUTOMATIC NOTIFICATION
  //================================================

  if (status != lastStatus) {

    String message = "";

    message += "SMART WATER MONITORING SYSTEM\n";
    message += "------------------------------\n";
    message += "Status : " + status + "\n";
    message += "pH : " + String(pH, 2) + "\n";
    message += "Turbidity : " + turbidity + "\n\n";

    if (status == "GOOD") {

      message += "Water quality is GOOD.";
    }

    if (status == "WARNING") {

      message += "Warning! Water quality needs attention.";
    }

    if (status == "BAD") {

      message += "ALERT!\n";
      message += "Water quality is BAD!\n";
      message += "Immediate inspection is recommended.";
    }

    bot.sendMessage(
      CHAT_ID,
      message,
      ""
    );

    Serial.println("Telegram Notification Sent!");

    lastStatus = status;
  }

  //================================================
  // DELAY
  //================================================

  delay(1500);
}