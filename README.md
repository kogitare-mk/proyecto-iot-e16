# Seguimiento y funcionamiento del proyecto P14 - Radar Ultrasónico de barrido
Integrantes del proyecto: Martin Ferreira, Lukas Fuentes, Lucas Miranda, Martin Caniuñir, Ángel Sepulveda

__________________________________________________________________________________________________________________

GT1-Sem3: https://wokwi.com/projects/472364804717868033 (Link proyecto en wokwi) - Datos simulados

m_CAL: 0.96 ; b_CAL: 2.094 ; Tolerancia: 4cm ; N_filtro: 5 (Usamos un N bajo para que el filtro limpie el ruido)

__________________________________________________________________________________________________________________

Sensor: HC-SR04 | Familia: D
Referencia: regla o huincha, contrastada en dos distancias del rango de trabajo
Referencia validada por: <Reiner>

| Condicion                          | Valor declarado |
|------------------------------------|-----------------|
| Alimentacion del modulo            | 5 V |
| Divisor en ECHO                    | 10 k / 20 k; tension medida en el punto medio: <V> |
| Pines                              | TRIG GPIO 5, ECHO GPIO 18 |
| Temperatura ambiente               | <20°C> (la velocidad del sonido depende de ella) |
| Velocidad del sonido empleada      | <343.4m/s> (343 a 20 °C) |
| Pausa entre disparos               | <200ms> |
| Arco del barrido                   | de 0 a 180, paso 5 grados |
| Montaje del sensor sobre el brazo  | <firme y centrado / con juego> |
| Alimentacion del servo             | riel de 5 V, masa comun: si |

### Tolerancia declarada ANTES de verificar
| Criterio                                        | Tolerancia aceptada |
|--------------------------------------------------|---------------------|
| Desviacion respecto de la regla                   | <+/- 3 cm> |
| Porcentaje de mediciones expiradas aceptado       | <10%> |

### Contraste con regla, dos distancias del rango de trabajo
| Distancia real (cm) | Media del sensor (cm) | Dispersion (cm) | Desviacion (cm) | Desviacion (%) | Expiradas (%) |
|---------------------|-----------------------|-----------------|-----------------|----------------|---------------|
| <>                  | <>                    | <>              | <>              | <>             | <>            |
| <>                  | <>                    | <>              | <>              | <>             | <>            |

### Caso fuera de rango
| Situacion provocada            | Respuesta del sensor | Tratamiento en el firmware |
|--------------------------------|----------------------|----------------------------|
| Sin obstaculo (apuntando al vacio) | <expira / 0>  | <Gatilla timeout, transiciona a ESTADO_ERROR y mueve el servo a 90 grados (salida segura).>                     |
| Objeto dentro de la zona muerta    | <lecturas erráticas>                | <Se filtrará mediante la tolerancia o se ignorará si excede los límites lógicos.>                         |

### Tiempo de asentamiento del servo (paso 3 + script)
Salto empleado: de <grados> a <grados>   Banda de estabilidad: <cm> (del paso 2)

| Repeticion | Valor final (cm) | Asentado a los (ms) |
|------------|------------------|---------------------|
| 1          | <>               | <>                  |
| 2          | <>               | <>                  |
| 3          | <>               | <>                  |
| 4          | <>               | <>                  |
| 5          | <>               | <>                  |

Asentamiento medio: <ms>    PEOR caso: <ms>
Parametro adoptado: ASENTAMIENTO_MS = <valor>

Se adopta el PEOR caso redondeado hacia arriba, no el promedio: un angulo mal
medido de cada cinco basta para deformar el mapa completo, y en un mapa el error
de un solo punto se ve.

### Barrido (paso 4 + script)
Escena declarada ANTES de barrer:
<describir que hay delante, en que angulo aproximado y a que distancia medida
con regla>

| Parametro                          | Valor medido |
|------------------------------------|--------------|
| Angulos por vuelta                 | <37>          |
| Vueltas registradas                | <5>          |
| Angulos sin eco                    | <10%>          |
| Repetibilidad entre vueltas        | <3cm> de dispersion media |
| Peor angulo                        | <grados> (<cm>) |
| Duracion de una vuelta completa    | <ms>         |

Los angulos sin eco se concentran en zonas donde el pulso ultrasónico impacta superficies con ángulo no perpendicular. Explicacion del equipo:
La señal acústica sufre reflexión especular al chocar contra superficies inclinadas, desviando la onda sonora fuera del alcance del receptor HC-SR04 y provocando un timeout por pérdida de eco.
