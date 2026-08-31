/**
 * @file Code.gs
 * @brief Googleスプレッドシート記録 ＆ Google Drive写真保存 ＆ Discord写真付きリッチ通知
 */

// Discord Webhook URL (Discordチャンネルの「連携サービス > ウェブフック」で取得したURL)
var DISCORD_WEBHOOK_URL = "YOUR_DISCORD_WEBHOOK_URL";

function doPost(e) {
  try {
    var ss = SpreadsheetApp.getActiveSpreadsheet();
    var sheet = ss.getSheetByName("ログデータ") || ss.getActiveSheet();
    
    // ヘッダー行の作成
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

    var data = JSON.parse(e.postData.contents);
    var now = new Date();
    var timestamp = Utilities.formatDate(now, "Asia/Tokyo", "yyyy/MM/dd HH:mm:ss");

    var moisture = data.moisture !== undefined ? data.moisture : "--";
    var ec = data.ec !== undefined ? data.ec : "--";
    var temp = data.temp !== undefined ? data.temp : "--";
    var valveAction = data.valveAction || "NONE";
    var reason = data.reason || "";
    var status = data.status || "OK";
    var photoUrl = "";
    var imageBlob = null;

    // 写真データの保存 (Base64 JPEG)
    if (data.photoBase64) {
      var folderName = "スマート菜園_写真ログ";
      var folders = DriveApp.getFoldersByName(folderName);
      var folder = folders.hasNext() ? folders.next() : DriveApp.createFolder(folderName);
      
      imageBlob = Utilities.newBlob(Utilities.base64Decode(data.photoBase64), "image/jpeg", "garden_" + Utilities.formatDate(now, "Asia/Tokyo", "yyyyMMdd_HHmmss") + ".jpg");
      var file = folder.createFile(imageBlob);
      file.setSharing(DriveApp.Access.ANYONE_WITH_LINK, DriveApp.Permission.VIEW);
      photoUrl = file.getUrl();
    }

    // 1. Googleスプレッドシートに行を追加
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

    // 2. Discordへ写真付きリッチ通知を送信
    if (DISCORD_WEBHOOK_URL && DISCORD_WEBHOOK_URL !== "YOUR_DISCORD_WEBHOOK_URL") {
      sendDiscordRichNotification(timestamp, moisture, ec, temp, valveAction, reason, status, photoUrl, imageBlob);
    }

    return ContentService.createTextOutput(JSON.stringify({ "result": "success", "timestamp": timestamp }))
      .setMimeType(ContentService.MimeType.JSON);

  } catch (error) {
    return ContentService.createTextOutput(JSON.stringify({ "result": "error", "message": error.toString() }))
      .setMimeType(ContentService.MimeType.JSON);
  }
}

/**
 * @brief Discordへ写真とステータスを含む埋め込み(Embed)通知を送信
 */
function sendDiscordRichNotification(timestamp, moisture, ec, temp, valveAction, reason, status, photoUrl, imageBlob) {
  var isAlert = (status === "FERTILIZER_LOW" || status === "ERROR");
  var embedColor = isAlert ? 15158332 : (valveAction.indexOf("OPEN") !== -1 ? 3066993 : 3447003); // 赤 / 緑 / 青

  var title = isAlert ? "⚠️ 【追肥サイン】スマート菜園レポート" : "🌿 【朝の菜園レポート】畑の様子";
  var statusText = isAlert ? "⚠️ 肥料分(EC)が低下中（追肥推奨）" : "✅ 順調に育成中";
  if (valveAction.indexOf("OPEN") !== -1) {
    statusText = "💧 雨水自動給水を実行しました (" + valveAction + ")";
  }

  var payload = {
    "embeds": [
      {
        "title": title,
        "description": "**状況サマリー**: " + statusText + (reason ? " (" + reason + ")" : ""),
        "color": embedColor,
        "fields": [
          { "name": "🌱 土壌水分", "value": moisture + " %", "inline": true },
          { "name": "🧪 肥料EC値", "value": ec + " mS/cm", "inline": true },
          { "name": "🌡️ 地中温度", "value": temp + " ℃", "inline": true },
          { "name": "🚰 バルブ動作", "value": valveAction, "inline": true },
          { "name": "⏰ 記録日時", "value": timestamp, "inline": true }
        ],
        "footer": { "text": "Google Sheets & Drive にログ保存済み" }
      }
    ]
  };

  // 写真がある場合はDiscordのメッセージに直接画像を添付
  if (imageBlob) {
    var formData = {
      "payload_json": JSON.stringify(payload),
      "file": imageBlob
    };
    UrlFetchApp.fetch(DISCORD_WEBHOOK_URL, {
      "method": "post",
      "payload": formData
    });
  } else {
    UrlFetchApp.fetch(DISCORD_WEBHOOK_URL, {
      "method": "post",
      "contentType": "application/json",
      "payload": JSON.stringify(payload)
    });
  }
}

function doGet(e) {
  return ContentService.createTextOutput("Smart Garden Logger & Discord Notifier is Active.");
}
