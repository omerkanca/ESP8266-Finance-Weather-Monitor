function doGet() {
  var sheet = SpreadsheetApp.getActiveSpreadsheet().getSheets()[0];
  
  // Hücrelerden verileri oku
  var data = {
    usd:    sheet.getRange("A1").getValue(),
    gold:   sheet.getRange("A2").getValue(),
    silver: sheet.getRange("A3").getValue(),
    bist:   sheet.getRange("A4").getValue(),
    btc:    sheet.getRange("A5").getValue()
  };
  
  // Veriyi JSON formatında gönder
  return ContentService.createTextOutput(JSON.stringify(data))
    .setMimeType(ContentService.MimeType.JSON);
}