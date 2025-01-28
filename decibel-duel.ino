#include <Wire.h>              // Include the Wire library for I2C
#include <LiquidCrystal_I2C.h> // Include the LiquidCrystal_I2C library

// Initialize the I2C LCD with the I2C address, number of columns, and rows
// Common I2C addresses for LCD are 0x27 or 0x3F
LiquidCrystal_I2C lcd(0x23, 16, 2);  // Change the address if necessary

// Configuration constants
namespace Config {
    constexpr int MICROPHONE_PIN = A0;
    constexpr int SAMPLE_RATE_MS = 50;  // Increased sampling rate
    constexpr int SERIAL_BAUD = 9600;

    constexpr int BASELINE_VALUE = 585;
    constexpr int MAX_ADJUSTED_VALUE = 300;
    constexpr int ANALOG_READ_MAX = 1023;

    constexpr int PEAK_HOLD_TIME_MS = 1000;
    constexpr float PEAK_DECAY_RATE = 0.95;
}

// LED pins for each category
enum class UserLEDPin {
    SILENT = 7,
    QUIET = 8,
    MODERATE = 9, 
    LOUD = 10,
    VERY_LOUD = 11
};

enum class BotLEDPin {
    SILENT = 2,
    QUIET = 3,
    MODERATE = 4,
    LOUD = 5,
    VERY_LOUD = 6
};

// Sound level thresholds (in percentage)
enum class Threshold {
    SILENT = 10,
    QUIET = 30,
    MODERATE = 60,
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
    static constexpr int AVERAGE_WINDOW = 10;
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
            case SoundCategory::Silent:   return static_cast<int>(UserLEDPin::SILENT);
            case SoundCategory::Quiet:    return static_cast<int>(UserLEDPin::QUIET);
            case SoundCategory::Moderate: return static_cast<int>(UserLEDPin::MODERATE);
            case SoundCategory::Loud:     return static_cast<int>(UserLEDPin::LOUD);
            case SoundCategory::VeryLoud: return static_cast<int>(UserLEDPin::VERY_LOUD);
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
            for (int i = static_cast<int>(UserLEDPin::SILENT); i <= static_cast<int>(UserLEDPin::VERY_LOUD); i++) {
                digitalWrite(i, LOW);
            }
            // Turn on LEDs up to the new category
            for (int i = static_cast<int>(UserLEDPin::SILENT); i <= getCategoryLEDPin(newCategory); i++) {
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

void updateLCD(SoundCategory category) {
    lcd.setCursor(0, 0);               // Set cursor to the first column of the first row
    lcd.print("                ");     // Clear the line by printing spaces
    lcd.setCursor(0, 0);               // Reset cursor to the start of the line
    lcd.print(getCategoryString(category));  // Print the current category
}

public:
    void begin() {
        Serial.begin(Config::SERIAL_BAUD);
        initializeLEDs();

        lcd.init();           // Initialize the LCD
        lcd.backlight();      // Turn on the backlight
        lcd.print("Sound Monitor");  // Print a welcome message
        delay(1000);
        lcd.clear();  // Clear the LCD

        for (int i = 0; i < AVERAGE_WINDOW; i++) {
            addReading(getMaxSample());
            delay(1);
        }

        digitalWrite(getCategoryLEDPin(currentCategory), HIGH);
    }

    void initializeLEDs() {
        for (int i = static_cast<int>(UserLEDPin::SILENT); i <= static_cast<int>(UserLEDPin::VERY_LOUD); i++) {
            pinMode(i, OUTPUT);
            digitalWrite(i, LOW);
        }
        for (int i = static_cast<int>(BotLEDPin::SILENT); i <= static_cast<int>(BotLEDPin::VERY_LOUD); i++) {
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
        updateLCD(newCategory);  // Update the LCD with the current category

        printDebugInfo(sensorValue, soundLevel, newCategory);
    }
};

SoundMonitor soundMonitor;

class BotLEDController {
private:
    SoundCategory currentBotCategory = SoundCategory::Silent;
    unsigned long lastChangeTime = 0;
    unsigned long changeInterval = 0;

    int getCategoryLEDPin(SoundCategory category) {
        switch (category) {
            case SoundCategory::Silent:   return static_cast<int>(BotLEDPin::SILENT);
            case SoundCategory::Quiet:    return static_cast<int>(BotLEDPin::QUIET);
            case SoundCategory::Moderate: return static_cast<int>(BotLEDPin::MODERATE);
            case SoundCategory::Loud:     return static_cast<int>(BotLEDPin::LOUD);
            case SoundCategory::VeryLoud: return static_cast<int>(BotLEDPin::VERY_LOUD);
            default:                      return -1;
        }
    }

    void updateBotLEDs(SoundCategory newCategory) {
        if (newCategory != currentBotCategory) {
            // Turn off all Bot LEDs
            for (int i = static_cast<int>(BotLEDPin::SILENT); i <= static_cast<int>(BotLEDPin::VERY_LOUD); i++) {
                digitalWrite(i, LOW);
            }
            // Turn on Bot LEDs up to the new category
            for (int i = static_cast<int>(BotLEDPin::SILENT); i <= getCategoryLEDPin(newCategory); i++) {
                digitalWrite(i, HIGH);
            }
            currentBotCategory = newCategory;
        }
    }

    SoundCategory getRandomCategory() {
        int randomValue = random(0, 5);  // Random value between 0 and 4
        return static_cast<SoundCategory>(randomValue);
    }

public:
    void begin() {
        for (int i = static_cast<int>(BotLEDPin::SILENT); i <= static_cast<int>(BotLEDPin::VERY_LOUD); i++) {
            pinMode(i, OUTPUT);
            digitalWrite(i, LOW);
        }
        randomSeed(analogRead(0));  // Seed the random number generator
        lastChangeTime = millis();
        changeInterval = random(2000, 3000);  // Random interval between 2 and 10 seconds
    }

    void update() {
        unsigned long currentTime = millis();
        if (currentTime - lastChangeTime >= changeInterval) {
            SoundCategory newCategory = getRandomCategory();
            updateBotLEDs(newCategory);
            lastChangeTime = currentTime;
            changeInterval = random(2000, 3000);  // Set a new random interval
        }
    }
};

BotLEDController botLEDController;

void setup() {
    soundMonitor.begin();
    botLEDController.begin();
}

void loop() {
    soundMonitor.update();
    botLEDController.update();
    delay(Config::SAMPLE_RATE_MS);
}