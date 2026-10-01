#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ---------- Pines ----------
#define PIN_DHT        4
#define PIN_POT        34
#define PIN_LED_VENT   12   // LED rojo: ventilación
#define PIN_LED_RIEGO  26   // LED celeste: riego
#define PIN_BOTON      27

// ---------- Configuración del OLED ----------
#define ANCHO_OLED  128
#define ALTO_OLED   64
#define DIR_OLED    0x3C

// ---------- Configuración del sensor ----------
#define TIPO_DHT              DHT22
#define INTERVALO_LECTURA_MS  2000   // el DHT22 necesita al menos 2 segundos entre lecturas

// ---------- Configuración de la pantalla ----------
#define INTERVALO_PANTALLA_MS 250    // cada cuánto se redibuja el OLED

// ---------- Configuración del potenciómetro ----------
#define ADC_MAXIMO      4095.0       // la ESP32 lee de 0 a 4095 (12 bits)
#define TEMP_REF_MIN    10.0         // con el potenciómetro al mínimo
#define TEMP_REF_MAX    40.0         // con el potenciómetro al máximo

// ---------- Configuración del riego ----------
#define HUMEDAD_UMBRAL_MIN     40    // rango del umbral aleatorio de humedad
#define HUMEDAD_UMBRAL_MAX     60
#define INTERVALO_PARPADEO_MS  300   // velocidad del parpadeo del LED de riego

// ---------- Configuración del botón ----------
#define DEBOUNCE_MS 200

// ---------- Objetos ----------
DHT dht(PIN_DHT, TIPO_DHT);
Adafruit_SSD1306 oled(ANCHO_OLED, ALTO_OLED, &Wire, -1);

// ---------- Variables globales ----------
float temperatura = 0;
float humedad = 0;
float temperaturaReferencia = TEMP_REF_MIN;
bool ventilacionActiva = false;
bool ventilacionActivaAnterior = false;

int humedadUmbral = 0;
bool riegoActivo = false;
bool riegoActivoAnterior = false;
bool ledRiegoEncendido = false;

int pantallaActual = 0;   // 0 = temperatura, 1 = humedad
bool estadoBotonAnterior = HIGH;
unsigned long ultimoCambioBoton = 0;

unsigned long ultimaLectura = 0;
unsigned long ultimaPantalla = 0;
unsigned long ultimoParpadeo = 0;

// Lee el sensor y guarda los valores. Si falla, no pisa los valores anteriores.
void leerSensor() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println("Error al leer el DHT22");
    return;
  }

  temperatura = t;
  humedad = h;
}

// Convierte la posición del potenciómetro en una temperatura de referencia
void leerPotenciometro() {
  int lectura = analogRead(PIN_POT);   // 0 a 4095
  float ref = TEMP_REF_MIN + (lectura / ADC_MAXIMO) * (TEMP_REF_MAX - TEMP_REF_MIN);

  // Redondeamos de a 0,5 °C para que el valor no "tiemble" en pantalla
  temperaturaReferencia = round(ref * 2) / 2.0;
}

// Decide si la ventilación está activa y prende o apaga el LED rojo
void actualizarVentilacion() {
  ventilacionActiva = (temperatura > temperaturaReferencia);
  digitalWrite(PIN_LED_VENT, ventilacionActiva ? HIGH : LOW);

  if (ventilacionActiva != ventilacionActivaAnterior) {
    Serial.print("Ventilacion ");
    Serial.println(ventilacionActiva ? "ACTIVADA (LED ON)" : "DESACTIVADA (LED OFF)");
    ventilacionActivaAnterior = ventilacionActiva;
  }
}

// Decide si el riego está activo y hace parpadear el LED celeste mientras lo esté
void actualizarRiego() {
  riegoActivo = (humedad < humedadUmbral);

  if (riegoActivo != riegoActivoAnterior) {
    Serial.print("Riego ");
    Serial.println(riegoActivo ? "ACTIVADO (LED intermitente)" : "DESACTIVADO");
    riegoActivoAnterior = riegoActivo;
  }

  if (riegoActivo) {
    unsigned long ahora = millis();
    if (ahora - ultimoParpadeo >= INTERVALO_PARPADEO_MS) {
      ultimoParpadeo = ahora;
      ledRiegoEncendido = !ledRiegoEncendido;
      digitalWrite(PIN_LED_RIEGO, ledRiegoEncendido ? HIGH : LOW);
    }
  } else {
    ledRiegoEncendido = false;
    digitalWrite(PIN_LED_RIEGO, LOW);
  }
}

// Lee el botón con antirrebote y alterna la pantalla mostrada en el OLED
void leerBoton() {
  bool estadoBoton = digitalRead(PIN_BOTON);   // botón a GND -> LOW al presionar
  unsigned long ahora = millis();

  if (estadoBoton == LOW && estadoBotonAnterior == HIGH &&
      (ahora - ultimoCambioBoton) >= DEBOUNCE_MS) {
    ultimoCambioBoton = ahora;
    pantallaActual = 1 - pantallaActual;
  }

  estadoBotonAnterior = estadoBoton;
}

// Pantalla 1: temperatura actual, referencia y estado de la ventilación
void mostrarPantallaTemperatura() {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("TEMPERATURA (1/2)");

  oled.setTextSize(2);
  oled.setCursor(0, 14);
  oled.print(temperatura, 1);
  oled.println(" C");

  oled.setTextSize(1);
  oled.setCursor(0, 38);
  oled.print("Referencia: ");
  oled.print(temperaturaReferencia, 1);
  oled.println(" C");

  oled.setCursor(0, 50);
  oled.print("Ventilacion: ");
  oled.println(ventilacionActiva ? "ON" : "OFF");

  oled.display();
}

// Pantalla 2: humedad actual, umbral aleatorio y estado del riego
void mostrarPantallaHumedad() {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("HUMEDAD (2/2)");

  oled.setTextSize(2);
  oled.setCursor(0, 14);
  oled.print(humedad, 1);
  oled.println(" %");

  oled.setTextSize(1);
  oled.setCursor(0, 38);
  oled.print("Umbral: ");
  oled.print(humedadUmbral);
  oled.println(" %");

  oled.setCursor(0, 50);
  oled.print("Riego: ");
  oled.println(riegoActivo ? "ON" : "OFF");

  oled.display();
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(PIN_LED_VENT, OUTPUT);
  pinMode(PIN_LED_RIEGO, OUTPUT);
  pinMode(PIN_BOTON, INPUT_PULLUP);
  digitalWrite(PIN_LED_VENT, LOW);
  digitalWrite(PIN_LED_RIEGO, LOW);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, DIR_OLED)) {
    Serial.println("No se encontro el OLED");
    while (true) {}   // se queda frenado si no hay pantalla
  }

  // Umbral aleatorio de humedad mínima deseada, generado al iniciar el sistema
  randomSeed(esp_random());
  humedadUmbral = random(HUMEDAD_UMBRAL_MIN, HUMEDAD_UMBRAL_MAX + 1);

  Serial.println("=== Sistema de control de invernadero iniciado ===");
  Serial.print("Umbral de humedad generado: ");
  Serial.print(humedadUmbral);
  Serial.println(" %");

  leerSensor();
}

void loop() {
  unsigned long ahora = millis();

  // El sensor es lento: lo leemos cada 2 segundos
  if (ahora - ultimaLectura >= INTERVALO_LECTURA_MS) {
    ultimaLectura = ahora;
    leerSensor();
  }

  // El potenciómetro y el botón son instantáneos: se leen en cada vuelta
  leerPotenciometro();
  leerBoton();

  actualizarVentilacion();
  actualizarRiego();

  // La pantalla se refresca cada tanto para que responda rápido al potenciómetro
  if (ahora - ultimaPantalla >= INTERVALO_PANTALLA_MS) {
    ultimaPantalla = ahora;
    if (pantallaActual == 0) {
      mostrarPantallaTemperatura();
    } else {
      mostrarPantallaHumedad();
    }
  }
}
