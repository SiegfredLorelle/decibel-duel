// Configuration constants
namespace Config {
    constexpr int MICROPHONE_PIN = A0;
    constexpr int SAMPLE_RATE_MS = 50;  // Increased sampling rate
    constexpr int SERIAL_BAUD = 9600;

    constexpr int BASELINE_VALUE = 590;
    constexpr int MAX_ADJUSTED_VALUE = 300;
    constexpr int ANALOG_READ_MAX = 1023;

    constexpr int PEAK_HOLD_TIME_MS = 1000;
    constexpr float PEAK_DECAY_RATE = 0.95;
}

// LED pins for each category
enum class LEDPin {
    SILENT = 2,     // Green
    QUIET = 3,      // Blue
    MODERATE = 4,   // Yellow
    LOUD = 5,       // Orange
    VERY_LOUD = 6   // Red
};

// Sound level thresholds (in percentage)
enum class Threshold {
    SILENT = 25,
    QUIET = 50,
    MODERATE = 75,
    LOUD = 99
};

// Sound categories
enum class SoundCategory {
    Silent,
    Quiet,
    Moderate,
    Loud,
    VeryLoud
};

class SoundMonitor {
private:
    static constexpr int AVERAGE_WINDOW = 7;
    int readings[AVERAGE_WINDOW] = {0};
    int readIndex = 0;
    int total = 0;

    int peakValue = 0;
    unsigned long lastPeakTime = 0;

    SoundCategory currentCategory = SoundCategory::Silent;

    void addReading(int value) {
        total = total - readings[readIndex];
        readings[readIndex] = value;
        total = total + value;
        readIndex = (readIndex + 1) % AVERAGE_WINDOW;
    }

    int getAverage() const {
        return total / AVERAGE_WINDOW;
    }

    int getMaxSample() {
        int maxSample = 0;
        for (int i = 0; i < 4; i++) {
            int sample = analogRead(Config::MICROPHONE_PIN);
            maxSample = max(maxSample, sample);
            delayMicroseconds(100);
        }
        return maxSample;
    }

    int getAverageReading() {
        int maxSample = getMaxSample();
        addReading(maxSample);
        return getAverage();
    }

    int calculateSoundLevel(int sensorValue) {
        int adjustedValue = max(sensorValue - Config::BASELINE_VALUE, 0);
        float scaledValue = pow(adjustedValue / static_cast<float>(Config::MAX_ADJUSTED_VALUE), 0.8f) * 100;
        return constrain(static_cast<int>(scaledValue), 0, 100);
    }

    SoundCategory categorizeSoundLevel(int soundLevel) {
        if (soundLevel <= static_cast<int>(Threshold::SILENT))   return SoundCategory::Silent;
        if (soundLevel <= static_cast<int>(Threshold::QUIET))    return SoundCategory::Quiet;
        if (soundLevel <= static_cast<int>(Threshold::MODERATE)) return SoundCategory::Moderate;
        if (soundLevel <= static_cast<int>(Threshold::LOUD))     return SoundCategory::Loud;
        return SoundCategory::VeryLoud;
    }

    const char* getCategoryString(SoundCategory category) {
        switch (category) {
            case SoundCategory::Silent:   return "Silent";
            case SoundCategory::Quiet:    return "Quiet";
            case SoundCategory::Moderate: return "Moderate";
            case SoundCategory::Loud:     return "Loud";
            case SoundCategory::VeryLoud: return "Very Loud";
            default:                      return "Unknown";
        }
    }

    int getCategoryLEDPin(SoundCategory category) {
        switch (category) {
            case SoundCategory::Silent:   return static_cast<int>(LEDPin::SILENT);
            case SoundCategory::Quiet:    return static_cast<int>(LEDPin::QUIET);
            case SoundCategory::Moderate: return static_cast<int>(LEDPin::MODERATE);
            case SoundCategory::Loud:     return static_cast<int>(LEDPin::LOUD);
            case SoundCategory::VeryLoud: return static_cast<int>(LEDPin::VERY_LOUD);
            default:                      return -1;
        }
    }

    void updatePeakValue(int soundLevel) {
        unsigned long currentTime = millis();
        if (soundLevel > peakValue) {
            peakValue = soundLevel;
            lastPeakTime = currentTime;
        } else if (currentTime - lastPeakTime > Config::PEAK_HOLD_TIME_MS) {
            peakValue *= Config::PEAK_DECAY_RATE;
        }
    }

    void updateLEDs(SoundCategory newCategory) {
        if (newCategory != currentCategory) {
            // Turn off all LEDs
            for (int i = static_cast<int>(LEDPin::SILENT); i <= static_cast<int>(LEDPin::VERY_LOUD); i++) {
                digitalWrite(i, LOW);
            }
            // Turn on LEDs up to the new category
            for (int i = static_cast<int>(LEDPin::SILENT); i <= getCategoryLEDPin(newCategory); i++) {
                digitalWrite(i, HIGH);
            }
            currentCategory = newCategory;
        }
    }

    void printDebugInfo(int rawValue, int soundLevel, SoundCategory category) {
        Serial.print(F("Raw: "));
        Serial.print(rawValue);
        Serial.print(F(" | Level: "));
        Serial.print(soundLevel);
        Serial.print(F("% | Peak: "));
        Serial.print(peakValue);
        Serial.print(F("% | Category: "));
        Serial.println(getCategoryString(category));
    }

public:
    void begin() {
        Serial.begin(Config::SERIAL_BAUD);
        initializeLEDs();

        for (int i = 0; i < AVERAGE_WINDOW; i++) {
            addReading(getMaxSample());
            delay(1);
        }

        digitalWrite(getCategoryLEDPin(currentCategory), HIGH);
    }

    void initializeLEDs() {
        for (int i = static_cast<int>(LEDPin::SILENT); i <= static_cast<int>(LEDPin::VERY_LOUD); i++) {
            pinMode(i, OUTPUT);
            digitalWrite(i, LOW);
        }
    }

    void update() {
        int sensorValue = getAverageReading();
        int soundLevel = calculateSoundLevel(sensorValue);
        updatePeakValue(soundLevel);

        SoundCategory newCategory = categorizeSoundLevel(soundLevel);
        updateLEDs(newCategory);

        printDebugInfo(sensorValue, soundLevel, newCategory);
    }
};

SoundMonitor soundMonitor;

void setup() {
    soundMonitor.begin();
}

void loop() {
    soundMonitor.update();
    delay(Config::SAMPLE_RATE_MS);
}