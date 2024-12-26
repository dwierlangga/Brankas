#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

const char* ssid = "0000";
const char* pass = "tadiganti";

int Alarm = D8; // Pin Alarm
int flamePin = D1; // Pin input sensor flame (ubah jika perlu)

String url;
WiFiClient client;

void setup() {
  pinMode(Alarm, OUTPUT);
  pinMode(flamePin, INPUT); // Pin flame sensor sebagai input
  Serial.begin(115200); // Inisialisasi komunikasi serial

  // Koneksi WiFi
  WiFi.hostname("NodeMCU");
  WiFi.begin(ssid, pass);

  // Tunggu sampai WiFi terhubung
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Menghubungkan ke WiFi...");
  }
  Serial.println("WiFi terhubung!");
}

void loop() {
  int flameState = digitalRead(flamePin); // Membaca status sensor

  if (flameState == LOW) { // Api terdeteksi
    Serial.println("Api terdeteksi!");
    msg_wa("Api terdeteksi!"); // Mengirim notifikasi WhatsApp
    digitalWrite(Alarm, HIGH);
  } else {
    Serial.println("Tidak ada api.");
    digitalWrite(Alarm, LOW);
  }

  delay(500); // Tunggu 500 ms sebelum membaca lagi
}

void msg_wa(String msg) {
  url = "http://api.callmebot.com/whatsapp.php?phone=6283872503123&text=" + urlencode(msg) + "&apikey=4750157";
  postData();
}

void postData() {
  int httpCode;
  HTTPClient http;
  http.begin(client, url);
  httpCode = http.POST("");

  if (httpCode == 200) {
    Serial.println("Notifikasi terkirim.");
  } else {
    Serial.println("Gagal mengirim notifikasi!");
  }

  http.end();
}

String urlencode(String str) {
  String encodedString = "";
  char c;
  char code0, code1;
  
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);

    if (c == ' ') {
      encodedString += '+';
    } else if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }

      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }

    yield();
  }
  
  return encodedString;
}
