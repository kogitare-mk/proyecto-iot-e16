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

const float M_CAL = 1.0; 
const float B_CAL = 0.0; 

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
const uint8_t DIR_OLED = 0x3C;
bool bus_ok = false;

const bool     BUZZER_PASIVO  = true; 
const uint32_t FREC_BUZZER_HZ = 2000; 

const uint8_t  ANGULO_MIN  = 0;
const uint8_t  ANGULO_MAX  = 180;
const uint8_t  PASO_ANGULO = 5;
const uint32_t T_ASENTAMIENTO_MS = 200; 
const uint32_t T_ANTIRREBOTE_MS  = 80;  
const uint32_t TIMEOUT_ECO_US    = 6000; 
const uint8_t  UMBRAL_ERRORES_BARRIDO = 4;

enum Estado : uint8_t { BARRIDO, ASENTAMIENTO, DISPARAR_PULSO, ESPERAR_HIGH, ESPERAR_LOW, ACTUALIZAR_PANTALLA, ERROR_SEGURO };

Servo    miServo;
Estado   estado      = BARRIDO;
uint32_t t_entrada   = 0;
int16_t  angulo      = ANGULO_MIN;
int8_t   direccion   = 1;  
uint8_t  invalidas   = 0;
float    ultima_dist = -1.0; 
uint32_t t_eco_inicio = 0;
uint32_t t_timeout_inicio = 0;

const char* nombreEstado(Estado e) {
  switch (e) {
    case BARRIDO:             return "BARRIDO";
    case ASENTAMIENTO:        return "ASENTAMIENTO";
    case DISPARAR_PULSO:      return "DISPARAR";
    case ESPERAR_HIGH:        return "ESP_HIGH";
    case ESPERAR_LOW:        return "ESP_LOW";
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
      {
        buzzer(false);
        digitalWrite(PIN_LED_AL, LOW);
        angulo += (PASO_ANGULO * direccion);
        
        bool fin_vuelta = false;
        
        if (angulo >= ANGULO_MAX) {
          angulo = ANGULO_MAX;
          direccion = -1;
          fin_vuelta = true;
        } else if (angulo <= ANGULO_MIN) {
          angulo = ANGULO_MIN;
          direccion = 1;
          fin_vuelta = true;
        }
        
        if (fin_vuelta) { //Para verificar de manera mas efectiva un error, se siguieron los consejos de la NP1 y se implemento la lectura de invalidas al final del barrido 
          if (invalidas >= UMBRAL_ERRORES_BARRIDO) {
            cambiar(ERROR_SEGURO);
            break; 
          }
          invalidas = 0; 
        }
        
        miServo.write(angulo);
        cambiar(ASENTAMIENTO);
      }
      break;

    case ASENTAMIENTO:
      buzzer(false);
      if (millis() - t_entrada >= T_ASENTAMIENTO_MS) {
        cambiar(DISPARAR_PULSO);
      }
      break;

    case DISPARAR_PULSO: //Se cambio el estado MIDIENDO por uno en 3 etapas que elimina el bloqueo del pulseIN
      buzzer(false);
      digitalWrite(PIN_TRIG, LOW);
      delayMicroseconds(2);
      digitalWrite(PIN_TRIG, HIGH);
      delayMicroseconds(10);
      digitalWrite(PIN_TRIG, LOW);
      
      t_timeout_inicio = micros();
      cambiar(ESPERAR_HIGH);
      break;

    case ESPERAR_HIGH: // Congela el echo de inicio y pasa al siguiente estado
      if (digitalRead(PIN_ECHO) == HIGH) {
        t_eco_inicio = micros();
        cambiar(ESPERAR_LOW);
      } else if (micros() - t_timeout_inicio > TIMEOUT_ECO_US) {
        invalidas++;
        ultima_dist = -1.0; 
        cambiar(ACTUALIZAR_PANTALLA); 
      }
      break;

    case ESPERAR_LOW: //Lee en caso de que llegue correctamente el ECHO y realiza el calculo
      {
        if (digitalRead(PIN_ECHO) == LOW) {
          long duracion = micros() - t_eco_inicio;
          float distancia_cruda = duracion * 0.0341 / 2.0;
          ultima_dist = (distancia_cruda * M_CAL) + B_CAL; 
          
          Serial.printf("%lu,%d,%.1f,%s,%u\n", millis(), angulo, ultima_dist, nombreEstado(estado), bus_ok ? 1 : 0);
          cambiar(ACTUALIZAR_PANTALLA);
          
        } else if (micros() - t_eco_inicio > TIMEOUT_ECO_US) {
          invalidas++;
          ultima_dist = -1.0; 
          cambiar(ACTUALIZAR_PANTALLA); 
        }
      }
      break;

    case ACTUALIZAR_PANTALLA:
      Wire.beginTransmission(DIR_OLED);
      if (Wire.endTransmission() != 0) { //Agregamos una verificacion de conectividad para el display
        bus_ok = false;
        cambiar(ERROR_SEGURO);
      } else {
        bus_ok = true;
        mostrarOLED(angulo, ultima_dist, estado);
        cambiar(BARRIDO);
      }
      break;

    case ERROR_SEGURO:
      miServo.write(90); //Cambia el asentamiento a 90 grados segun lo recalcado en la GT2
      
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
