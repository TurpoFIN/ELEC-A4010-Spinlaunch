const int hallPin = A0;

const unsigned long serialBaud = 9600;

const uint8_t pulsesPerRev = 1; 

const int thresholdHighAdc = 25; 
const int thresholdLowAdc  = 12; 

const unsigned long minBlankingMicros = 250;

const float rpmAlpha = 0.2f;                
const unsigned long printEveryMs = 100;     
const bool verboseSerial = false;           
const bool logRpmWhileLaunching = false;    
const unsigned long rpmTimeoutMicros = 500000UL; 

const float baselineAlpha = 0.001f;         

int baselineAdc = 0;
bool inPulse = false;

unsigned long lastEdgeMicros = 0;
unsigned long lastPeriodMicros = 0;
float rpmFiltered = 0.0f;

unsigned long lastPrintMs = 0;
unsigned long t2 = micros(); 

int launchPin = 10;
int magnetPin = 11;

const unsigned long stabilityDurationMicros = 2000000UL; 
const float stabilityMargin = 0.01f;                      
const bool dryRunLaunch = false;                           
const unsigned long launchFalsePrintEveryMs = 200;        

const bool launchActiveHigh = true;

const float armLengthCm = 23.0f;

const float payloadPhaseOffsetRev = 0.5f;

const float launchArcOffsetCm = 3.0f;

const unsigned long releaseMechLatencyMicros = 0;

enum Mode : uint8_t { WAIT_FOR_LAUNCH = 0, MEASURE_FOR_STABILITY = 1, ARMED_DELAY = 2, FIRED_WAIT_RELEASE = 3 };
Mode mode = WAIT_FOR_LAUNCH;

unsigned long stabilityStartMicros = 0;
const uint8_t stabilityWindowN = 12; 
unsigned long periodWindow[stabilityWindowN];
uint8_t periodIdx = 0;
uint8_t periodCount = 0;
unsigned long stabilityMinPeriod = 0;
unsigned long stabilityMaxPeriod = 0;
unsigned long stabilityAvgPeriod = 0;
bool haveStabilityData = false;
unsigned long lastLaunchPrintMs = 0;
unsigned long scheduledLaunchMicros = 0;
unsigned long stableRpmForLaunch = 0;

bool launchAttemptActive = false;
bool launchAttemptSucceeded = false;

static inline unsigned long ulabsdiff(unsigned long a, unsigned long b) { return (a > b) ? (a - b) : (b - a); }

void setup() {
  Serial.begin(serialBaud);
  Serial.println("=== Magnet Detector INIT ===");
  pinMode(hallPin, INPUT);

  if (launchActiveHigh) {
    pinMode(launchPin, INPUT);
  } else {
    pinMode(launchPin, INPUT_PULLUP);
  }

  pinMode(magnetPin, OUTPUT);

  digitalWrite(magnetPin, HIGH);
  
  delay(1000);
  long sum = 0;
  for (int i = 0; i < 200; i++) {
    sum += analogRead(hallPin);
  }
  baselineAdc = (int)(sum / 200);
  
  Serial.println("=== Magnet Detector Ready ===");
  Serial.print("Baseline ADC: ");
  Serial.println(baselineAdc);
  Serial.println("Waiting for magnet...\n");
  lastLaunchPrintMs = millis();
}

void loop() {
  unsigned long now = micros();

  if (now - t2 > 5000000UL) {
    digitalWrite(magnetPin, HIGH);
  }

  bool launchActive = launchActiveHigh ? (digitalRead(launchPin) == HIGH) : (digitalRead(launchPin) == LOW);

  if (mode == WAIT_FOR_LAUNCH) {
    if (launchActive) {
      mode = MEASURE_FOR_STABILITY;
      stabilityStartMicros = 0;
      periodIdx = 0;
      periodCount = 0;
      stabilityMinPeriod = 0;
      stabilityMaxPeriod = 0;
      stabilityAvgPeriod = 0;
      haveStabilityData = false;
      rpmFiltered = 0.0f;
      lastEdgeMicros = 0;
      lastPeriodMicros = 0;
      inPulse = false;
      lastPrintMs = millis();

      launchAttemptActive = true;
      launchAttemptSucceeded = false;
    }

    if (!launchActive) {
      unsigned long nowMs = millis();
      if (nowMs - lastLaunchPrintMs >= launchFalsePrintEveryMs) {
        lastLaunchPrintMs = nowMs;
        //Serial.println("FALSE");
      }
    }
    return; 
  }

  if (mode == ARMED_DELAY) {
    if (!launchActive) {
      if (launchAttemptActive && !launchAttemptSucceeded) {
        Serial.println("Launch failed");
      }
      mode = WAIT_FOR_LAUNCH;
      lastLaunchPrintMs = millis();
      //Serial.println("FALSE");
      return;
    }

    if ((long)(now - scheduledLaunchMicros) >= 0) {
      if (dryRunLaunch) {
        Serial.print("WOULD_LAUNCH us=");
        Serial.print(now);
        Serial.print(" rpm=");
        Serial.println(stableRpmForLaunch);
      } else {
        digitalWrite(magnetPin, LOW);
      }

      t2 = now;
      mode = FIRED_WAIT_RELEASE;
      launchAttemptSucceeded = true;
    }
    return;
  }

  if (mode == FIRED_WAIT_RELEASE) {
    if (!launchActive) {
      mode = WAIT_FOR_LAUNCH;
      lastLaunchPrintMs = millis();
    }
    return;
  }

  if (!launchActive) {
    if (launchAttemptActive && !launchAttemptSucceeded) {
      Serial.println("Launch failed");
    }
    mode = WAIT_FOR_LAUNCH;
    return;
  }

  int sensorValue = analogRead(hallPin);
  int diffAdc = abs(sensorValue - baselineAdc);

  unsigned long adaptiveBlanking = minBlankingMicros;
  if (lastPeriodMicros > 0) {
    unsigned long quarter = lastPeriodMicros / 4;
    if (quarter > adaptiveBlanking) adaptiveBlanking = quarter;
  }

  bool newRpmSample = false;
  unsigned long newPeriod = 0;
  if (!inPulse) {
    if (diffAdc < thresholdLowAdc) {
      float b = (float)baselineAdc;
      b = b * (1.0f - baselineAlpha) + (float)sensorValue * baselineAlpha;
      baselineAdc = (int)(b + 0.5f);
    }

    if (diffAdc >= thresholdHighAdc && (lastEdgeMicros == 0 || (now - lastEdgeMicros) >= adaptiveBlanking)) {
      if (lastEdgeMicros != 0) {
        unsigned long period = now - lastEdgeMicros;
        lastPeriodMicros = period;
        newPeriod = period;

        if (period > 0 && pulsesPerRev > 0) {
          float rpmInstant = 60000000.0f / ((float)period * (float)pulsesPerRev);
          rpmFiltered = (rpmFiltered == 0.0f) ? rpmInstant : (rpmFiltered * (1.0f - rpmAlpha) + rpmInstant * rpmAlpha);
          newRpmSample = true;
        }
      }

      lastEdgeMicros = now;
      inPulse = true;
    }
  } else {
    if (diffAdc <= thresholdLowAdc) {
      inPulse = false;
    }
  }

  bool rpmValid = !(lastEdgeMicros == 0 || (now - lastEdgeMicros) > rpmTimeoutMicros);
  if (!rpmValid) {
    haveStabilityData = false;
    stabilityStartMicros = 0;
    periodIdx = 0;
    periodCount = 0;
  }

  if (newRpmSample && rpmValid && newPeriod > 0) {
    periodWindow[periodIdx] = newPeriod;
    periodIdx = (uint8_t)((periodIdx + 1) % stabilityWindowN);
    if (periodCount < stabilityWindowN) periodCount++;

    unsigned long minP = periodWindow[0];
    unsigned long maxP = periodWindow[0];
    unsigned long sumP = 0;
    for (uint8_t i = 0; i < periodCount; i++) {
      unsigned long p = periodWindow[i];
      if (p < minP) minP = p;
      if (p > maxP) maxP = p;
      sumP += p;
    }
    unsigned long avgP = (periodCount > 0) ? (sumP / periodCount) : 0;

    if (!haveStabilityData) {
      haveStabilityData = true;
      stabilityStartMicros = now;
    }

    stabilityMinPeriod = minP;
    stabilityMaxPeriod = maxP;
    stabilityAvgPeriod = avgP;

    float midP = (float)(minP + maxP) * 0.5f;
    float spanP = (float)(maxP - minP);
    bool withinMargin = (midP > 0.0f) ? (spanP <= (midP * stabilityMargin)) : false;

    const uint8_t minSamplesForStability = 6;

    if (withinMargin && periodCount >= minSamplesForStability && (now - stabilityStartMicros) >= stabilityDurationMicros) {
      unsigned long avgPeriod = stabilityAvgPeriod;
      if (avgPeriod > 0 && armLengthCm > 0.0f) {
        const float twoPiR = 2.0f * 3.1415926f * armLengthCm;
        const float extraPhaseRev = launchArcOffsetCm / twoPiR; 
        const float totalPhaseRev = payloadPhaseOffsetRev + extraPhaseRev;

        float delayUsF = totalPhaseRev * (float)avgPeriod;
        long delayUs = (long)(delayUsF + 0.5f) - (long)releaseMechLatencyMicros;
        if (delayUs < 0) delayUs = 0;

        scheduledLaunchMicros = now + (unsigned long)delayUs;
        unsigned long rpmForDelay = (unsigned long)(60000000UL / (avgPeriod * (unsigned long)pulsesPerRev));
        stableRpmForLaunch = rpmForDelay;
        mode = ARMED_DELAY;
      } else {
        haveStabilityData = false;
        stabilityStartMicros = 0;
        periodIdx = 0;
        periodCount = 0;
      }
    }

    if (!withinMargin) {
      stabilityStartMicros = now;
    }
  }

  if (logRpmWhileLaunching) {
    unsigned long nowMs = millis();
    if (nowMs - lastPrintMs >= printEveryMs) {
      lastPrintMs = nowMs;
      if (rpmValid && rpmFiltered >= 0.5f) {
        if (!verboseSerial) {
          Serial.println((unsigned long)(rpmFiltered + 0.5f));
        } else {
          Serial.print("rpm=");
          Serial.print((unsigned long)(rpmFiltered + 0.5f));
          Serial.print("  diff=");
          Serial.print(diffAdc);
          Serial.print("  baseline=");
          Serial.println(baselineAdc);
        }
      }
    }
  }
}
