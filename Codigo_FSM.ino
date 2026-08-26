// ============================================================
// FSM Definitiva - P14 Radar Ultrasónico
// GT2 - Implementación No Bloqueante (Salida CSV)
// ============================================================

#include <ESP32Servo.h>

// ---------- 1. Configuración de Pines y Constantes ----------
const int PIN_TRIG  = 26;
const int PIN_ECHO  = 25;
const int PIN_SERVO = 33; 

const unsigned long TIMEOUT_US = 18000;         
const float VELOCIDAD_CM_US = 0.0343;
const unsigned long ASENTAMIENTO_MS = 200;      

const int ANGULO_INICIAL = 15;
const int ANGULO_FINAL   = 165;
int paso_grados = 5;
int angulo_actual = ANGULO_INICIAL;
int vuelta_actual = 1; // <-- Variable  para Python

// ---------- 2. Máquina de Estados (Enum) ----------
enum EstadoRadar {
  BARRIENDO,
  MIDIENDO,
  ESTADO_ERROR
};

EstadoRadar estado = BARRIENDO;
unsigned long t_estado = 0; 
Servo servo;

// ---------- 3. Función de Medición ----------
float medir_distancia() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  
  unsigned long t_us = pulseIn(PIN_ECHO, HIGH, TIMEOUT_US); 
  return (t_us == 0) ? -1.0 : (t_us * VELOCIDAD_CM_US) / 2.0;
}

// ---------- 4. Configuración Inicial ----------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  
  servo.attach(PIN_SERVO);
  servo.write(angulo_actual);
  
  Serial.println("vuelta,angulo,distancia_cm"); 
  t_estado = millis();
}

void loop() {
  unsigned long ahora = millis();

  switch (estado) {
    
    case BARRIENDO:
      if (ahora - t_estado >= ASENTAMIENTO_MS) {
        estado = MIDIENDO; 
      }
      break;

    case MIDIENDO: {
      float d = medir_distancia();
      
      if (d > 0) {
        Serial.printf("%d,%d,%.2f\n", vuelta_actual, angulo_actual, d);
        
        angulo_actual += paso_grados;
        if (angulo_actual >= ANGULO_FINAL || angulo_actual <= ANGULO_INICIAL) {
          paso_grados = -paso_grados; 
          vuelta_actual++; 
        }
        
        servo.write(angulo_actual); 
        estado = BARRIENDO;         
        t_estado = ahora;           
        
      } else {
        Serial.println("ALERTA: Sin eco. Entrando en modo de seguridad.");
        estado = ESTADO_ERROR;
      }
      break;
    }

    case ESTADO_ERROR:
      servo.write(90); 
      break;
  }

  static unsigned long t_latido = 0;
  if (ahora - t_latido >= 1000) { 
    // Serial.print("."); 
    t_latido = ahora;
  }
}