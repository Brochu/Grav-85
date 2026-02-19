#pragma once
#include "shared_types.hpp"
#include "qg_memory.hpp"

// Define your event types here
enum class event_type : u16 {
    NONE = 0,

    // ENGINE EVENTS (0-499)
    // These are reserved for engine-level events
    PERF_FRAME_STAT,
    PERF_MEMORY_STAT,
    PERF_BUDGET_EXCEEDED,

    RENDER_RESOLUTION_CHANGED,
    RENDER_BACKEND_LOST,
    RENDER_BACKEND_RESTORED,

    ASSET_LOADED,
    ASSET_UNLOADED,

    AUDIO_REQUEST_PLAY,

    // Add more engine event types as needed (up to 499)
    ENGINE_RESERVED_END = 499,
    GAME_EVENTS_START = 512,

    // GAME EVENTS (500-2047)
    // Games can define their own event types starting from here
    // Example in game code:
    //   enum class game_event : u16 {
    //       QUEST_COMPLETED = (u16)event_type::GAME_EVENTS_START,
    //       DIALOGUE_STARTED,
    //       MERCHANT_OPENED,
    //       // ... more game events
    //   };

    COUNT = 1024  // Total capacity for all event types
};

// ===== ENGINE EVENT DATA STRUCTURES =====
// Define the data structures for engine-provided events here

struct perf_frame_stats_event {
    f32 delta_time;
    f32 fps;
};

struct perf_memory_stats_event {
    u64 used_bytes;
    u64 peak_bytes;
    u64 capacity_bytes;
};

struct perf_budget_exceeded_event {
    const char *budget_name; // Needs to be static litteral
    f32 current_value;
    f32 budget_value;
};

struct render_resolution_changed_event {
    u32 old_width;
    u32 old_height;

    u32 new_width;
    u32 new_height;
};
// No specific data to send for RENDER_BACKEND_LOST and RENDER_BACKEND_RESTORED

struct asset_loaded_event {
    u32 asset_id;
};

struct asset_loaded_unevent {
    u32 asset_id;
};

struct audio_request_play_event {
    u32 sound_id;
    u32 emitter_id;

    f32 volume;
    f32 pitch;
};

// Add more engine event data structures as needed

// ===== EVENT HANDLER =====

// Event handler signature: receives event type and data pointer
typedef void (*event_handler_fn)(event_type type, void* data, void* user_data);

// Handler ID that encodes generation, slot index, and event type
struct handler_id {
    union {
        u64 packed;
        struct {
            u32 generation;  // Must match slot's generation to be valid
            u16 slot_idx;    // Which slot in the handlers array
            u16 type_idx;    // Event type index
        };
    };
};

#define INVALID_HANDLER_ID (handler_id{0})

#define BUS_MAX_HANDLERS_PER_TYPE 16
#define BUS_MAX_EVENTS_PER_FRAME 512  // Must be power of 2!
#define BUS_EVENT_MASK (BUS_MAX_EVENTS_PER_FRAME - 1)

// Safety limit to prevent infinite event loops
#define BUS_MAX_EVENTS_PER_PROCESS (BUS_MAX_EVENTS_PER_FRAME * 2)

struct event_entry {
    event_type type;
    arena_ptr data;
    u32 data_size;
};

struct event_handler {
    event_handler_fn fn;
    void* user_data;
    u32 generation;  // Incremented each time this slot is reused
    bool active;
};

struct event_bus {
    // Ring buffer for event queue
    event_entry events[BUS_MAX_EVENTS_PER_FRAME];
    u32 head;  // Next event to process
    u32 tail;  // Next free slot

    // Handlers registered for each event type
    event_handler handlers[(u32)event_type::COUNT][BUS_MAX_HANDLERS_PER_TYPE];
    u32 handler_counts[(u32)event_type::COUNT];

    // Memory for event data
    mem_arena event_arena;
};

// Initialize the event bus with arena capacity
void bus_init(event_bus* bus, u64 arena_capacity);

// Cleanup
void bus_free(event_bus* bus);

// Subscribe to an event type
// Returns handler_id that can be used to unsubscribe
handler_id bus_subscribe(event_bus* bus, event_type type, event_handler_fn handler, void* user_data = nullptr);

// Unsubscribe a handler (type and slot are encoded in handler_id)
// Returns true if successfully unsubscribed, false if handler_id is invalid or stale
bool bus_unsubscribe(event_bus* bus, handler_id id);

// Fire an event (copies data into arena)
// Returns true on success, false if queue or arena is full
// Can be called from event handlers - new events will be processed in the same frame
bool bus_fire(event_bus* bus, event_type type, const void* data, u32 data_size);

// Macro helper for type-safe event firing
// Usage: bus_fire_event(&bus, event_type::PLAYER_DAMAGED, my_event_struct)
#define bus_fire_event(bus, type, event_data) \
    g_eng.bus_fire(bus, type, &(event_data), sizeof(event_data))

// Process all queued events and dispatch to handlers
// Call this at the end of each frame
// Handles recursive events - events fired during processing will be handled in the same frame
void bus_process(event_bus* bus);

// Clear all events and reset arena (called automatically by bus_process)
void bus_reset(event_bus* bus);
