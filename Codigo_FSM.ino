/* ==========================================================================
   PROYECTO P14 - RADAR ULTRASONICO DE BARRIDO 2.0
   Arquitectura de FSM no bloqueante con alertas sonoras
   ========================================================================== */

#include <ESP32Servo.h>

// ---------------------------------------------------------------------------
// 1. Configuracion
// ---------------------------------------------------------------------------
const uint8_t PIN_TRIG    = 5;
const uint8_t PIN_ECHO    = 18;
const uint8_t PIN_SERVO   = 19;
const uint8_t PIN_BOTON   = 25; // multifuncion: paro / rearme (soporta pull-down interno)
const uint8_t PIN_LED_AL  = 33; // rojo: indicador de error/alerta
const uint8_t PIN_BUZZER  = 27; // salida al buzzer

// Parametros del Buzzer
const bool     BUZZER_PASIVO  = true; // Generacion de onda por software
const uint32_t FREC_BUZZER_HZ = 2000; // [Hz] tono de la alerta

// Parametros de barrido
const uint8_t ANGULO_MIN  = 0;
const uint8_t ANGULO_MAX  = 180;
const uint8_t PASO_ANGULO = 5;

// Tiempos
const uint32_t T_ASENTAMIENTO_MS = 200; // Estabilizacion mecanica del SG90
const uint32_t T_ANTIRREBOTE_MS  = 80;  // Ventana de antirrebote de botones

// ---------------------------------------------------------------------------
// 2. Maquina de estados
// ---------------------------------------------------------------------------
enum Estado : uint8_t { BARRIDO, ASENTAMIENTO, MEDICION, ERROR_SEGURO };

Servo    miServo;
Estado   estado      = BARRIDO;
uint32_t t_entrada   = 0;
int16_t  angulo      = ANGULO_MIN;
int8_t   direccion   = 1;  // 1 = subiendo, -1 = bajando
uint32_t vuelta      = 1;
uint8_t  invalidas   = 0;

const uint8_t MAX_INVALIDAS = 3; 

const char* nombreEstado(Estado e) {
  switch (e) {
    case BARRIDO:      return "BARRIDO";
    case ASENTAMIENTO: return "ASENTAMIENTO";
    case MEDICION:     return "MEDICION";
    case ERROR_SEGURO: return "ERROR_SEGURO";
  }
  return "?";
}

void cambiar(Estado e, const char* motivo) {
  if (e == ERROR_SEGURO || estado == ERROR_SEGURO) {
    Serial.printf("# [%8lu ms] TRANSICION: %s -> %s  (%s)\n",
                  millis(), nombreEstado(estado), nombreEstado(e), motivo);
  }
  estado    = e;
  t_entrada = millis();
}

// ---------------------------------------------------------------------------
// 3. Entradas y salidas
// ---------------------------------------------------------------------------
bool boton() {
  static uint32_t t_ultimo    = 0;
  static bool     nivel_prev  = false;
  bool nivel  = digitalRead(PIN_BOTON) == HIGH;
  bool flanco = nivel && !nivel_prev && (millis() - t_ultimo >= T_ANTIRREBOTE_MS);
  if (flanco) t_ultimo = millis();
  nivel_prev = nivel;
  return flanco;
}

void buzzer(bool on) {
  if (!BUZZER_PASIVO) { digitalWrite(PIN_BUZZER, on ? HIGH : LOW); return; }

  static uint32_t t_us  = 0;
  static bool     nivel = false;
  if (!on) { digitalWrite(PIN_BUZZER, LOW); nivel = false; return; }

  const uint32_t semiperiodo_us = 500000UL / FREC_BUZZER_HZ;
  if (micros() - t_us >= semiperiodo_us) {
    t_us  = micros();
    nivel = !nivel;
    digitalWrite(PIN_BUZZER, nivel ? HIGH : LOW);
  }
}

float medirDistancia() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  
  long duracion = pulseIn(PIN_ECHO, HIGH, 30000); 
  
  if (duracion == 0) return -1.0; 
  return duracion * 0.0343 / 2.0;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_BOTON, INPUT_PULLDOWN); 
  pinMode(PIN_LED_AL, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  miServo.setPeriodHertz(50);
  miServo.attach(PIN_SERVO, 500, 2400);
  miServo.write(angulo);

  Serial.println();
  Serial.println("# ========================================================");
  Serial.println("# Proyecto P14 - Radar Ultrasónico con Alertas Sonoras");
  Serial.println("# ========================================================");
  Serial.println("Vuelta,Angulo,Distancia"); 

  cambiar(ASENTAMIENTO, "arranque");
}

// ---------------------------------------------------------------------------
// 4. Lazo principal
// ---------------------------------------------------------------------------
void loop() {
  switch (estado) {

    case BARRIDO:
      buzzer(false);
      digitalWrite(PIN_LED_AL, LOW);
      angulo += (PASO_ANGULO * direccion);
      
      if (angulo >= ANGULO_MAX) {
        angulo = ANGULO_MAX;
        direccion = -1;
        vuelta++;
      } else if (angulo <= ANGULO_MIN) {
        angulo = ANGULO_MIN;
        direccion = 1;
        vuelta++;
      }
      
      miServo.write(angulo);
      cambiar(ASENTAMIENTO, "movimiento iniciado");
      break;

    case ASENTAMIENTO:
      buzzer(false);
      if (millis() - t_entrada >= T_ASENTAMIENTO_MS) {
        cambiar(MEDICION, "asentamiento mecanico completo");
      }
      break;

    case MEDICION:
      {
        buzzer(false);
        float distancia = medirDistancia();
        
        if (distancia < 0) {
          invalidas++;
          if (invalidas >= MAX_INVALIDAS) {
            cambiar(ERROR_SEGURO, "sensor: 3 lecturas HC-SR04 invalidas");
          } else {
            cambiar(BARRIDO, "lectura invalida aislada");
          }
        } else {
          invalidas = 0;
          Serial.printf("%lu,%d,%.1f\n", vuelta, angulo, distancia);
          cambiar(BARRIDO, "medicion exitosa");
        }
      }
      break;

    case ERROR_SEGURO:
      digitalWrite(PIN_LED_AL, (millis() / 300) % 2);
      buzzer((millis() / 300) % 2); // Buzzer intermitente sincronizado con el LED
      
      if (boton()) {
        invalidas = 0;
        buzzer(false); // Apagar inmediatamente al rearmar
        cambiar(BARRIDO, "boton: REARME");
      }
      break;
  }

  // Guardia global del boton
  if (estado != ERROR_SEGURO) {
    if (boton()) {
      cambiar(ERROR_SEGURO, "boton: PARO");
    }
  }
}
