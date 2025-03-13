#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>

// Display setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Bluetooth setup
SoftwareSerial HM10(10, 11);  // RX (pin 10), TX (pin 11)

// Speaker and LED pins
#define SHOOT_PIN 8
#define UFO_ENEMY_PIN 9  // Already corrected to match your speaker on pin 9
#define EXPLOSION_PIN 12
#define LED_LEFT 5
#define LED_RIGHT 6
#define LED_SHOOT 7

// Game constants
#define PLAYER_WIDTH 11
#define PLAYER_HEIGHT 7
#define ALIEN_WIDTH 8
#define ALIEN_HEIGHT 8
#define MAX_PROJECTILES 1
#define MAX_ALIEN_PROJECTILES 3
#define NUM_ALIENS 12
#define ALIEN_SPACING_X 10
#define ALIEN_SPACING_Y 8
#define EXPLOSION_WIDTH 8
#define EXPLOSION_HEIGHT 8
#define EXPLOSION_DURATION 200
#define UFO_WIDTH 12
#define UFO_HEIGHT 5
#define BUNKER_WIDTH 8
#define BUNKER_HEIGHT 6
#define NUM_BUNKERS 4
#define PLAYER_DESTROYED_DURATION 1000
#define LIFE_ICON_WIDTH 8
#define LIFE_ICON_HEIGHT 5
#define MAX_LIVES 3
#define LIVES_SCREEN_DURATION 2000
#define UFO_SOUND_INTERVAL 200  // Increased to 200 ms for authentic UFO siren
#define PLAYER_PROJECTILE_WIDTH 1
#define PLAYER_PROJECTILE_HEIGHT 5
#define ALIEN_PROJECTILE_WIDTH 4
#define ALIEN_PROJECTILE_HEIGHT 6

// Game state structures
struct Projectile {
  int x, y;
  bool active;
  bool isAlien;
  byte type;
};
struct Alien {
  int x, y;
  bool alive;
  byte type;
};
struct Explosion {
  int x, y;
  unsigned long startTime;
  bool active;
};
struct UFO {
  int x;
  bool active;
  bool movingRight;
};
struct Bunker {
  int x, y;
  byte damageLevel;
};
struct Player {
  int x;
  int score;
  int lives;
  bool destroyed;
  unsigned long destroyedTime;
  bool moveLeft;
  bool moveRight;
  bool shoot;
};

// Sound structure
struct Sound {
  int pin;
  unsigned long period;        // in microseconds
  unsigned long lastToggle;    // in microseconds
  bool active;
  unsigned long endTimeMillis; // in milliseconds
  bool state;                  // current pin state
};

// Game variables
Projectile projectiles[MAX_PROJECTILES + MAX_ALIEN_PROJECTILES];
Alien aliens[NUM_ALIENS];
Explosion explosion;
UFO ufo = {0, false, true};
Bunker bunkers[NUM_BUNKERS];
Player players[2] = {
  {SCREEN_WIDTH / 2 - PLAYER_WIDTH / 2, 0, MAX_LIVES, false, 0, false, false, false},
  {SCREEN_WIDTH / 2 - PLAYER_WIDTH / 2, 0, MAX_LIVES, false, 0, false, false, false}
};
int aliensAlive = NUM_ALIENS;
bool aliensMoveRight = true;
int baseMoveInterval = 500;
bool isMultiplayer = false;
int currentPlayer = 0;
bool ufoSoundHigh = false;
unsigned long lastUfoSoundToggle = 0;

// Animation and timing
byte animationFrame = 0;
unsigned long lastFrameChange = 0;
unsigned long lastAlienMove = 0;
byte alienMoveSoundIndex = 0;
unsigned long lastUFOSpawn = 0;

// Sound channels
Sound shootSound = {SHOOT_PIN, 0, 0, false, 0, LOW};
Sound ufoEnemySound = {UFO_ENEMY_PIN, 0, 0, false, 0, LOW};
Sound explosionSound = {EXPLOSION_PIN, 0, 0, false, 0, LOW};

// Sprites in PROGMEM
const uint16_t playerSprite[PLAYER_HEIGHT] PROGMEM = {0x010, 0x038, 0x07C, 0x0FE, 0x1FF, 0x3FF, 0x3FF};
const uint16_t playerDestroyedSprite[PLAYER_HEIGHT] PROGMEM = {0x091, 0x122, 0x244, 0x088, 0x155, 0x222, 0x091};
const byte lifeIconSprite[LIFE_ICON_HEIGHT] PROGMEM = {0x08, 0x1C, 0x3E, 0x7F, 0x7F};
const byte lifeDestroyedSprite[LIFE_ICON_HEIGHT] PROGMEM = {0x14, 0x22, 0x08, 0x55, 0x22};
const byte playerProjectileSprite[PLAYER_PROJECTILE_HEIGHT] PROGMEM = {0x01, 0x01, 0x01, 0x01, 0x01};
const byte alienProjectileSprites[3][ALIEN_PROJECTILE_HEIGHT] PROGMEM = {
  {0x06, 0x06, 0x06, 0x06, 0x06, 0x06},
  {0x02, 0x05, 0x0A, 0x04, 0x09, 0x06},
  {0x06, 0x06, 0x00, 0x06, 0x06, 0x00}
};
const byte alienSprites[3][2][ALIEN_HEIGHT] PROGMEM = {
  {{0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x24, 0x5A, 0x81}, {0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x42, 0xA5, 0x42}},
  {{0x00, 0x3C, 0x7E, 0xDB, 0xFF, 0x5A, 0x81, 0x42}, {0x00, 0x3C, 0x7E, 0xDB, 0xFF, 0x24, 0x5A, 0xA5}},
  {{0x3C, 0x7E, 0xFF, 0xDB, 0xFF, 0x18, 0x24, 0x42}, {0x3C, 0x7E, 0xFF, 0xDB, 0xFF, 0x18, 0x42, 0x81}}
};
const byte explosionSprite[EXPLOSION_HEIGHT] PROGMEM = {0x14, 0x22, 0x5D, 0xA5, 0xA5, 0x5D, 0x22, 0x14};
const byte ufoSprite[UFO_HEIGHT] PROGMEM = {0x10, 0x7C, 0xFE, 0x7C, 0x10};
const byte bunkerSprites[5][BUNKER_HEIGHT] PROGMEM = {
  {0x3C, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF},
  {0x38, 0x7C, 0xFE, 0xFF, 0xFF, 0xFF},
  {0x30, 0x58, 0xFE, 0xFD, 0xFF, 0xFE},
  {0x20, 0x50, 0xF8, 0xF4, 0xFE, 0xE8},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

// Input flags
bool gameStarted = false;

// Tones
const int alienMoveTones[4] = {120, 100, 80, 60};  // Authentic alien march tones
const int ufoTones[2] = {900, 700};  // Authentic UFO siren tones

void showScoreTableAnimation() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  char title[] = "SPACE INVADERS";
  int titleWidth = strlen(title) * 6;
  int titleX = (SCREEN_WIDTH - titleWidth) / 2;
  int titleY = 0;
  for (int i = 0; i < (int)strlen(title); i++) {
    display.setCursor(titleX + i * 6, titleY);
    display.print(title[i]);
    display.display();
    delay(200);
  }
  delay(500);

  display.clearDisplay();
  char scoreAdvance[] = "SCORE ADVANCE TABLE";
  int saWidth = strlen(scoreAdvance) * 6;
  int saX = (SCREEN_WIDTH - saWidth) / 2;
  int saY = 0;
  for (int i = 0; i < (int)strlen(scoreAdvance); i++) {
    display.setCursor(saX + i * 6, saY);
    display.print(scoreAdvance[i]);
    display.display();
    delay(200);
  }
  delay(500);

  drawUFO(10, 8);
  char ufoRow[] = "= 100";
  int ufoTextX = 30;
  int ufoTextY = 10;
  for (int i = 0; i < (int)strlen(ufoRow); i++) {
    display.setCursor(ufoTextX + i * 6, ufoTextY);
    display.print(ufoRow[i]);
    display.display();
    delay(200);
  }

  drawAlien(10, 18, 0, 0);
  char alienRow1[] = "= 30";
  int alienRow1X = 30;
  int alienRow1Y = 20;
  for (int i = 0; i < (int)strlen(alienRow1); i++) {
    display.setCursor(alienRow1X + i * 6, alienRow1Y);
    display.print(alienRow1[i]);
    display.display();
    delay(200);
  }

  drawAlien(10, 28, 1, 0);
  char alienRow2[] = "= 20";
  int alienRow2X = 30;
  int alienRow2Y = 30;
  for (int i = 0; i < (int)strlen(alienRow2); i++) {
    display.setCursor(alienRow2X + i * 6, alienRow2Y);
    display.print(alienRow2[i]);
    display.display();
    delay(200);
  }

  drawAlien(10, 38, 2, 0);
  char alienRow3[] = "= 10";
  int alienRow3X = 30;
  int alienRow3Y = 40;
  for (int i = 0; i < (int)strlen(alienRow3); i++) {
    display.setCursor(alienRow3X + i * 6, alienRow3Y);
    display.print(alienRow3[i]);
    display.display();
    delay(200);
  }
  display.fillRect(0, 0, 16, 16, SSD1306_BLACK);  // Patchwork fix for garbage
  display.display();
  delay(1000);
}

void attractMode() {
  unsigned long lastLedChange = millis();
  byte ledPatternStep = 0;
  bool blinkCoin = true;
  unsigned long lastBlink = millis();
  
  while (!gameStarted) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    int coinX = (SCREEN_WIDTH - 66) / 2;
    int coinY = 20;
    if (blinkCoin) {
      display.setCursor(coinX, coinY);
      display.print("INSERT COIN");
      display.setCursor(coinX - 12, coinY + 10);
      display.print("1P          2P");
    }
    display.fillRect(0, 0, 24, 16, SSD1306_BLACK);  // Patchwork fix for garbage output
    display.display();

    if (millis() - lastLedChange > 500) {
      ledPatternStep = (ledPatternStep + 1) % 6;
      switch (ledPatternStep) {
        case 0: digitalWrite(LED_LEFT, HIGH); digitalWrite(LED_RIGHT, LOW); digitalWrite(LED_SHOOT, LOW); break;
        case 1: digitalWrite(LED_LEFT, LOW); digitalWrite(LED_RIGHT, LOW); digitalWrite(LED_SHOOT, LOW); break;
        case 2: digitalWrite(LED_LEFT, HIGH); digitalWrite(LED_RIGHT, HIGH); digitalWrite(LED_SHOOT, LOW); break;
        case 3: digitalWrite(LED_LEFT, LOW); digitalWrite(LED_RIGHT, LOW); digitalWrite(LED_SHOOT, LOW); break;
        case 4: digitalWrite(LED_LEFT, HIGH); digitalWrite(LED_RIGHT, HIGH); digitalWrite(LED_SHOOT, HIGH); break;
        case 5: digitalWrite(LED_LEFT, LOW); digitalWrite(LED_RIGHT, LOW); digitalWrite(LED_SHOOT, LOW); break;
      }
      lastLedChange = millis();
    }

    if (millis() - lastBlink > 500) {
      blinkCoin = !blinkCoin;
      lastBlink = millis();
    }
    parseBluetooth();
  }
  digitalWrite(LED_LEFT, LOW);
  digitalWrite(LED_RIGHT, LOW);
  digitalWrite(LED_SHOOT, LOW);
  showScoreTableAnimation();
  initGame();
}

void showLivesScreen() {
  unsigned long startTime = millis();
  while (millis() - startTime < LIVES_SCREEN_DURATION) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    for (int p = 0; p < (isMultiplayer ? 2 : 1); p++) {
      int yOffset = p * 20;
      display.setCursor(10, 10 + yOffset);
      display.print("PLAYER ");
      display.print(p + 1);
      display.print(" LIVES: ");
      display.print(players[p].lives);

      int startX = 10;
      int y = 20 + yOffset;
      for (int i = 0; i < MAX_LIVES; i++) {
        int x = startX + i * (LIFE_ICON_WIDTH + 2);
        if (i < players[p].lives) {
          for (int row = 0; row < LIFE_ICON_HEIGHT; row++) {
            byte data = pgm_read_byte(&lifeIconSprite[row]);
            for (int col = 0; col < LIFE_ICON_WIDTH; col++) {
              if (data & (1 << (7 - col))) {
                display.drawPixel(x + col, y + row, SSD1306_WHITE);
              }
            }
          }
        } else {
          for (int row = 0; row < LIFE_ICON_HEIGHT; row++) {
            byte data = pgm_read_byte(&lifeDestroyedSprite[row]);
            for (int col = 0; col < LIFE_ICON_WIDTH; col++) {
              if (data & (1 << (7 - col))) {
                display.drawPixel(x + col, y + row, SSD1306_WHITE);
              }
            }
          }
        }
      }
    }
    display.display();
    delay(16);
  }
}

void gameOver() {
  unsigned long startTime = millis();
  bool interrupted = false;
  while (millis() - startTime < 3000 && !interrupted) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    int goX = (SCREEN_WIDTH - (9 * 6)) / 2;
    display.setCursor(goX, 10);
    display.print("GAME OVER");

    for (int p = 0; p < (isMultiplayer ? 2 : 1); p++) {
      int yOffset = p * 20;
      display.setCursor(10, 20 + yOffset);
      display.print("PLAYER ");
      display.print(p + 1);
      display.print(" SCORE: ");
      display.print(players[p].score);
    }
    display.display();
    parseBluetooth();  // Check for input
    if (players[currentPlayer].moveLeft || players[currentPlayer].moveRight || players[currentPlayer].shoot) {
      interrupted = true;
    }
    delay(50);
  }
  gameStarted = false;
  // Reset input flags to require explicit B1/B2 to start
  players[0].moveLeft = false;
  players[0].moveRight = false;
  players[0].shoot = false;
  players[1].moveLeft = false;
  players[1].moveRight = false;
  players[1].shoot = false;
  attractMode();
}

void initGame() {
  aliensAlive = NUM_ALIENS;
  aliensMoveRight = true;
  
  players[currentPlayer].x = SCREEN_WIDTH / 2 - PLAYER_WIDTH / 2;
  players[currentPlayer].destroyed = false;
  players[currentPlayer].moveLeft = false;
  players[currentPlayer].moveRight = false;
  players[currentPlayer].shoot = false;
  
  int alienIndex = 0;
  for (int row = 0; row < 3; row++) {
    int aliensInRow = 5 - row;
    int startX = (SCREEN_WIDTH - (aliensInRow * ALIEN_SPACING_X)) / 2;
    for (int col = 0; col < aliensInRow; col++) {
      aliens[alienIndex].x = startX + col * ALIEN_SPACING_X;
      aliens[alienIndex].y = 2 + row * ALIEN_SPACING_Y;
      aliens[alienIndex].alive = true;
      aliens[alienIndex].type = row;
      alienIndex++;
    }
  }
  
  for (int i = 0; i < MAX_PROJECTILES + MAX_ALIEN_PROJECTILES; i++) {
    projectiles[i].active = false;
  }
  
  explosion.active = false;
  ufo.active = false;
  animationFrame = 0;
  alienMoveSoundIndex = 0;
  lastFrameChange = millis();
  lastAlienMove = millis();
  lastUFOSpawn = millis();
  lastUfoSoundToggle = millis();
  baseMoveInterval = max(50, baseMoveInterval - 50);
  
  for (int i = 0; i < NUM_BUNKERS; i++) {
    bunkers[i].x = 16 + i * 32;
    bunkers[i].y = 40;
    bunkers[i].damageLevel = 0;
  }
}

void drawPlayer() {
  const uint16_t* sprite = players[currentPlayer].destroyed ? playerDestroyedSprite : playerSprite;
  for (int row = 0; row < PLAYER_HEIGHT; row++) {
    uint16_t data = pgm_read_word(&sprite[row]);
    for (int col = 0; col < PLAYER_WIDTH; col++) {
      if (data & (1 << (10 - col))) {
        display.drawPixel(players[currentPlayer].x + col, 57 + row, SSD1306_WHITE);
      }
    }
  }
}

void drawProjectile(int x, int y, bool isAlien, byte type) {
  if (!isAlien) {
    for (int row = 0; row < PLAYER_PROJECTILE_HEIGHT; row++) {
      byte data = pgm_read_byte(&playerProjectileSprite[row]);
      if (data & 0x01) {  // 1-bit wide, rightmost bit
        display.drawPixel(x, y + row, SSD1306_WHITE);
      }
    }
  } else {
    for (int row = 0; row < ALIEN_PROJECTILE_HEIGHT; row++) {
      byte data = pgm_read_byte(&alienProjectileSprites[type][row]);
      for (int col = 0; col < ALIEN_PROJECTILE_WIDTH; col++) {
        if (data & (1 << (3 - col))) {  // 4-bit wide, right-aligned
          display.drawPixel(x + col, y + row, SSD1306_WHITE);
        }
      }
    }
  }
}

void drawAlien(int x, int y, byte type, byte frame) {
  for (int row = 0; row < ALIEN_HEIGHT; row++) {
    byte data = pgm_read_byte(&alienSprites[type][frame][row]);
    for (int col = 0; col < ALIEN_WIDTH; col++) {
      if (data & (1 << (7 - col))) {
        display.drawPixel(x + col, y + row, SSD1306_WHITE);
      }
    }
  }
}

void drawExplosion(int x, int y) {
  for (int row = 0; row < EXPLOSION_HEIGHT; row++) {
    byte data = pgm_read_byte(&explosionSprite[row]);
    for (int col = 0; col < EXPLOSION_WIDTH; col++) {
      if (data & (1 << (7 - col))) {
        display.drawPixel(x + col, y + row, SSD1306_WHITE);
      }
    }
  }
}

void drawUFO(int x, int y) {
  for (int row = 0; row < UFO_HEIGHT; row++) {
    byte data = pgm_read_byte(&ufoSprite[row]);
    for (int col = 0; col < UFO_WIDTH; col++) {
      if (data & (1 << (7 - col))) {
        display.drawPixel(x + col, y + row, SSD1306_WHITE);
      }
    }
  }
}

void drawBunker(int x, int y, int bunkerIndex) {
  byte damageLevel = bunkers[bunkerIndex].damageLevel;
  if (damageLevel < 4) {
    for (int row = 0; row < BUNKER_HEIGHT; row++) {
      byte data = pgm_read_byte(&bunkerSprites[damageLevel][row]);
      for (int col = 0; col < BUNKER_WIDTH; col++) {
        if (data & (1 << (7 - col))) {
          display.drawPixel(x + col, y + row, SSD1306_WHITE);
        }
      }
    }
  }
}

void damageBunker(int bunkerIndex) {
  if (bunkers[bunkerIndex].damageLevel < 4) {
    bunkers[bunkerIndex].damageLevel++;
  }
}

void triggerExplosion(int x, int y) {
  explosion.x = x - EXPLOSION_WIDTH / 2;
  explosion.y = y - EXPLOSION_HEIGHT / 2;
  explosion.startTime = millis();
  explosion.active = true;
  explosionSound.period = 1000000 / 200;  // 200 Hz for alien explosion
  explosionSound.lastToggle = micros();
  explosionSound.state = HIGH;
  digitalWrite(explosionSound.pin, HIGH);
  explosionSound.active = true;
  explosionSound.endTimeMillis = millis() + 200;  // 200 ms duration
}

bool isAlienBlocked(int alienIndex) {
  int shooterX = aliens[alienIndex].x;
  int shooterY = aliens[alienIndex].y;
  for (int i = 0; i < NUM_ALIENS; i++) {
    if (i != alienIndex && aliens[i].alive) {
      int otherX = aliens[i].x;
      int otherY = aliens[i].y;
      if (otherY > shooterY && 
          otherX + ALIEN_WIDTH > shooterX && 
          otherX < shooterX + ALIEN_WIDTH) {
        return true;
      }
    }
  }
  return false;
}

void updateGame() {
  if (players[currentPlayer].destroyed) {
    if (millis() - players[currentPlayer].destroyedTime > PLAYER_DESTROYED_DURATION) {
      players[currentPlayer].lives--;
      showLivesScreen();
      if ((!isMultiplayer && players[0].lives <= 0) || (isMultiplayer && players[0].lives <= 0 && players[1].lives <= 0)) {
        gameOver();
        return;
      } else if (isMultiplayer) {
        currentPlayer = (currentPlayer + 1) % 2;
        while (players[currentPlayer].lives <= 0) {
          currentPlayer = (currentPlayer + 1) % 2;
          if (players[0].lives <= 0 && players[1].lives <= 0) gameOver();
        }
      }
      initGame();
    }
    return;
  }

  // Player movement
  if (players[currentPlayer].moveLeft && players[currentPlayer].x > 0) {
    players[currentPlayer].x -= 3;
    digitalWrite(LED_LEFT, HIGH);
  } else {
    digitalWrite(LED_LEFT, LOW);
  }
  if (players[currentPlayer].moveRight && players[currentPlayer].x < SCREEN_WIDTH - PLAYER_WIDTH) {
    players[currentPlayer].x += 3;
    digitalWrite(LED_RIGHT, HIGH);
  } else {
    digitalWrite(LED_RIGHT, LOW);
  }

  // Shooting
  if (players[currentPlayer].shoot) {
    if (!projectiles[0].active) {
      projectiles[0].x = players[currentPlayer].x + PLAYER_WIDTH / 2;
      projectiles[0].y = 63 - PLAYER_HEIGHT - PLAYER_PROJECTILE_HEIGHT;
      projectiles[0].active = true;
      projectiles[0].isAlien = false;
      projectiles[0].type = 0;
      players[currentPlayer].shoot = false;
      shootSound.period = 1000000 / 1200;  // 1200 Hz for authentic shoot sound
      shootSound.lastToggle = micros();
      shootSound.state = HIGH;
      digitalWrite(shootSound.pin, HIGH);
      shootSound.active = true;
      shootSound.endTimeMillis = millis() + 100;  // 100 ms duration
      digitalWrite(LED_SHOOT, HIGH);
    }
  } else {
    digitalWrite(LED_SHOOT, LOW);
  }

  // Projectile movement
  if (projectiles[0].active && !projectiles[0].isAlien) {
    projectiles[0].y -= 3;
    if (projectiles[0].y + PLAYER_PROJECTILE_HEIGHT <= 0) {
      triggerExplosion(projectiles[0].x, 0);
      projectiles[0].active = false;
      shootSound.active = false;
    }
  }

  // Alien projectiles
  bool alienProjectileActive = false;
  for (int i = MAX_PROJECTILES; i < MAX_PROJECTILES + MAX_ALIEN_PROJECTILES; i++) {
    if (projectiles[i].active) {
      alienProjectileActive = true;
      break;
    }
  }
  if (!alienProjectileActive) {
    for (int j = 0; j < NUM_ALIENS; j++) {
      if (aliens[j].alive && !isAlienBlocked(j)) {
        int randomAlien = j + random(NUM_ALIENS - j);
        if (aliens[randomAlien].alive && !isAlienBlocked(randomAlien)) {
          for (int i = MAX_PROJECTILES; i < MAX_PROJECTILES + MAX_ALIEN_PROJECTILES; i++) {
            if (!projectiles[i].active) {
              projectiles[i].x = aliens[randomAlien].x + ALIEN_WIDTH / 2 - ALIEN_PROJECTILE_WIDTH / 2;
              projectiles[i].y = aliens[randomAlien].y + ALIEN_HEIGHT;
              projectiles[i].active = true;
              projectiles[i].isAlien = true;
              projectiles[i].type = random(3);
              break;
            }
          }
          break;
        }
      }
    }
  }

  for (int i = MAX_PROJECTILES; i < MAX_PROJECTILES + MAX_ALIEN_PROJECTILES; i++) {
    if (projectiles[i].active && projectiles[i].isAlien) {
      projectiles[i].y += 1;
      if (projectiles[i].y >= SCREEN_HEIGHT) {
        projectiles[i].active = false;
      }
    }
  }

  // Alien movement
  int moveInterval = baseMoveInterval - ((baseMoveInterval - 50) * (NUM_ALIENS - aliensAlive) / NUM_ALIENS);
  if (millis() - lastAlienMove > moveInterval) {
    lastAlienMove = millis();
    bool edgeHit = false;
    for (int i = 0; i < NUM_ALIENS; i++) {
      if (aliens[i].alive) {
        if (aliensMoveRight && aliens[i].x + ALIEN_WIDTH >= SCREEN_WIDTH - 5) edgeHit = true;
        else if (!aliensMoveRight && aliens[i].x <= 5) edgeHit = true;
      }
    }
    for (int i = 0; i < NUM_ALIENS; i++) {
      if (aliens[i].alive) {
        if (edgeHit) aliens[i].y += 4;
        else aliens[i].x += aliensMoveRight ? 2 : -2;
        if (aliens[i].y + ALIEN_HEIGHT >= 50) {
          gameOver();
          return;
        }
      }
    }
    if (edgeHit) aliensMoveRight = !aliensMoveRight;
    if (!ufo.active) {
      ufoEnemySound.period = 1000000 / alienMoveTones[alienMoveSoundIndex];
      ufoEnemySound.lastToggle = micros();
      ufoEnemySound.state = HIGH;
      digitalWrite(ufoEnemySound.pin, HIGH);
      ufoEnemySound.active = true;
      ufoEnemySound.endTimeMillis = millis() + 50;  // 50 ms for alien move sound
    }
    alienMoveSoundIndex = (alienMoveSoundIndex + 1) % 4;
  }

  // Animation
  if (millis() - lastFrameChange > 500) {
    lastFrameChange = millis();
    animationFrame = 1 - animationFrame;
  }

  // UFO
  if (!ufo.active && (millis() - lastUFOSpawn > random(15000, 25000))) {
    ufo.x = 0;
    ufo.active = true;
    ufo.movingRight = true;
    lastUFOSpawn = millis();
    lastUfoSoundToggle = millis();
    ufoSoundHigh = true;
    ufoEnemySound.period = 1000000 / ufoTones[0];
    ufoEnemySound.lastToggle = micros();
    ufoEnemySound.state = HIGH;
    digitalWrite(ufoEnemySound.pin, HIGH);
    ufoEnemySound.active = true;
    ufoEnemySound.endTimeMillis = 0xFFFFFFFF;
  }
  if (ufo.active) {
    ufo.x += ufo.movingRight ? 2 : -2;
    if (ufo.x <= 0 || ufo.x >= SCREEN_WIDTH - UFO_WIDTH) {
      ufo.active = false;
      ufoEnemySound.active = false;
    } else if (millis() - lastUfoSoundToggle > UFO_SOUND_INTERVAL) {
      ufoSoundHigh = !ufoSoundHigh;
      ufoEnemySound.period = 1000000 / ufoTones[ufoSoundHigh ? 0 : 1];
      lastUfoSoundToggle = millis();
    }
  }

  // Collisions
  for (int i = 0; i < MAX_PROJECTILES + MAX_ALIEN_PROJECTILES; i++) {
    if (projectiles[i].active) {
      int projWidth = projectiles[i].isAlien ? ALIEN_PROJECTILE_WIDTH : PLAYER_PROJECTILE_WIDTH;
      int projHeight = projectiles[i].isAlien ? ALIEN_PROJECTILE_HEIGHT : PLAYER_PROJECTILE_HEIGHT;
      if (!projectiles[i].isAlien) {
        for (int j = 0; j < NUM_ALIENS; j++) {
          if (aliens[j].alive &&
              projectiles[i].x >= aliens[j].x &&
              projectiles[i].x < aliens[j].x + ALIEN_WIDTH &&
              projectiles[i].y < aliens[j].y + ALIEN_HEIGHT &&
              projectiles[i].y + projHeight > aliens[j].y) {
            triggerExplosion(projectiles[i].x, projectiles[i].y + projHeight / 2);
            aliens[j].alive = false;
            projectiles[i].active = false;
            shootSound.active = false;
            aliensAlive--;
            players[currentPlayer].score += 10 * (3 - aliens[j].type);
            break;
          }
        }
        if (ufo.active &&
            projectiles[i].x >= ufo.x &&
            projectiles[i].x < ufo.x + UFO_WIDTH &&
            projectiles[i].y < 2 + UFO_HEIGHT &&
            projectiles[i].y + projHeight > 2) {
          triggerExplosion(projectiles[i].x, projectiles[i].y + projHeight / 2);
          ufo.active = false;
          ufoEnemySound.active = false;
          projectiles[i].active = false;
          shootSound.active = false;
          players[currentPlayer].score += 100;
        }
        for (int j = MAX_PROJECTILES; j < MAX_PROJECTILES + MAX_ALIEN_PROJECTILES; j++) {
          if (projectiles[j].active && projectiles[j].isAlien &&
              projectiles[i].x >= projectiles[j].x &&
              projectiles[i].x < projectiles[j].x + ALIEN_PROJECTILE_WIDTH &&
              projectiles[i].y < projectiles[j].y + ALIEN_PROJECTILE_HEIGHT &&
              projectiles[i].y + projHeight > projectiles[j].y) {
            triggerExplosion(projectiles[i].x, projectiles[i].y + projHeight / 2);
            projectiles[i].active = false;
            shootSound.active = false;
            projectiles[j].active = false;
            break;
          }
        }
      } else if (projectiles[i].x < players[currentPlayer].x + PLAYER_WIDTH &&
                 projectiles[i].x + projWidth > players[currentPlayer].x &&
                 projectiles[i].y < 57 + PLAYER_HEIGHT &&
                 projectiles[i].y + projHeight > 57) {
        players[currentPlayer].destroyed = true;
        players[currentPlayer].destroyedTime = millis();
        explosionSound.period = 1000000 / 150;  // 150 Hz for player destruction
        explosionSound.lastToggle = micros();
        explosionSound.state = HIGH;
        digitalWrite(explosionSound.pin, HIGH);
        explosionSound.active = true;
        explosionSound.endTimeMillis = millis() + 300;  // 300 ms duration
        projectiles[i].active = false;
      }
      for (int k = 0; k < NUM_BUNKERS; k++) {
        if (bunkers[k].damageLevel < 4 &&
            projectiles[i].x < bunkers[k].x + BUNKER_WIDTH &&
            projectiles[i].x + projWidth > bunkers[k].x &&
            projectiles[i].y < bunkers[k].y + BUNKER_HEIGHT &&
            projectiles[i].y + projHeight > bunkers[k].y) {
          damageBunker(k);
          projectiles[i].active = false;
          if (!projectiles[i].isAlien) shootSound.active = false;
          break;
        }
      }
    }
  }

  if (aliensAlive == 0) initGame();
  if (explosion.active && (millis() - explosion.startTime > EXPLOSION_DURATION)) explosion.active = false;
}

void drawGame() {
  display.clearDisplay();
  drawPlayer();
  for (int i = 0; i < NUM_ALIENS; i++) {
    if (aliens[i].alive) {
      drawAlien(aliens[i].x, aliens[i].y, aliens[i].type, animationFrame);
    }
  }
  for (int i = 0; i < MAX_PROJECTILES + MAX_ALIEN_PROJECTILES; i++) {
    if (projectiles[i].active) {
      drawProjectile(projectiles[i].x, projectiles[i].y, projectiles[i].isAlien, projectiles[i].type);
    }
  }
  for (int k = 0; k < NUM_BUNKERS; k++) {
    drawBunker(bunkers[k].x, bunkers[k].y, k);
  }
  if (explosion.active) {
    drawExplosion(explosion.x, explosion.y);
  }
  if (ufo.active) {
    drawUFO(ufo.x, 2);
  }
  display.display();
}

void parseBluetooth() {
  static char buffer[16];
  static byte pos = 0;
  while (HM10.available()) {
    char c = HM10.read();
    if (c == '\n') {
      buffer[pos] = '\0';
      if (strncmp(buffer, "B0", 2) == 0) {
        players[currentPlayer].shoot = true;
      }
      else if (strncmp(buffer, "B1", 2) == 0 && !gameStarted) {
        isMultiplayer = false;
        gameStarted = true;
        currentPlayer = 0;
      }
      else if (strncmp(buffer, "B2", 2) == 0 && !gameStarted) {
        isMultiplayer = true;
        gameStarted = true;
        currentPlayer = 0;
      }
      else if (strncmp(buffer, "J0:", 3) == 0) {
        char* comma = strchr(buffer, ',');
        if (comma) {
          float angle = atof(buffer + 3);
          float dist = atof(comma + 1);
          if (dist < 0.2) {
            players[currentPlayer].moveLeft = false;
            players[currentPlayer].moveRight = false;
          } else {
            if (angle > 90.0 && angle <= 270.0) {
              players[currentPlayer].moveLeft = true;
              players[currentPlayer].moveRight = false;
            } else {
              players[currentPlayer].moveRight = true;
              players[currentPlayer].moveLeft = false;
            }
          }
        }
      }
      pos = 0;
      buffer[0] = '\0';
    }
    else if (pos < 15) {
      buffer[pos++] = c;
      buffer[pos] = '\0';
    }
    else {
      pos = 0;
      buffer[0] = '\0';
    }
  }
}

void toggleSounds() {
  unsigned long currentMicros = micros();
  unsigned long currentMillis = millis();

  // Shoot sound
  if (shootSound.active && currentMillis < shootSound.endTimeMillis) {
    if (currentMicros - shootSound.lastToggle >= shootSound.period / 2) {
      shootSound.state = !shootSound.state;
      digitalWrite(shootSound.pin, shootSound.state);
      shootSound.lastToggle = currentMicros;
    }
  } else if (shootSound.active) {
    shootSound.active = false;
    digitalWrite(shootSound.pin, LOW);
  }

  // UFO/Enemy sound
  if (ufoEnemySound.active && currentMillis < ufoEnemySound.endTimeMillis) {
    if (currentMicros - ufoEnemySound.lastToggle >= ufoEnemySound.period / 2) {
      ufoEnemySound.state = !ufoEnemySound.state;
      digitalWrite(ufoEnemySound.pin, ufoEnemySound.state);
      ufoEnemySound.lastToggle = currentMicros;
    }
  } else if (ufoEnemySound.active) {
    ufoEnemySound.active = false;
    digitalWrite(ufoEnemySound.pin, LOW);
  }

  // Explosion sound
  if (explosionSound.active && currentMillis < explosionSound.endTimeMillis) {
    if (currentMicros - explosionSound.lastToggle >= explosionSound.period / 2) {
      explosionSound.state = !explosionSound.state;
      digitalWrite(explosionSound.pin, explosionSound.state);
      explosionSound.lastToggle = currentMicros;
    }
  } else if (explosionSound.active) {
    explosionSound.active = false;
    digitalWrite(explosionSound.pin, LOW);
  }
}

void setup() {
  pinMode(SHOOT_PIN, OUTPUT);
  pinMode(UFO_ENEMY_PIN, OUTPUT);
  pinMode(EXPLOSION_PIN, OUTPUT);
  pinMode(LED_LEFT, OUTPUT);
  pinMode(LED_RIGHT, OUTPUT);
  pinMode(LED_SHOOT, OUTPUT);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) while (1);
  display.setRotation(2);
  HM10.begin(9600);
  attractMode();
}

void loop() {
  static unsigned long lastFrame = 0;
  toggleSounds();  // Run as fast as possible
  unsigned long currentMillis = millis();
  if (currentMillis - lastFrame >= 16) {
    lastFrame = currentMillis;
    parseBluetooth();
    updateGame();
    drawGame();
  }
}
