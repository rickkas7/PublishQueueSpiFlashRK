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
    
    return circBuffer->load();
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

    size_t size = strlen(eventName) + strlen(data) + 20;
    char *buf = (char *)dataBuffer.allocate(size);

    memset(buf, 0, size);
    JSONBufferWriter writer(buf, size);

    writer.beginObject();
    writer.name("n").value(eventName);
    writer.name("d").value(data);
    writer.name("f").value((int)(uint8_t)flags);
    writer.endObject();

    dataBuffer.truncate(strlen(buf) + 1);


    return false;
}


void PublishQueueSpiFlashRK::publishCompleteCallback(bool succeeded, const char *eventName, const char *eventData) {
    publishComplete = true;
    publishSuccess = succeeded;

    if (publishCompleteUserCallback) {
        publishCompleteUserCallback(succeeded, eventName, eventData);
    }
}


size_t PublishQueueSpiFlashRK::getNumEvents() {
    
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
    
    curFileNum = fileQueue.getFileFromQueue(false);
    if (curFileNum) {
        curEvent = readQueueFile(curFileNum);
        if (!curEvent) {
            // Probably a corrupted file, discard
            _log.info("discarding corrupted file %d", curFileNum);
            fileQueue.getFileFromQueue(true);
            fileQueue.removeFileNum(curFileNum, false);
        }
    }
    else {
        if (!ramQueue.empty()) {
            curEvent = ramQueue.front();
            ramQueue.pop_front();
        }
        else {
            curEvent = NULL;
        }
    }

    if (curEvent) {
        stateTime = millis();
        stateHandler = &PublishQueueSpiFlashRK::statePublishWait;
        publishComplete = false;
        publishSuccess = false;
        canSleep = false;

        // This message is monitored by the automated test tool. If you edit this, change that too.
        _log.trace("publishing %s event=%s data=%s", (curFileNum ? "file" : "ram"), curEvent->eventName, curEvent->eventData);

        if (BackgroundPublishRK::instance().publish(curEvent->eventName, curEvent->eventData, curEvent->flags, 
            [this](bool succeeded, const char *eventName, const char *eventData, const void *context) {
                publishCompleteCallback(succeeded, eventName, eventData);
            })) {
            // Successfully started publish
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
        _log.trace("publish success %d", curFileNum);

        if (curFileNum) {
            // Was from the file-based queue
            int fileNum = fileQueue.getFileFromQueue(false);
            if (fileNum == curFileNum) {
                fileQueue.getFileFromQueue(true);
                fileQueue.removeFileNum(fileNum, false);
                _log.trace("removed file %d", fileNum);
            }
            curFileNum = 0;
        }

        delete curEvent;
        curEvent = NULL;
        durationMs = waitBetweenPublish;
    }
    else {
        // Wait and retry
        // This message is monitored by the automated test tool. If you edit this, change that too.
        _log.trace("publish failed %d", curFileNum);
        durationMs = waitAfterFailure;

        if (curFileNum) {
            // Was from the file-based queue
            delete curEvent;
            curEvent = NULL;
        }
        else {
            // Was in the RAM-based queue, put back
            WITH_LOCK(*this) {
                ramQueue.push_front(curEvent);
            }
            // Then write the entire queue to files
            _log.trace("writing to files after publish failure");
            writeQueueToFiles();
        }
    }

    stateHandler = &PublishQueueSpiFlashRK::stateWait;
    stateTime = millis();
}
