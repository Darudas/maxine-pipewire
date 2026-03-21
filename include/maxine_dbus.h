// D-Bus interface for maxine-pipewire daemon
// Bus name: org.maxine.Effects
// Object path: /org/maxine/Effects

#pragma once

#include <systemd/sd-bus.h>
#include <stdbool.h>

struct maxine_audio_chain;  // forward decl

struct maxine_dbus {
    sd_bus *bus;
    sd_bus_slot *slot;
    struct maxine_audio_chain *chain;    // not owned
    char gpu_name[256];
    char gpu_arch[32];
    bool running;
};

#define MAXINE_DBUS_NAME  "org.maxine.Effects"
#define MAXINE_DBUS_PATH  "/org/maxine/Effects"
#define MAXINE_DBUS_IFACE "org.maxine.Effects"

// Initialize D-Bus service. chain must remain valid for lifetime of dbus.
// Returns 0 on success.
int maxine_dbus_init(struct maxine_dbus *dbus, struct maxine_audio_chain *chain);

// Get the sd_bus file descriptor for integration with PipeWire's event loop.
int maxine_dbus_get_fd(struct maxine_dbus *dbus);

// Process pending D-Bus messages. Call this when the fd is readable.
int maxine_dbus_dispatch(struct maxine_dbus *dbus);

// Clean up D-Bus resources.
void maxine_dbus_destroy(struct maxine_dbus *dbus);
