#include "ui.h"
#include "config.h"
#include <string.h>
#include <wups.h>

static bool s_initialized = false;
static uint32_t s_notif_handle = 0;

void UI_Init(void) {
    if (s_initialized) return;

    s_initialized = true;
}

void UI_ShowNotification(const char *title, const char *message) {
    if (!g_config.show_notification || !s_initialized) return;

    // Aroma NotificationModule provides these functions.
    // Module must be loaded in environment setup.
    // Header: <notification/notification.h>
    // Library: -lnotifications
    //
    // NotificationModule_GetHandle(&handle)
    // NotificationModule_AddBroadcastNotification(handle, durationMs, color, title, message)
    //
    // If the module isn't loaded, this is a no-op.
    // The plugin will still function - just without on-screen notifications.
    (void)s_notif_handle;
    (void)title;
    (void)message;
}

void UI_Deinit(void) {
    s_initialized = false;
}
