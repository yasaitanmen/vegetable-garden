/**
 * @file esp32_smart_garden.ino
 * @brief 4G携帯回線 × 雨水重力給水 × 土壌水分・ECセンサー制御 メインスケッチ
 */

#include "config.h"
#include <HardwareSerial.h>

// RS485 Modbus RTU クエリフレーム (土壌水分・温度・EC読み取り用)
// アドレス 0x01, ファンクション 0x03, レジスタ 0x0000, 3ワード読み取り
const byte SOIL_QUERY[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x03, 0x05, 0xCB};
byte soilResponse[11];

// 測定データの保持
float soilMoisture = 0.0;    // 水分率 (%)
float soilTemperature = 0.0; // 地中温度 (℃)
float soilEC = 0.0;          // 肥料濃度 (mS/cm または us/cm)

// ハードウェアシリアル
HardwareSerial RS485Serial(2);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== 🌿 Smart Garden 4G System Starting ===");

  // 1. ピンモード初期化
  pinMode(PIN_VALVE_RELAY, OUTPUT);
  digitalWrite(PIN_VALVE_RELAY, LOW); // 初期状態はバルブ閉 (安全)

  pinMode(PIN_RS485_DE_RE, OUTPUT);
  digitalWrite(PIN_RS485_DE_RE, LOW); // 受信モード

  // 2. RS485通信初期化 (9600 baud, 8N1)
  RS485Serial.begin(9600, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX);

  // 3. 4Gモデム初期化 (SIM7600)
  init4GModem();

  Serial.println("System Ready. Monitoring soil and schedule...");
}

void loop() {
  // 1. センサー値の定期読み取り (テスト用に10秒ごと、本番はスケジュール実行)
  if (readSoilSensor()) {
    Serial.printf("[Sensor] 水分: %.1f%% | 温度: %.1fC | EC(肥料): %.2f mS/cm\n",
                  soilMoisture, soilTemperature, soilEC);

    // 2. 追肥判定 (EC低下アラート)
    checkFertilizerAlert();
  }

  // 3. スケジュール判定 (給水・写真送信)
  checkDailySchedule();

  delay(5000);
}

/**
 * @brief RS485 Modbus経由で土壌 水分・EC・温度を読み取る
 */
bool readSoilSensor() {
  // 送信モードへ切り替え
  digitalWrite(PIN_RS485_DE_RE, HIGH);
  delay(10);
  RS485Serial.write(SOIL_QUERY, sizeof(SOIL_QUERY));
  RS485Serial.flush();

  // 受信モードへ切り替え
  digitalWrite(PIN_RS485_DE_RE, LOW);
  delay(50);

  // レスポンス受信 (11バイト期待)
  int bytesRead = 0;
  unsigned long timeout = millis() + 1000;
  while (millis() < timeout && bytesRead < 11) {
    if (RS485Serial.available()) {
      soilResponse[bytesRead++] = RS485Serial.read();
    }
  }

  if (bytesRead >= 11 && soilResponse[0] == 0x01 && soilResponse[1] == 0x03) {
    // データのパース (ビッグエンディアン)
    int rawMoisture = (soilResponse[3] << 8) | soilResponse[4];
    int rawTemp     = (soilResponse[5] << 8) | soilResponse[6];
    int rawEC       = (soilResponse[7] << 8) | soilResponse[8];

    soilMoisture    = rawMoisture * 0.1; // 0.1%単位
    soilTemperature = (rawTemp > 32767 ? rawTemp - 65536 : rawTemp) * 0.1; // 0.1℃単位
    soilEC          = rawEC * 0.001;     // mS/cm単位 (または us/cm / 1000)
    return true;
  }

  Serial.println("[Warn] RS485 Sensor read timeout or invalid response");
  return false;
}

/**
 * @brief 電動バルブを開いて安全に点滴給水を行う
 */
void executeWatering(int durationSeconds) {
  // 安全ガード: 最大開放時間を超えないように制限
  if (durationSeconds > VALVE_MAX_SAFETY_SEC) {
    durationSeconds = VALVE_MAX_SAFETY_SEC;
  }

  Serial.printf("[Watering] 12V電動バルブ開放 (%d秒間)\n", durationSeconds);
  digitalWrite(PIN_VALVE_RELAY, HIGH); // バルブ開

  delay(durationSeconds * 1000);

  digitalWrite(PIN_VALVE_RELAY, LOW);  // バルブ閉
  Serial.println("[Watering] 12V電動バルブ閉鎖完了");

  // 給水完了のログ ＆ Discord写真付き通知 (GAS経由)
  logDataToGoogleSheets("OPEN_" + String(durationSeconds) + "s", "Auto Moisture Low");
}

/**
 * @brief 追肥が必要か判定してアラートを送信
 */
void checkFertilizerAlert() {
  if (soilEC > 0.01 && soilEC < EC_ALERT_THRESHOLD) {
    static unsigned long lastAlertTime = 0;
    // 1週間に1回のみ通知 (多重通知防止)
    if (millis() - lastAlertTime > 7UL * 24 * 3600 * 1000 || lastAlertTime == 0) {
      lastAlertTime = millis();
      Serial.println("[Alert] Soil EC is low. Triggering Discord alert via GAS...");
      logDataToGoogleSheets("NONE", "EC Low (Fertilizer Alert)");
    }
  }
}

/**
 * @brief 4Gモデムの初期化
 */
void init4GModem() {
  Serial.println("[4G] Connecting to cellular network...");
  // 実際の実装では TinyGSM ライブラリを使用
  // modem.restart();
  // modem.gprsConnect(APN_NAME, APN_USER, APN_PASS);
  Serial.println("[4G] Network connected.");
}

/**
 * @brief 4G経由でGoogleスプレッドシート(GAS)へデータ送信 (GAS側でDiscord通知も自動実行)
 * @param valveAction バルブ動作内容 ("OPEN_90s" / "NONE")
 * @param reason 動作理由 ("Auto (Moisture < 35%)" / "Manual" 等)
 * @param photoBase64 写真データ (JPEG Base64文字列, なければ空文字)
 */
void logDataToGoogleSheets(String valveAction, String reason, String photoBase64 = "") {
  Serial.println("[Logger] Sending data to Google Sheets & Drive & Discord...");
  
  // 送信JSONデータの構築
  String jsonPayload = "{";
  jsonPayload += "\"moisture\":" + String(soilMoisture, 1) + ",";
  jsonPayload += "\"ec\":" + String(soilEC, 2) + ",";
  jsonPayload += "\"temp\":" + String(soilTemperature, 1) + ",";
  jsonPayload += "\"valveAction\":\"" + valveAction + "\",";
  jsonPayload += "\"reason\":\"" + reason + "\",";
  jsonPayload += "\"status\":\"" + String(soilEC < EC_ALERT_THRESHOLD ? "FERTILIZER_LOW" : "OK") + "\"";
  if (photoBase64.length() > 0) {
    jsonPayload += ",\"photoBase64\":\"" + photoBase64 + "\"";
  }
  jsonPayload += "}";

  Serial.println("[Google Logger] Payload: " + jsonPayload);
  // HTTPS POST to GAS_WEBAPP_URL
}

/**
 * @brief 毎日の定時スケジュール実行 (朝の給水判定・写真送信)
 */
void checkDailySchedule() {
  // 朝 6:30 の給水判定
  // if (currentHour == SCHEDULE_WATER_HOUR && currentMin == SCHEDULE_WATER_MIN) {
  //   if (soilMoisture < WATER_THRESHOLD_MOISTURE) {
  //     executeWatering(VALVE_OPEN_DURATION_SEC);
  //     logDataToGoogleSheets("OPEN_" + String(VALVE_OPEN_DURATION_SEC) + "s", "Auto Moisture Low");
  //   } else {
  //     logDataToGoogleSheets("NONE", "Moisture OK (Skip)");
  //   }
  // }
}
