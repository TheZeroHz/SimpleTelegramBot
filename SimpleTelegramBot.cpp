/*
   SimpleTelegramBot - Implementation (FIXED VERSION)
*/

#include "SimpleTelegramBot.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FFat.h>

SimpleTelegramBot::SimpleTelegramBot(const String& token, const String& chatId, Client& client) 
    : _token(token), _chatId(chatId), _client(&client), _lastError("") {
}

SimpleTelegramBot::~SimpleTelegramBot() {
    closeConnection();
}

bool SimpleTelegramBot::sendMessage(const String& text) {
    if (!isReady()) {
        setError("Bot not initialized");
        return false;
    }
    
    if (text.length() == 0) {
        setError("Empty message");
        return false;
    }
    
    String params = "chat_id=" + _chatId + "&text=" + urlEncode(text);
    return sendGetRequest("sendMessage", params);
}

bool SimpleTelegramBot::sendAudio(const uint8_t* audioData, size_t audioSize, const String& filename) {
    if (!isReady()) {
        setError("Bot not initialized");
        return false;
    }
    
    if (audioData == nullptr || audioSize == 0) {
        setError("Invalid audio data");
        return false;
    }
    
    return sendMultipartFormData("sendAudio", "audio", filename, "audio/wav", 
                                 audioData, audioSize, nullptr, nullptr);
}

bool SimpleTelegramBot::sendAudioStream(size_t audioSize, bool (*hasMoreData)(), 
                                        uint8_t (*getNextByte)(), const String& filename) {
    if (!isReady()) {
        setError("Bot not initialized");
        return false;
    }
    
    if (hasMoreData == nullptr || getNextByte == nullptr || audioSize == 0) {
        setError("Invalid stream parameters");
        return false;
    }
    
    return sendMultipartFormData("sendAudio", "audio", filename, "audio/wav", 
                                 nullptr, audioSize, hasMoreData, getNextByte);
}

bool SimpleTelegramBot::sendAudioFromFFAT(const String& filepath) {
    if (!isReady()) {
        setError("Bot not initialized");
        return false;
    }
    
    // Check if FFAT is mounted
    if (!FFat.begin(false)) {  // Changed from true to false - don't format!
        setError("FFAT not mounted");
        return false;
    }
    
    // Open file
    File audioFile = FFat.open(filepath, "r");
    if (!audioFile) {
        setError("File not found: " + filepath);
        return false;
    }
    
    // Check if file is valid
    if (audioFile.isDirectory()) {
        audioFile.close();
        setError("Path is a directory");
        return false;
    }
    
    size_t fileSize = audioFile.size();
    if (fileSize == 0) {
        audioFile.close();
        setError("File is empty");
        return false;
    }
    
    // Check Telegram file size limit (50 MB)
    if (fileSize > 50 * 1024 * 1024) {
        audioFile.close();
        setError("File too large (max 50 MB)");
        return false;
    }
    
    // Extract filename from path
    String filename = filepath;
    int lastSlash = filepath.lastIndexOf('/');
    if (lastSlash != -1) {
        filename = filepath.substring(lastSlash + 1);
    }
    
    // Send using streaming to avoid loading entire file into RAM
    bool success = sendAudioFromFile(audioFile, fileSize, filename);
    
    audioFile.close();
    return success;
}

bool SimpleTelegramBot::sendAudioFromFile(File& file, size_t fileSize, const String& filename) {
    if (!ensureConnection()) {
        return false;
    }
    
    String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
    
    // Build form data header
    String formHeader = "--" + boundary + "\r\n";
    formHeader += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
    formHeader += _chatId + "\r\n";
    formHeader += "--" + boundary + "\r\n";
    formHeader += "Content-Disposition: form-data; name=\"audio\"; filename=\"" + filename + "\"\r\n";
    formHeader += "Content-Type: audio/wav\r\n\r\n";
    
    String formFooter = "\r\n--" + boundary + "--\r\n";
    
    size_t contentLength = formHeader.length() + fileSize + formFooter.length();
    
    // Send HTTP headers - FIXED FORMAT
    _client->print("POST /bot");
    _client->print(_token);
    _client->print("/sendAudio HTTP/1.1\r\n");
    _client->print("Host: ");
    _client->print(TELEGRAM_HOST);
    _client->print("\r\n");
    _client->print("User-Agent: ESP32-TelegramBot/1.0\r\n");
    _client->print("Accept: */*\r\n");
    _client->print("Content-Type: multipart/form-data; boundary=");
    _client->print(boundary);
    _client->print("\r\n");
    _client->print("Content-Length: ");
    _client->print(contentLength);
    _client->print("\r\n");
    _client->print("Connection: close\r\n");
    _client->print("\r\n");
    
    _client->print(formHeader);
    
    // Stream file data in chunks
    uint8_t buffer[BUFFER_SIZE];
    size_t totalSent = 0;
    
    while (totalSent < fileSize) {
        size_t toRead = (fileSize - totalSent > BUFFER_SIZE) ? BUFFER_SIZE : (fileSize - totalSent);
        size_t bytesRead = file.read(buffer, toRead);
        
        if (bytesRead == 0) {
            setError("File read error");
            closeConnection();
            return false;
        }
        
        _client->write(buffer, bytesRead);
        totalSent += bytesRead;
        
        // Print progress every 10KB
        if (totalSent % 10240 == 0 || totalSent == fileSize) {
            Serial.print("Uploaded: ");
            Serial.print(totalSent);
            Serial.print(" / ");
            Serial.print(fileSize);
            Serial.print(" bytes (");
            Serial.print((totalSent * 100) / fileSize);
            Serial.println("%)");
        }
        
        yield(); // Allow system to breathe
    }
    
    _client->print(formFooter);
    
    String response;
    if (!readResponse(response)) {
        closeConnection();
        return false;
    }
    
    closeConnection();
    return parseJsonResponse(response);
}

bool SimpleTelegramBot::ensureConnection() {
    // Close existing connection if any
    if (_client->connected()) {
        _client->stop();
        delay(100);  // Give it time to close
    }
    
    Serial.print("Connecting to ");
    Serial.print(TELEGRAM_HOST);
    Serial.print(":");
    Serial.println(TELEGRAM_SSL_PORT);
    
    // Try to connect with timeout
    unsigned long startTime = millis();
    bool connected = false;
    
    while (millis() - startTime < 10000) {  // 10 second timeout
        if (_client->connect(TELEGRAM_HOST, TELEGRAM_SSL_PORT)) {
            connected = true;
            Serial.println("✓ Connected to Telegram!");
            break;
        }
        delay(100);
    }
    
    if (!connected) {
        setError("Connection failed - check WiFi and certificate");
        Serial.println("✗ Connection failed!");
        return false;
    }
    
    return true;
}

void SimpleTelegramBot::closeConnection() {
    if (_client && _client->connected()) {
        _client->stop();
    }
}

bool SimpleTelegramBot::sendGetRequest(const String& endpoint, const String& params) {
    if (!ensureConnection()) {
        return false;
    }
    
    // FIXED: Better HTTP request format
    _client->print("GET /bot");
    _client->print(_token);
    _client->print("/");
    _client->print(endpoint);
    _client->print("?");
    _client->print(params);
    _client->print(" HTTP/1.1\r\n");
    _client->print("Host: ");
    _client->print(TELEGRAM_HOST);
    _client->print("\r\n");
    _client->print("Accept: application/json\r\n");
    _client->print("Cache-Control: no-cache\r\n");
    _client->print("Connection: close\r\n");
    _client->print("\r\n");
    
    String response;
    if (!readResponse(response)) {
        closeConnection();
        return false;
    }
    
    closeConnection();
    return parseJsonResponse(response);
}

bool SimpleTelegramBot::sendMultipartFormData(const String& endpoint, const String& fieldName,
                                              const String& filename, const String& contentType,
                                              const uint8_t* data, size_t dataSize,
                                              bool (*hasMoreData)(), uint8_t (*getNextByte)()) {
    if (!ensureConnection()) {
        return false;
    }
    
    String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
    
    // Build form data header
    String formHeader = "--" + boundary + "\r\n";
    formHeader += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
    formHeader += _chatId + "\r\n";
    formHeader += "--" + boundary + "\r\n";
    formHeader += "Content-Disposition: form-data; name=\"" + fieldName + "\"; filename=\"" + filename + "\"\r\n";
    formHeader += "Content-Type: " + contentType + "\r\n\r\n";
    
    String formFooter = "\r\n--" + boundary + "--\r\n";
    
    size_t contentLength = formHeader.length() + dataSize + formFooter.length();
    
    // Send HTTP headers - FIXED FORMAT
    _client->print("POST /bot");
    _client->print(_token);
    _client->print("/");
    _client->print(endpoint);
    _client->print(" HTTP/1.1\r\n");
    _client->print("Host: ");
    _client->print(TELEGRAM_HOST);
    _client->print("\r\n");
    _client->print("Content-Type: multipart/form-data; boundary=");
    _client->print(boundary);
    _client->print("\r\n");
    _client->print("Content-Length: ");
    _client->print(contentLength);
    _client->print("\r\n");
    _client->print("Connection: close\r\n");
    _client->print("\r\n");
    
    _client->print(formHeader);
    
    // Send audio data
    if (data != nullptr) {
        // Buffer mode
        size_t sent = 0;
        while (sent < dataSize) {
            size_t chunk = (dataSize - sent > BUFFER_SIZE) ? BUFFER_SIZE : (dataSize - sent);
            _client->write(data + sent, chunk);
            sent += chunk;
            yield(); // Allow system to breathe
        }
    } else if (hasMoreData != nullptr && getNextByte != nullptr) {
        // Stream mode
        uint8_t buffer[BUFFER_SIZE];
        size_t bufferIndex = 0;
        
        while (hasMoreData()) {
            buffer[bufferIndex++] = getNextByte();
            
            if (bufferIndex >= BUFFER_SIZE) {
                _client->write(buffer, BUFFER_SIZE);
                bufferIndex = 0;
                yield();
            }
        }
        
        // Send remaining bytes
        if (bufferIndex > 0) {
            _client->write(buffer, bufferIndex);
        }
    }
    
    _client->print(formFooter);
    
    String response;
    if (!readResponse(response)) {
        closeConnection();
        return false;
    }
    
    closeConnection();
    return parseJsonResponse(response);
}

bool SimpleTelegramBot::readResponse(String& response) {
    unsigned long startTime = millis();
    bool headersParsed = false;
    bool responseStarted = false;
    
    response = "";
    String headerBuffer = "";
    
    while (millis() - startTime < RESPONSE_TIMEOUT) {
        if (_client->available()) {
            responseStarted = true;
            char c = _client->read();
            
            if (!headersParsed) {
                // Skip headers - look for \r\n\r\n
                headerBuffer += c;
                
                if (headerBuffer.endsWith("\r\n\r\n")) {
                    headersParsed = true;
                    headerBuffer = "";
                    
                    Serial.println("Headers parsed, reading body...");
                }
                
                // Prevent header buffer overflow
                if (headerBuffer.length() > 2048) {
                    headerBuffer = headerBuffer.substring(1024);
                }
            } else {
                // Read body
                response += c;
            }
        } else if (responseStarted && headersParsed && response.length() > 0) {
            // Response complete
            delay(10);  // Small delay to ensure we got everything
            if (!_client->available()) {
                break;
            }
        }
        
        yield();
    }
    
    if (!responseStarted) {
        setError("No response from server");
        Serial.println("✗ No response received");
        return false;
    }
    
    if (!headersParsed) {
        setError("Failed to parse headers");
        Serial.println("✗ Failed to parse headers");
        return false;
    }
    
    Serial.print("Response received: ");
    Serial.print(response.length());
    Serial.println(" bytes");
    
    return true;
}

bool SimpleTelegramBot::parseJsonResponse(const String& response) {
    // Simple JSON parsing - look for "ok":true
    int okIndex = response.indexOf("\"ok\"");
    if (okIndex == -1) {
        setError("Invalid JSON response");
        Serial.println("✗ Invalid JSON response");
        Serial.println(response.substring(0, 200));  // Print first 200 chars
        return false;
    }
    
    int trueIndex = response.indexOf("true", okIndex);
    int falseIndex = response.indexOf("false", okIndex);
    
    if (trueIndex != -1 && (falseIndex == -1 || trueIndex < falseIndex)) {
        _lastError = "";
        Serial.println("✓ Request successful");
        return true;
    }
    
    // Extract error description if available
    int descIndex = response.indexOf("\"description\"");
    if (descIndex != -1) {
        int startQuote = response.indexOf("\"", descIndex + 14);
        int endQuote = response.indexOf("\"", startQuote + 1);
        if (startQuote != -1 && endQuote != -1) {
            _lastError = response.substring(startQuote + 1, endQuote);
            Serial.print("✗ Telegram error: ");
            Serial.println(_lastError);
            return false;
        }
    }
    
    setError("Request failed");
    Serial.println("✗ Request failed");
    return false;
}

String SimpleTelegramBot::urlEncode(const String& str) {
    String encoded = "";
    char c;
    
    for (size_t i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else if (c == ' ') {
            encoded += '+';
        } else {
            encoded += '%';
            char hex[3];
            sprintf(hex, "%02X", (unsigned char)c);
            encoded += hex;
        }
    }
    
    return encoded;
}

void SimpleTelegramBot::setError(const String& error) {
    _lastError = error;
}