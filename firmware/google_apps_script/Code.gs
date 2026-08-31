/**
 * @file Code.gs
 * @brief Google Apps Script (GAS) スマート菜園データロギング ＆ 写真保存エンドポイント
 * 
 * 【使い方】
 * 1. Googleドライブで新規「Googleスプレッドシート」を作成。
 * 2. 拡張機能 > Apps Script を開き、本コードを貼り付けて保存。
 * 3. 「デプロイ」>「新しいデプロイ」>「ウェブアプリ」を選択。
 *    - 次のユーザーとして実行: 自分
 *    - アクセスできるユーザー: 全員 (Anyone)
 * 4. 発行された「ウェブアプリURL」をマイコンの config.h (GAS_WEBAPP_URL) に設定。
 */

function doPost(e) {
  try {
    var ss = SpreadsheetApp.getActiveSpreadsheet();
    var sheet = ss.getSheetByName("ログデータ") || ss.getActiveSheet();
    
    // ヘッダー行が存在しない場合は作成
    if (sheet.getLastRow() === 0) {
      sheet.appendRow([
        "日時 (Timestamp)",
        "土壌水分 (%)",
        "土壌EC・肥料 (mS/cm)",
        "地中温度 (℃)",
        "給水動作 (Valve Action)",
        "動作理由 (Reason)",
        "ステータス (Status)",
        "写真リンク (Photo URL)"
      ]);
      sheet.getRange("A1:H1").setBackground("#d9ead3").setFontWeight("bold");
    }

    // JSONデータのパース
    var data = JSON.parse(e.postData.contents);
    var now = new Date();
    var timestamp = Utilities.formatDate(now, "Asia/Tokyo", "yyyy/MM/dd HH:mm:ss");

    var moisture = data.moisture !== undefined ? data.moisture : "";
    var ec = data.ec !== undefined ? data.ec : "";
    var temp = data.temp !== undefined ? data.temp : "";
    var valveAction = data.valveAction || "NONE";
    var reason = data.reason || "";
    var status = data.status || "OK";
    var photoUrl = "";

    // Base64形式の写真データが含まれる場合は Google Drive に保存
    if (data.photoBase64) {
      var folderName = "スマート菜園_写真ログ";
      var folders = DriveApp.getFoldersByName(folderName);
      var folder = folders.hasNext() ? folders.next() : DriveApp.createFolder(folderName);
      
      var imageBlob = Utilities.newBlob(Utilities.base64Decode(data.photoBase64), "image/jpeg", "garden_" + Utilities.formatDate(now, "Asia/Tokyo", "yyyyMMdd_HHmmss") + ".jpg");
      var file = folder.createFile(imageBlob);
      file.setSharing(DriveApp.Access.ANYONE_WITH_LINK, DriveApp.Permission.VIEW);
      photoUrl = file.getUrl();
    }

    // スプレッドシートに行を追加
    sheet.appendRow([
      timestamp,
      moisture,
      ec,
      temp,
      valveAction,
      reason,
      status,
      photoUrl
    ]);

    return ContentService.createTextOutput(JSON.stringify({ "result": "success", "timestamp": timestamp }))
      .setMimeType(ContentService.MimeType.JSON);

  } catch (error) {
    return ContentService.createTextOutput(JSON.stringify({ "result": "error", "message": error.toString() }))
      .setMimeType(ContentService.MimeType.JSON);
  }
}

function doGet(e) {
  return ContentService.createTextOutput("Smart Garden Logger API is Active.");
}
