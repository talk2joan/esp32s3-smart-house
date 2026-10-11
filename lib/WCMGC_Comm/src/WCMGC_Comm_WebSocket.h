#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

/* Lightweight WebSocket client (implementation lives in the precompiled library). */
enum WStype_t {
    WStype_DISCONNECTED,
    WStype_CONNECTED,
    WStype_TEXT,
    WStype_BIN
};

class WCMGC_LightWsClient {
public:
    typedef void (*EventCallback)(WStype_t type, uint8_t* payload, size_t length);

    WCMGC_LightWsClient();
    ~WCMGC_LightWsClient();

    void onEvent(EventCallback cb);
    void setReconnectInterval(uint32_t);
    void enableHeartbeat(uint32_t pingIntervalMs, uint32_t pongTimeoutMs, uint8_t missedPongs);

    /*
     * Extra headers are one-shot: the value is consumed by the next connection
     * attempt and is never reused after that attempt fails. This matters for
     * timestamped/HMAC Upgrade requests. Header names which could alter the
     * WebSocket handshake are rejected.
     */
    void setExtraHeaders(const String& headers);
    void setExtraHeaders(const char* headers);
    void clearExtraHeaders();

    /* TLS is authenticated by default. Insecure TLS must be an explicit opt-in. */
    void setCACert(const char* rootCA);
    void setInsecure(bool enabled = true);

    /* Inbound frame/message and HTTP response-header limits. */
    void setMaxFrameSize(size_t bytes);
    void setMaxMessageSize(size_t bytes);
    void setMaxResponseHeaderSize(size_t bytes);

    void begin(const char* h, int p, const char* u);
    void beginSSL(const char* h, int p, const char* u);
    void begin(const char* h, int p, const char* u, const char* extraHeaders);
    void beginSSL(const char* h, int p, const char* u, const char* extraHeaders);

    bool sendTXT(const String& text);
    bool sendTXT(const char* text);
    bool sendBIN(const uint8_t* data, size_t len);
    void loop();
    bool isConnected() const;
    void disconnect();

    /* 0 means no HTTP response (DNS/TCP/TLS failure); otherwise status line code. */
    int lastHandshakeStatus() const;
    int lastTransportError() const;
    const char* lastTransportErrorText() const;

    /* Time since the most recent successfully sent or received WebSocket frame. */
    uint32_t idleMs() const;

private:
    enum FrameReadResult {
        FRAME_NEED_MORE,
        FRAME_COMPLETE,
        FRAME_CLOSED
    };

    WCMGC_LightWsClient(const WCMGC_LightWsClient&) = delete;
    WCMGC_LightWsClient& operator=(const WCMGC_LightWsClient&) = delete;

    WiFiClient plainClient;
    WiFiClientSecure secureClient;
    Client* client = nullptr;
    EventCallback callback = nullptr;

    String host;
    String path;
    String tlsCaCert;
    String pendingExtraHeaders;
    int port = 0;
    bool ssl = false;
    bool tlsInsecure = false;
    bool connected = false;
    bool connectPending = false;
    int handshakeStatus = 0;
    int transportError = 0;
    char transportErrorText[160] = {0};

    uint32_t heartbeatIntervalMs = 0;
    uint32_t heartbeatTimeoutMs = 0;
    uint32_t lastActivityMs = 0;
    uint32_t lastPingSentMs = 0;
    bool awaitingPong = false;
    uint8_t pendingPingPayload[4] = {0, 0, 0, 0};
    size_t pendingPingLength = 0;
    uint8_t heartbeatMaxMissedPongs = 1;
    uint8_t heartbeatMissedPongs = 0;

    bool inLoop = false;
    bool inCallback = false;
    bool deferredDisconnectEvent = false;

    size_t maxFramePayloadBytes = 64U * 1024U;
    size_t maxMessagePayloadBytes = 64U * 1024U;
    size_t maxResponseHeaderBytes = 8U * 1024U;
    size_t maxResponseLineBytes = 2048U;

    uint8_t* rxMessage = nullptr;
    size_t rxCapacity = 0;
    size_t rxLength = 0;
    uint8_t rxOpcode = 0;
    bool rxFragmented = false;

    uint8_t rxWireHeader[10] = {0};
    size_t rxWireHeaderRead = 0;
    size_t rxWireHeaderNeeded = 2;
    bool rxFramePrepared = false;
    bool rxFrameFin = false;
    bool rxFrameControl = false;
    uint8_t rxFrameOpcode = 0;
    size_t rxFramePayloadLength = 0;
    size_t rxFramePayloadRead = 0;
    size_t rxFrameWriteOffset = 0;
    uint8_t rxControlPayload[125] = {0};

    void beginInternal(bool useSsl, const char* h, int p, const char* u,
                       const char* headers, bool replaceHeaders);
    void resetSecureClient();
    bool connectNow();
    void failConnect();
    void closeClient(bool notify);
    void loopInternal();
    bool sendHeartbeatPing(uint32_t now);
    void dispatchEvent(WStype_t type, uint8_t* payload, size_t length);
    void flushDeferredEvents();

    bool appendExtraHeaders(String& request, const String& headers) const;
    bool readHttpLine(String& line, size_t& totalBytes, uint32_t startedAt);
    bool validateHandshake(const String& expectedAccept, uint32_t startedAt);

    bool writeAll(const uint8_t* data, size_t len);
    bool sendFrame(uint8_t opcode, const uint8_t* data, size_t len);
    FrameReadResult readFrame();
    bool prepareRxFrame();
    FrameReadResult finishRxFrame();
    bool ensureRxCapacity(size_t required);
    void resetFrameParser();
    void resetRxMessage();
    void protocolClose(uint16_t code);
};
