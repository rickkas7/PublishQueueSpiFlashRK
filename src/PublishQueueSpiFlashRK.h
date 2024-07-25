#ifndef __PUBLISHQUEUESPIFLASHRK_H
#define __PUBLISHQUEUESPIFLASHRK_H

#include "Particle.h"

#include "CircularBufferSpiFlashRK.h"

/**
 * This class is a singleton; you do not create one as a global, on the stack, or with new.
 * 
 * From global application setup you must call:
 * PublishQueueSpiFlashRK::instance().setup();
 * 
 * From global application loop you must call:
 * PublishQueueSpiFlashRK::instance().loop();
 */
class PublishQueueSpiFlashRK {
public:
    /**
     * @brief Gets the singleton instance of this class, allocating it if necessary
     * 
     * Use PublishQueueSpiFlashRK::instance() to instantiate the singleton.
     */
    static PublishQueueSpiFlashRK &instance();


    /**
     * @brief 
     * 
     * @param spiFlash The SpiFlashRK object for the SPI NOR flash chip.
     * @param addrStart Address to start at (typically 0). Must be sector aligned (multiple of 4096 bytes).
     * @param addrEnd Address to end at (not inclusive). Must be sector aligned (multiple of 4096 bytes).
     * @return PublishQueueSpiFlashRK& 
     */
    PublishQueueSpiFlashRK &withSpiFlash(SpiFlash *spiFlash, size_t addrStart, size_t addrEnd);


    /**
     * @brief Adds a callback function to call with publish is complete
     * 
     * @param cb Callback function or C++ lambda.
     * @return PublishQueuePosix& 
     * 
     * The callback has this prototype and can be a function or a C++11 lambda, which allows the callback to be a class method.
     * 
     * void callback(bool succeeded, const char *eventName, const char *eventData)
     * 
     * The parameters are:
     * - succeeded: true if the publish succeeded or false if faled
     * - eventName: The original event name that was published (a copy of it, not the original pointer)
     * - eventData: The original event data
     * 
     * Note that this callback will be called from the background thread used for publishing. You should not
     * perform any lengthy operations and you should avoid using large amounts of stack space during this
     * callback. 
     */
    PublishQueueSpiFlashRK &withPublishCompleteUserCallback(std::function<void(bool succeeded, const char *eventName, const char *eventData)> cb) { publishCompleteUserCallback = cb; return *this; };



    /**
     * @brief Perform setup operations; call this from global application setup()
     * 
     * You typically use PublishQueueSpiFlashRK::instance().setup();
     */
    bool setup();

    /**
     * @brief Perform application loop operations; call this from global application loop()
     * 
     * You typically use PublishQueueSpiFlashRK::instance().loop();
     */
    void loop();

	/**
	 * @brief Overload for publishing an event
	 *
	 * @param eventName The name of the event (63 character maximum).
	 *
	 * @param flags1 Normally PRIVATE. You can also use PUBLIC, but one or the other must be specified.
	 *
	 * @param flags2 (optional) You can use NO_ACK or WITH_ACK if desired.
	 *
	 * @return true if the event was queued or false if it was not.
	 *
	 * This function almost always returns true. If you queue more events than fit in the buffer the
	 * oldest (sometimes second oldest) is discarded.
	 */
	inline bool publish(const char *eventName, PublishFlags flags1, PublishFlags flags2 = PublishFlags()) {
		return publishCommon(eventName, "", 60, flags1, flags2);
	}

	/**
	 * @brief Overload for publishing an event
	 *
	 * @param eventName The name of the event (63 character maximum).
	 *
	 * @param data The event data (255 bytes maximum, 622 bytes in system firmware 0.8.0-rc.4 and later).
	 *
	 * @param flags1 Normally PRIVATE. You can also use PUBLIC, but one or the other must be specified.
	 *
	 * @param flags2 (optional) You can use NO_ACK or WITH_ACK if desired.
	 *
	 * @return true if the event was queued or false if it was not.
	 *
	 * This function almost always returns true. If you queue more events than fit in the buffer the
	 * oldest (sometimes second oldest) is discarded.
	 */
	inline bool publish(const char *eventName, const char *data, PublishFlags flags1, PublishFlags flags2 = PublishFlags()) {
		return publishCommon(eventName, data, 60, flags1, flags2);
	}

	/**
	 * @brief Overload for publishing an event
	 *
	 * @param eventName The name of the event (63 character maximum).
	 *
	 * @param data The event data (255 bytes maximum, 622 bytes in system firmware 0.8.0-rc.4 and later).
	 *
	 * @param ttl The time-to-live value. If not specified in one of the other overloads, the value 60 is
	 * used. However, the ttl is ignored by the cloud, so it doesn't matter what you set it to. Essentially
	 * all events are discarded immediately if not subscribed to so they essentially have a ttl of 0.
	 *
	 * @param flags1 Normally PRIVATE. You can also use PUBLIC, but one or the other must be specified.
	 *
	 * @param flags2 (optional) You can use NO_ACK or WITH_ACK if desired.
	 *
	 * @return true if the event was queued or false if it was not.
	 *
	 * This function almost always returns true. If you queue more events than fit in the buffer the
	 * oldest (sometimes second oldest) is discarded.
	 */
	inline bool publish(const char *eventName, const char *data, int ttl, PublishFlags flags1, PublishFlags flags2 = PublishFlags()) {
		return publishCommon(eventName, data, ttl, flags1, flags2);
	}

	/**
	 * @brief Common publish function. All other overloads lead here. This is a pure virtual function, implemented in subclasses.
	 *
	 * @param eventName The name of the event (63 character maximum).
	 *
	 * @param data The event data (255 bytes maximum, 622 bytes in system firmware 0.8.0-rc.4 and later).
	 *
	 * @param ttl The time-to-live value. If not specified in one of the other overloads, the value 60 is
	 * used. However, the ttl is ignored by the cloud, so it doesn't matter what you set it to. Essentially
	 * all events are discarded immediately if not subscribed to so they essentially have a ttl of 0.
	 *
	 * @param flags1 Normally PRIVATE. You can also use PUBLIC, but one or the other must be specified.
	 *
	 * @param flags2 (optional) You can use NO_ACK or WITH_ACK if desired.
	 *
	 * @return true if the event was queued or false if it was not.
	 *
	 * This function almost always returns true. If you queue more events than fit in the buffer the
	 * oldest (sometimes second oldest) is discarded.
	 */
	virtual bool publishCommon(const char *eventName, const char *data, int ttl, PublishFlags flags1, PublishFlags flags2 = PublishFlags());

    /**
     * @brief Empty both the RAM and file based queues. Any queued events are discarded. 
     */
    void clearQueues();

    /**
     * @brief Pause or resume publishing events
     * 
     * @param value The value to set, true = pause, false = normal operation
     * 
     * If called while a publish is in progress, that publish will still proceed, but
     * the next event (if any) will not be attempted.
     * 
     * This is used by the automated test tool; you probably won't need to manually
     * manage this under normal circumstances.
     */
    void setPausePublishing(bool value);

    /**
     * @brief Gets the state of the pause publishing flag
     */
    bool getPausePublishing() const { return pausePublishing; };

    /**
     * @brief Determine if it's a good time to go to sleep
     * 
     * If a publish is not in progress and the queue is empty, returns true. 
     * 
     * If pausePublishing is true, then return true if either the current publish has
     * completed, or not cloud connected.
     */
    bool getCanSleep() const { return canSleep; };

    /**
     * @brief Gets the total number of events queued
     * 
     * This is the number of events in the RAM-based queue and the file-based
     * queue. This operation is fast; the file queue length is stored in RAM,
     * so this command does not need to access the file system.
     * 
     * If an event is currently being sent, the result includes this event.
     */
    size_t getNumEvents();

    /**
     * @brief Locks the mutex that protects shared resources
     * 
     * This is compatible with `WITH_LOCK(*this)`.
     * 
     * The mutex is not recursive so do not lock it within a locked section.
     */
    void lock() { os_mutex_recursive_lock(mutex); };

    /**
     * @brief Attempts to lock the mutex that protects shared resources
     * 
     * @return true if the mutex was locked or false if it was busy already.
     */
    bool tryLock() { return os_mutex_recursive_trylock(mutex); };

    /**
     * @brief Unlocks the mutex that protects shared resources
     */
    void unlock() { os_mutex_recursive_unlock(mutex); };


protected:
    /**
     * @brief The constructor is protected because the class is a singleton
     * 
     * Use PublishQueueSpiFlashRK::instance() to instantiate the singleton.
     */
    PublishQueueSpiFlashRK();

    /**
     * @brief The destructor is protected because the class is a singleton and cannot be deleted
     */
    virtual ~PublishQueueSpiFlashRK();

    /**
     * This class is a singleton and cannot be copied
     */
    PublishQueueSpiFlashRK(const PublishQueueSpiFlashRK&) = delete;

    /**
     * This class is a singleton and cannot be copied
     */
    PublishQueueSpiFlashRK& operator=(const PublishQueueSpiFlashRK&) = delete;


    /**
     * @brief Callback for BackgroundPublishRK library
     */
    void publishCompleteCallback(bool succeeded, const char *eventName, const char *eventData);

    /**
     * @brief State handler for waiting to connect to the Particle cloud
     * 
     * Next state: stateWait
     */
    void stateConnectWait();

    /**
     * @brief State handler for waiting to publish
     * 
     * stateTime and durationMs determine whether to stay in this state waiting, or whether
     * to publish and go into statePublishWait.
     * 
     * Next state: statePublishWait or stateConnectWait
     */
    void stateWait();

    /**
     * @brief State handler for waiting for publish to complete
     * 
     * Next state: stateWait
     */
    void statePublishWait();
    /**
     * @brief Mutex to protect shared resources
     * 
     * This is initialized in setup() so make sure you call the setup() method from the global application setup.
     */
    os_mutex_recursive_t mutex = 0;

    SpiFlash *spiFlash = nullptr;
    size_t addrStart = 0;
    size_t addrEnd = 0;
    CircularBufferSpiFlashRK *circBuffer = nullptr;

    unsigned long stateTime = 0; //!< millis() value when entering the state, used for stateWait
    unsigned long durationMs = 0; //!< how long to wait before publishing in milliseconds, used in stateWait
    bool publishComplete = false; //!< true if the publish has completed (successfully or not)
    bool publishSuccess = false; //!< true if the publish succeeded
    bool pausePublishing = false; //!< flag to pause publishing (used from automated test)
    bool canSleep = false; //!< returns true if this is a good time to go to sleep
    CircularBufferSpiFlashRK::ReadInfo curEvent;

    unsigned long waitAfterConnect = 2000; //!< time to wait after Particle.connected() before publishing
    unsigned long waitBetweenPublish = 1000; //!< how long to wait in milliseconds between publishes
    unsigned long waitAfterFailure = 30000; //!< how long to wait after failing to publish before trying again

    std::function<void(bool succeeded, const char *eventName, const char *eventData)> publishCompleteUserCallback = 0; //!< User callback for publish complete

    std::function<void(PublishQueueSpiFlashRK&)> stateHandler = 0; //!< state handler (stateConnectWait, stateWait, etc).


    static void systemEventHandler(system_event_t event, int param); //!< system event handler, used to detect reset events

    /**
     * @brief Singleton instance of this class
     * 
     * The object pointer to this class is stored here. It's NULL at system boot.
     */
    static PublishQueueSpiFlashRK *_instance;

};
#endif  /* __PUBLISHQUEUESPIFLASHRK_H */