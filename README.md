# 🤖 Robot de Escritorio con ESP32-C3 y Pantalla OLED

Un pequeño robot interactivo para escritorio, construido con un **ESP32-C3, una pantalla OLED de 0.96" y un sensor táctil**.

El robot cuenta con ojos animados, diferentes expresiones faciales, reloj digital y un minijuego inspirado en Flappy Bird. Además, incorpora un servidor web que permite controlarlo desde el celular mediante WiFi, sin necesidad de instalar ninguna aplicación.

## ✨ Características

### 👀 Expresiones animadas
- Ojos animados con movimientos suaves.
- Parpadeo automático.
- Movimiento de pupilas para simular que el robot mira hacia diferentes direcciones.
- Expresiones faciales:
  - Normal.
  - Feliz.
  - Sorpresa.
  - Guiño.
  - Corazones.
- Animación de descanso después de un período de inactividad.

### 🕒 Reloj digital
- Muestra la hora y la fecha en la pantalla OLED.
- Sincronización automática desde el navegador del celular.
- La hora continúa avanzando aunque se cierre la página de control.
- Después de 30 segundos de inactividad, el robot cambia automáticamente al modo reloj.

### 🐤 Minijuego Flappy Bird
- Versión simplificada de Flappy Bird adaptada a la pantalla OLED.
- Control mediante el sensor táctil.
- Obstáculos generados aleatoriamente.
- Sistema de puntuación y récord durante la ejecución.

### 📱 Control desde el celular
El ESP32-C3 crea su propia red WiFi y aloja una interfaz web para controlar el robot.

Desde el navegador se puede:
- Cambiar entre el modo robot, reloj y juego.
- Seleccionar las diferentes expresiones faciales.
- Sincronizar la hora del celular.
- Consultar el estado actual del robot.

No requiere conexión a Internet ni instalar aplicaciones adicionales.

## 🧰 Componentes

| Componente | Cantidad |
|---|---:|
| ESP32-C3 Dev Module | 1 |
| Pantalla OLED SSD1306 0.96" I2C (128x64) | 1 |
| Sensor táctil capacitivo TTP223 | 1 |
| Cables Dupont | Según necesidad |
| Estructura impresa en 3D | 1 |

## 🔌 Conexiones

https://github.com/electrodeuna/desk-buddy-esp32/blob/main/diagrama.png

### Pantalla OLED SSD1306

| OLED | ESP32-C3 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 4 |
| SCL | GPIO 5 |

### Sensor táctil TTP223

| TTP223 | ESP32-C3 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| OUT | GPIO 3 |

**Importante:** El sensor está configurado para entregar una señal HIGH cuando se toca.

## 💻 Software y bibliotecas

El proyecto está desarrollado en C++ utilizando Arduino IDE.

### Requisitos

- Arduino IDE.
- Soporte para placas ESP32 de Espressif.
- Bibliotecas:
  - Adafruit SSD1306.
  - Adafruit GFX Library.

Las bibliotecas se pueden instalar desde el administrador de bibliotecas de Arduino IDE.

### Configuración de la placa

Seleccionar:

`ESP32C3 Dev Module`

También se puede utilizar la configuración específica correspondiente a la placa ESP32-C3 utilizada.

### Cargar el programa

1. Descargar o clonar este repositorio.
2. Abrir el archivo `RobotEscritorio.ino` en Arduino IDE.
3. Instalar las bibliotecas necesarias.
4. Conectar el ESP32-C3 mediante USB.
5. Seleccionar la placa y el puerto COM correspondiente.
6. Cargar el programa.

## 📶 Cómo conectar el robot al celular

Una vez encendido, el robot crea una red WiFi propia.

**Datos de conexión:**

| Parámetro | Valor |
|---|---|
| Red WiFi | ElectroRobot |
| Contraseña | robot1234 |
| Dirección web | http://192.168.4.1 |

### Pasos

1. Desde el celular, buscar la red WiFi `ElectroRobot`.
2. Conectarse utilizando la contraseña `robot1234`.
3. Abrir el navegador.
4. Ingresar a `http://192.168.4.1`.
5. ¡Listo! Ya se puede controlar el robot.

La interfaz permite seleccionar los modos de funcionamiento y cambiar las expresiones del personaje.

## 👆 Controles táctiles

El sensor TTP223 permite interactuar directamente con el robot.

| Acción | Función |
|---|---|
| Toque corto | Despierta al robot y muestra una expresión alegre |
| Soltar después de un toque | Cambia entre expresiones |
| Mantener presionado | Muestra ojos con corazones |
| Toque en modo reloj | Regresa al modo robot |
| Toque en el juego | Hace saltar al personaje |
| Toque después de perder | Reinicia el juego |

## ⚙️ Funcionamiento automático

El robot incorpora diferentes comportamientos para darle más personalidad:

- Parpadea automáticamente.
- Mueve los ojos de manera aleatoria.
- Cambia suavemente entre expresiones.
- Después de 30 segundos sin interacción, activa el reloj.
- Después de 60 segundos de inactividad, muestra una animación de descanso.

## 🕒 Sincronización del reloj

La hora se obtiene del dispositivo que abre la interfaz web.

Cuando se conecta al robot, el navegador envía la hora local al ESP32-C3. A partir de ese momento, el microcontrolador mantiene el conteo utilizando su temporizador interno.

**Importante:** La hora no se conserva después de reiniciar o apagar el ESP32. Es necesario volver a sincronizarla desde el celular.

## 📁 Estructura del proyecto

```text
robot-de-escritorio/
│
├── robot-de-escritorio.ino
├── README.md
└── 3d-models/
    └── (Archivos STL de la estructura)
```


## 🎥 Proyecto de Electro De Una

Este proyecto forma parte de **Electro De Una**, un canal dedicado a la electrónica, Arduino, ESP32, robótica y proyectos DIY.

Si te gustan estos proyectos, podés seguir el canal:

[▶️ YouTube - Electro De Una](https://www.youtube.com/@ElectronicaDeUna)


