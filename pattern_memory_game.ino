#include <LedControl.h>

// ---------- Pins ----------
const int PIN_DIN = 12;
const int PIN_CLK = 11;
const int PIN_CS  = 10;

const int JOY_X  = A0;
const int JOY_Y  = A1;
const int JOY_SW = 2;   

LedControl lc = LedControl(PIN_DIN, PIN_CLK, PIN_CS, 1);

// ---------- Tweakables ----------
const int START_LEVEL   = 1;
const int MAX_LEVEL     = 16;

const int STAGE1_LEVELS = 6;    
const int STAGE2_LEVELS = 4;    

const int START_CELLS_S1 = 3;
const int START_CELLS_S2 = 3;   
const int START_CELLS_S3 = 4;   

const unsigned long SHOW_TIME_BASE = 2000; 
const unsigned long SHOW_TIME_PER_CELL = 300; 
const unsigned long LEVEL_DISPLAY_TIME = 1500; 
const unsigned long BLINK_TIME = 200;       


const unsigned long MOVE_DELAY_S1 = 220;
const unsigned long MOVE_DELAY_S2 = 190;
const unsigned long MOVE_DELAY_S3 = 150;

const bool INVERT_X = false;
const bool INVERT_Y = false;

const bool MIRROR_COLUMNS = false;

const int STICK_LOW  = 300;
const int STICK_HIGH = 700;

// ---------- Game state ----------
const int MAXG = 8;           

byte pattern[MAXG];      
byte locked[MAXG];

int  level = START_LEVEL;
int  cellCount = START_CELLS_S1; 
int  curRow = 0, curCol = 0;
unsigned long lastMove = 0;


int cellW = 2, cellH = 2;     
int gridCols = 4, gridRows = 4;
unsigned long moveDelay = MOVE_DELAY_S1;


const byte CROSS[8] = {
  B10000001, B01000010, B00100100, B00011000,
  B00011000, B00100100, B01000010, B10000001
};


const byte FONT[10][5] = {
  {B111, B101, B101, B101, B111},  
  {B010, B110, B010, B010, B111},  
  {B111, B001, B111, B100, B111},  
  {B111, B001, B111, B001, B111},  
  {B101, B101, B111, B001, B001},  
  {B111, B100, B111, B001, B111},  
  {B111, B100, B111, B101, B111},  
  {B111, B001, B001, B001, B001},  
  {B111, B101, B111, B101, B111},  
  {B111, B101, B111, B001, B111}  
};

// ---------- Stage handling ----------
void setStage() {
  int firstLevel, startCells;

  if (level <= STAGE1_LEVELS) {                          
    cellW = 2; cellH = 2; moveDelay = MOVE_DELAY_S1;
    firstLevel = 1;
    startCells = START_CELLS_S1;
  } else if (level <= STAGE1_LEVELS + STAGE2_LEVELS) {  
    cellW = 2; cellH = 1; moveDelay = MOVE_DELAY_S2;
    firstLevel = STAGE1_LEVELS + 1;
    startCells = START_CELLS_S2;
  } else {                                           
    cellW = 1; cellH = 1; moveDelay = MOVE_DELAY_S3;
    firstLevel = STAGE1_LEVELS + STAGE2_LEVELS + 1;
    startCells = START_CELLS_S3;
  }

  gridCols = 8 / cellW;
  gridRows = 8 / cellH;
  cellCount = startCells + (level - firstLevel);
}

// ---------- Drawing helpers ----------
byte fixBits(byte b) {
  if (!MIRROR_COLUMNS) return b;
  byte r = 0;
  for (int i = 0; i < 8; i++) if (b & (1 << i)) r |= (1 << (7 - i));
  return r;
}


void drawFrame(const byte frame[8]) {
  for (int r = 0; r < 8; r++) lc.setRow(0, r, fixBits(frame[r]));
}


void expandCells(const byte cells[MAXG], byte out[8]) {
  for (int i = 0; i < 8; i++) out[i] = 0;
  byte colMask = (1 << cellW) - 1;
  for (int r = 0; r < gridRows; r++) {
    byte row = 0;
    for (int c = 0; c < gridCols; c++) {
      if (cells[r] & (1 << c)) row |= (colMask << (8 - (c + 1) * cellW));
    }
    for (int k = 0; k < cellH; k++) out[r * cellH + k] = row;
  }
}

void drawCells(const byte cells[MAXG]) {
  byte frame[8];
  expandCells(cells, frame);
  drawFrame(frame);
}

void clearCells(byte cells[MAXG]) {
  for (int r = 0; r < MAXG; r++) cells[r] = 0;
}

int countCells(const byte cells[MAXG]) {
  int n = 0;
  for (int r = 0; r < MAXG; r++) {
    byte b = cells[r];
    while (b) { n += b & 1; b >>= 1; }
  }
  return n;
}

bool cellsEqual(const byte a[MAXG], const byte b[MAXG]) {
  for (int r = 0; r < MAXG; r++) if (a[r] != b[r]) return false;
  return true;
}

void flashCells(const byte cells[MAXG], int times, int ms) {
  for (int i = 0; i < times; i++) {
    drawCells(cells);
    delay(ms);
    lc.clearDisplay(0);
    delay(ms);
  }
}

void flashFrame(const byte frame[8], int times, int ms) {
  for (int i = 0; i < times; i++) {
    drawFrame(frame);
    delay(ms);
    lc.clearDisplay(0);
    delay(ms);
  }
}


void showNumber(int n) {
  byte frame[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  for (int r = 0; r < 5; r++) {
    if (n >= 10) {
      frame[r + 1] = (FONT[(n / 10) % 10][r] << 5) | (FONT[n % 10][r] << 1);
    } else {
      frame[r + 1] = FONT[n][r] << 3;
    }
  }
  drawFrame(frame);
}

// ---------- Pattern generation ----------
void generatePattern(int count) {
  clearCells(pattern);
  int placed = 0;
  while (placed < count) {
    int r = random(gridRows);
    int c = random(gridCols);
    if (!(pattern[r] & (1 << c))) {
      pattern[r] |= (1 << c);
      placed++;
    }
  }
}

// ---------- Input ----------
void handleMove() {
  if (millis() - lastMove < moveDelay) return;

  int x = analogRead(JOY_X);
  int y = analogRead(JOY_Y);
  int dx = 0, dy = 0;

  if (x < STICK_LOW) dx = -1; else if (x > STICK_HIGH) dx = 1;
  if (y < STICK_LOW) dy = -1; else if (y > STICK_HIGH) dy = 1;

  if (INVERT_X) dx = -dx;
  if (INVERT_Y) dy = -dy;

  if (dx != 0 || dy != 0) {
    curCol = (curCol + dx + gridCols) % gridCols; 
    curRow = (curRow + dy + gridRows) % gridRows;
    lastMove = millis();
  }
}

bool buttonPressed() {
  static bool lastState = HIGH;
  static unsigned long lastChange = 0;
  bool state = digitalRead(JOY_SW);
  bool pressed = false;

  if (state != lastState && millis() - lastChange > 40) {   // debounce
    lastChange = millis();
    if (state == LOW) pressed = true;
    lastState = state;
  }
  return pressed;
}

// ---------- Game phases ----------
void showPattern() {
  drawCells(pattern);
  delay(SHOW_TIME_BASE + SHOW_TIME_PER_CELL * cellCount);
  lc.clearDisplay(0);
  delay(400);
}


bool playerInput(int target) {
  clearCells(locked);
  curRow = gridRows / 2;
  curCol = gridCols / 2;
  bool blinkOn = true;
  unsigned long lastBlink = millis();

  while (true) {
    handleMove();

    if (buttonPressed()) {
      locked[curRow] ^= (1 << curCol);          
      if (countCells(locked) == target) {      
        drawCells(locked);
        delay(500);
        return cellsEqual(locked, pattern);
      }
    }

    if (millis() - lastBlink >= BLINK_TIME) {
      blinkOn = !blinkOn;
      lastBlink = millis();
    }

    byte cells[MAXG];
    for (int r = 0; r < MAXG; r++) cells[r] = locked[r];
    if (blinkOn) cells[curRow] ^= (1 << curCol);
    drawCells(cells);
  }
}

// ---------- Arduino ----------
void setup() {
  pinMode(JOY_SW, INPUT_PULLUP);

  lc.shutdown(0, false);   
  lc.setIntensity(0, 5);   
  lc.clearDisplay(0);

  randomSeed(analogRead(A5));  

  byte allOn[8] = {255, 255, 255, 255, 255, 255, 255, 255};
  flashFrame(allOn, 2, 200);
  delay(500);
}

void loop() {
  setStage();                         

  generatePattern(cellCount);
  showPattern();

  bool correct = playerInput(cellCount);

  if (correct) {
    flashCells(pattern, 2, 150);
    if (level < MAX_LEVEL) level++;
    showNumber(level);                 
    delay(LEVEL_DISPLAY_TIME);
  } else {
    drawFrame(CROSS);
    delay(1200);
    flashCells(pattern, 3, 300);       
    level = START_LEVEL;
  }

  lc.clearDisplay(0);
  delay(800);
}
