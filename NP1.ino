#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const uint8_t PIN_TRIG    = 5;
const uint8_t PIN_ECHO    = 18;
const uint8_t PIN_SERVO   = 19;
const uint8_t PIN_BOTON   = 25; 
const uint8_t PIN_LED_AL  = 33; 
const uint8_t PIN_BUZZER  = 27; 
//Aplicar correcion a M y B 07-09
const float M_CAL = 1.0; 
const float B_CAL = 0.0; 

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
const uint8_t DIR_OLED = 0x3C;
bool bus_ok = false;

const bool     BUZZER_PASIVO  = true; 
const uint32_t FREC_BUZZER_HZ = 2000; 

const uint8_t ANGULO_MIN  = 0;
const uint8_t ANGULO_MAX  = 180;
const uint8_t PASO_ANGULO = 5;
const uint32_t T_ASENTAMIENTO_MS = 200; 
const uint32_t T_ANTIRREBOTE_MS  = 80;  

enum Estado : uint8_t { BARRIDO, ASENTAMIENTO, MEDICION, ACTUALIZAR_PANTALLA, ERROR_SEGURO };

Servo    miServo;
Estado   estado      = BARRIDO;
uint32_t t_entrada   = 0;
int16_t  angulo      = ANGULO_MIN;
int8_t   direccion   = 1;  
uint8_t  invalidas   = 0;
float    ultima_dist = -1.0; 

const uint8_t MAX_INVALIDAS = 3; 

const char* nombreEstado(Estado e) {
  switch (e) {
    case BARRIDO:             return "BARRIDO";
    case ASENTAMIENTO:        return "ASENTAMIENTO";
    case MEDICION:            return "MEDICION";
    case ACTUALIZAR_PANTALLA: return "ACT_PANTALLA";
    case ERROR_SEGURO:        return "ERROR_SEGURO";
  }
  return "?";
}

void cambiar(Estado e) {
  estado    = e;
  t_entrada = millis();
}

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

void mostrarOLED(int ang, float dist, Estado est) {
  if (!bus_ok) return;
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.printf("ESTADO: %s\n", nombreEstado(est));
  
  display.setTextSize(2);
  display.setCursor(0, 16);
  display.printf("Ang: %d\n", ang);
  
  display.setCursor(0, 36);
  if (dist < 0) {
    display.println("D: ERROR");
  } else {
    display.printf("D: %.1f cm\n", dist);
  }
  
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.printf("BUS: I2C %s", bus_ok ? "OK" : "ERROR");
  
  display.display();
}

void setup() {
  Serial.begin(115200);
  
  Wire.begin(21, 22);
  Wire.setClock(400000); 
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, DIR_OLED)) {
    bus_ok = false;
  } else {
    bus_ok = true;
    display.setTextColor(WHITE);
  }

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_BOTON, INPUT_PULLDOWN); 
  pinMode(PIN_LED_AL, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  miServo.setPeriodHertz(50);
  miServo.attach(PIN_SERVO, 500, 2400);
  miServo.write(angulo);

  Serial.println("t_ms,angulo,distancia_cm,estado,bus_ok"); 

  cambiar(ASENTAMIENTO);
}

void loop() {
  switch (estado) {

    case BARRIDO:
      buzzer(false);
      digitalWrite(PIN_LED_AL, LOW);
      angulo += (PASO_ANGULO * direccion);
      
      if (angulo >= ANGULO_MAX) {
        angulo = ANGULO_MAX;
        direccion = -1;
      } else if (angulo <= ANGULO_MIN) {
        angulo = ANGULO_MIN;
        direccion = 1;
      }
      
      miServo.write(angulo);
      cambiar(ASENTAMIENTO);
      break;

    case ASENTAMIENTO:
      buzzer(false);
      if (millis() - t_entrada >= T_ASENTAMIENTO_MS) {
        cambiar(MEDICION);
      }
      break;

    case MEDICION:
      {
        buzzer(false);
        float distancia_cruda = medirDistancia();
        
        if (distancia_cruda < 0) {
          invalidas++;
          if (invalidas >= MAX_INVALIDAS) {
            cambiar(ERROR_SEGURO);
          } else {
            cambiar(BARRIDO);
          }
        } else {
          invalidas = 0;
          
          ultima_dist = (distancia_cruda * M_CAL) + B_CAL; 
          
          Serial.printf("%lu,%d,%.1f,%s,%u\n", millis(), angulo, ultima_dist, nombreEstado(estado), bus_ok ? 1 : 0);
          
          cambiar(ACTUALIZAR_PANTALLA);
        }
      }
      break;

    case ACTUALIZAR_PANTALLA:
      mostrarOLED(angulo, ultima_dist, estado);
      cambiar(BARRIDO);
      break;

    case ERROR_SEGURO:
      digitalWrite(PIN_LED_AL, (millis() / 300) % 2);
      buzzer((millis() / 300) % 2); 
      
      static uint32_t t_oled_error = 0;
      if (millis() - t_oled_error > 500) {
        mostrarOLED(angulo, -1.0, estado);
        Serial.printf("%lu,%d,-1.0,%s,%u\n", millis(), angulo, nombreEstado(estado), bus_ok ? 1 : 0);
        t_oled_error = millis();
      }
      
      if (boton()) {
        invalidas = 0;
        buzzer(false); 
        cambiar(BARRIDO);
      }
      break;
  }

  if (estado != ERROR_SEGURO) {
    if (boton()) {
      cambiar(ERROR_SEGURO);
    }
  }
}
