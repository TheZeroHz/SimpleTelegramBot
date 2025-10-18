/*
   SimpleTelegramBot - Lightweight Telegram Bot Library
   Optimized for ESP8266/ESP32 with minimal memory footprint
   
   Features:
   - Send text messages
   - Send WAV audio files
   - No memory leaks
   - Industrial grade error handling
   - Minimal RAM usage
*/

#ifndef SimpleTelegramBot_h
#define SimpleTelegramBot_h

#include <Arduino.h>
#include <Client.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FFat.h>
#define TELEGRAM_HOST "api.telegram.org"
#define TELEGRAM_SSL_PORT 443
#define BUFFER_SIZE 512
#define RESPONSE_TIMEOUT 10000

class SimpleTelegramBot {
public:
    SimpleTelegramBot(const String& token, const String& chatId, Client& client);
    ~SimpleTelegramBot();
    
    // Send text message
    bool sendMessage(const String& text);
    
    // Send WAV audio file from buffer
    bool sendAudio(const uint8_t* audioData, size_t audioSize, const String& filename = "audio.wav");
    
    // Send WAV audio file with callback (for streaming)
    bool sendAudioStream(size_t audioSize, bool (*hasMoreData)(), uint8_t (*getNextByte)(), const String& filename = "audio.wav");
    
    // Send WAV audio file directly from FFAT filesystem (memory efficient)
    bool sendAudioFromFFAT(const String& filepath);
    
    // Get last error message
    String getLastError() const { return _lastError; }
    
    // Check if bot is ready
    bool isReady() const { return _client != nullptr && _token.length() > 0; }

private:
    String _token;
    String _chatId;
    Client* _client;
    String _lastError;
    
    // HTTP methods
    bool sendGetRequest(const String& endpoint, const String& params);
    bool sendPostRequest(const String& endpoint, const String& body, const String& contentType);
    bool sendMultipartFormData(const String& endpoint, const String& fieldName, 
                               const String& filename, const String& contentType,
                               const uint8_t* data, size_t dataSize,
                               bool (*hasMoreData)(), uint8_t (*getNextByte)());
    
    // Response handling
    bool readResponse(String& response);
    bool parseJsonResponse(const String& response);
    
    // Connection management
    bool ensureConnection();
    void closeConnection();
    
    // Helper methods
    String urlEncode(const String& str);
    void setError(const String& error);
    bool sendAudioFromFile(File& file, size_t fileSize, const String& filename);
};

#endif