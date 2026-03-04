/**
 * startup_animation.h
 * 
 * Epic startup sequence for ESP32-S3 gauges
 * A dramatic boot animation inspired by high-end automotive displays
 */

#ifndef STARTUP_ANIMATION_H
#define STARTUP_ANIMATION_H

#include <TFT_eSPI.h>

// Animation timing (ms)
#define ANIM_PHASE1_DURATION  800   // Particle vortex
#define ANIM_PHASE2_DURATION  600   // Ring expansion
#define ANIM_PHASE3_DURATION  400   // Flash
#define ANIM_PHASE4_DURATION  1200  // Needle sweep with bounce
#define ANIM_PHASE5_DURATION  500   // Digital scramble
#define ANIM_PHASE6_DURATION  300   // Final pulse

// Colors for animation
#define ANIM_COL_PRIMARY    0x07FF  // Cyan
#define ANIM_COL_SECONDARY  0xF81F  // Magenta
#define ANIM_COL_ACCENT     0xFFE0  // Yellow
#define ANIM_COL_WHITE      0xFFFF
#define ANIM_COL_DIM        0x4208  // Dark gray

class StartupAnimation {
public:
  StartupAnimation(TFT_eSPI& display, TFT_eSprite& sprite, int width, int height)
    : _tft(display), _spr(sprite), _w(width), _h(height), _cx(width/2), _cy(height/2) {}

  // Run the full startup sequence
  void play() {
    phase1_particleVortex();
    phase2_ringExpansion();
    phase3_flash();
    phase4_needleSweep();
    phase5_digitalScramble();
    phase6_finalPulse();
  }

private:
  TFT_eSPI& _tft;
  TFT_eSprite& _spr;
  int _w, _h, _cx, _cy;

  // ========================================================================
  // PHASE 1: Particle Vortex - particles spiral inward from edges
  // ========================================================================
  void phase1_particleVortex() {
    const int NUM_PARTICLES = 60;
    float px[NUM_PARTICLES], py[NUM_PARTICLES];
    float angle[NUM_PARTICLES], radius[NUM_PARTICLES];
    float speed[NUM_PARTICLES];
    uint16_t colors[NUM_PARTICLES];
    
    // Initialize particles at random positions around the edge
    for (int i = 0; i < NUM_PARTICLES; i++) {
      angle[i] = random(360) * DEG_TO_RAD;
      radius[i] = 130 + random(40);
      speed[i] = 2.0f + (random(100) / 50.0f);
      
      // Color gradient from cyan to magenta
      int colorPhase = random(3);
      if (colorPhase == 0) colors[i] = ANIM_COL_PRIMARY;
      else if (colorPhase == 1) colors[i] = ANIM_COL_SECONDARY;
      else colors[i] = ANIM_COL_ACCENT;
    }
    
    unsigned long startTime = millis();
    while (millis() - startTime < ANIM_PHASE1_DURATION) {
      float progress = (millis() - startTime) / (float)ANIM_PHASE1_DURATION;
      
      _spr.fillSprite(TFT_BLACK);
      
      // Draw particles spiraling inward
      for (int i = 0; i < NUM_PARTICLES; i++) {
        // Spiral inward
        radius[i] -= speed[i] * (1.0f + progress * 2.0f);
        angle[i] += 0.08f + (progress * 0.15f);
        
        if (radius[i] < 5) {
          radius[i] = 130 + random(20);
          angle[i] = random(360) * DEG_TO_RAD;
        }
        
        px[i] = _cx + cos(angle[i]) * radius[i];
        py[i] = _cy + sin(angle[i]) * radius[i];
        
        // Particle size based on distance from center
        int size = map(radius[i], 0, 150, 4, 1);
        
        // Draw particle with glow effect
        _spr.fillCircle(px[i], py[i], size + 1, _spr.alphaBlend(80, colors[i], TFT_BLACK));
        _spr.fillCircle(px[i], py[i], size, colors[i]);
      }
      
      // Central glow growing
      int glowSize = progress * 30;
      for (int r = glowSize; r > 0; r -= 2) {
        uint8_t alpha = map(r, 0, glowSize, 200, 20);
        _spr.drawCircle(_cx, _cy, r, _spr.alphaBlend(alpha, ANIM_COL_PRIMARY, TFT_BLACK));
      }
      
      _spr.pushSprite(0, 0);
      delay(16);
    }
  }

  // ========================================================================
  // PHASE 2: Ring Expansion - concentric rings explode outward
  // ========================================================================
  void phase2_ringExpansion() {
    unsigned long startTime = millis();
    
    while (millis() - startTime < ANIM_PHASE2_DURATION) {
      float progress = (millis() - startTime) / (float)ANIM_PHASE2_DURATION;
      float eased = easeOutBack(progress);
      
      _spr.fillSprite(TFT_BLACK);
      
      // Multiple expanding rings with different speeds
      for (int ring = 0; ring < 5; ring++) {
        float ringProgress = constrain(progress - (ring * 0.1f), 0.0f, 1.0f);
        float ringEased = easeOutCubic(ringProgress);
        int radius = ringEased * 140;
        
        if (radius > 0) {
          // Ring color shifts from center outward
          uint16_t ringColor;
          if (ring % 3 == 0) ringColor = ANIM_COL_PRIMARY;
          else if (ring % 3 == 1) ringColor = ANIM_COL_SECONDARY;
          else ringColor = ANIM_COL_ACCENT;
          
          // Fade out as it expands
          uint8_t alpha = map(radius, 0, 140, 255, 50);
          
          // Draw thick ring
          for (int t = 0; t < 3; t++) {
            _spr.drawCircle(_cx, _cy, radius + t, _spr.alphaBlend(alpha, ringColor, TFT_BLACK));
          }
        }
      }
      
      // Central bright core
      int coreSize = 20 - (progress * 15);
      if (coreSize > 0) {
        _spr.fillCircle(_cx, _cy, coreSize, ANIM_COL_WHITE);
        _spr.fillCircle(_cx, _cy, coreSize - 3, ANIM_COL_PRIMARY);
      }
      
      // Radial lines shooting outward
      int numLines = 12;
      for (int i = 0; i < numLines; i++) {
        float lineAngle = (i * 360.0f / numLines) * DEG_TO_RAD;
        int innerR = eased * 30;
        int outerR = eased * 120;
        
        int x1 = _cx + cos(lineAngle) * innerR;
        int y1 = _cy + sin(lineAngle) * innerR;
        int x2 = _cx + cos(lineAngle) * outerR;
        int y2 = _cy + sin(lineAngle) * outerR;
        
        _spr.drawLine(x1, y1, x2, y2, _spr.alphaBlend(150, ANIM_COL_ACCENT, TFT_BLACK));
      }
      
      _spr.pushSprite(0, 0);
      delay(16);
    }
  }

  // ========================================================================
  // PHASE 3: Flash - bright flash transition
  // ========================================================================
  void phase3_flash() {
    // Quick white flash
    for (int brightness = 0; brightness < 255; brightness += 30) {
      _spr.fillSprite(_spr.alphaBlend(brightness, ANIM_COL_WHITE, TFT_BLACK));
      _spr.pushSprite(0, 0);
      delay(10);
    }
    
    // Hold
    _spr.fillSprite(ANIM_COL_WHITE);
    _spr.pushSprite(0, 0);
    delay(80);
    
    // Fade to black
    for (int brightness = 255; brightness > 0; brightness -= 25) {
      _spr.fillSprite(_spr.alphaBlend(brightness, ANIM_COL_WHITE, TFT_BLACK));
      _spr.pushSprite(0, 0);
      delay(15);
    }
    
    _spr.fillSprite(TFT_BLACK);
    _spr.pushSprite(0, 0);
  }

  // ========================================================================
  // PHASE 4: Needle Sweep - gauge appears, needle sweeps with physics
  // ========================================================================
  void phase4_needleSweep() {
    const float ANGLE_MIN = -145.0f;
    const float ANGLE_MAX = -35.0f;
    const int R_TICK1 = 115;
    const int R_TICK2 = 105;
    const int R_NEEDLE = 90;
    
    unsigned long startTime = millis();
    
    // Physics simulation for needle bounce
    float needleAngle = ANGLE_MIN;
    float needleVelocity = 0;
    float targetAngle = ANGLE_MIN;
    
    // Animation stages
    // 0-30%: sweep to max with overshoot
    // 30-60%: bounce back and settle at max
    // 60-100%: sweep back to min with settle
    
    while (millis() - startTime < ANIM_PHASE4_DURATION) {
      float progress = (millis() - startTime) / (float)ANIM_PHASE4_DURATION;
      
      // Determine target based on progress
      if (progress < 0.4f) {
        // Sweep to max (with overshoot built into easing)
        float subProgress = progress / 0.4f;
        float eased = easeOutBack(subProgress);
        targetAngle = ANGLE_MIN + eased * (ANGLE_MAX - ANGLE_MIN + 15); // Overshoot by 15 degrees
      } else if (progress < 0.6f) {
        // Settle at max
        float subProgress = (progress - 0.4f) / 0.2f;
        float startAngle = ANGLE_MAX + 15;
        targetAngle = startAngle + easeOutElastic(subProgress) * (ANGLE_MAX - startAngle);
      } else {
        // Sweep back to min
        float subProgress = (progress - 0.6f) / 0.4f;
        float eased = easeOutCubic(subProgress);
        targetAngle = ANGLE_MAX - eased * (ANGLE_MAX - ANGLE_MIN);
      }
      
      // Spring physics for smooth following
      float spring = 0.3f;
      float damping = 0.7f;
      float force = (targetAngle - needleAngle) * spring;
      needleVelocity = (needleVelocity + force) * damping;
      needleAngle += needleVelocity;
      
      _spr.fillSprite(TFT_BLACK);
      
      // Draw tick marks with fade-in effect
      float tickAlpha = constrain(progress * 3.0f, 0.0f, 1.0f);
      drawTickMarks(_spr, tickAlpha);
      
      // Draw needle with glow
      drawAnimatedNeedle(_spr, needleAngle, R_NEEDLE);
      
      // Center cap
      _spr.fillCircle(_cx, _cy, 20, TFT_DARKGREY);
      _spr.fillCircle(_cx, _cy, 18, TFT_BLACK);
      
      // Add motion blur trail when moving fast
      if (abs(needleVelocity) > 1.5f) {
        for (int trail = 1; trail <= 3; trail++) {
          float trailAngle = needleAngle - (needleVelocity * trail * 0.3f);
          uint8_t trailAlpha = 100 - (trail * 30);
          drawNeedleTrail(_spr, trailAngle, R_NEEDLE, trailAlpha);
        }
      }
      
      _spr.pushSprite(0, 0);
      delay(16);
    }
  }

  // ========================================================================
  // PHASE 5: Digital Scramble - numbers randomize then settle
  // ========================================================================
  void phase5_digitalScramble() {
    unsigned long startTime = millis();
    
    const float ANGLE_MIN = -145.0f;
    const int R_NEEDLE = 90;
    
    while (millis() - startTime < ANIM_PHASE5_DURATION) {
      float progress = (millis() - startTime) / (float)ANIM_PHASE5_DURATION;
      
      _spr.fillSprite(TFT_BLACK);
      
      // Draw static gauge elements
      drawTickMarks(_spr, 1.0f);
      drawAnimatedNeedle(_spr, ANGLE_MIN, R_NEEDLE);
      
      // Center cap
      _spr.fillCircle(_cx, _cy, 20, TFT_DARKGREY);
      _spr.fillCircle(_cx, _cy, 18, TFT_BLACK);
      
      // Digital readout with scramble effect
      _spr.setTextDatum(MC_DATUM);
      _spr.setTextSize(3);
      
      char buf[8];
      if (progress < 0.7f) {
        // Scrambling random numbers
        int scrambleSpeed = map(progress * 100, 0, 70, 5, 50);
        if (random(100) < scrambleSpeed) {
          snprintf(buf, sizeof(buf), "%d", random(-99, 999));
        } else {
          snprintf(buf, sizeof(buf), "%d", random(-99, 999));
        }
        
        // Glitch color effect
        uint16_t textColor = (random(3) == 0) ? ANIM_COL_SECONDARY : ANIM_COL_PRIMARY;
        _spr.setTextColor(textColor, TFT_BLACK);
      } else {
        // Settling to 0
        float settleProgress = (progress - 0.7f) / 0.3f;
        int displayVal = (int)((1.0f - easeOutCubic(settleProgress)) * random(-20, 50));
        if (settleProgress > 0.8f) displayVal = 0;
        snprintf(buf, sizeof(buf), "%d", displayVal);
        _spr.setTextColor(ANIM_COL_PRIMARY, TFT_BLACK);
      }
      
      _spr.drawString(buf, _cx, _cy + 65);
      
      // Glitch lines during scramble
      if (progress < 0.6f && random(10) < 3) {
        int glitchY = random(_h);
        int glitchH = random(2, 8);
        _spr.fillRect(0, glitchY, _w, glitchH, _spr.alphaBlend(100, ANIM_COL_SECONDARY, TFT_BLACK));
      }
      
      _spr.pushSprite(0, 0);
      delay(30);
    }
  }

  // ========================================================================
  // PHASE 6: Final Pulse - subtle pulse to indicate ready
  // ========================================================================
  void phase6_finalPulse() {
    const float ANGLE_MIN = -145.0f;
    const int R_NEEDLE = 90;
    
    // Pulse outward
    for (int pulse = 0; pulse < 3; pulse++) {
      for (int frame = 0; frame < 10; frame++) {
        float pulseProgress = frame / 10.0f;
        
        _spr.fillSprite(TFT_BLACK);
        drawTickMarks(_spr, 1.0f);
        drawAnimatedNeedle(_spr, ANGLE_MIN, R_NEEDLE);
        
        _spr.fillCircle(_cx, _cy, 20, TFT_DARKGREY);
        _spr.fillCircle(_cx, _cy, 18, TFT_BLACK);
        
        // Digital readout
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(ANIM_COL_PRIMARY, TFT_BLACK);
        _spr.setTextSize(3);
        _spr.drawString("0", _cx, _cy + 65);
        
        // Pulse ring
        int pulseRadius = 20 + (pulseProgress * 100);
        uint8_t pulseAlpha = 150 - (pulseProgress * 150);
        if (pulseAlpha > 10) {
          _spr.drawCircle(_cx, _cy, pulseRadius, _spr.alphaBlend(pulseAlpha, ANIM_COL_PRIMARY, TFT_BLACK));
          _spr.drawCircle(_cx, _cy, pulseRadius + 1, _spr.alphaBlend(pulseAlpha / 2, ANIM_COL_PRIMARY, TFT_BLACK));
        }
        
        _spr.pushSprite(0, 0);
        delay(25);
      }
    }
  }

  // ========================================================================
  // HELPER: Draw tick marks
  // ========================================================================
  void drawTickMarks(TFT_eSprite& spr, float alpha) {
    const float ANGLE_MIN = -145.0f;
    const float ANGLE_MAX = -35.0f;
    const int R_TICK1 = 115;
    const int R_TICK2 = 105;
    const int totalTicks = 7;
    
    uint16_t tickColor = spr.alphaBlend(alpha * 255, ANIM_COL_WHITE, TFT_BLACK);
    
    for (int i = 0; i < totalTicks; i++) {
      float t = (float)i / (float)(totalTicks - 1);
      float ang = ANGLE_MIN + t * (ANGLE_MAX - ANGLE_MIN);
      float rad = ang * DEG_TO_RAD;
      
      int x1 = _cx + cos(rad) * R_TICK1;
      int y1 = _cy + sin(rad) * R_TICK1;
      int x2 = _cx + cos(rad) * R_TICK2;
      int y2 = _cy + sin(rad) * R_TICK2;
      
      bool isMajor = (i == 0) || (i == totalTicks - 1) || (i == totalTicks / 2);
      
      if (isMajor) {
        int x3 = _cx + cos(rad) * (R_TICK2 - 8);
        int y3 = _cy + sin(rad) * (R_TICK2 - 8);
        
        // Thick major tick
        float nx = -sin(rad);
        float ny = cos(rad);
        for (int tt = -3; tt <= 3; tt++) {
          spr.drawLine(x1 + nx*tt, y1 + ny*tt, x3 + nx*tt, y3 + ny*tt, tickColor);
        }
      } else {
        spr.drawLine(x1, y1, x2, y2, tickColor);
      }
    }
  }

  // ========================================================================
  // HELPER: Draw animated needle with glow
  // ========================================================================
  void drawAnimatedNeedle(TFT_eSprite& spr, float angleDeg, int radius) {
    float rad = angleDeg * DEG_TO_RAD;
    
    int xTip = _cx + cos(rad) * radius;
    int yTip = _cy + sin(rad) * radius;
    
    float nx = -sin(rad);
    float ny = cos(rad);
    
    const float baseW = 6.0f;
    const float tipW = 2.0f;
    
    int xBL = _cx + nx * (baseW/2);
    int yBL = _cy + ny * (baseW/2);
    int xBR = _cx - nx * (baseW/2);
    int yBR = _cy - ny * (baseW/2);
    int xTL = xTip + nx * (tipW/2);
    int yTL = yTip + ny * (tipW/2);
    int xTR = xTip - nx * (tipW/2);
    int yTR = yTip - ny * (tipW/2);
    
    // Glow effect
    for (int g = 3; g >= 0; g--) {
      uint8_t glowAlpha = 50 - (g * 15);
      uint16_t glowColor = spr.alphaBlend(glowAlpha, ANIM_COL_PRIMARY, TFT_BLACK);
      
      int gxBL = _cx + nx * (baseW/2 + g);
      int gyBL = _cy + ny * (baseW/2 + g);
      int gxBR = _cx - nx * (baseW/2 + g);
      int gyBR = _cy - ny * (baseW/2 + g);
      int gxTL = xTip + nx * (tipW/2 + g);
      int gyTL = yTip + ny * (tipW/2 + g);
      int gxTR = xTip - nx * (tipW/2 + g);
      int gyTR = yTip - ny * (tipW/2 + g);
      
      spr.fillTriangle(gxBL, gyBL, gxBR, gyBR, gxTL, gyTL, glowColor);
      spr.fillTriangle(gxBR, gyBR, gxTR, gyTR, gxTL, gyTL, glowColor);
    }
    
    // Main needle
    spr.fillTriangle(xBL, yBL, xBR, yBR, xTL, yTL, ANIM_COL_WHITE);
    spr.fillTriangle(xBR, yBR, xTR, yTR, xTL, yTL, ANIM_COL_WHITE);
  }

  // ========================================================================
  // HELPER: Draw needle motion trail
  // ========================================================================
  void drawNeedleTrail(TFT_eSprite& spr, float angleDeg, int radius, uint8_t alpha) {
    float rad = angleDeg * DEG_TO_RAD;
    
    int xTip = _cx + cos(rad) * radius;
    int yTip = _cy + sin(rad) * radius;
    
    uint16_t trailColor = spr.alphaBlend(alpha, ANIM_COL_PRIMARY, TFT_BLACK);
    spr.drawLine(_cx, _cy, xTip, yTip, trailColor);
  }

  // ========================================================================
  // EASING FUNCTIONS
  // ========================================================================
  float easeOutCubic(float t) {
    return 1.0f - pow(1.0f - t, 3.0f);
  }
  
  float easeOutBack(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return 1.0f + c3 * pow(t - 1.0f, 3.0f) + c1 * pow(t - 1.0f, 2.0f);
  }
  
  float easeOutElastic(float t) {
    if (t == 0 || t == 1) return t;
    const float c4 = (2.0f * PI) / 3.0f;
    return pow(2.0f, -10.0f * t) * sin((t * 10.0f - 0.75f) * c4) + 1.0f;
  }
};

#endif // STARTUP_ANIMATION_H
