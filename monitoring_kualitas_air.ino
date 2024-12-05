#include <OneWire.h>
#include <DallasTemperature.h>
#include <HTTPClient.h>
#include <string>
#include <WiFi.h>
#include <WebServer.h>

// Konstanta dan Pin
#define ONE_WIRE_BUS 4
#define TURBIDITY_PIN 34
#define TdsSensorPin 32
#define VREF 3.3
#define SCOUNT 30

// webserver
WebServer server(80);

// Sensor suhu
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Variabel global
float tdsValue = 0;
float temp = 0;
String kekeruhan;

// Konstanta turbiditas
const float a = -108.11;
const float b = 167.83;

// Buffer untuk TDS
int analogBuffer[SCOUNT];
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;

// Waktu pengiriman data
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 60000;

// wifi
const char* ssid = "Redmi Note 7";
const char* password = "udinpetot";
const char* serverUrl = "https://api.telegram.org/bot7678406236:AAHUhoSbgsJ5ZiEI5kzVme6M_siRVnjCGbc/sendMessage?chat_id=5941692561&";

// Fungsi setup
void setup() {
    Serial.begin(115200);
    pinMode(TURBIDITY_PIN, INPUT);
    pinMode(TdsSensorPin, INPUT);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    server.on("/cekair", HTTP_GET, handleRequest);
	  server.begin();
    sensors.begin();
    Serial.println("Setup selesai.");
}

// Fungsi loop utama
void loop() {
    updateSensors();
    printSensorData();

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    if (shouldSendAlert()) {
        // sendAlertToServer(temp, tdsValue, kekeruhan); // Uncomment jika menggunakan WiFi
        lastSendTime = millis();
    }
    server.handleClient();
}

// --- Fungsi Pendukung ---

// Fungsi membaca suhu
float readTemp() {
    sensors.requestTemperatures();
    float temperatureC = sensors.getTempCByIndex(0);
    return temperatureC != DEVICE_DISCONNECTED_C ? temperatureC : 0;
}

// Fungsi menghitung nilai median
int getMedianNum(int bArray[], int iFilterLen) {
    int bTab[iFilterLen];
    memcpy(bTab, bArray, iFilterLen * sizeof(int));
    for (int j = 0; j < iFilterLen - 1; j++) {
        for (int i = 0; i < iFilterLen - j - 1; i++) {
            if (bTab[i] > bTab[i + 1]) {
                int temp = bTab[i];
                bTab[i] = bTab[i + 1];
                bTab[i + 1] = temp;
            }
        }
    }
    return (iFilterLen & 1) ? bTab[iFilterLen / 2] : (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
}

// Fungsi membaca kualitas air
String kualitasAir() {
    int analogValue = analogRead(TURBIDITY_PIN);
    float volt = analogValue * (3.3 / 4095);
    int turbidity = (a * volt) + b;
    if (turbidity < 20) return "Jernih";
    if (turbidity < 50) return "Keruh";
    return "Kotor";
}

// Fungsi memperbarui sensor
void updateSensors() {
    // Update suhu
    temp = readTemp();

    // Update kekeruhan
    kekeruhan = kualitasAir();

    // Update TDS
    static unsigned long analogSampleTimepoint = millis();
    if (millis() - analogSampleTimepoint > 40U) {
        analogSampleTimepoint = millis();
        analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);
        analogBufferIndex = (analogBufferIndex + 1) % SCOUNT;
    }

    static unsigned long tdsUpdateTimepoint = millis();
    if (millis() - tdsUpdateTimepoint > 800U) {
        tdsUpdateTimepoint = millis();
        memcpy(analogBufferTemp, analogBuffer, SCOUNT * sizeof(int));
        float averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * (float)VREF / 4095.0;
        float compensationVoltage = averageVoltage / (1.0 + 0.02 * (temp - 25.0));
        tdsValue = (133.42 * pow(compensationVoltage, 3) - 255.86 * pow(compensationVoltage, 2) + 857.39 * compensationVoltage) * 0.5;
    }
}

// Fungsi mencetak data sensor
void printSensorData() {
    Serial.print("TDS Value: ");
    Serial.print(tdsValue, 2);
    Serial.println(" ppm");
    Serial.print("Kualitas Air: ");
    Serial.println(kekeruhan);
    Serial.print("Suhu: ");
    Serial.println(temp);
    Serial.println();
}

// Fungsi menentukan apakah perlu mengirim alert
bool shouldSendAlert() {
    bool tempStat = temp > 27;
    bool tdsStat = tdsValue > 200;
    bool keruhStat = (kekeruhan == "Keruh") || (kekeruhan == "Kotor");
    return (tempStat || tdsStat || keruhStat) && (millis() - lastSendTime > sendInterval);
}

 void handleRequest() {
      // Membuat JSON response

      String response = "{\"Kekeruhan\" : \"";
      response += kekeruhan;
      response += "\", \"Temperatur\" : \"";
      response += String(temp);
      response += "\", \"TDS\" : \"";
      response += String(tdsValue);
      response += "\"}";

      Serial.println(response);
      // Kirim respons JSON ke klien
      server.send(200, "application/json", response);
  }
