#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// BLE 서버 이름 (advertising 시 설정한 이름과 동일해야 함)
#define SERVER_NAME "ESP32-BLE-Server"

// 내장 LED 핀
const int LED_PIN = 2;

// 거리 계산용 변수
int txPower = -57;    // 1m에서의 RSSI (측정 필요)
float n = 2.0;        // 환경계수 (실내 2.0~4.0)

// 최신 RSSI 및 거리 저장 변수
int latestRSSI = -100;
float latestDistance = 0.0;

// Wi-Fi 설정 (자신의 네트워크 정보 입력)
const char* ssid = "123123"
const char* password = "123123"

// 웹서버 실행 (80 포트)
WiFiServer server(80);

// BLE 스캔 콜백 클래스
float estimateDistance(int rssi) {
  return pow(10.0, ((float)(txPower - rssi)) / (10.0 * n));
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveName() && advertisedDevice.getName() == SERVER_NAME) {
      int rssi = advertisedDevice.getRSSI();
      float distance = estimateDistance(rssi);

      // 값 저장
      latestRSSI = rssi;
      latestDistance = distance;

      Serial.print("RSSI: ");
      Serial.print(rssi);
      Serial.print(" dBm → Estimated Distance: ");
      Serial.print(distance, 2);
      Serial.println(" meters");

      // 1m 이내이면 LED ON
      if (distance <= 1.0) {
        digitalWrite(LED_PIN, HIGH);
      } else {
        digitalWrite(LED_PIN, LOW);
      }
    }
  }
};

void setup() {
  Serial.begin(115200);

  // Wi-Fi 연결
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi connected.");
  Serial.print("🌐 IP address: ");
  Serial.println(WiFi.localIP());

  // 웹서버 시작
  server.begin();
  Serial.println("🌐 Web server started.");

  // LED 설정
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // BLE 초기화 및 스캔 시작
  BLEDevice::init("");
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pScan->setActiveScan(true);
}

void loop() {
  // 웹 요청 처리
  WiFiClient client = server.available();

  if (client) {
    Serial.println("📥 New Client connected.");

    // 🟡 웹 응답 전에 스캔을 수행
    BLEScan* pScan = BLEDevice::getScan();
    pScan->clearResults();
    pScan->start(1, false);  // 1초 동안 스캔 수행
    // → latestRSSI, latestDistance 업데이트됨

    while (client.connected()) {
      if (client.available()) {
        client.read();  // 요청 무시

        // ✨ 최신 BLE 측정 결과를 바로 출력
        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:text/html");
        client.println();
        client.println("<!DOCTYPE html><html><head><meta charset='utf-8'>");
        client.println("<meta http-equiv='refresh' content='2'>");
        client.println("<title>BLE Distance</title></head><body>");
        client.println("<h2>📡 BLE Distance Estimation</h2>");
        client.print("<p><strong>RSSI:</strong> ");
        client.print(latestRSSI);
        client.println(" dBm</p>");
        client.print("<p><strong>Estimated Distance:</strong> ");
        client.print(latestDistance, 2);
        client.println(" meters</p>");
        client.println("<p><small>Measured in real-time</small></p>");
        client.println("</body></html>");
        break;
      }
    }

    client.stop();
    Serial.println("Client disconnected.");
  }

  // 주기적으로 스캔 (1초)
  BLEScan* pScan = BLEDevice::getScan();
  pScan->clearResults();             // 이전 결과 제거
  pScan->start(1, false);            // 1초 스캔 (blocking)
  delay(500);                        // 약간의 여유를 줌
}
