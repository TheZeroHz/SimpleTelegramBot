#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FFat.h>
#include "SimpleTelegramBot.h"

const char* ssid = ""; // Wifi SSID
const char* password = ""; //Wifi Password

// Telegram credentials
const char* BOT_TOKEN = "";
const char* CHAT_ID = "";

WiFiClientSecure client;
SimpleTelegramBot* bot = nullptr;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n========================================");
    Serial.println("SimpleTelegramBot - FFAT Audio Example");
    Serial.println("========================================\n");
    
    // Initialize FFAT
    if (!initFFAT()) {
        Serial.println("FFAT initialization failed!");
        return;
    }
    
    // Connect to WiFi
    connectWiFi();
    
    // Configure secure client with PROPER CERTIFICATE
    Serial.println("Configuring SSL certificate...");
    
    // OPTION 1: Use certificate (RECOMMENDED)
    client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
    Serial.println("✓ Certificate loaded");
    
    // OPTION 2: If certificate fails, uncomment this line (LESS SECURE)
    // client.setInsecure();
    // Serial.println("⚠ Warning: Using insecure mode (no certificate verification)");
    
    // Initialize bot
    bot = new SimpleTelegramBot(BOT_TOKEN, CHAT_ID, client);
    
    if (!bot->isReady()) {
        Serial.println("✗ Bot initialization failed!");
        return;
    }
    
    Serial.println("✓ Bot initialized successfully!\n");
    
    // Test connection with a simple message first
    Serial.println("Testing connection with simple message...");
    if (bot->sendMessage("🤖 Bot started! Testing connection...")) {
        Serial.println("✓ Test message sent successfully!\n");
    } else {
        Serial.println("✗ Test message failed!");
        Serial.print("Error: ");
        Serial.println(bot->getLastError());
        
        // If certificate fails, try insecure mode
        Serial.println("\n⚠ Trying insecure mode...");
        delete bot;
        client.setInsecure();
        bot = new SimpleTelegramBot(BOT_TOKEN, CHAT_ID, client);
        
        if (bot->sendMessage("🤖 Bot started (insecure mode)")) {
            Serial.println("✓ Working in insecure mode");
        } else {
            Serial.println("✗ Still failing. Check WiFi and credentials.");
            return;
        }
    }
    
    // List all WAV files
    listWavFiles();
    
    // Send audio if file exists
    if (FFat.exists("/recording.wav")) {
        delay(2000);
        sendAudioWithMessage("/recording.wav", "🎵 Test Audio From Rakib");
    } else {
        Serial.println("\n⚠ /recording.wav not found!");
        Serial.println("Creating a test WAV file...");
        createTestWavFile("/test_audio.wav");
        delay(1000);
        sendAudioWithMessage("/test_audio.wav", "🎵 Test Audio (Generated)");
    }
}

void loop() {
    // Your main code here
    delay(10000);
}

bool initFFAT() {
    Serial.println("Mounting FFAT...");
    
    if (!FFat.begin(false)) {  // false = don't format if mount fails
        Serial.println("✗ FFAT Mount Failed! Trying to format...");
        if (!FFat.begin(true)) {  // true = format on fail
            Serial.println("✗ FFAT Format Failed!");
            return false;
        }
    }
    
    Serial.println("✓ FFAT Mounted Successfully!");
    
    // Print filesystem info
    size_t totalBytes = FFat.totalBytes();
    size_t usedBytes = FFat.usedBytes();
    size_t freeBytes = totalBytes - usedBytes;
    
    Serial.println("\nFilesystem Info:");
    Serial.print("  Total: ");
    Serial.print(totalBytes / 1024);
    Serial.println(" KB");
    Serial.print("  Used: ");
    Serial.print(usedBytes / 1024);
    Serial.println(" KB");
    Serial.print("  Free: ");
    Serial.print(freeBytes / 1024);
    Serial.println(" KB");
    Serial.println();
    
    return true;
}

void connectWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✓ WiFi connected!");
        Serial.print("  IP address: ");
        Serial.println(WiFi.localIP());
        Serial.print("  Signal strength: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        
        // Test internet connectivity
        Serial.println("\nTesting internet connectivity...");
        if (client.connect("www.google.com", 443)) {
            Serial.println("✓ Internet connection OK");
            client.stop();
        } else {
            Serial.println("✗ No internet connection!");
        }
        Serial.println();
    } else {
        Serial.println("\n✗ WiFi connection failed!");
    }
}

void listWavFiles() {
    Serial.println("========================================");
    Serial.println("Available WAV Files:");
    Serial.println("========================================");
    
    File root = FFat.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println("✗ Failed to open root directory!");
        return;
    }
    
    int fileCount = 0;
    File file = root.openNextFile();
    
    while (file) {
        if (!file.isDirectory()) {
            String filename = String(file.name());
            
            // Check if it's a WAV file
            if (filename.endsWith(".wav") || filename.endsWith(".WAV")) {
                fileCount++;
                Serial.print(fileCount);
                Serial.print(". ");
                Serial.print(filename);
                Serial.print(" (");
                Serial.print(file.size());
                Serial.println(" bytes)");
            }
        }
        file = root.openNextFile();
    }
    
    if (fileCount == 0) {
        Serial.println("  No WAV files found!");
    }
    
    Serial.println("========================================\n");
}

void sendAudioWithMessage(const char* filepath, const char* message) {
    Serial.println("--- Sending Audio with Message ---");
    
    // First send text message
    Serial.print("Sending message: ");
    Serial.println(message);
    
    if (bot->sendMessage(message)) {
        Serial.println("✓ Message sent");
        
        delay(1000);
        
        // Then send audio
        Serial.print("Sending audio: ");
        Serial.println(filepath);
        
        unsigned long startTime = millis();
        
        if (bot->sendAudioFromFFAT(filepath)) {
            unsigned long duration = millis() - startTime;
            Serial.println("✓ Audio sent successfully!");
            Serial.print("  Upload time: ");
            Serial.print(duration);
            Serial.println(" ms");
        } else {
            Serial.print("✗ Failed: ");
            Serial.println(bot->getLastError());
        }
    } else {
        Serial.print("✗ Message failed: ");
        Serial.println(bot->getLastError());
    }
    Serial.println();
}

void printMemoryUsage() {
    Serial.print("Free heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.print(" bytes (");
    Serial.print(ESP.getFreeHeap() / 1024);
    Serial.println(" KB)");
}

// Create a test WAV file (simple 1 second tone at 440Hz)
void createTestWavFile(const char* filepath) {
    Serial.print("Creating test WAV file: ");
    Serial.println(filepath);
    
    File file = FFat.open(filepath, "w");
    if (!file) {
        Serial.println("✗ Failed to create file!");
        return;
    }
    
    // WAV header for 1 second, 8kHz, 8-bit mono
    const int sampleRate = 8000;
    const int duration = 1; // seconds
    const int dataSize = sampleRate * duration;
    const int fileSize = 44 + dataSize;
    
    // RIFF header
    file.write((uint8_t*)"RIFF", 4);
    uint32_t chunkSize = fileSize - 8;
    file.write((uint8_t*)&chunkSize, 4);
    file.write((uint8_t*)"WAVE", 4);
    
    // fmt subchunk
    file.write((uint8_t*)"fmt ", 4);
    uint32_t subchunk1Size = 16;
    file.write((uint8_t*)&subchunk1Size, 4);
    uint16_t audioFormat = 1; // PCM
    file.write((uint8_t*)&audioFormat, 2);
    uint16_t numChannels = 1; // Mono
    file.write((uint8_t*)&numChannels, 2);
    uint32_t sampleRateVal = sampleRate;
    file.write((uint8_t*)&sampleRateVal, 4);
    uint32_t byteRate = sampleRate * 1 * 1; // SampleRate * NumChannels * BitsPerSample/8
    file.write((uint8_t*)&byteRate, 4);
    uint16_t blockAlign = 1;
    file.write((uint8_t*)&blockAlign, 2);
    uint16_t bitsPerSample = 8;
    file.write((uint8_t*)&bitsPerSample, 2);
    
    // data subchunk
    file.write((uint8_t*)"data", 4);
    uint32_t subchunk2Size = dataSize;
    file.write((uint8_t*)&subchunk2Size, 4);
    
    // Generate 440 Hz tone
    for (int i = 0; i < dataSize; i++) {
        uint8_t sample = 128 + (uint8_t)(127 * sin(2 * PI * 440 * i / (float)sampleRate));
        file.write(sample);
    }
    
    file.close();
    
    Serial.println("✓ Test WAV file created!");
    Serial.print("  File size: ");
    Serial.print(fileSize);
    Serial.println(" bytes");
}

// Advanced: Send with detailed statistics
void sendAudioWithStats(const char* filepath) {
    Serial.println("\n========================================");
    Serial.print("Sending: ");
    Serial.println(filepath);
    Serial.println("========================================");
    
    if (!FFat.exists(filepath)) {
        Serial.println("✗ File not found!");
        return;
    }
    
    File file = FFat.open(filepath);
    if (!file) {
        Serial.println("✗ Failed to open file!");
        return;
    }
    
    size_t fileSize = file.size();
    file.close();
    
    Serial.print("File size: ");
    Serial.print(fileSize);
    Serial.print(" bytes (");
    Serial.print(fileSize / 1024.0, 2);
    Serial.println(" KB)");
    
    unsigned long startTime = millis();
    size_t startHeap = ESP.getFreeHeap();
    
    bool success = bot->sendAudioFromFFAT(filepath);
    
    unsigned long duration = millis() - startTime;
    size_t endHeap = ESP.getFreeHeap();
    
    Serial.println("========================================");
    if (success) {
        Serial.println("✓ SUCCESS");
        Serial.print("  Duration: ");
        Serial.print(duration);
        Serial.println(" ms");
        Serial.print("  Speed: ");
        Serial.print((fileSize * 1000.0) / (duration * 1024.0), 2);
        Serial.println(" KB/s");
        Serial.print("  Memory used: ");
        Serial.print((int)(startHeap - endHeap));
        Serial.println(" bytes");
    } else {
        Serial.println("✗ FAILED");
        Serial.print("  Error: ");
        Serial.println(bot->getLastError());
    }
    Serial.println("========================================\n");
}