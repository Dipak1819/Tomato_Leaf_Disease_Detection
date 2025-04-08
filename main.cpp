#include "mbed.h"
#include "LCDi2c.h" // Include the I2C LCD library

// GPIO pin for triggering ESP32
DigitalOut trigger(PTC7);  // Use PTC7 as the trigger pin

// GPIO pin for receiving signal from ESP32
DigitalIn esp32Signal(PTC2);  // Input pin for ESP32 signal

// GPIO pin for motor control (spray system)
DigitalOut motorControl(PTC12);  // Pin to activate/deactivate motor

// GPIO pin for DS18B20 data line
DigitalInOut dataPin(PTB9);  // Replace PTB9 with your actual pin

// Serial communication for debugging
BufferedSerial pc(USBTX, USBRX, 115200);

// I2C LCD setup
LCDi2c lcd(PTE25, PTE24, LCD16x2); // SDA, SCL pins

// Function to scroll text on a specific row of the LCD
void scrollText(LCDi2c &lcd, const char* message, int row, int delay_ms) {
    int messageLength = strlen(message);
    
    // If message fits on screen, just display it
    if (messageLength <= 16) {
        lcd.locate(0, row);
        lcd.printf("%s", message);
        return;
    }
    
    // Create a padded message with spaces at the end for smooth scrolling
    char paddedMessage[100]; // Make sure this is large enough
    sprintf(paddedMessage, "%s    ", message); // Add 4 spaces for a pause at the end
    int paddedLength = strlen(paddedMessage);
    
    // Scroll the message
    for (int i = 0; i < paddedLength - 15; i++) {
        lcd.locate(0, row);
        // Display the current 16-character window of the message
        for (int j = 0; j < 16; j++) {
            if (i + j < paddedLength) {
                lcd.putc(paddedMessage[i + j]);
            } else {
                lcd.putc(' ');
            }
        }
        ThisThread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
}

// Function to print messages to serial
void print_message(const char* message) {
    pc.write(message, strlen(message));
}

// Function to reset 1-Wire bus
bool reset() {
    dataPin.output();
    dataPin = 0; // Pull bus low
    wait_us(480);

    dataPin.input(); // Release bus
    wait_us(70);

    bool presence = !dataPin.read(); // Check for presence pulse (active low)
    wait_us(410);

    return presence;
}

// Function to write a single bit
void writeBit(bool bit) {
    dataPin.output();
    dataPin = 0;
    wait_us(bit ? 6 : 60);
    dataPin.input();
    wait_us(bit ? 64 : 10);
}

// Function to write a byte
void writeByte(uint8_t byte) {
    for (int i = 0; i < 8; i++) {
        writeBit(byte & 0x01);
        byte >>= 1;
    }
}

// Function to read a single bit
bool readBit() {
    dataPin.output();
    dataPin = 0;
    wait_us(6);
    dataPin.input();
    wait_us(9);

    bool bit = dataPin.read();
    wait_us(55);

    return bit;
}

// Function to read a byte
uint8_t readByte() {
    uint8_t value = 0;
    for (int i = 0; i < 8; i++) {
        if (readBit()) {
            value |= (1 << i);
        }
    }
    return value;
}

// Function to read temperature from DS18B20 with retries
float readTemperature() {
    int retries = 3; // Maximum number of retries
    while (retries--) {
        if (!reset()) continue; // Retry if no sensor detected

        writeByte(0xCC); // Skip ROM command
        writeByte(0x44); // Start temperature conversion

        ThisThread::sleep_for(800ms); // Increased delay slightly

        if (!reset()) continue; // Retry if reset fails

        writeByte(0xCC); // Skip ROM command again
        writeByte(0xBE); // Read scratchpad command

        uint8_t lsb = readByte();
        uint8_t msb = readByte();

        int16_t rawTemp = (msb << 8) | lsb;
        float temperature = rawTemp * 0.0625;

        if (temperature >= -55.0 && temperature <= 125.0) {
            return temperature; // Valid temperature reading
        }
    }

    return -999.0; // Return error after retries
}

// Temperature monitoring and spray control mode
void temperatureSprayMode() {
    print_message("Entering Temperature Spray Mode...\r\n");
    
    // Update LCD with alert message
    lcd.cls();
    lcd.locate(0, 0);
    lcd.printf("ALERT!");
    scrollText(lcd, "Disease Detected - Starting Treatment System", 1, 300);
    
    bool exitMode = false;
    float lastValidTemperature = 0.0;
    bool hasValidReading = false;
    bool lastMotorState = motorControl;

    while (!exitMode) {
        float temperature = readTemperature();
        
        if (temperature != -999.0) { 
            // Valid temperature reading
            lastValidTemperature = temperature;
            hasValidReading = true;
            
            char tempMessage[50];
            sprintf(tempMessage, "Current Temperature: %.2f °C\r\n", temperature);
            print_message(tempMessage);

            // Control spray based on temperature threshold
            if (temperature > 28.0) {
                if (motorControl == 0) {
                    motorControl = 1; 
                    print_message("Motor ON - Temperature above threshold\r\n");
                    
                    // Update LCD when motor state changes
                    lcd.cls();
                    lcd.locate(0, 0);
                    scrollText(lcd, "Disease Alert - High Temp Detected", 0, 300);
                    scrollText(lcd, "Spraying Medicine Now", 1, 300);
                }
            } else {
                if (motorControl == 1) {
                    motorControl = 0; 
                    print_message("Motor OFF - Temperature below threshold\r\n");
                    
                    // Update LCD when motor state changes
                    lcd.cls();
                    lcd.locate(0, 0);
                    scrollText(lcd, "Disease Alert - Temp Normalized", 0, 300);
                    lcd.locate(0, 1);
                    lcd.printf("Motor OFF");
                }
            }
        } else {
            // Error reading temperature - don't print error message, just retry
            // Only use last valid reading if we've had at least one good reading
            if (hasValidReading) {
                // Continue using the last valid temperature for control decisions
                // but don't print the value to avoid confusing the user
                if (lastValidTemperature > 28.0) {
                    if (motorControl == 0) {
                        motorControl = 1;
                        print_message("Motor ON - Using last valid temperature\r\n");
                        
                        // Update LCD when motor state changes
                        lcd.cls();
                        lcd.locate(0, 0);
                        scrollText(lcd, "Disease Alert - Using Last Reading", 0, 300);
                        scrollText(lcd, "Spraying Medicine", 1, 300);
                    }
                } else {
                    if (motorControl == 1) {
                        motorControl = 0;
                        print_message("Motor OFF - Using last valid temperature\r\n");
                        
                        // Update LCD when motor state changes
                        lcd.cls();
                        lcd.locate(0, 0);
                        scrollText(lcd, "Disease Alert - Using Last Reading", 0, 300);
                        lcd.locate(0, 1);
                        lcd.printf("Motor OFF");
                    }
                }
            }
        }

        // Check for exit command
        if (pc.readable()) {
            char input;
            pc.read(&input, 1);
            if (input == 'x') {  
                print_message("Exiting Temperature Spray Mode...\r\n");
                motorControl = 0; // Ensure motor is OFF when exiting
                
                // Reset LCD to regular status
                lcd.cls();
                lcd.locate(0, 0);
                lcd.printf("Plant Monitor");
                lcd.locate(0, 1);
                lcd.printf("System Ready");
                
                exitMode = true;
                break;
            }
        }

        ThisThread::sleep_for(500ms); // Check temperature every 500ms
    }
}

int main() {
    print_message("FRDM-K64F Integrated System\r\n");
    print_message("Press Enter to trigger ESP32 image capture\r\n");
    print_message("System will automatically enter temperature spray mode if unhealthy plant is detected\r\n");
    
    motorControl = 0; // Ensure motor is OFF initially
    
    // Initialize LCD
    lcd.cls();
    lcd.locate(0, 0);
    lcd.printf("Plant Monitor");
    lcd.locate(0, 1);
    scrollText(lcd, "System Ready - Press Enter to Start", 1, 300);
    
    bool lastSignalState = false;

    while (true) {
        char input;

        // Check for user input
        if (pc.readable()) {
            pc.read(&input, 1);
            if (input == '\r') {  
                // Send trigger signal to ESP32 for image capture
                trigger = 1; 
                
                // Update LCD
                lcd.cls();
                lcd.locate(0, 0);
                lcd.printf("Sending Signal");
                scrollText(lcd, "To ESP32 Camera System...", 1, 300);
                
                ThisThread::sleep_for(100ms);
                trigger = 0;
                print_message("Trigger signal sent to ESP32 for image capture!\r\n");
                
                // Reset LCD after short delay
                ThisThread::sleep_for(900ms);
                lcd.cls();
                lcd.locate(0, 0);
                lcd.printf("Plant Monitor");
                scrollText(lcd, "Waiting for Analysis Result...", 1, 300);
            }
        }

        // Check if ESP32 has sent a signal (unhealthy plant detected)
        bool currentSignalState = esp32Signal.read();

        if (currentSignalState && !lastSignalState) {
            print_message("Signal HIGH received from ESP32 - UNHEALTHY PLANT DETECTED\r\n");
            print_message("Starting temperature-controlled spray system\r\n");
            
            // Update LCD
            lcd.cls();
            lcd.locate(0, 0);
            lcd.printf("ESP32 Signal");
            scrollText(lcd, "Plant Disease Detected! Starting Treatment", 1, 300);
            ThisThread::sleep_for(1000ms);
            
            // Enter temperature spray mode - will continuously monitor temperature
            // and control spray accordingly until user presses 'x'
            temperatureSprayMode();
            
            print_message("Returned to main mode. Press Enter to trigger ESP32 for new image.\r\n");
        }

        lastSignalState = currentSignalState;

        ThisThread::sleep_for(50ms);
    }
}