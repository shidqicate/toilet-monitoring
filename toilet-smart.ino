#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

// ================= WIFI =================
const char* ssid = "0109";
const char* password = "12345678";

// ================= MQTT =================
const char* hostname = "160.187.144.142";
const int port_number = 1883;

const char* topicMQTT =
"pcr/24trkb/kelompok4/toilet/mesjid";

WiFiClient espClient;
PubSubClient clientMQTT(espClient);

// ================= NTP =================
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600; // WIB
const int daylightOffset_sec = 0;

// ================= PIN =================
#define PIR_PIN     13
#define IR_PIN      14

#define RED_LED     32
#define GREEN_LED   27

#define SDA_PIN     25
#define SCL_PIN     26

// ================= LCD =================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= STATE =================
enum ToiletState
{
  AVAILABLE,
  OCCUPIED
};

ToiletState state = AVAILABLE;

bool lastIR = HIGH;

bool waitingForMotion = false;
bool waitingForExit = false;

unsigned long waitStart = 0;

// =====================================================
// WIFI
// =====================================================
void setup_wifi()
{
  Serial.print("Connecting WiFi ");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");

  Serial.print("IP Address : ");
  Serial.println(WiFi.localIP());
}

// =====================================================
// NTP
// =====================================================
void setupTime()
{
  configTime(
    gmtOffset_sec,
    daylightOffset_sec,
    ntpServer
  );

  Serial.print("Sync NTP ");

  struct tm timeinfo;

  while (!getLocalTime(&timeinfo))
  {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("NTP Connected");
}

String getTimestamp()
{
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo))
  {
    return "UNKNOWN";
  }

  char buffer[30];

  strftime(
    buffer,
    sizeof(buffer),
    "%Y-%m-%d %H:%M:%S",
    &timeinfo
  );

  return String(buffer);
}

// =====================================================
// MQTT
// =====================================================
void setup_MQTT()
{
  clientMQTT.setServer(hostname, port_number);
}

void connectMQTT()
{
  while (!clientMQTT.connected())
  {
    String clientId =
      "Kelompok4-Toilet-";

    clientId += String((uint32_t)ESP.getEfuseMac());

    Serial.println();
    Serial.println("=======================");
    Serial.println("Connecting MQTT...");

    WiFiClient test;

    if (test.connect(hostname, port_number))
    {
      Serial.println("TCP MQTT OK");
      test.stop();
    }
    else
    {
      Serial.println("TCP MQTT FAILED");
    }

    if (clientMQTT.connect(clientId.c_str()))
    {
      Serial.println("MQTT CONNECTED");
    }
    else
    {
      Serial.print("MQTT FAILED rc=");
      Serial.println(clientMQTT.state());

      delay(3000);
    }
  }
}

// =====================================================
// MQTT PUBLISH
// =====================================================
void publishStatus(String status)
{
  if (!clientMQTT.connected())
  {
    Serial.println("MQTT belum connect");
    return;
  }

  String payload =
    "{\"status\":\"" +
    status +
    "\",\"timestamp\":\"" +
    getTimestamp() +
    "\"}";

  bool result =
    clientMQTT.publish(
      topicMQTT,
      payload.c_str()
    );

  Serial.println();
  Serial.print("Publish : ");
  Serial.println(payload);

  if (result)
    Serial.println("BERHASIL");
  else
    Serial.println("GAGAL");
}

// =====================================================
// LCD + LED
// =====================================================
void showAvailable()
{
  Serial.println("STATUS : TERSEDIA");

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("TOILET");

  lcd.setCursor(0, 1);
  lcd.print("TERSEDIA");

  publishStatus("TERSEDIA");
}

void showOccupied()
{
  Serial.println("STATUS : TERISI");

  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, LOW);

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("TOILET");

  lcd.setCursor(0, 1);
  lcd.print("TERISI");

  publishStatus("TERISI");
}

// =====================================================
// SETUP
// =====================================================
void setup()
{
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(IR_PIN, INPUT);

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);

  setup_wifi();

  setupTime();

  setup_MQTT();

  connectMQTT();

  Wire.begin(SDA_PIN, SCL_PIN);

  lcd.init();
  lcd.backlight();

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("QUEUE TOILET");

  lcd.setCursor(0, 1);
  lcd.print("STARTING...");

  delay(2000);

  showAvailable();

  Serial.println("QUEUE TOILET START");
}

// =====================================================
// LOOP
// =====================================================
void loop()
{
  if (!clientMQTT.connected())
  {
    connectMQTT();
  }

  clientMQTT.loop();

  bool motion = digitalRead(PIR_PIN);
  bool irNow = digitalRead(IR_PIN);

  bool doorEvent =
    (lastIR == HIGH && irNow == LOW);

  lastIR = irNow;

  // =====================================
  // TOILET TERSEDIA
  // =====================================
  if (state == AVAILABLE)
  {
    if (doorEvent)
    {
      Serial.println("PINTU TERBUKA");
      Serial.println("MENUNGGU GERAKAN PIR");

      waitingForMotion = true;
      waitStart = millis();
    }

    if (waitingForMotion)
    {
      if (motion)
      {
        state = OCCUPIED;

        waitingForMotion = false;

        showOccupied();
      }

      if (millis() - waitStart > 10000)
      {
        waitingForMotion = false;

        Serial.println("TIDAK ADA ORANG MASUK");
      }
    }
  }

  // =====================================
  // TOILET TERISI
  // =====================================
  else
  {
    if (doorEvent)
    {
      Serial.println("PINTU TERBUKA");
      Serial.println("MENUNGGU KONFIRMASI KELUAR");

      waitingForExit = true;
      waitStart = millis();
    }

    if (waitingForExit)
    {
      if (millis() - waitStart > 10000)
      {
        if (!motion)
        {
          state = AVAILABLE;

          showAvailable();
        }
        else
        {
          Serial.println("MASIH ADA ORANG DI DALAM");
        }

        waitingForExit = false;
      }
    }
  }

  delay(50);
}