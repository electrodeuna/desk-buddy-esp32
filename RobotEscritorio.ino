/* ESP32-C3 + OLED SSD1306 I2C 128x64 + TTP223 (momentaneo activo HIGH).
   OLED: VCC=3V3, GND=GND, SDA=GPIO4, SCL=GPIO5.
   TTP223: VCC=3V3, GND=GND, OUT=GPIO3.
   Control WiFi: ElectroRobot / robot1234 / http://192.168.4.1
   Hora local sincronizada desde el navegador, se pierde al reiniciar.
   Bibliotecas: Adafruit SSD1306 y Adafruit GFX Library.
   Placa: ESP32C3 Dev Module (o la seleccion exacta de tu placa).
*/
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_timer.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

constexpr int SDA_PIN = 4;
constexpr int SCL_PIN = 5;
constexpr int TOUCH_PIN = 3;
constexpr int TOUCH_ACTIVE = HIGH;
constexpr uint32_t SLEEP_MS = 60000;
constexpr uint32_t AUTO_CLOCK_MS = 30000;
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
WebServer server(80);
constexpr const char* AP_NAME = "ElectroRobot";
constexpr const char* AP_PASSWORD = "robot1234";
bool clockMode = false, gameMode = false, clockSynced = false;
uint64_t clockEpoch = 0;
int64_t clockSyncUs = 0;

const char CONTROL_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Electro Robot</title><style>
*{box-sizing:border-box}body{margin:0;background:#0c1220;color:#eef3ff;font:16px system-ui,sans-serif}
main{max-width:460px;margin:auto;padding:28px 20px}h1{font-size:30px;margin:8px 0}
p{color:#aab9d2;line-height:1.5}.card{background:#182237;border:1px solid #2b3a55;border-radius:22px;padding:20px;margin:18px 0}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}button{font:inherit;color:#eef3ff;background:#293952;border:0;border-radius:14px;padding:17px 10px;cursor:pointer;touch-action:manipulation}
button.active{background:#56d8bb;color:#08251d}button:active{transform:scale(.97)}button:disabled{opacity:.5}
.label{font-size:12px;letter-spacing:2px;color:#56d8bb}.time{font-size:44px;font-weight:700;font-variant-numeric:tabular-nums}
#feedback{min-height:48px;font-size:14px}footer{color:#aab9d2;font-size:13px}</style></head>
<body><main><div class="label">ELECTRO DE UNA</div><h1>Mi robot</h1>
<p>Elegí qué mostrar en la pantalla.</p><section class="card"><div class="grid">
<button id="robot" onclick="changeMode('robot')">🤖 Robot</button>
<button id="clock" onclick="changeMode('clock')">🕒 Hora</button>
<button id="game" onclick="changeMode('game')" style="grid-column:1/-1">🐤 Flappy Bird</button></div>
<p>En el juego, tocá el sensor para saltar. Al perder, tocá para reiniciar.</p>
<p id="state">Conectando...</p><div id="time" class="time">--:--</div></section>
<section class="card"><h2>Expresiones</h2><div class="grid">
<button onclick="face(0)">👀 Normal</button><button onclick="face(1)">😊 Feliz</button>
<button onclick="face(2)">😮 Sorpresa</button><button onclick="face(3)">😉 Guiño</button>
<button onclick="face(4)">💖 Corazones</button><button onclick="syncClock()">⏱ Sincronizar hora</button>
</div><p>Elegir una expresión vuelve al modo robot.</p></section>
<p id="feedback" role="status" aria-live="polite"></p>
<footer>La hora se copia de tu celular. Después sigue funcionando aunque cierres esta página.</footer>
</main><script>
const el=id=>document.getElementById(id);
let busy=false;
async function request(path,data){
 const options=data?{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(data)}:{};
 const response=await fetch(path,{...options,cache:'no-store',signal:AbortSignal.timeout(5000)});
 if(!response.ok)throw new Error('No se pudo completar el cambio.');
 return response.json();
}
function show(s){
 el('robot').classList.toggle('active',s.mode==='robot');
 el('clock').classList.toggle('active',s.mode==='clock');
 el('game').classList.toggle('active',s.mode==='game');
 el('state').textContent=s.mode==='game'?'Pantalla: Flappy Bird':s.mode==='clock'?'Pantalla: reloj':'Pantalla: robot';
 el('time').textContent=s.synced?s.time:'--:--';
}
async function action(fn){
 if(busy)return;busy=true;
 try{await fn();await refresh();el('feedback').textContent='Listo ✓';}
 catch(e){el('feedback').textContent='Sin conexión. Revisá que sigas en la red ElectroRobot.';}
 finally{busy=false;}
}
async function refresh(){show(await request('/api/status'));}
async function sendTime(){const d=new Date();const localEpoch=Math.floor(Date.UTC(d.getFullYear(),d.getMonth(),d.getDate(),d.getHours(),d.getMinutes(),d.getSeconds())/1000);await request('/api/time',{epoch:localEpoch});}
function syncClock(){return action(sendTime);}
function changeMode(mode){return action(async()=>{if(mode==='clock')await sendTime();await request('/api/mode',{mode});});}
function face(id){return action(()=>request('/api/face',{id}));}
async function start(){await action(sendTime);setInterval(()=>{if(!busy)refresh().catch(()=>{el('feedback').textContent='Sin conexión con el robot.';});},2000);}
start();
</script></body></html>
)HTML";

uint64_t currentClockEpoch() {
  uint64_t elapsed = (esp_timer_get_time() - clockSyncUs) / 1000000ULL;
  return clockEpoch + elapsed;
}

uint32_t currentClockSeconds() {
  return currentClockEpoch() % 86400;
}

void sendStatus() {
  uint32_t seconds = currentClockSeconds();
  char timeText[9];
  snprintf(timeText, sizeof(timeText), "%02u:%02u:%02u",
           (unsigned)(seconds / 3600), (unsigned)(seconds / 60 % 60), (unsigned)(seconds % 60));
  String json = String("{\"mode\":\"") + (gameMode ? "game" : clockMode ? "clock" : "robot") +
                "\",\"synced\":" + (clockSynced ? "true" : "false") +
                ",\"time\":\"" + timeText + "\"}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

bool decimalArgument(const String& value) {
  if (!value.length() || value.length() > 10) return false;
  for (unsigned int i = 0; i < value.length(); ++i)
    if (value[i] < '0' || value[i] > '9') return false;
  return true;
}

void drawClock() {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  if (!clockSynced) {
    oled.setTextSize(1);
    oled.setCursor(13, 16); oled.print("Abrir en celular:");
    oled.setCursor(22, 31); oled.print("192.168.4.1");
    oled.setCursor(10, 47); oled.print("Sincronizar la hora");
  } else {
    uint32_t seconds = currentClockSeconds();
    char digits[6];
    snprintf(digits, sizeof(digits), "%02u%c%02u", (unsigned)(seconds / 3600),
             seconds % 2 ? ':' : ' ', (unsigned)(seconds / 60 % 60));
    oled.setTextSize(4);
    oled.setCursor(4, 13); oled.print(digits);
    time_t localEpoch = (time_t)currentClockEpoch();
    struct tm calendar;
    gmtime_r(&localEpoch, &calendar);
    const char* days[] = {"Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado"};
    const char* months[] = {"Ene", "Feb", "Mar", "Abr", "May", "Jun",
                            "Jul", "Ago", "Sep", "Oct", "Nov", "Dic"};
    char dateText[24];
    snprintf(dateText, sizeof(dateText), "%s, %d %s",
             days[calendar.tm_wday], calendar.tm_mday, months[calendar.tm_mon]);
    oled.setTextSize(1);
    int dateWidth = (int)strlen(dateText) * 6;
    oled.setCursor((128 - dateWidth) / 2, 52); oled.print(dateText);
  }
  oled.display();
}


uint32_t lastInteraction, rawChanged, pressedAt;
uint32_t expressionStart, expressionDuration = 0;
uint32_t blinkStart, nextBlink, nextLook, lastFrame;
bool rawTouch = false, stableTouch = false, longHandled = false;
bool blinking = false;
uint8_t expression = 0, expressionCycle = 0;
float lookX = 0, lookY = 0;
int targetX = 0, targetY = 0;
float eyeWidth = 38, eyeHeightL = 40, eyeHeightR = 40;
float happyL = 0, happyR = 0, heartAmount = 0, sleepAmount = 0;

float ease(float current, float target, float rate = 0.23f) {
  return current + (target - current) * rate;
}

void setExpression(uint8_t value, uint32_t duration, uint32_t now) {
  expression = value;
  expressionStart = now;
  expressionDuration = duration;
  blinking = false;
}

// Flappy Bird: fisica a pasos fijos de 30 ms, dos obstaculos reciclados.
constexpr int BIRD_X = 28, BIRD_RADIUS = 3;
constexpr int PIPE_WIDTH = 13, PIPE_GAP = 25, FLOOR_Y = 60;
float birdY = 28, birdSpeed = 0;
float pipeX[2] = {128, 208};
int gapTop[2] = {18, 24};
bool pipeScored[2] = {false, false};
bool gameStarted = false, gameOver = false;
unsigned int gameScore = 0, bestScore = 0;
uint32_t gameStep = 0;

void resetGame() {
  birdY = 28;
  birdSpeed = 0;
  pipeX[0] = 128; pipeX[1] = 208;
  for (int i = 0; i < 2; ++i) {
    gapTop[i] = random(10, 29);
    pipeScored[i] = false;
  }
  gameScore = 0;
  gameStarted = false;
  gameOver = false;
  gameStep = millis();
}

void gameTouch(uint32_t now) {
  if (gameOver) resetGame();
  gameStarted = true;
  birdSpeed = -2.25f;
  gameStep = now; // Respuesta inmediata sin pasos acumulados.
}

void updateGame(uint32_t now) {
  if (!gameStarted || gameOver) { gameStep = now; return; }
  int steps = 0;
  while (now - gameStep >= 30 && steps++ < 4 && !gameOver) {
    gameStep += 30;
    birdSpeed += 0.17f;
    if (birdSpeed > 3.0f) birdSpeed = 3.0f;
    birdY += birdSpeed;
    float speed = 1.15f + (gameScore > 15 ? 15 : gameScore) * 0.025f;
    for (int i = 0; i < 2; ++i) {
      pipeX[i] -= speed;
      if (!pipeScored[i] && pipeX[i] + PIPE_WIDTH < BIRD_X - BIRD_RADIUS) {
        pipeScored[i] = true;
        ++gameScore;
      }
      if (pipeX[i] + PIPE_WIDTH < 0) {
        pipeX[i] = pipeX[1 - i] + 80;
        gapTop[i] = random(10, 29);
        pipeScored[i] = false;
      }
      if (BIRD_X + BIRD_RADIUS >= pipeX[i] &&
          BIRD_X - BIRD_RADIUS <= pipeX[i] + PIPE_WIDTH &&
          (birdY - BIRD_RADIUS < gapTop[i] ||
           birdY + BIRD_RADIUS > gapTop[i] + PIPE_GAP)) gameOver = true;
    }
    if (birdY - BIRD_RADIUS <= 0 || birdY + BIRD_RADIUS >= FLOOR_Y) gameOver = true;
    if (gameScore > bestScore) bestScore = gameScore;
  }
  if (now - gameStep >= 120) gameStep = now; // Evita saltos tras una pausa del servidor.
}

void drawGame(uint32_t now) {
  oled.clearDisplay();
  for (int i = 0; i < 2; ++i) {
    int x = (int)roundf(pipeX[i]);
    oled.fillRect(x, 0, PIPE_WIDTH, gapTop[i], SSD1306_WHITE);
    oled.fillRect(x, gapTop[i] + PIPE_GAP, PIPE_WIDTH,
                  FLOOR_Y - gapTop[i] - PIPE_GAP, SSD1306_WHITE);
    oled.drawFastHLine(x - 1, gapTop[i] - 2, PIPE_WIDTH + 2, SSD1306_WHITE);
    oled.drawFastHLine(x - 1, gapTop[i] + PIPE_GAP + 1, PIPE_WIDTH + 2, SSD1306_WHITE);
  }
  int y = (int)roundf(birdY);
  oled.fillCircle(BIRD_X, y, BIRD_RADIUS, SSD1306_WHITE);
  oled.drawPixel(BIRD_X + 1, y - 1, SSD1306_BLACK);
  oled.drawLine(BIRD_X + 3, y, BIRD_X + 5, y + 1, SSD1306_WHITE);
  int wing = (now / 120 % 2) ? -2 : 2;
  oled.drawLine(BIRD_X - 2, y, BIRD_X - 5, y + wing, SSD1306_WHITE);
  oled.drawFastHLine(0, FLOOR_Y, 128, SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  oled.setCursor(2, 2); oled.print(gameScore);
  if (!gameStarted || gameOver) {
    oled.fillRoundRect(9, 13, 110, 39, 4, SSD1306_BLACK);
    oled.drawRoundRect(9, 13, 110, 39, 4, SSD1306_WHITE);
    oled.setCursor(28, 18); oled.print(gameOver ? "FIN DEL JUEGO" : "FLAPPY BIRD");
    oled.setCursor(20, 30);
    if (gameOver) { oled.print("Puntos: "); oled.print(gameScore); }
    else oled.print("Toca para saltar");
    oled.setCursor(20, 42);
    if (gameOver) oled.print("Toca: reiniciar");
    else { oled.print("Record: "); oled.print(bestScore); }
  }
  oled.setTextColor(SSD1306_WHITE);
  oled.display();
}

void startControl() {
  WiFi.mode(WIFI_AP);
  IPAddress ip(192, 168, 4, 1);
  bool configured = WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
  bool started = WiFi.softAP(AP_NAME, AP_PASSWORD);
  if (!configured || !started) Serial.println("Error al iniciar WiFi AP.");
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html; charset=utf-8", CONTROL_PAGE);
  });
  server.on("/api/status", HTTP_GET, sendStatus);
  server.on("/api/time", HTTP_POST, []() {
    String value = server.arg("epoch");
    uint64_t epoch = strtoull(value.c_str(), nullptr, 10);
    if (!decimalArgument(value) || epoch < 946684800ULL || epoch >= 4102444800ULL) {
      server.send(400, "application/json", "{\"error\":\"Hora invalida\"}"); return;
    }
    clockEpoch = epoch;
    clockSyncUs = esp_timer_get_time();
    clockSynced = true;
    sendStatus();
  });
  server.on("/api/mode", HTTP_POST, []() {
    String mode = server.arg("mode");
    if (mode != "robot" && mode != "clock" && mode != "game") {
      server.send(400, "application/json", "{\"error\":\"Modo invalido\"}"); return;
    }
    clockMode = mode == "clock";
    gameMode = mode == "game";
    if (gameMode) resetGame();
    lastInteraction = millis();
    sendStatus();
  });
  server.on("/api/face", HTTP_POST, []() {
    String value = server.arg("id");
    if (!decimalArgument(value) || value.toInt() > 4) {
      server.send(400, "application/json", "{\"error\":\"Expresion invalida\"}"); return;
    }
    clockMode = false;
    gameMode = false;
    lastInteraction = millis();
    setExpression(value.toInt(), 5000, millis());
    sendStatus();
  });
  server.onNotFound([]() { server.send(404, "application/json", "{\"error\":\"No encontrado\"}"); });
  server.begin();
  Serial.print("Control: http://"); Serial.println(WiFi.softAPIP());
}

void readTouch(uint32_t now) {
  bool reading = digitalRead(TOUCH_PIN) == TOUCH_ACTIVE;
  if (reading != rawTouch) {
    rawTouch = reading;
    rawChanged = now;
  }
  if (rawTouch != stableTouch && now - rawChanged >= 35) {
    stableTouch = rawTouch;
    if (gameMode) {
      lastInteraction = now;
      if (stableTouch) gameTouch(now);
      return;
    }
    lastInteraction = now;
    if (stableTouch) {
      clockMode = false; // Un toque vuelve del reloj al robot.
      pressedAt = now;
      longHandled = false;
      // El toque despierta al robot y muestra alegria inmediatamente.
      setExpression(1, 1800, now);
    } else if (!longHandled) {
      expressionCycle = (expressionCycle % 3) + 1;
      setExpression(expressionCycle, 2500, now);
    }
  }
  if (gameMode) {
    if (stableTouch) lastInteraction = now;
    return;
  }
  if (stableTouch) {
    lastInteraction = now;
    if (!longHandled && now - pressedAt >= 900) {
      longHandled = true;
      setExpression(4, 2500, now);
    }
    if (longHandled) {
      expressionStart = now; // Conserva los corazones mientras lo acaricias.
    }
  }
}

// Pupila circular con brillo. Se reduce al cerrar el parpado.
void drawPupil(int x, int y, int w, int h, float happy) {
  if (happy > 0.35f || h < 13 || w < 15) return;
  int radius = expression == 2 ? 7 : 11;
  int limitX = w / 2 - 4;
  int limitY = h / 2 - 3;
  if (radius > limitX) radius = limitX;
  if (radius > limitY) radius = limitY;
  if (radius < 3) return;
  int roomX = w / 2 - radius - 3;
  int roomY = h / 2 - radius - 3;
  if (roomX < 0) roomX = 0;
  if (roomY < 0) roomY = 0;
  int dx = constrain((int)roundf(lookX * 0.65f), -roomX, roomX);
  int dy = constrain((int)roundf(lookY * 0.7f), -roomY, roomY);
  int px = x + dx, py = y + dy;
  // Dibujar solo sobre la superficie blanca conserva la forma del parpado.
  for (int j = -radius; j <= radius; ++j) {
    for (int i = -radius; i <= radius; ++i) {
      if (i * i + j * j > radius * radius) continue;
      if (!oled.getPixel(px + i, py + j)) continue;
      int spotX = i + radius / 3, spotY = j + radius / 3;
      int shine = radius >= 6 ? 2 : 1;
      if (spotX * spotX + spotY * spotY > shine * shine)
        oled.drawPixel(px + i, py + j, SSD1306_BLACK);
    }
  }
}

// Dibuja un ojo que se transforma suavemente de rectangulo a arco feliz.
void animatedEye(int x, int y, float width, float height, float happy) {
  int w = (int)roundf(width);
  int h = (int)roundf(height);
  if (w < 4) w = 4;
  if (h < 3) h = 3;
  int half = w / 2;
  int thickness = (int)roundf(h * (1 - happy) + 5 * happy);
  if (thickness < 3) thickness = 3;
  if (happy < 0.03f) {
    int radius = h / 3;
    if (radius > 9) radius = 9;
    if (radius < 1) radius = 1;
    oled.fillRoundRect(x - half, y - h / 2, w, h, radius, SSD1306_WHITE);
    drawPupil(x, y, w, h, happy);
    return;
  }
  for (int i = -half; i <= half; ++i) {
    float position = (float)i / half;
    int curve = (int)roundf(happy * (12 * position * position - 6));
    oled.drawFastVLine(x + i, y + curve - thickness / 2,
                      thickness, SSD1306_WHITE);
  }
  drawPupil(x, y, w, thickness, happy);
}

void heart(int x, int y, float scale) {
  int r = (int)roundf(8 * scale);
  if (r < 1) return;
  int offset = (int)roundf(7 * scale);
  int top = y - (int)roundf(5 * scale);
  oled.fillCircle(x - offset, top, r, SSD1306_WHITE);
  oled.fillCircle(x + offset, top, r, SSD1306_WHITE);
  int half = (int)roundf(14 * scale);
  oled.fillTriangle(x - half, y - (int)(2 * scale),
                    x + half, y - (int)(2 * scale),
                    x, y + (int)(17 * scale), SSD1306_WHITE);
}

// Boca pequena tipo mascota: gesto de "w", sonrisa y sorpresa.
void drawMouth(uint32_t now, bool sleeping) {
  int x = 64 + (int)roundf(lookX * 0.3f);
  int y = 56;
  if (sleeping) {
    int r = (now / 700 % 2) ? 3 : 2;
    oled.drawCircle(x, y, r, SSD1306_WHITE);
  } else if (expression == 2) {
    int r = 4 + (int)(0.5f + 0.5f * sinf(now * 0.008f));
    oled.drawCircle(x, y, r, SSD1306_WHITE);
  } else if (expression == 1 || expression == 4) {
    // Sonrisa abierta, con un pequeno movimiento al estar contento.
    int bounce = (int)roundf(sinf(now * 0.009f));
    oled.drawLine(x - 10, y - 3 + bounce, x - 5, y + 3 + bounce, SSD1306_WHITE);
    oled.drawLine(x - 5, y + 3 + bounce, x + 5, y + 3 + bounce, SSD1306_WHITE);
    oled.drawLine(x + 5, y + 3 + bounce, x + 10, y - 3 + bounce, SSD1306_WHITE);
    oled.drawFastHLine(x - 9, y - 3 + bounce, 19, SSD1306_WHITE);
  } else {
    // Dos pequenas curvas unidas, como una boca de gatito.
    oled.drawLine(x - 10, y - 1, x - 7, y + 3, SSD1306_WHITE);
    oled.drawLine(x - 7, y + 3, x - 3, y + 3, SSD1306_WHITE);
    oled.drawLine(x - 3, y + 3, x, y, SSD1306_WHITE);
    oled.drawLine(x, y, x + 3, y + 3, SSD1306_WHITE);
    oled.drawLine(x + 3, y + 3, x + 7, y + 3, SSD1306_WHITE);
    oled.drawLine(x + 7, y + 3, x + 10, y - 1, SSD1306_WHITE);
  }
}

void drawFace(uint32_t now, bool sleeping) {
  float t = now * 0.001f;
  float age = (now - expressionStart) * 0.001f;
  sleepAmount = ease(sleepAmount, sleeping ? 1.0f : 0.0f, 0.045f);
  float width = 38, heightL = 40, heightR = 40;
  float smileL = 0, smileR = 0;
  float bob = 1.2f * sinf(t * 2.4f);
  if (!sleeping) {
    if (expression == 1) {
      smileL = smileR = 1;
      bob = 2.5f * sinf(age * 12);
      width = 38 + 2 * sinf(age * 12);
    } else if (expression == 2) {
      width = 40 + 2 * sinf(age * 9);
      heightL = heightR = 44 + 2 * sinf(age * 9);
    } else if (expression == 3) {
      // Guinio repetido con cierre y apertura progresivos.
      float wink = powf(0.5f + 0.5f * sinf(age * 7 - 1.57f), 3);
      heightL = 40 - 37 * wink;
      smileR = 0.15f; // Mantiene visible la pupila del ojo abierto.
    }
  }
  if (blinking && !sleeping) {
    uint32_t elapsed = now - blinkStart;
    float close = elapsed < 90 ? elapsed / 90.0f :
                  (elapsed < 130 ? 1.0f : 1.0f - (elapsed - 130) / 140.0f);
    close = constrain(close, 0.0f, 1.0f);
    heightL *= 1 - close;
    heightR *= 1 - close;
  }
  heightL = heightL * (1 - sleepAmount) + 4 * sleepAmount;
  heightR = heightR * (1 - sleepAmount) + 4 * sleepAmount;
  smileL *= 1 - sleepAmount;
  smileR *= 1 - sleepAmount;
  eyeWidth = ease(eyeWidth, width);
  eyeHeightL = ease(eyeHeightL, heightL, blinking ? 0.65f : 0.25f);
  eyeHeightR = ease(eyeHeightR, heightR, blinking ? 0.65f : 0.25f);
  happyL = ease(happyL, smileL);
  happyR = ease(happyR, smileR);
  heartAmount = ease(heartAmount, !sleeping && expression == 4 ? 1.0f : 0.0f);

  oled.clearDisplay();
  int left = (int)roundf(36 + lookX * 0.4f), right = (int)roundf(92 + lookX * 0.4f);
  int cy = (int)roundf(26 + lookY * 0.6f + bob);
  // Los ojos se achican antes de transformarse en corazones.
  if (heartAmount < 0.95f) {
    float factor = 1 - heartAmount;
    animatedEye(left, cy, eyeWidth * factor, eyeHeightL * factor, happyL);
    animatedEye(right, cy, eyeWidth * factor, eyeHeightR * factor, happyR);
  }
  if (heartAmount > 0.08f) {
    float pulse = heartAmount * (0.88f + 0.10f * sinf(t * 8));
    heart(left, cy, pulse);
    heart(right, cy, pulse);
    // Brillos que aparecen alternativamente a los lados.
    if (heartAmount > 0.85f) {
      int sx = (now / 300 % 2) ? 15 : 113;
      oled.drawFastHLine(sx - 2, 16, 5, SSD1306_WHITE);
      oled.drawFastVLine(sx, 14, 5, SSD1306_WHITE);
    }
  }
  drawMouth(now, sleeping);
  if (sleepAmount > 0.8f) {
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    int phase = now / 500 % 4;
    for (int i = 0; i < phase; ++i) {
      oled.setCursor(104 + i * 7, 22 - i * 7);
      oled.print("z");
    }
  }
  oled.display();
}

void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT_PULLDOWN);
  Wire.begin(SDA_PIN, SCL_PIN);
  // Detecta las dos direcciones habituales del SSD1306.
  uint8_t address = 0;
  for (uint8_t candidate = 0x3C; candidate <= 0x3D; ++candidate) {
    Wire.beginTransmission(candidate);
    if (Wire.endTransmission() == 0) {
      address = candidate;
      break;
    }
  }
  if (!address || !oled.begin(SSD1306_SWITCHCAPVCC, address)) {
    Serial.println("OLED no detectado. Revisar SDA/SCL, alimentacion y controlador SSD1306.");
    while (true) { delay(1000); }
  }
  oled.clearDisplay();
  oled.display();
  delay(1000); // Dejar calibrar el TTP223 sin tocarlo.
  uint32_t now = millis();
  lastInteraction = now;
  rawChanged = now;
  nextBlink = now;
  nextLook = now;
  startControl();
}

void loop() {
  server.handleClient();
  uint32_t now = millis();
  readTouch(now);
  if (!clockMode && now - lastInteraction >= AUTO_CLOCK_MS) {
    clockMode = true;
    gameMode = false;
  }
  if (gameMode) {
    updateGame(now);
    if (now - lastFrame >= 30) {
      lastFrame = now;
      drawGame(now);
    }
    delay(1);
    return;
  }
  if (clockMode) {
    if (now - lastFrame >= 100) {
      lastFrame = now;
      drawClock();
    }
    delay(1);
    return;
  }
  bool sleeping = now - lastInteraction >= SLEEP_MS;
  if (expression && now - expressionStart >= expressionDuration) expression = 0;

  // Comparacion de plazos valida tambien al desbordar millis().
  if (!sleeping && expression == 0) {
    if (!blinking && (int32_t)(now - nextBlink) >= 0) {
      blinking = true;
      blinkStart = now;
    }
    if (blinking && now - blinkStart >= 270) {
      blinking = false;
      nextBlink = now + (random(0, 5) == 0 ? 180 : random(2200, 5200));
    }
    if ((int32_t)(now - nextLook) >= 0) {
      targetX = random(-8, 9);
      targetY = random(-3, 4);
      nextLook = now + random(1300, 3200);
    }
  } else {
    blinking = false;
    targetX = targetY = 0;
  }
  if (now - lastFrame >= 30) {
    lastFrame = now;
    lookX = ease(lookX, targetX, 0.14f);
    lookY = ease(lookY, targetY, 0.14f);
    drawFace(now, sleeping);
  }
  delay(1);
}
