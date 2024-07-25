#include "PublishQueueSpiFlashRK.h"
#include "BackgroundPublishRK.h"

PublishQueueSpiFlashRK *PublishQueueSpiFlashRK::_instance;

static Logger _log("app.pubq");

// [static]
PublishQueueSpiFlashRK &PublishQueueSpiFlashRK::instance() {
    if (!_instance) {
        _instance = new PublishQueueSpiFlashRK();
    }
    return *_instance;
}

PublishQueueSpiFlashRK::PublishQueueSpiFlashRK() {
}

PublishQueueSpiFlashRK::~PublishQueueSpiFlashRK() {
}

PublishQueueSpiFlashRK &PublishQueueSpiFlashRK::withSpiFlash(SpiFlash *spiFlash, size_t addrStart, size_t addrEnd) {
    this->spiFlash = spiFlash;
    this->addrStart = addrStart;
    this->addrEnd = addrEnd;
    return *this;
}


bool PublishQueueSpiFlashRK::setup() {
    if (system_thread_get_state(nullptr) != spark::feature::ENABLED) {
        _log.error("SYSTEM_THREAD(ENABLED) is required");
        return false;
    }
    if (!spiFlash) {
        _log.error("spiFlash is not set");
        return false;
    }

    os_mutex_recursive_create(&mutex);

    // Register a system reset handler
    System.on(reset | cloud_status, systemEventHandler);

    // Start the background publish thread
    BackgroundPublishRK::instance().start();

    stateHandler = &PublishQueueSpiFlashRK::stateConnectWait;

    circBuffer = new CircularBufferSpiFlashRK(spiFlash, addrStart, addrEnd);
    
    bool bResult = circBuffer->load();
    if (!bResult) {
        bResult = circBuffer->format();
    }
    if (!bResult) {
        _log.error("circular buffer not initialized");
    }

    return bResult;
}

void PublishQueueSpiFlashRK::loop() {
    if (stateHandler) {
        stateHandler(*this);
    }
}


bool PublishQueueSpiFlashRK::publishCommon(const char *eventName, const char *data, int ttl, PublishFlags flags1, PublishFlags flags2) {
    PublishFlags flags = flags1 | flags2;

    if (!data) { 
        data = "";
    }

    CircularBufferSpiFlashRK::DataBuffer dataBuffer;

    size_t size = strlen(eventName) + strlen(data) + 64;
    char *buf = (char *)dataBuffer.allocate(size);

    memset(buf, 0, size);
    JSONBufferWriter writer(buf, size);

    writer.beginObject();
    writer.name("n").value(eventName);
    writer.name("d").value(data);
    writer.name("NO_ACK").value((flags.value() & NO_ACK.value()) != 0);
    writer.name("WITH_ACK").value((flags.value() & WITH_ACK.value()) != 0);
    writer.endObject();

    dataBuffer.truncate(strlen(buf) + 1);

    bool bResult = circBuffer->writeData(dataBuffer);
    if (bResult) {
        _log.trace("event %s queued", eventName);
    }
    else {
        _log.error("event %s not queued", eventName);
    }

    _log.print(buf);
    _log.print("\n");

    return bResult;
}


void PublishQueueSpiFlashRK::publishCompleteCallback(bool succeeded, const char *eventName, const char *eventData) {
    publishComplete = true;
    publishSuccess = succeeded;

    if (publishCompleteUserCallback) {
        publishCompleteUserCallback(succeeded, eventName, eventData);
    }
}


size_t PublishQueueSpiFlashRK::getNumEvents() {
    size_t numEvents = 0;

    CircularBufferSpiFlashRK::UsageStats stats;
    if (circBuffer->getUsageStats(stats)) {
        numEvents = stats.recordCount;
    }
    return numEvents;
}


void PublishQueueSpiFlashRK::clearQueues() {
    WITH_LOCK(*this) {
        circBuffer->format();
    }

    _log.trace("clearQueues");
}

void PublishQueueSpiFlashRK::setPausePublishing(bool value) { 
    pausePublishing = value; 

    if (!value) {
        // When resuming publishing, update the canSleep flag
        if (getNumEvents() != 0) {
            canSleep = false;
        }
    }
}


void PublishQueueSpiFlashRK::stateConnectWait() {
    canSleep = (pausePublishing || getNumEvents() == 0);

    if (Particle.connected()) {
        stateTime = millis();
        durationMs = waitAfterConnect;
        stateHandler = &PublishQueueSpiFlashRK::stateWait;
    }
}


void PublishQueueSpiFlashRK::stateWait() {
    if (!Particle.connected()) {
        stateHandler = &PublishQueueSpiFlashRK::stateConnectWait;
        return;
    }

    if (pausePublishing) {
        canSleep = true;
        return;
    }

    if (millis() - stateTime < durationMs) {
        canSleep = (getNumEvents() == 0);
        return;
    }
    
    if (circBuffer->readData(curEvent)) {
        _log.trace("got event from queue %s", curEvent.c_str());

        stateTime = millis();
        stateHandler = &PublishQueueSpiFlashRK::statePublishWait;
        publishComplete = false;
        publishSuccess = false;
        canSleep = false;

        String eventName;
        String eventData;
        PublishFlags eventFlags;

        JSONValue outerObj = JSONValue::parseCopy(curEvent.c_str());

        JSONObjectIterator iter(outerObj);
        while(iter.next()) {
            if (iter.name() == "n") {
                eventName = iter.value().toString().data();
            }
            else
            if (iter.name() == "d") {
                eventData = iter.value().toString().data();
            }
            else
            if (iter.name() == "NO_ACK" && iter.value().toBool()) {
                eventFlags |= NO_ACK;
            }
            else
            if (iter.name() == "WITH_ACK" && iter.value().toBool()) {
                eventFlags |= WITH_ACK;
            }
        }
        if (eventName.length()) {
            // This message is monitored by the automated test tool. If you edit this, change that too.
            _log.trace("publishing event=%s data=%s", eventName.c_str(), eventData.c_str());

            if (BackgroundPublishRK::instance().publish(eventName, eventData, eventFlags, 
                [this](bool succeeded, const char *eventName, const char *eventData, const void *context) {
                    publishCompleteCallback(succeeded, eventName, eventData);
                })) {
                // Successfully started publish
            }
        }
        else {
            // Invalid event
            _log.error("invalid event, no event name, discarding");
            circBuffer->markAsRead(curEvent);

            durationMs = waitAfterFailure;
            stateHandler = &PublishQueueSpiFlashRK::stateWait;
            stateTime = millis();

            canSleep = true;
        }
    }
    else {
        // No events, can sleep
        canSleep = true;
    }
}
void PublishQueueSpiFlashRK::statePublishWait() {
    if (!publishComplete) {
        return;
    }

    if (publishSuccess) {
        // Remove from the queue
        _log.trace("publish success");

        circBuffer->markAsRead(curEvent);
        durationMs = waitBetweenPublish;
    }
    else {
        // Wait and retry
        // This message is monitored by the automated test tool. If you edit this, change that too.
        _log.trace("publish failed");
        durationMs = waitAfterFailure;
    }

    stateHandler = &PublishQueueSpiFlashRK::stateWait;
    stateTime = millis();
}


void PublishQueueSpiFlashRK::systemEventHandler(system_event_t event, int param) {
    if ((event == reset) || ((event == cloud_status) && (param == cloud_status_disconnecting))) {
        _log.trace("reset or disconnect event");
    }
}

