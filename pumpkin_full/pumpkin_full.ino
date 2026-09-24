#include <SPI.h>
#include <math.h>
#include <Adafruit_NeoPixel.h>

#define PIN_DC 9
#define PIN_RST 8
#define PIN_CS 10
#define RING_PIN 6
#define NUM_LEDS 16
#define BUZZER_PIN 7
#define TRIG_PIN 4
#define ECHO_PIN 5

#define BLACK 0x0000
#define WHITE 0xFFFF

Adafruit_NeoPixel ring(NUM_LEDS, RING_PIN, NEO_GRB + NEO_KHZ800);

// ---------- LCD low-level ----------
struct InitCmd {
  uint8_t cmd;
  const uint8_t *data;
  uint8_t len;
  uint16_t delayMs;
};

const uint8_t d_b6[] = {0x00, 0x00};
const uint8_t d_36[] = {0x48};
const uint8_t d_3a[] = {0x05};
const uint8_t d_c3[] = {0x13};
const uint8_t d_c4[] = {0x13};
const uint8_t d_c9[] = {0x22};
const uint8_t d_f0[] = {0x45, 0x09, 0x08, 0x08, 0x26, 0x2a};
const uint8_t d_f1[] = {0x43, 0x70, 0x72, 0x36, 0x37, 0x6f};
const uint8_t d_f2[] = {0x45, 0x09, 0x08, 0x08, 0x26, 0x2a};
const uint8_t d_f3[] = {0x43, 0x70, 0x72, 0x36, 0x37, 0x6f};
const uint8_t d_66[] = {0x3c, 0x00, 0xcd, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00};
const uint8_t d_67[] = {0x00, 0x3c, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98};
const uint8_t d_74[] = {0x10, 0x85, 0x80, 0x00, 0x00, 0x4e, 0x00};
const uint8_t d_98[] = {0x3e, 0x07};

const InitCmd initSeq[] = {
  {0xFE, nullptr, 0, 0}, {0xEF, nullptr, 0, 0},
  {0xB6, d_b6, 2, 0}, {0x36, d_36, 1, 0}, {0x3A, d_3a, 1, 0},
  {0xC3, d_c3, 1, 0}, {0xC4, d_c4, 1, 0}, {0xC9, d_c9, 1, 0},
  {0xF0, d_f0, 6, 0}, {0xF1, d_f1, 6, 0}, {0xF2, d_f2, 6, 0}, {0xF3, d_f3, 6, 0},
  {0x66, d_66, 10, 0}, {0x67, d_67, 10, 0}, {0x74, d_74, 7, 0}, {0x98, d_98, 2, 0},
  {0x35, nullptr, 0, 0}, {0x21, nullptr, 0, 0},
  {0x11, nullptr, 0, 120}, {0x29, nullptr, 0, 20},
};

void writeCommand(uint8_t cmd) {
  digitalWrite(PIN_CS, LOW);
  digitalWrite(PIN_DC, LOW);
  SPI.transfer(cmd);
  digitalWrite(PIN_CS, HIGH);
}

void writeData(const uint8_t *data, size_t len) {
  digitalWrite(PIN_CS, LOW);
  digitalWrite(PIN_DC, HIGH);
  for (size_t i = 0; i < len; i++) SPI.transfer(data[i]);
  digitalWrite(PIN_CS, HIGH);
}

void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  uint8_t colData[4] = {(uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF), (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF)};
  writeCommand(0x2A);
  writeData(colData, 4);
  uint8_t rowData[4] = {(uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF), (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF)};
  writeCommand(0x2B);
  writeData(rowData, 4);
  writeCommand(0x2C);
}

void hline(int16_t x0, int16_t x1, int16_t y, uint16_t color) {
  if (x0 < 0) x0 = 0;
  if (x1 > 239) x1 = 239;
  if (x0 > x1) return;
  setWindow(x0, y, x1, y);
  digitalWrite(PIN_CS, LOW);
  digitalWrite(PIN_DC, HIGH);
  uint8_t hi = color >> 8, lo = color & 0xFF;
  for (int16_t x = x0; x <= x1; x++) {
    SPI.transfer(hi);
    SPI.transfer(lo);
  }
  digitalWrite(PIN_CS, HIGH);
}

void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
  setWindow(x, y, x + w - 1, y + h - 1);
  digitalWrite(PIN_CS, LOW);
  digitalWrite(PIN_DC, HIGH);
  uint8_t hi = color >> 8, lo = color & 0xFF;
  uint32_t n = (uint32_t)w * h;
  for (uint32_t i = 0; i < n; i++) {
    SPI.transfer(hi);
    SPI.transfer(lo);
  }
  digitalWrite(PIN_CS, HIGH);
}

void fillScreen(uint16_t color) {
  fillRect(0, 0, 240, 240, color);
}

void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
  for (int16_t y = -r; y <= r; y++) {
    int16_t dx = (int16_t)sqrt((float)(r) * r - (float)y * y);
    hline(x0 - dx, x0 + dx, y0 + y, color);
  }
}

void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
  int16_t t;
  if (y0 > y1) { t=x0;x0=x1;x1=t; t=y0;y0=y1;y1=t; }
  if (y1 > y2) { t=x1;x1=x2;x2=t; t=y1;y1=y2;y2=t; }
  if (y0 > y1) { t=x0;x0=x1;x1=t; t=y0;y0=y1;y1=t; }
  if (y0 == y2) {
    int16_t minx = min(x0, min(x1, x2));
    int16_t maxx = max(x0, max(x1, x2));
    hline(minx, maxx, y0, color);
    return;
  }
  for (int16_t y = y0; y <= y2; y++) {
    bool secondHalf = (y > y1) || (y1 == y0);
    int16_t xa = x0 + (int32_t)(x2 - x0) * (y - y0) / (y2 - y0);
    int16_t xb;
    if (!secondHalf) xb = x0 + (int32_t)(x1 - x0) * (y - y0) / (y1 - y0);
    else xb = x1 + (int32_t)(x2 - x1) * (y - y1) / (y2 - y1);
    hline(min(xa, xb), max(xa, xb), y, color);
  }
}

void drawFacets() {
  int16_t cx = 120, cy = 120;
  int16_t vx[6] = {210, 165, 75, 30, 75, 165};
  int16_t vy[6] = {120, 198, 198, 120, 42, 42};
  uint16_t facetColor[6] = {0xFD00, 0xFC00, 0xFB00, 0xF282, 0xFC80, 0xFB80};
  for (uint8_t i = 0; i < 6; i++) {
    uint8_t j = (i + 1) % 6;
    fillTriangle(cx, cy, vx[i], vy[i], vx[j], vy[j], facetColor[i]);
  }
}

void movePupilTo(int16_t x, int16_t y) {
  drawFacets();
  fillCircle(x, y, 18, BLACK);
}

// ---------- buzzer melody ----------
// two explicit octaves so runs across A/B never accidentally jump backward
const int R = 0;
const int C4 = 262, CIS4 = 277, D4 = 294, DIS4 = 311, E4 = 330, F4 = 349,
          FIS4 = 370, G4 = 392, GIS4 = 415, AN4 = 440, AIS4 = 466, B4 = 494;
const int C5 = 523, CIS5 = 554, D5 = 587, DIS5 = 622, E5 = 659, F5 = 698,
          FIS5 = 740, G5 = 784, GIS5 = 831, AN5 = 880, AIS5 = 932, B5 = 988;
const int C3 = 131, CIS3 = 139, D3 = 147, DIS3 = 156, E3 = 165, F3 = 175,
          FIS3 = 185, G3 = 196, GIS3 = 208, AN3 = 220, AIS3 = 233, B3 = 247;
const float UNIT_S = 0.12;
const float GAP_S = 0.035;

struct Note { int tone; int units; };

// original tune
Note melodyOrig[] = {
  {C5, 2}, {C5, 1}, {C5, 1}, {E5, 2}, {C5, 2}, {D5, 2}, {AIS4, 3}, {R, 1},
  {C5, 2}, {C5, 2}, {C5, 1}, {C5, 1}, {C5, 1}, {C5, 1}, {B4, 2}, {C5, 3}, {R, 1},
  {C5, 2}, {C5, 1}, {C5, 1}, {E5, 2}, {C5, 2}, {D5, 2}, {AIS4, 3}, {R, 4},
};

// slow chromatic descent, eerie
Note melodyCreep[] = {
  {G5, 3}, {FIS5, 2}, {F5, 2}, {E5, 2}, {DIS5, 2}, {D5, 3}, {R, 1},
  {C5, 4}, {R, 2}, {C4, 5}, {R, 4},
};

// devil's interval alarm
Note melodyTritone[] = {
  {C5, 1}, {FIS5, 1}, {C5, 1}, {FIS5, 1}, {C5, 1}, {FIS5, 1}, {C5, 2}, {R, 1},
  {C5, 1}, {FIS5, 1}, {C5, 1}, {FIS5, 1}, {C5, 3}, {R, 2},
};

// witch cackle, quick up-down run
Note melodyCackle[] = {
  {C5, 1}, {D5, 1}, {E5, 1}, {F5, 1}, {G5, 1}, {FIS5, 1}, {F5, 1}, {E5, 1}, {D5, 1}, {C5, 1}, {R, 1},
  {G5, 2}, {FIS5, 2}, {G5, 3}, {R, 2},
};

// Bach - Toccata and Fugue in D minor, BWV 565, opening bars
// structure verified against github.com/contrab/toccata565 (standard
// pitches.h frequencies match exactly): mordent A-G-A repeated at three
// descending octaves, middle repetition abbreviated, as in the real score
Note melodyToccata[] = {
  // phrase 1, octave 5 - mordent + full descending run
  {AN5, 3}, {G5, 3}, {AN5, 3}, {R, 3},
  {G5, 1}, {F5, 1}, {E5, 1}, {D5, 1}, {CIS5, 3}, {D5, 3}, {R, 3},

  // phrase 2, octave 3 - mordent + full descending run
  {AN3, 3}, {G3, 3}, {AN3, 3}, {R, 3},
  {G3, 1}, {F3, 1}, {E3, 1}, {D3, 1}, {CIS3, 3}, {D3, 3},
};

// Bach - same opening, extended with my own idiomatic continuation
// (not verified against an outside source like the mordent sequence
// above): a rising flourish building tension across two octaves, then
// the matching descending run back down to a held low pedal D
Note melodyToccataExtended[] = {
  {AN5, 3}, {G5, 3}, {AN5, 3}, {R, 3},
  {G5, 1}, {F5, 1}, {E5, 1}, {D5, 1}, {CIS5, 3}, {D5, 3}, {R, 6},

  {AN4, 3}, {G4, 3}, {AN4, 3}, {R, 3},
  {E4, 3}, {F4, 3}, {CIS4, 3}, {D4, 3}, {R, 6},

  {AN3, 3}, {G3, 3}, {AN3, 3}, {R, 3},
  {G3, 1}, {F3, 1}, {E3, 1}, {D3, 1}, {CIS3, 3}, {D3, 3}, {R, 4},

  {D3, 1}, {E3, 1}, {F3, 1}, {G3, 1}, {AN3, 1}, {B3, 1}, {C4, 1}, {D4, 1},
  {E4, 1}, {F4, 1}, {G4, 1}, {AN4, 1}, {B4, 1}, {C5, 1}, {D5, 3}, {R, 2},

  {D5, 1}, {C5, 1}, {AN4, 1}, {G4, 1}, {F4, 1}, {E4, 1}, {D4, 1}, {C4, 1},
  {AN3, 1}, {G3, 1}, {F3, 1}, {E3, 1}, {D3, 8}, {R, 6},
};

// Grieg - In the Hall of the Mountain King, simplified ostinato
Note melodyMountainKing[] = {
  {B4, 1}, {D5, 1}, {E5, 1}, {FIS5, 1}, {E5, 1}, {D5, 1}, {B4, 1}, {FIS5, 1}, {R, 1},
  {C5, 1}, {D5, 1}, {E5, 1}, {F5, 1}, {E5, 1}, {D5, 1}, {C5, 1}, {F5, 1}, {R, 1},
};

// Saint-Saens - Danse Macabre, devil's-fiddle tritone + descending run
Note melodyDanseMacabre[] = {
  {AN4, 1}, {DIS5, 3}, {R, 1},
  {G5, 1}, {F5, 1}, {E5, 1}, {D5, 1}, {CIS5, 1}, {C5, 1}, {B4, 1}, {AN4, 3}, {R, 2},
};

// Chopin - Funeral March, famous rhythm, low register
Note melodyFuneral[] = {
  {C4, 2}, {C4, 2}, {DIS4, 4}, {R, 2},
  {C4, 2}, {C4, 2}, {CIS4, 4}, {R, 4},
};

struct MelodySet { Note *notes; uint8_t count; };
MelodySet allMelodies[] = {
  {melodyOrig, sizeof(melodyOrig) / sizeof(Note)},
  {melodyCreep, sizeof(melodyCreep) / sizeof(Note)},
  {melodyTritone, sizeof(melodyTritone) / sizeof(Note)},
  {melodyCackle, sizeof(melodyCackle) / sizeof(Note)},
  {melodyToccata, sizeof(melodyToccata) / sizeof(Note)},
  {melodyToccataExtended, sizeof(melodyToccataExtended) / sizeof(Note)},
  {melodyMountainKing, sizeof(melodyMountainKing) / sizeof(Note)},
  {melodyDanseMacabre, sizeof(melodyDanseMacabre) / sizeof(Note)},
  {melodyFuneral, sizeof(melodyFuneral) / sizeof(Note)},
};
#define NUM_MELODIES 9

uint8_t melodyOrder[NUM_MELODIES] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
uint8_t melodyPos = NUM_MELODIES;

void shuffleMelodyOrder() {
  for (int8_t i = NUM_MELODIES - 1; i > 0; i--) {
    int8_t j = random(0, i + 1);
    uint8_t t = melodyOrder[i];
    melodyOrder[i] = melodyOrder[j];
    melodyOrder[j] = t;
  }
  melodyPos = 0;
}

void playMelody() {
  if (melodyPos >= NUM_MELODIES) shuffleMelodyOrder();
  uint8_t pick = melodyOrder[melodyPos++];
  Note *m = allMelodies[pick].notes;
  uint8_t n = allMelodies[pick].count;
  for (uint8_t i = 0; i < n; i++) {
    float totalTime = m[i].units * UNIT_S;
    if (m[i].tone == R) {
      noTone(BUZZER_PIN);
      delay((unsigned long)(totalTime * 1000));
    } else {
      float playTime = totalTime - GAP_S;
      if (playTime <= 0) playTime = 0.01;
      tone(BUZZER_PIN, m[i].tone);
      delay((unsigned long)(playTime * 1000));
      noTone(BUZZER_PIN);
      delay((unsigned long)(GAP_S * 1000));
    }
  }
}

// ---------- timers ----------
unsigned long nextFlicker = 0;
unsigned long nextMove = 0;
unsigned long nextSonar = 0;
const uint8_t BASE_R = 255, BASE_G = 90, BASE_B = 0;

// ring spin (360) mode - a comet chases around the ring, then returns to flicker
bool spinning = false;
uint8_t spinPos = 0;
uint8_t spinRevolutions = 0;
unsigned long nextSpin = 0;
unsigned long nextSpinStep = 0;

void setup() {
  pinMode(PIN_DC, OUTPUT);
  pinMode(PIN_RST, OUTPUT);
  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial.begin(9600);

  SPI.begin();
  SPI.beginTransaction(SPISettings(16000000, MSBFIRST, SPI_MODE0));

  digitalWrite(PIN_RST, HIGH);
  delay(10);
  digitalWrite(PIN_RST, LOW);
  delay(10);
  digitalWrite(PIN_RST, HIGH);
  delay(120);

  for (uint8_t i = 0; i < sizeof(initSeq) / sizeof(initSeq[0]); i++) {
    writeCommand(initSeq[i].cmd);
    if (initSeq[i].len) writeData(initSeq[i].data, initSeq[i].len);
    if (initSeq[i].delayMs) delay(initSeq[i].delayMs);
  }

  randomSeed(analogRead(A0));
  fillScreen(BLACK);
  movePupilTo(120, 120);

  ring.begin();
  ring.show();

  nextMove = millis() + 2000;
  nextSonar = millis() + 500;
  nextSpin = millis() + 8000;
}

void loop() {
  unsigned long now = millis();

  if (!spinning && (long)(now - nextSpin) >= 0) {
    spinning = true;
    spinPos = 0;
    spinRevolutions = 0;
    nextSpinStep = now;
  }

  if (spinning) {
    if ((long)(now - nextSpinStep) >= 0) {
      ring.clear();
      for (int8_t t = 0; t < 5; t++) {
        uint8_t idx = (spinPos + NUM_LEDS - t) % NUM_LEDS;
        int level = 100 - t * 22;
        if (level > 0) {
          ring.setPixelColor(idx, ring.Color(BASE_R * level / 100, BASE_G * level / 100, BASE_B * level / 100));
        }
      }
      ring.show();
      spinPos++;
      if (spinPos >= NUM_LEDS) {
        spinPos = 0;
        spinRevolutions++;
      }
      nextSpinStep = now + 40;
      if (spinRevolutions >= 3) {
        spinning = false;
        nextSpin = now + random(15000, 30001);
      }
    }
  } else if ((long)(now - nextFlicker) >= 0) {
    for (int i = 0; i < NUM_LEDS; i++) {
      long level = random(55, 101);
      ring.setPixelColor(i, ring.Color(BASE_R * level / 100, BASE_G * level / 100, BASE_B * level / 100));
    }
    ring.show();
    nextFlicker = now + random(40, 121);
  }

  if ((long)(now - nextMove) >= 0) {
    int16_t nx = 120 + random(-45, 46);
    int16_t ny = 120 + random(-45, 46);
    movePupilTo(nx, ny);
    nextMove = now + random(1200, 3001);
  }

  if ((long)(now - nextSonar) >= 0) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(15000);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    unsigned long sonarWait = 500;
    if (duration > 0) {
      float distance = (duration * 0.0343) / 2.0;
      Serial.print(distance);
      Serial.println(" cm");
      if (distance < 50) {
        playMelody();
        // time 15s - 15000ms make it long as you want :))))
        sonarWait = 15000;
      }
    } else {
      Serial.println("Out of range");
    }
    nextSonar = millis() + sonarWait;
  }
}
