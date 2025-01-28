// Configuration constants
namespace Config {
    // Existing pins and constants
    constexpr int MICROPHONE_PIN = A0;
    constexpr int SAMPLE_RATE_MS = 100;
    constexpr int SERIAL_BAUD = 9600;

    // Enhanced sound level calibration
    constexpr int BASELINE_VALUE = 590;
    constexpr int MAX_ADJUSTED_VALUE = 50;
    constexpr int ANALOG_READ_MAX = 1023;

    // Peak detection
    constexpr int PEAK_HOLD_TIME_MS = 1000;  // How long to hold peak value
    constexpr float PEAK_DECAY_RATE = 0.95;  // How quickly peak decays
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
    SILENT = 20,
    QUIET = 40,
    MODERATE = 60,
    LOUD = 80
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
    // Rolling average variables
    static constexpr int AVERAGE_WINDOW = 10;  // Increased window size
    int readings[AVERAGE_WINDOW] = {0};       // Raw array instead of std::array
    int readIndex = 0;
    int total = 0;

    // Peak detection variables
    int peakValue = 0;
    unsigned long lastPeakTime = 0;

    // Track current category for LED control
    SoundCategory currentCategory = SoundCategory::Silent;

    // Add a new reading to the rolling average buffer
    void addReading(int value) {
        total = total - readings[readIndex];
        readings[readIndex] = value;
        total = total + value;
        readIndex = (readIndex + 1) % AVERAGE_WINDOW;
    }

    // Get the current rolling average
    int getAverage() const {
        return total / AVERAGE_WINDOW;
    }

    // Take multiple samples and return the maximum value
    int getMaxSample() {
        int maxSample = 0;
        for (int i = 0; i < 4; i++) {  // Take 4 quick samples
            int sample = analogRead(Config::MICROPHONE_PIN);
            maxSample = max(maxSample, sample);
            delayMicroseconds(100);  // Small delay between samples
        }
        return maxSample;
    }

    // Get the average reading (combines getMaxSample and getAverage)
    int getAverageReading() {
        int maxSample = getMaxSample(); // Take multiple samples and get the maximum
        addReading(maxSample);          // Add the sample to the rolling average
        return getAverage();            // Return the current average
    }

    // Calculate the scaled sound level
    int calculateSoundLevel(int sensorValue) {
        // Apply noise floor reduction
        int adjustedValue = max(sensorValue - Config::BASELINE_VALUE, 0);

        // Apply non-linear scaling for better sensitivity
        float scaledValue = pow(adjustedValue / static_cast<float>(Config::MAX_ADJUSTED_VALUE), 0.8f) * 100;

        return constrain(static_cast<int>(scaledValue), 0, 100);
    }

    // Categorize the sound level
    SoundCategory categorizeSoundLevel(int soundLevel) {
        if (soundLevel <= static_cast<int>(Threshold::SILENT))   return SoundCategory::Silent;
        if (soundLevel <= static_cast<int>(Threshold::QUIET))    return SoundCategory::Quiet;
        if (soundLevel <= static_cast<int>(Threshold::MODERATE)) return SoundCategory::Moderate;
        if (soundLevel <= static_cast<int>(Threshold::LOUD))     return SoundCategory::Loud;
        return SoundCategory::VeryLoud;
    }

    // Get the string representation of a sound category
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

    // Get the LED pin for a sound category
    int getCategoryLEDPin(SoundCategory category) {
        switch (category) {
            case SoundCategory::Silent:   return static_cast<int>(LEDPin::SILENT);
            case SoundCategory::Quiet:    return static_cast<int>(LEDPin::QUIET);
            case SoundCategory::Moderate: return static_cast<int>(LEDPin::MODERATE);
            case SoundCategory::Loud:     return static_cast<int>(LEDPin::LOUD);
            case SoundCategory::VeryLoud: return static_cast<int>(LEDPin::VERY_LOUD);
            default:                      return -1;  // Invalid pin
        }
    }

    // Update the peak value with decay
    void updatePeakValue(int soundLevel) {
        unsigned long currentTime = millis();

        // Update peak if new value is higher
        if (soundLevel > peakValue) {
            peakValue = soundLevel;
            lastPeakTime = currentTime;
        }
        // Decay peak value after hold time
        else if (currentTime - lastPeakTime > Config::PEAK_HOLD_TIME_MS) {
            peakValue *= Config::PEAK_DECAY_RATE;
        }
    }

    // Update LEDs based on the current sound category
    void updateLEDs(SoundCategory newCategory) {
        // Only update LEDs if category has changed
        if (newCategory != currentCategory) {
            // Turn off LED for previous category
            digitalWrite(getCategoryLEDPin(currentCategory), LOW);
            // Turn on LED for new category
            digitalWrite(getCategoryLEDPin(newCategory), HIGH);
            currentCategory = newCategory;
        }
    }

    // Print debug information to the serial monitor
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
    // Initialize the sound monitor
    void begin() {
        Serial.begin(Config::SERIAL_BAUD);
        initializeLEDs();

        // Initialize moving average with multiple samples
        for (int i = 0; i < AVERAGE_WINDOW; i++) {
            addReading(getMaxSample());
            delay(1);  // Small delay between initial readings
        }

        // Set initial LED state
        digitalWrite(getCategoryLEDPin(currentCategory), HIGH);
    }

    // Initialize LED pins
    void initializeLEDs() {
        pinMode(static_cast<int>(LEDPin::SILENT), OUTPUT);
        pinMode(static_cast<int>(LEDPin::QUIET), OUTPUT);
        pinMode(static_cast<int>(LEDPin::MODERATE), OUTPUT);
        pinMode(static_cast<int>(LEDPin::LOUD), OUTPUT);
        pinMode(static_cast<int>(LEDPin::VERY_LOUD), OUTPUT);

        // Turn off all LEDs initially
        digitalWrite(static_cast<int>(LEDPin::SILENT), LOW);
        digitalWrite(static_cast<int>(LEDPin::QUIET), LOW);
        digitalWrite(static_cast<int>(LEDPin::MODERATE), LOW);
        digitalWrite(static_cast<int>(LEDPin::LOUD), LOW);
        digitalWrite(static_cast<int>(LEDPin::VERY_LOUD), LOW);
    }

    // Update the sound monitor
    void update() {
        // Get smoothed sensor reading
        int sensorValue = getAverageReading();

        // Calculate sound level and category
        int soundLevel = calculateSoundLevel(sensorValue);
        updatePeakValue(soundLevel);

        SoundCategory newCategory = categorizeSoundLevel(soundLevel);

        // Update LEDs based on new category
        updateLEDs(newCategory);

        // Print debug information
        printDebugInfo(sensorValue, soundLevel, newCategory);
    }
};

// Global instance of SoundMonitor
SoundMonitor soundMonitor;

void setup() {
    soundMonitor.begin();
}

void loop() {
    soundMonitor.update();
    delay(Config::SAMPLE_RATE_MS);
}