// Pregunta: ¿En qué momentos durante las últimas 24 horas el radar detectó un objeto a una distancia crítica (menor a 10 cm)?
from(bucket: "mediciones")
  |> range(start: -24h)
  |> filter(fn: (r) => r._measurement == "mediciones")
  |> filter(fn: (r) => r.equipo == "E16" and r.proyecto == "P14" and r.nodo == "radar")
  |> filter(fn: (r) => r._field == "distancia")
  |> filter(fn: (r) => r._value < 10)
  |> yield(name: "alertas_proximidad")