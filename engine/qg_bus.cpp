#include "qg_bus.hpp"
#include <cstring>

void bus_init(event_bus* bus, u64 arena_capacity) {
    mem_arena_init(&bus->event_arena, arena_capacity);

    bus->head = 0;
    bus->tail = 0;

    // Initialize handler arrays
    for (u32 i = 0; i < (u32)event_type::COUNT; i++) {
        bus->handler_counts[i] = 0;
        for (u32 j = 0; j < BUS_MAX_HANDLERS_PER_TYPE; j++) {
            bus->handlers[i][j].fn = nullptr;
            bus->handlers[i][j].user_data = nullptr;
            bus->handlers[i][j].generation = 0;
            bus->handlers[i][j].active = false;
        }
    }
}

void bus_free(event_bus* bus) {
    mem_arena_clear(&bus->event_arena);
}

handler_id bus_subscribe(event_bus* bus, event_type type, event_handler_fn handler, void* user_data) {
    u32 type_idx = (u32)type;

    if (type_idx >= (u32)event_type::COUNT) {
        return INVALID_HANDLER_ID;
    }

    // Find a free slot
    u32 slot = BUS_MAX_HANDLERS_PER_TYPE;
    for (u32 i = 0; i < BUS_MAX_HANDLERS_PER_TYPE; i++) {
        if (!bus->handlers[type_idx][i].active) {
            slot = i;
            break;
        }
    }

    if (slot == BUS_MAX_HANDLERS_PER_TYPE) {
        // No free slots
        return INVALID_HANDLER_ID;
    }

    // Increment generation for this slot (reuse detection)
    bus->handlers[type_idx][slot].generation++;
    u32 generation = bus->handlers[type_idx][slot].generation;

    bus->handlers[type_idx][slot].fn = handler;
    bus->handlers[type_idx][slot].user_data = user_data;
    bus->handlers[type_idx][slot].active = true;
    bus->handler_counts[type_idx]++;

    // Pack generation, slot, and type into handler_id
    handler_id id;
    id.generation = generation;
    id.slot_idx = (u16)slot;
    id.type_idx = (u16)type_idx;

    return id;
}

bool bus_unsubscribe(event_bus* bus, handler_id id) {
    if (id.packed == 0) {
        return false;
    }

    u16 type_idx = id.type_idx;
    u16 slot_idx = id.slot_idx;

    if (type_idx >= (u32)event_type::COUNT || slot_idx >= BUS_MAX_HANDLERS_PER_TYPE) {
        return false;
    }

    event_handler& handler = bus->handlers[type_idx][slot_idx];

    // Check if handler is active and generation matches
    if (!handler.active || handler.generation != id.generation) {
        return false;  // Stale handler_id or already unsubscribed
    }

    // Deactivate the handler
    handler.active = false;
    handler.fn = nullptr;
    handler.user_data = nullptr;
    // Note: we don't reset generation - it stays incremented for next reuse
    bus->handler_counts[type_idx]--;

    return true;
}

bool bus_fire(event_bus* bus, event_type type, const void* data, u32 data_size) {
    // Check if ring buffer is full
    u32 num_events = bus->tail - bus->head;
    if (num_events >= BUS_MAX_EVENTS_PER_FRAME) {
        // Ring buffer full
        return false;
    }

    // Allocate space in arena for event data
    arena_ptr event_data = mem_arena_alloc(&bus->event_arena, data_size, alignof(void*));

    if (event_data.p == nullptr) {
        // Arena is full
        return false;
    }

    // Copy event data into arena
    if (data && data_size > 0) {
        memcpy(event_data.p, data, data_size);
    }

    // Add event to ring buffer using power-of-2 masking
    u32 idx = bus->tail & BUS_EVENT_MASK;
    event_entry& entry = bus->events[idx];
    entry.type = type;
    entry.data = event_data;
    entry.data_size = data_size;

    bus->tail++;

    return true;
}

void bus_process(event_bus* bus) {
    u32 events_processed = 0;

    // Process events until head catches up with tail
    // Handlers can fire new events which will extend tail
    while (bus->head < bus->tail) {
        // Safety check to prevent infinite loops
        if (events_processed >= BUS_MAX_EVENTS_PER_PROCESS) {
            // TODO: Log error or assert - infinite event loop detected
            break;
        }

        // Get event using power-of-2 masking
        u32 idx = bus->head & BUS_EVENT_MASK;
        event_entry& evt = bus->events[idx];

        u16 type_idx = (u16)evt.type;

        if (type_idx < (u32)event_type::COUNT) {
            // Call all active handlers for this event type
            for (u32 j = 0; j < BUS_MAX_HANDLERS_PER_TYPE; j++) {
                event_handler& handler = bus->handlers[type_idx][j];
                if (handler.active && handler.fn) {
                    handler.fn(evt.type, evt.data.p, handler.user_data);
                }
            }
        }

        bus->head++;
        events_processed++;
    }

    // Clear events for next frame
    bus_reset(bus);
}

void bus_reset(event_bus* bus) {
    bus->head = 0;
    bus->tail = 0;
    mem_arena_reset(&bus->event_arena);
}
