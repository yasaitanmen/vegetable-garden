/**
 * @file config.h
 * @brief スマート菜園 4G制御システム 設定ファイル
 */

#ifndef CONFIG_H
#define CONFIG_H

// ==========================================
// 1. 通信設定 (4G/LTE SIM)
// ==========================================
// 使用するSIMのAPN設定 (例: povo2.0 の場合)
#define APN_NAME      "povo.jp"
#define APN_USER      ""
#define APN_PASS      ""

// LINE Messaging API または LINE Notify 設定
#define LINE_TOKEN    "YOUR_LINE_ACCESS_TOKEN"
#define LINE_USER_ID  "YOUR_LINE_USER_ID" // またはグループID

// ==========================================
// 2. ピンアサイン設定 (LilyGO T-SIM7600 / ESP32)
// ==========================================
// バルブ制御リレー
#define PIN_VALVE_RELAY   12  // 12V電動バルブ開閉用リレー (HIGHで開放)

// RS485 土壌 水分・ECセンサー通信ピン (HardwareSerial 2)
#define PIN_RS485_RX      16
#define PIN_RS485_TX      17
#define PIN_RS485_DE_RE   4   // 送受信切り替えピン (HIGH:送信, LOW:受信)

// ==========================================
// 3. 自動化パラメータ設定 (茎ブロッコリー等)
// ==========================================
// 給水判定ルール (毎朝 6:30)
#define WATER_THRESHOLD_MOISTURE  35.0  // 土壌水分が35%未満で給水
#define VALVE_OPEN_DURATION_SEC   90    // バルブ開放時間 (秒)
#define VALVE_MAX_SAFETY_SEC      120   // 安全リミット最大開放時間 (秒)

// 追肥判定ルール (EC値)
#define EC_ALERT_THRESHOLD        0.5   // 土壌ECが0.5 mS/cm未満で追肥アラート

// 定時実行スケジュール (24時間表記)
#define SCHEDULE_WATER_HOUR       6     // 給水判定時刻: 6時
#define SCHEDULE_WATER_MIN        30    // 30分
#define SCHEDULE_PHOTO_HOUR       8     // 写真撮影時刻: 8時
#define SCHEDULE_PHOTO_MIN        0     // 00分

#endif // CONFIG_H
