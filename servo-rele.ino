// Biliotecas a usar
#include <Arduino.h>
#include <ESP32Servo.h> 
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h> 
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Cpmfiguracion del MODO AP
#define AP_SSID "ESP32_Config" 
#define AP_PASSWORD "12345678"
#define EEPROM_SIZE 128 

// Variables para almacenar credenciales WiFi
String wifiSSID = "";
String wifiPassword = "";

// Firebase configuration
#define API_KEY "AIzaSyCfWNCq64wbz4RR2Wx8i8pbcMb0PmyH3-o"
#define USER_EMAIL "mervinr6@gmail.com"
#define USER_PASSWORD "mervin06"
#define DATABASE_URL "prueba3-652e6-default-rtdb.firebaseio.com"

// Firebase objetos
FirebaseData stream;
FirebaseAuth auth;
FirebaseConfig config;

String listenerPath = "board1/outputs/digital/"; // Ruta en Firebase para escuchar cambios

// Definicion Servos
Servo servoMG995;  // Servo de rotación continua 
Servo servoSG90;   // Servo de 90° 
const int servoMG995Pin = 12; // Pin al que está conectado el MG995
const int servoSG90Pin = 13;  // Pin al que está conectado el SG90

// Definicion Relé
const int relayPin = 14; // Pin al que está conectado el relé

// Web server
WebServer server(80);

// Inicializar WiFi
void initWiFi() {
  // Leer credenciales de la EEPROM
  wifiSSID = readEEPROM(0);
  wifiPassword = readEEPROM(64);

  if (wifiSSID.length() > 0) {
    Serial.println("Conectando a WiFi guardado...");
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConectado. IP: " + WiFi.localIP().toString());
      return;
    }
  }

  // Si no se pudo conectar, entrar en modo AP
  Serial.println("No se pudo conectar. Iniciando modo AP...");
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.println("Modo AP iniciado. SSID: " + String(AP_SSID) + ", Contraseña: " + String(AP_PASSWORD));
  Serial.println("IP del AP: " + WiFi.softAPIP().toString());

  // Iniciar servidor web para configuración
  startWebServer();
}

// Leer datos de la EEPROM
String readEEPROM(int offset) {
  String data = "";
  for (int i = offset; i < offset + 64; i++) {
    char c = EEPROM.read(i);
    if (c == 0) break;
    data += c;
  }
  return data;
}

// Escribir datos en la EEPROM
void writeEEPROM(int offset, String data) {
  for (int i = 0; i < data.length(); i++) {
    EEPROM.write(offset + i, data[i]);
  }
  EEPROM.write(offset + data.length(), 0); // Null terminator
  EEPROM.commit();
}

// Servidor web para configuración
void startWebServer() {
  server.on("/", HTTP_GET, []() {
    String html = "<form method='post' action='/save'>"
                  "SSID: <input type='text' name='ssid'><br>"
                  "Contraseña: <input type='password' name='password'><br>"
                  "<input type='submit' value='Guardar'>"
                  "</form>";
    server.send(200, "text/html", html);
  });

  server.on("/save", HTTP_POST, []() {
    wifiSSID = server.arg("ssid");
    wifiPassword = server.arg("password");
    writeEEPROM(0, wifiSSID);
    writeEEPROM(64, wifiPassword);
    server.send(200, "text/html", "Credenciales guardadas. Reiniciando...");
    delay(2000);
    ESP.restart();
  });

  server.begin();
  Serial.println("Servidor web iniciado.");
}

// Función para activar la rotación continua del MG995
void activarMG995() {
  servoMG995.write(0); // Gira en una dirección
  Serial.println("MG995: Girando");
}

// Función para mover el SG90 (abrir y cerrar compuerta)
void moverSG90() {
  // Mover de 90° a 0° (abrir compuerta)
  for (int pos = 90; pos >= 0; pos -= 1) {
    servoSG90.write(pos);
    delay(15); // Ajusta la velocidad del movimiento
  }
  Serial.println("SG90: Compuerta abierta");

  delay(1000); // Espera 1 segundo

  // Mover de 0° a 90° (cerrar compuerta)
  for (int pos = 0; pos <= 90; pos += 1) {
    servoSG90.write(pos);
    delay(15); // Ajusta la velocidad del movimiento
  }
  Serial.println("SG90: Compuerta cerrada");
}

// Función para controlar el relé
void controlarRele(int state) {
  if (state == 1) {
    digitalWrite(relayPin, LOW); // Encender el relé (0V)
    Serial.println("Relé: Encendido");
  } else {
    digitalWrite(relayPin, HIGH); // Apagar el relé (5V)
    Serial.println("Relé: Apagado");
  }
}

// Callback function que se ejecuta cuando hay cambios en la base de datos
void streamCallback(FirebaseStream data) {
  Serial.println("Cambio detectado en Firebase:");
  Serial.println("Ruta: " + String(data.dataPath()));
  Serial.println("Valor: " + String(data.intData()));

  if (data.dataTypeEnum() == fb_esp_rtdb_data_type_integer) {
    int state = data.intData();
    if (data.dataPath() == "/rele") { // Ruta específica para el relé
      controlarRele(state); // Controlar el relé
    } else if (state == 1) {
      activarMG995(); // Activar el MG995
    } else if (state == 2) {
      moverSG90(); // Activar el SG90
    } else {
      servoMG995.write(90); // Detener el MG995
      Serial.println("MG995: Detenido");
    }
  }
}

void streamTimeoutCallback(bool timeout) {
  if (timeout) Serial.println("Error: Timeout en Firebase");
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE); // Inicializar EEPROM

  // Inicializar los servos
  servoMG995.attach(servoMG995Pin);
  servoSG90.attach(servoSG90Pin);
  servoMG995.write(90); // Inicialmente, el MG995 está detenido
  servoSG90.write(90);  // Inicialmente, el SG90 está en posición cerrada
  Serial.println("Servos: Inicializados");

  // Inicializar el relé
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH); // Inicialmente, el relé está apagado
  Serial.println("Relé: Inicializado y apagado");

  // Inicializar WiFi
  initWiFi();

  // Solo configurar Firebase si estamos conectados a una red WiFi
  if (WiFi.status() == WL_CONNECTED) {
    // Configurar Firebase
    config.api_key = API_KEY;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;
    config.database_url = DATABASE_URL;

    Firebase.reconnectWiFi(true);
    config.token_status_callback = tokenStatusCallback;
    config.max_token_generation_retry = 5;

    Firebase.begin(&config, &auth);

    // Iniciar el stream en la base de datos
    if (!Firebase.RTDB.beginStream(&stream, listenerPath.c_str()))
      Serial.println("Error al iniciar stream: " + String(stream.errorReason().c_str()));

    // Asignar la función callback para detectar cambios
    Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);

    Serial.println("Firebase: Configurado y listo");
  } else {
    Serial.println("Firebase: No configurado (modo AP activo)");
  }

  delay(2000);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    server.handleClient(); // Manejar solicitudes del servidor web
  } else {
    if (Firebase.isTokenExpired()) {
      Firebase.refreshToken(&config);
      Serial.println("Token de Firebase actualizado");
    }
  }
}
