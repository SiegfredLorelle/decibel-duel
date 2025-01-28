// Configuration constants
namespace Config {
    constexpr int MICROPHONE_PIN = A0;
    constexpr int SAMPLE_RATE_MS = 100;    // Sampling rate in milliseconds
    constexpr int SERIAL_BAUD = 9600;

    struct LEDPins {
      constexpr static int SILENT = 2;
      constexpr static int QUIET = 3;
      constexpr static int MODERATE = 4;
      constexpr static int LOUD = 5;
    };

    // Sound level calibration
    constexpr int BASELINE_VALUE = 525;     // Ambient noise level
    constexpr int MAX_ADJUSTED_VALUE = 50;  // Sensitivity multiplier
    constexpr int ANALOG_READ_MAX = 1023;   // Maximum ADC value
    
    // Sound level thresholds (in percentage)
    struct Thresholds {
        constexpr static int SILENT = 20;
        constexpr static int QUIET = 40;
        constexpr static int MODERATE = 60;
        constexpr static int LOUD = 80;
        // VeryLoud is anything above LOUD
    };
}

// Sound level categories
enum class SoundCategory {
    Silent,
    Quiet,
    Moderate,
    Loud,
    VeryLoud
};

// Class to handle sound monitoring functionality
class SoundMonitor {
private:
    // Rolling average variables
    static constexpr int AVERAGE_WINDOW = 5;
    int readings[AVERAGE_WINDOW] = {0};
    int readIndex = 0;
    int total = 0;
    
    // Get moving average of readings
    int getAverageReading() {
        total = total - readings[readIndex];
        readings[readIndex] = analogRead(Config::MICROPHONE_PIN);
        total = total + readings[readIndex];
        readIndex = (readIndex + 1) % AVERAGE_WINDOW;
        return total / AVERAGE_WINDOW;
    }
    
    // Convert raw reading to sound level percentage
    int calculateSoundLevel(int sensorValue) {
        int adjustedValue = max(sensorValue - Config::BASELINE_VALUE, 0);
        return constrain(
            map(adjustedValue, 0, Config::MAX_ADJUSTED_VALUE, 0, 100),
            0, 100
        );
    }
    
    // Determine sound category based on level
    SoundCategory categorizeSoundLevel(int soundLevel) {
        if (soundLevel <= Config::Thresholds::SILENT)   return SoundCategory::Silent;
        if (soundLevel <= Config::Thresholds::QUIET)    return SoundCategory::Quiet;
        if (soundLevel <= Config::Thresholds::MODERATE) return SoundCategory::Moderate;
        if (soundLevel <= Config::Thresholds::LOUD)     return SoundCategory::Loud;
        return SoundCategory::VeryLoud;
    }
    
    // Convert category to string
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

public:
    void begin() {
        Serial.begin(Config::SERIAL_BAUD);
        // Initialize moving average array
        for (int i = 0; i < AVERAGE_WINDOW; i++) {
            readings[i] = analogRead(Config::MICROPHONE_PIN);
            total += readings[i];
        }
    }
    
    void update() {
        // Get smoothed sensor reading
        int sensorValue = getAverageReading();
        
        // Calculate sound level and category
        int soundLevel = calculateSoundLevel(sensorValue);
        SoundCategory category = categorizeSoundLevel(soundLevel);
        
        // Print results
        Serial.print(F("Raw: "));
        Serial.print(sensorValue);
        Serial.print(F(" | Level: "));
        Serial.print(soundLevel);
        Serial.print(F("% | Category: "));
        Serial.println(getCategoryString(category));
    }
};

// Global instance
SoundMonitor soundMonitor;

void setup() {
    soundMonitor.begin();
}

void loop() {
    soundMonitor.update();
    delay(Config::SAMPLE_RATE_MS);
}