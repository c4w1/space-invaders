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
#define SPEAKER_PIN 8
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
#define PLAYER_DESTROYED_DURATION 1000  // 1 second delay for destroyed player
#define LIFE_ICON_WIDTH 8
#define LIFE_ICON_HEIGHT 5
#define MAX_LIVES 3
#define LIVES_SCREEN_DURATION 2000  // 2 seconds for lives screen
#define UFO_SOUND_INTERVAL 200  // UFO sound toggle every 200ms
#define PLAYER_PROJECTILE_WIDTH 1
#define PLAYER_PROJECTILE_HEIGHT 5
#define ALIEN_PROJECTILE_WIDTH 4
#define ALIEN_PROJECTILE_HEIGHT 6

// Game state structures
struct Projectile {
  int x, y;
  bool active;
  bool isAlien;
  byte type;  // For alien projectiles: 0 (dot), 1 (squiggly), 2 (double-segment)
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
  byte damageLevel;  // 0 (full) to 4 (destroyed)
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

// Game variables
Projectile projectiles[MAX_PROJECTILES + MAX_ALIEN_PROJECTILES];
Alien aliens[NUM_ALIENS];
Explosion explosion;
UFO ufo = {0, false, true};
Bunker bunkers[NUM_BUNKERS];
Player players[2] = {
  {SCREEN_WIDTH / 2 - PLAYER_WIDTH / 2, 0, MAX_LIVES, false, 0, false, false, false},  // Player 1
  {SCREEN_WIDTH / 2 - PLAYER_WIDTH / 2, 0, MAX_LIVES, false, 0, false, false, false}   // Player 2
};
int aliensAlive = NUM_ALIENS;
bool aliensMoveRight = true;
int baseMoveInterval = 500;
bool isMultiplayer = false;
int currentPlayer = 0;  // 0 for Player 1, 1 for Player 2
bool ufoSoundHigh = false;
unsigned long lastUfoSoundToggle = 0;

// Animation and timing
byte animationFrame = 0;
unsigned long lastFrameChange = 0;
unsigned long lastAlienMove = 0;
byte alienMoveSoundIndex = 0;
unsigned long lastUFOSpawn = 0;

// Flag for continuous shoot tone
bool shootToneActive = false;

// Sprites in PROGMEM
const uint16_t playerSprite[PLAYER_HEIGHT] PROGMEM = {
  0x010, 0x038, 0x07C, 0x0FE, 0x1FF, 0x3FF, 0x3FF
};
const uint16_t playerDestroyedSprite[PLAYER_HEIGHT] PROGMEM = {
  0x091, 0x122, 0x244, 0x088, 0x155, 0x222, 0x091  // Scattered debris
};
const byte lifeIconSprite[LIFE_ICON_HEIGHT] PROGMEM = {
  0x08, 0x1C, 0x3E, 0x7F, 0x7F  // Small ship for lives
};
const byte lifeDestroyedSprite[LIFE_ICON_HEIGHT] PROGMEM = {
  0x14, 0x22, 0x08, 0x55, 0x22  // Debris for destroyed life
};
const byte playerProjectileSprite[PLAYER_PROJECTILE_HEIGHT] PROGMEM = {
  0x01, 0x01, 0x01, 0x01, 0x01  // Thin, straight laser bolt (1x5)
};
const byte alienProjectileSprites[3][ALIEN_PROJECTILE_HEIGHT] PROGMEM = {
  {0x06, 0x06, 0x06, 0x06, 0x06, 0x06},  // Dot/pellet (4x6, small square)
  {0x02, 0x05, 0x0A, 0x04, 0x09, 0x06},  // Squiggly/wavy (4x6, twisting)
  {0x06, 0x06, 0x00, 0x06, 0x06, 0x00}   // Double-segment (4x6, two parts)
};
const byte alienSprites[3][2][ALIEN_HEIGHT] PROGMEM = {
  { 
    {0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x24, 0x5A, 0x81},  // Type 0 (30 pts)
    {0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x42, 0xA5, 0x42}
  },
  { 
    {0x00, 0x3C, 0x7E, 0xDB, 0xFF, 0x5A, 0x81, 0x42},  // Type 1 (20 pts)
    {0x00, 0x3C, 0x7E, 0xDB, 0xFF, 0x24, 0x5A, 0xA5}
  },
  { 
    {0x3C, 0x7E, 0xFF, 0xDB, 0xFF, 0x18, 0x24, 0x42},  // Type 2 (10 pts)
    {0x3C, 0x7E, 0xFF, 0xDB, 0xFF, 0x18, 0x42, 0x81}
  }
};
const byte explosionSprite[EXPLOSION_HEIGHT] PROGMEM = {
  0x14, 0x22, 0x5D, 0xA5, 0xA5, 0x5D, 0x22, 0x14
};
const byte ufoSprite[UFO_HEIGHT] PROGMEM = {
  0x10, 0x7C, 0xFE, 0x7C, 0x10
};
const byte bunkerSprites[5][BUNKER_HEIGHT] PROGMEM = {
  {0x3C, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF},  // Full
  {0x38, 0x7C, 0xFE, 0xFF, 0xFF, 0xFF},  // Slightly damaged
  {0x30, 0x58, 0xFE, 0xFD, 0xFF, 0xFE},  // Moderately damaged
  {0x20, 0x50, 0xF8, 0xF4, 0xFE, 0xE8},  // Heavily damaged
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}   // Destroyed
};

// Input flags
bool gameStarted = false;

// Alien movement tones
const int alienMoveTones[4] = {120, 100, 80, 60};
// UFO sound tones
const int ufoTones[2] = {800, 600};  // High-low pitch pattern

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
      display.print("B1: 1P  B2: 2P");
    }
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
  tone(SPEAKER_PIN, 200, 150);
}

bool isAlienBlocked(int alienIndex) {
  int shooterX = aliens[alienIndex].x;
  int shooterY = aliens[alienIndex].y;
  for (int i = 0; i < NUM_ALIENS; i++) {
    if (i != alienIndex && aliens[i].alive) {
      int otherX = aliens[i].x;
      int otherY = aliens[i].y;
      // Check if another alien is below and in the same column
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
        currentPlayer = (currentPlayer + 1) % 2;  // Switch players
        while (players[currentPlayer].lives <= 0) {  // Skip dead players
          currentPlayer = (currentPlayer + 1) % 2;
          if (players[0].lives <= 0 && players[1].lives <= 0) {
            gameOver();
            return;
          }
        }
      }
      initGame();  // Restart with new player or same player in 1P
    }
    return;
  }

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
  
  if (players[currentPlayer].shoot) {
    if (!projectiles[0].active) {
      projectiles[0].x = players[currentPlayer].x + PLAYER_WIDTH / 2;
      projectiles[0].y = 63 - PLAYER_HEIGHT - PLAYER_PROJECTILE_HEIGHT;
      projectiles[0].active = true;
      projectiles[0].isAlien = false;
      projectiles[0].type = 0;  // Not used for player
      players[currentPlayer].shoot = false;
      tone(SPEAKER_PIN, 1000);
      shootToneActive = true;
      digitalWrite(LED_SHOOT, HIGH);
    }
  } else {
    digitalWrite(LED_SHOOT, LOW);
  }
  
  if (projectiles[0].active && !projectiles[0].isAlien) {
    projectiles[0].y -= 3;  // Fast upward movement
    if (projectiles[0].y + PLAYER_PROJECTILE_HEIGHT <= 0) {
      triggerExplosion(projectiles[0].x, 0);
      projectiles[0].active = false;
    }
  }
  if (!projectiles[0].active && shootToneActive) {
    noTone(SPEAKER_PIN);
    shootToneActive = false;
  }
  
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
              projectiles[i].type = random(3);  // Randomly select: 0 (dot), 1 (squiggly), 2 (double)
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
      projectiles[i].y += 1;  // Slow downward movement
      if (projectiles[i].y >= SCREEN_HEIGHT) {
        projectiles[i].active = false;
      }
    }
  }
  
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
    if (!shootToneActive && !ufo.active)
      tone(SPEAKER_PIN, alienMoveTones[alienMoveSoundIndex], 50);
    alienMoveSoundIndex = (alienMoveSoundIndex + 1) % 4;
  }
  
  if (millis() - lastFrameChange > 500) {
    lastFrameChange = millis();
    animationFrame = 1 - animationFrame;
  }
  
  if (!ufo.active && (millis() - lastUFOSpawn > random(15000, 25000))) {
    ufo.x = 0;
    ufo.active = true;
    ufo.movingRight = true;
    lastUFOSpawn = millis();
    lastUfoSoundToggle = millis();
    ufoSoundHigh = true;
  }
  if (ufo.active) {
    ufo.x += ufo.movingRight ? 2 : -2;
    if (ufo.x <= 0 || ufo.x >= SCREEN_WIDTH - UFO_WIDTH) {
      ufo.active = false;
      noTone(SPEAKER_PIN);
    } else if (!shootToneActive && millis() - lastUfoSoundToggle > UFO_SOUND_INTERVAL) {
      ufoSoundHigh = !ufoSoundHigh;
      tone(SPEAKER_PIN, ufoTones[ufoSoundHigh ? 0 : 1], UFO_SOUND_INTERVAL);
      lastUfoSoundToggle = millis();
    }
  }
  
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
          projectiles[i].active = false;
          players[currentPlayer].score += 100;
          noTone(SPEAKER_PIN);
        }
        // Check for collision with alien projectiles
        for (int j = MAX_PROJECTILES; j < MAX_PROJECTILES + MAX_ALIEN_PROJECTILES; j++) {
          if (projectiles[j].active && projectiles[j].isAlien &&
              projectiles[i].x >= projectiles[j].x &&
              projectiles[i].x < projectiles[j].x + ALIEN_PROJECTILE_WIDTH &&
              projectiles[i].y < projectiles[j].y + ALIEN_PROJECTILE_HEIGHT &&
              projectiles[i].y + projHeight > projectiles[j].y) {
            triggerExplosion(projectiles[i].x, projectiles[i].y + projHeight / 2);
            projectiles[i].active = false;
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
        tone(SPEAKER_PIN, 300, 500);
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
          break;
        }
      }
    }
  }
  
  if (aliensAlive == 0) initGame();
  
  if (explosion.active && (millis() - explosion.startTime > EXPLOSION_DURATION)) {
    explosion.active = false;
  }
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

void setup() {
  pinMode(SPEAKER_PIN, OUTPUT);
  pinMode(LED_LEFT, OUTPUT);
  pinMode(LED_RIGHT, OUTPUT);
  pinMode(LED_SHOOT, OUTPUT);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (1);
  }
  display.setRotation(2);
  HM10.begin(9600);
  attractMode();
}

void loop() {
  parseBluetooth();
  updateGame();
  drawGame();
  delay(16);
}
