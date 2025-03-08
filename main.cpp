#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>

// Display setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Bluetooth setup
SoftwareSerial HM10(10, 11);  // RX (pin 2, from HM10 TX), TX (pin 3, to HM10 RX)

// Game constants
#define PLAYER_WIDTH 11
#define PLAYER_HEIGHT 7
#define ALIEN_WIDTH 8
#define ALIEN_HEIGHT 8
#define MAX_PROJECTILES 3
#define NUM_ALIENS 15
#define ALIEN_SPACING_X 14
#define ALIEN_SPACING_Y 10
#define EXPLOSION_WIDTH 8
#define EXPLOSION_HEIGHT 8
#define EXPLOSION_DURATION 200  // ms

// Game state
struct Projectile {
  int x, y;
  bool active;
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
Projectile projectiles[MAX_PROJECTILES];
Alien aliens[NUM_ALIENS];
Explosion explosion;  // Single explosion for simplicity
int playerX = SCREEN_WIDTH / 2 - PLAYER_WIDTH / 2;
int score = 0;
int aliensAlive = NUM_ALIENS;
bool aliensMoveRight = true;

// Animation
byte animationFrame = 0;
unsigned long lastFrameChange = 0;
unsigned long lastAlienMove = 0;

// Sprites in PROGMEM
const uint16_t playerSprite[PLAYER_HEIGHT] PROGMEM = {
  0x020, 0x070, 0x070, 0x7F8, 0x7FF, 0x7FF, 0x7FF
};

const byte alienSprites[3][2][8] PROGMEM = {
  { // Type 0 - Squid-like
    {0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x24, 0x5A, 0x81},
    {0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x42, 0xA5, 0x42}
  },
  { // Type 1 - Crab-like
    {0x00, 0x3C, 0x7E, 0xDB, 0xFF, 0x5A, 0x81, 0x42},
    {0x00, 0x3C, 0x7E, 0xDB, 0xFF, 0x24, 0x5A, 0xA5}
  },
  { // Type 2 - Octopus-like
    {0x3C, 0x7E, 0xFF, 0xDB, 0xFF, 0x18, 0x24, 0x42},
    {0x3C, 0x7E, 0xFF, 0xDB, 0xFF, 0x18, 0x42, 0x81}
  }
};

const byte explosionSprite[EXPLOSION_HEIGHT] PROGMEM = {
  0x14,  // 00010100
  0x22,  // 00100010
  0x5D,  // 01011101
  0xA5,  // 10100101
  0xA5,  // 10100101
  0x5D,  // 01011101
  0x22,  // 00100010
  0x14   // 00010100
};

// Input flags
bool moveLeft = false;
bool moveRight = false;
bool shoot = false;

void initGame() {
  playerX = SCREEN_WIDTH / 2 - PLAYER_WIDTH / 2;
  score = 0;
  aliensAlive = NUM_ALIENS;
  aliensMoveRight = true;

  for (int i = 0; i < NUM_ALIENS; i++) {
    int row = i / 5;
    int col = i % 5;
    aliens[i].x = 10 + col * ALIEN_SPACING_X;
    aliens[i].y = 10 + row * ALIEN_SPACING_Y;
    aliens[i].alive = true;
    aliens[i].type = row % 3;
  }

  for (int i = 0; i < MAX_PROJECTILES; i++) {
    projectiles[i].active = false;
  }

  explosion.active = false;
  animationFrame = 0;
  lastFrameChange = millis();
  lastAlienMove = millis();
}

void drawPlayer() {
  for (int row = 0; row < PLAYER_HEIGHT; row++) {
    uint16_t data = pgm_read_word(&playerSprite[row]);
    for (int col = 0; col < PLAYER_WIDTH; col++) {
      if (data & (1 << (10 - col))) {
        display.drawPixel(playerX + col, SCREEN_HEIGHT - PLAYER_HEIGHT + row, SSD1306_WHITE);
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

void triggerExplosion(int x, int y) {
  explosion.x = x - EXPLOSION_WIDTH / 2;  // Center on impact
  explosion.y = y - EXPLOSION_HEIGHT / 2;
  explosion.startTime = millis();
  explosion.active = true;
}

void updateGame() {
  if (moveLeft && playerX > 0) playerX -= 3;
  if (moveRight && playerX < SCREEN_WIDTH - PLAYER_WIDTH) playerX += 3;
  if (shoot) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
      if (!projectiles[i].active) {
        projectiles[i].x = playerX + PLAYER_WIDTH / 2;
        projectiles[i].y = SCREEN_HEIGHT - PLAYER_HEIGHT - 2;
        projectiles[i].active = true;
        shoot = false;
        break;
      }
    }
  }
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    if (projectiles[i].active) {
      projectiles[i].y -= 3;
      if (projectiles[i].y < 0) {
        triggerExplosion(projectiles[i].x, 0);  // Top of screen
        projectiles[i].active = false;
      }
    }
  }
  if (millis() - lastAlienMove > 500) {
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
        if (aliens[i].y + ALIEN_HEIGHT >= SCREEN_HEIGHT - PLAYER_HEIGHT) {
          triggerExplosion(aliens[i].x + ALIEN_WIDTH / 2, SCREEN_HEIGHT - PLAYER_HEIGHT);
          initGame();  // Game over
          return;
        }
      }
    }
    if (edgeHit) aliensMoveRight = !aliensMoveRight;
  }
  if (millis() - lastFrameChange > 500) {
    lastFrameChange = millis();
    animationFrame = 1 - animationFrame;
  }
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    if (projectiles[i].active) {
      for (int j = 0; j < NUM_ALIENS; j++) {
        if (aliens[j].alive &&
            projectiles[i].x >= aliens[j].x &&
            projectiles[i].x < aliens[j].x + ALIEN_WIDTH &&
            projectiles[i].y >= aliens[j].y &&
            projectiles[i].y < aliens[j].y + ALIEN_HEIGHT) {
          triggerExplosion(projectiles[i].x, projectiles[i].y);
          aliens[j].alive = false;
          projectiles[i].active = false;
          aliensAlive--;
          score += 10 * (3 - aliens[j].type);
          break;
        }
      }
    }
  }
  if (aliensAlive == 0) initGame();

  // Update explosion
  if (explosion.active && (millis() - explosion.startTime > EXPLOSION_DURATION)) {
    explosion.active = false;
  }
}

void drawGame() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 8);

  drawPlayer();
  for (int i = 0; i < NUM_ALIENS; i++) {
    if (aliens[i].alive) {
      drawAlien(aliens[i].x, aliens[i].y, aliens[i].type, animationFrame);
    }
  }
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    if (projectiles[i].active) {
      display.drawLine(projectiles[i].x, projectiles[i].y - 2, projectiles[i].x, projectiles[i].y, SSD1306_WHITE);
    }
  }
  if (explosion.active) {
    drawExplosion(explosion.x, explosion.y);
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
        shoot = true;
      }
      else if (strncmp(buffer, "J0:", 3) == 0) {
        char* comma = strchr(buffer, ',');
        if (comma) {
          float angle = atof(buffer + 3);
          float dist = atof(comma + 1);
          if (dist < 0.2) {
            moveLeft = false;
            moveRight = false;
          } else {
            if (angle > 90.0 && angle <= 270.0) {
              moveLeft = true;
              moveRight = false;
            } else {
              moveRight = true;
              moveLeft = false;
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
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (1);
  }

  HM10.begin(9600);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 20);
  display.println("SPACE INVADERS");
  display.display();
  delay(2000);

  initGame();
}

void loop() {
  parseBluetooth();
  updateGame();
  drawGame();
  delay(16);
}
