const int microphonePin = A0; // Microphone connected to analog pin A0

void setup() {
  Serial.begin(9600); // Initialize serial communication
}

void loop() {
  // Read the analog value from the microphone
  int sensorValue = analogRead(microphonePin);

  // Convert the analog reading to a voltage (assuming 5V reference)
  float voltage = sensorValue * (5.0 / 1023.0);

  // Estimate the sound level (relative volume)
  // This is not an exact dB measurement but a relative indicator
  float soundLevel = map(sensorValue, 0, 1023, 0, 100); // Map to a 0-100 range

  // Print the raw sensor value, voltage, and estimated sound level
  Serial.print("Sensor Value: ");
  Serial.print(sensorValue);
  Serial.print(" | Voltage: ");
  Serial.print(voltage);
  Serial.print(" V | Sound Level: ");
  Serial.print(soundLevel);
  Serial.println(" %");

  delay(10); // Short delay for stability
}