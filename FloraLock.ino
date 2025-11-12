#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <MPU6050.h>  // Librería para IMU MPU6050
#include <WebServer.h>  // ¡Falta esto antes!
#include <esp_sleep.h>

// Configuración WiFi y Telegram
const char* ssid = "Alumnos";
const char* password = "ITMH1234";
const String telegramToken = "7683862132:AAGJDjo_Hchti8T2WoXH67zsjAbk_vSibK0";  
const String chatId = "TU_CHAT_ID";  // ID DE CHAT (fallback si no se registra dinámico)

// Pines
#define PIR_PIN 2        // Sensor PIR movimiento
#define DOOR_PIN 4       // Switch puerta
#define HOOD_PIN 5       // Switch capó
#define ARM_PIN 15       // Pin para modo armado (conecta LED o switch)
#define SIREN_PIN 18     // Sirena
#define RELAY_PIN 19     // Relé para cortar encendido
#define SOLAR_VOLT A0    // Pin analógico para voltaje solar (divisor)
#define AUTO_VOLT A1     // Pin analógico para voltaje auto

// IMU
MPU6050 mpu;

// Variables
bool intrusion = false;
bool armed = true;  // Empieza armado
float solarVolt = 0;
float autoVolt = 0;
unsigned long lastCheck = 0;
const unsigned long sleepTime = 30000;  // Dormir cada 30s si no hay intrusión

// Variable global para chat ID dinámico (se guarda en EEPROM si quieres persistente después)
String dynamicChatId = "";  // Empieza vacío

WebServer server(80);  // ¡Declaración del servidor!

void setup() {
  Serial.begin(115200);
  
  // Pines
  pinMode(PIR_PIN, INPUT);
  pinMode(DOOR_PIN, INPUT_PULLUP);
  pinMode(HOOD_PIN, INPUT_PULLUP);
  pinMode(ARM_PIN, OUTPUT);  // Para indicar armado
  pinMode(SIREN_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(SIREN_PIN, LOW);
  digitalWrite(RELAY_PIN, HIGH);  // Relé activo alto para encendido normal
  digitalWrite(ARM_PIN, HIGH);    // Armado inicial
  
  // IMU
  Wire.begin(21, 22);  // SDA, SCL
  mpu.initialize();
  
  // WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando WiFi...");
  }
  Serial.println("WiFi conectado! IP: " + WiFi.localIP().toString());
  
  // Rutas servidor web
  server.on("/", handleRoot);      // Status principal
  server.on("/arm", [](){ armed = true; digitalWrite(ARM_PIN, HIGH); server.send(200, "text/plain", "Alarma ARMADA"); });
  server.on("/disarm", [](){ armed = false; digitalWrite(ARM_PIN, LOW); server.send(200, "text/plain", "Alarma DESARMADA"); });
  // Nueva ruta para registrar chat ID dinámico
  server.on("/setchat", [](){
    if (server.hasArg("chatid")) {
      dynamicChatId = server.arg("chatid");
      server.send(200, "text/plain", "Chat ID registrado: " + dynamicChatId);
      Serial.println("Chat ID dinámico registrado: " + dynamicChatId);
    } else {
      server.send(400, "text/plain", "Error: Envía ?chatid=TU_NUMERO");
    }
  });
  server.begin();
  
  // Config sleep con interrupciones (despierta en sensores)
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 1);  // PIR alto = wake
  esp_sleep_enable_ext1_wakeup(0xFFFFFFFF, ESP_GPIO_WAKEUP_ANY_HIGH);  // Cualquier pin alto
}

void loop() {
  // Monitoreo voltajes
  solarVolt = analogRead(SOLAR_VOLT) * (3.3 / 4095.0) * 5;  // Ajusta factor (ej: para 0-16V)
  autoVolt = analogRead(AUTO_VOLT) * (3.3 / 4095.0) * 14;   // Para 12V auto
  
  // Detección (solo si armado)
  bool pir = digitalRead(PIR_PIN);
  bool doorOpen = !digitalRead(DOOR_PIN);
  bool hoodOpen = !digitalRead(HOOD_PIN);
  
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  bool golpe = (abs(ax) > 5000 || abs(ay) > 5000 || abs(az) > 10000);  // Umbral
  
  intrusion = armed && (pir || doorOpen || hoodOpen || golpe);
  
  if (intrusion) {
    activarAlarma();
    lastCheck = millis();  // Reset timer
  } else {
    // Modo bajo consumo
    if (millis() - lastCheck > sleepTime) {
      Serial.println("Entrando en modo sleep...");
      esp_deep_sleep_start();  // Despierta con sensores/timer
    }
  }
  
  // Manejar web server
  server.handleClient();
  
  delay(100);  // Loop rápido
}

void activarAlarma() {
  Serial.println("¡INTRUSIÓN DETECTADA!");
  digitalWrite(SIREN_PIN, HIGH);   // Sirena on
  digitalWrite(RELAY_PIN, LOW);    // Cortar encendido
  
  // Notificación Telegram (usa dinámico si está set, sino el fijo)
  String currentChat = (dynamicChatId != "") ? dynamicChatId : chatId;
  if (WiFi.status() == WL_CONNECTED && currentChat != "TU_CHAT_ID") {  // Salta si es placeholder
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + telegramToken + "/sendMessage?chat_id=" + currentChat + "&text=¡Alerta FloraLock! Intrusión en tu auto. Voltaje solar: " + String(solarVolt) + "V";
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.println("Notificación enviada!");
    } else {
      Serial.println("Error en Telegram: " + String(httpCode));
    }
    http.end();
  } else {
    Serial.println("No hay chat ID válido, saltando Telegram.");
  }
  
  delay(5000);  // Sirena 5s
  digitalWrite(SIREN_PIN, LOW);
  digitalWrite(RELAY_PIN, HIGH);  // Restaurar
  armed = false;  // Desarma auto
  digitalWrite(ARM_PIN, LOW);
}

void handleRoot() {
  String html = R"(
<!DOCTYPE html>
<html><head><meta name="viewport" content="width=device-width, initial-scale=1"><style>
body { font-family: Arial; background: #222; color: #fff; text-align: center; padding: 20px; }
h1 { color: #0f0; }
button { background: #0f0; color: black; border: none; padding: 15px; font-size: 18px; margin: 10px; border-radius: 10px; width: 80%; }
button:disabled { background: #666; }
p { font-size: 16px; }
a { color: #0f0; text-decoration: none; }
</style></head><body>
<h1>🌿 FloraLock App</h1>
<p>Voltaje Solar: )" + String(solarVolt) + R"( V</p>
<p>Voltaje Auto: )" + String(autoVolt) + R"( V</p>
<p>Intrusión: )" + String(intrusion ? "SÍ ⚠️" : "No ✅") + R"(</p>
<p>Modo: )" + String(armed ? "ARMADO 🔒" : "DESARMADO 🔓") + R"(</p>
<p>Chat ID Actual: )" + String(dynamicChatId != "" ? dynamicChatId : "No registrado") + R"(</p>
<a href="/setchat?chatid=123456789"><button>Registrar Chat ID (ejemplo)</button></a>  <!-- Cambia el 123456789 por el tuyo -->
<button onclick="arm()">ARMAR Alarma</button>
<button onclick="disarm()">DESARMAR Alarma</button>
<script>
function arm() { fetch('/arm').then(() => location.reload()); }
function disarm() { fetch('/disarm').then(() => location.reload()); }
setInterval(() => location.reload(), 5000);  // Refresh auto cada 5s
</script>
</body></html>)";
  server.send(200, "text/html", html);
}