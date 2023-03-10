#include <LiquidCrystal_I2C.h>  // Librería para el LCD
#include <WiFi.h>               // Librería para el WiFi del ESP32
#include <PubSubClient.h>       // Librería para conexión con el servidor MQTT
#include <DHTesp.h>             // Librería para el DHT11

// CONSTANTES
#define DHT 4    // Se define el pin del sensor de temperatura y humedad
#define MQ9 32   // Se define el pin del sensor de CO2
#define POS 19   // Se define el pin del sensor de posición
#define RELAY 2  // Se define el pin del relay

// VARIABLES
float temperatura = 0;  // Se inicializa la variable de la temperatura
float humedad = 0;      // Se inicializa la variable de la humedad
bool cerrado = false;   // Se inicializa la variable de la posición de la puerta
int co = 0;             // Se inicializa la variable de la concentración de CO2
float setpoint = 0;     // Se inicializa el umbral de control por temperatura
const float band = 0;   // Se define la banda de tolerancia del control por temperatura

// Se inicializa el dht
DHTesp dht;  // Se crea un objeto del tipo DHTesp para manejar el sensor DHT11

// Se crea el objeto LCD con dirección 0x3F y 16 columnas x 2 filas
LiquidCrystal_I2C lcd(0x27, 16, 2);  //

// Configuración de la red WiFi
const char* ssid = "TP-LINK_7D36DE";
const char* password = "30744738";

// Configuración del servidor EMQ X
const char* mqtt_server = "192.168.1.5";
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
    Serial.println("Conectando al servidor MQTT...");
    if (client.connect("ESP32", "admin", "admin123")) {
      Serial.println("Conectado al servidor MQTT");
      client.subscribe("esp32/output");
      client.publish("esp32/output", "Mensaje de prueba");
    } else {
      Serial.print("Falló la conexión al servidor MQTT, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }

  // Se inicializa el LCD
  lcd.begin();

  // Se enciende la luz del LCD
  lcd.backlight();
}

void loop() {

  // Si no existe conexión con el servidor MQTT se reintenta la conexión
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
    String message = "{\"temperatura\":" + String(temperatura) + "}";  // Se crea un String en formato JSON que contiene los datos que deseamos enviar
    client.publish("esp32/temperatura", message.c_str());              // Se publica el mensaje en el servidor MQTT dentro del tópico esp32/temperatura
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

  // Se realiza un autoscroll para mostrar todos los datos en el LCD
  lcd.autoscroll();

  // Espera de 1 segundo
  delay(1000);
}

// Esta función es el callback que se ejecuta cuando llega un mensaje al cliente MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  // Imprime el mensaje recibido y el tema al que pertenece
  Serial.print("Mensaje recibido [");
  Serial.print(topic);
  Serial.print("] ");

  // Crea una variable String para almacenar el mensaje recibido
  String messageTemp;

  // Recorre los bytes del mensaje recibido y los imprime en el monitor serial
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    // Agrega cada byte del mensaje a la variable messageTemp
    messageTemp += (char)payload[i];
  }

  // Verifica si el tema del mensaje recibido es "esp32/output"
  if (String(topic) == "esp32/output") {
    // Si el mensaje es "Encendido", activa el relé y publica un mensaje en el tema "esp32/foco"
    if (messageTemp == "Encendido") {
      digitalWrite(RELAY, HIGH);
      client.publish("esp32/foco", "Encendido");
    }
    // Si el mensaje es "Apagado", desactiva el relé y publica un mensaje en el tema "esp32/foco"
    else if (messageTemp == "Apagado") {
      digitalWrite(RELAY, LOW);
      client.publish("esp32/foco", "Apagado");
    }
    // Si el mensaje no es "Encendido" ni "Apagado", se asume que es el valor deseado del setpoint para controlar la temperatura
    else {
      // Imprime el valor deseado del setpoint en el monitor serial
      Serial.print("Cambiando el setpoint a ");
      Serial.println(messageTemp);
      // Convierte el valor deseado del setpoint a un entero y lo guarda en la variable setpoint
      setpoint = messageTemp.toInt();
      // Imprime el valor actual de setpoint en el monitor serial
      Serial.println(setpoint);
      // Si la temperatura actual es mayor que el setpoint más la banda de tolerancia, se apaga el relé y se publica un mensaje en el tema "esp32/foco"
      if (temperatura > setpoint + band) {
        digitalWrite(RELAY, LOW);
        client.publish("esp32/foco", "Apagado");
      }
      // Si la temperatura actual es menor que el setpoint menos la banda de tolerancia, se enciende el relé y se publica un mensaje en el tema "esp32/foco"
      else if (temperatura < setpoint - band) {
        digitalWrite(RELAY, HIGH);
        client.publish("esp32/foco", "Encendido");
      }
    }
  }
}

void reconnect() {
  // Loop que se ejecuta hasta que se haya reconectado al servidor MQTT
  while (!client.connected()) {
    Serial.println("Conectando al servidor MQTT...");
    if (client.connect("ESP32", "admin", "admin123")) {
      Serial.println("Conectado al servidor MQTT");
      client.subscribe("esp32/output");
      client.publish("esp32/output", "Mensaje de prueba");
    } else {
      Serial.print("Falló la conexión al servidor MQTT, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}
