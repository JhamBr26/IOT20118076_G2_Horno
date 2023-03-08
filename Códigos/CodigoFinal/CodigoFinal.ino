#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>

// CONSTANTES
#define DHT 4   // Se define el pin del sensor de temperatura y humedad
#define MQ9 32  // Se define el pin del sensor de CO2
#define POS 19  // Se define el pin del sensor de posición
#define RELAY 2

// VARIABLES
// int RELAY = 2;
float temperatura = 0;  // Se inicializa la variable de la temperatura
float humedad = 0;      // Se inicializa la variable de la humedad
bool cerrado = false;   // Se inicializa la variable de la posición de la puerta
int co = 0;             // Se inicializa la variable de la concentración de CO2
float setpoint = 0;
const float band = 0;

// Se inicializa el dht
DHTesp dht;  // Se crea un objeto del tipo DHTesp para manejar el sensor DHT11

// Create the lcd object address 0x3F and 16 columns x 2 rows
LiquidCrystal_I2C lcd(0x27, 16, 2);  //

// Configuración de la red WiFi
const char* ssid = "GalaxyA205C7D";
const char* password = "xwcs2389";

// Configuración del servidor EMQ X
const char* mqtt_server = "192.168.182.214";
const int mqtt_port = 1883;
const char* mqtt_user = "admin";
const char* mqtt_password = "admin123";

// Configuración del cliente MQTT
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {

  // Inicialización del puerto serie
  Serial.begin(115200);           // Se inicia la comunicación serial
  dht.setup(DHT, DHTesp::DHT11);  // Se inicializa el sensor de temperatura y humedad
  pinMode(POS, INPUT);            // Se configura el pin del sensor de posición como entrada
  pinMode(RELAY, OUTPUT);
  Serial.println("Configuracion Inicial Realizada .....");  // Se envía un mensaje por la comunicación serial indicando que se ha terminado la configuración inicial

  // Conexión a la red WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a la red WiFi...");
    Serial.println(WiFi.status());
  }
  Serial.println("Conexión a la red WiFi establecida");

  // Conexión al servidor MQTT EMQ X
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.println("Connecting to MQTT server...");
    if (client.connect("ESP32", "admin", "admin123")) {
      Serial.println("Connected to MQTT server");
      client.subscribe("esp32/output");
      client.publish("esp32/output", "Mensaje de prueba");
    } else {
      Serial.print("Failed to connect to MQTT server, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }

  // Initialize the LCD connected
  lcd.begin();

  // Turn on the backlight on LCD.
  lcd.backlight();
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Toma de lecturas
  TempAndHumidity data = dht.getTempAndHumidity();  // Se lee la temperatura y humedad del sensor DHT11
  temperatura = data.temperature;                   // Se actualiza la variable de la temperatura
  humedad = data.humidity;                          // Se actualiza la variable de la humedad
  cerrado = digitalRead(POS);                       // Se lee la posición de la puerta
  co = map(analogRead(MQ9), 0, 4095, 0, 100);       // Se lee la concentración de CO2 y se mapea a un rango de 0 a 100

  // if(cerrado == 1) {
  //   RELAY = 14;
  // } else if(cerrado == 0) {
  //   RELAY = 2;
  // }

  //Salida Serial
  Serial.print("Temperatura: ");  // Se envía un mensaje por la comunicación serial indicando que se va a imprimir la temperatura
  Serial.println(temperatura);    // Se imprime la temperatura por la comunicación serial
  Serial.print("Humedad: ");      // Se envía un mensaje por la comunicación serial indicando que se va a imprimir la humedad
  Serial.println(humedad);        // Se imprime la humedad por la comunicación serial
  Serial.print("Posicion: ");     // Se envía un mensaje por la comunicación serial indicando que se va a imprimir la posición de la puerta
  Serial.println(cerrado);        // Se imprime la posición de la puerta por la comunicación serial
  Serial.print("CO: ");           // Se envía un mensaje por la comunicación serial indicando que se va a imprimir la concentración de CO2
  Serial.println(co);             // Se imprime la concentración de CO2 por la comunicación serial

  // Publicación de los datos en el servidor MQTT
  if (!isnan(temperatura)) {
    String message = "{\"temperatura\":" + String(temperatura) + "}";
    client.publish("esp32/temperatura", message.c_str());
    Serial.println("Datos de temperatura publicados en el servidor MQTT");
  } else {
    Serial.println("Error al leer los datos de temperatura");
  }

  if (!isnan(humedad)) {
    String message = "{\"humedad\":" + String(humedad) + "}";
    client.publish("esp32/humedad", message.c_str());
    Serial.println("Datos de humedad publicados en el servidor MQTT");
  } else {
    Serial.println("Error al leer los datos de humedad");
  }

  if (!isnan(cerrado)) {
    String message = "{\"posicion\":" + String(cerrado) + "}";
    client.publish("esp32/posicion", message.c_str());
    Serial.println("Datos de posicion publicados en el servidor MQTT");
  } else {
    Serial.println("Error al leer los datos de posicion");
  }

  if (!isnan(co)) {
    String message = "{\"monóxido de carbono\":" + String(co) + "}";
    client.publish("esp32/co", message.c_str());
    Serial.println("Datos de co publicados en el servidor MQTT");
  } else {
    Serial.println("Error al leer los datos de co");
  }

  // Limpiar la pantalla LCD
  // lcd.clear();

  // Imprimir los valores de temperatura, humedad, cerrado y co en la pantalla LCD
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(temperatura);
  lcd.print("C ");

  lcd.setCursor(0, 1);
  lcd.print("Hum: ");
  lcd.print(humedad);
  lcd.print("% ");

  lcd.setCursor(14, 0);
  lcd.print("Pos: ");
  lcd.print(cerrado);

  lcd.setCursor(13, 1);
  lcd.print("CO: ");
  lcd.print(co);
  lcd.print("%");


  lcd.autoscroll();

  // Espera de 5 segundos
  delay(1000);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    messageTemp += (char)payload[i];
  }
  Serial.println();

  if (String(topic) == "esp32/output") {
    if (messageTemp == "Encendido") {
      digitalWrite(RELAY, HIGH);
      client.publish("esp32/foco", "Encendido");
    } else if (messageTemp == "Apagado") {
      digitalWrite(RELAY, LOW);
      client.publish("esp32/foco", "Apagado");
    } else {
      // Control On/Off temperatura
      Serial.print("Cambiando el setpoint a ");
      Serial.println(messageTemp);
      setpoint = messageTemp.toInt();
      Serial.println(setpoint);
      if (temperatura > setpoint + band) {
        digitalWrite(RELAY, LOW);
        client.publish("esp32/foco", "Apagado");
      } else if (temperatura < setpoint - band) {
        digitalWrite(RELAY, HIGH);
        client.publish("esp32/foco", "Encendido");
      }
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.println("Connecting to MQTT server...");
    if (client.connect("ESP32", "admin", "admin123")) {
      Serial.println("Connected to MQTT server");
      client.subscribe("esp32/output");
      client.publish("esp32/output", "Mensaje de prueba");
    } else {
      Serial.print("Failed to connect to MQTT server, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}
