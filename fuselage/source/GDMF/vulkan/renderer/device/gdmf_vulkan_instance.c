// Vulkan instance and optional debug reporting.
// SEE: (GDMF_VULKAN.md, Instance creation and debug reporting).
#include "gdmf_vulkan_instance.h"
#include "../../surface/gdmf_vulkan_surface.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static VkInstance gdmf_instance = VK_NULL_HANDLE;
#ifdef DEBUG
static VkDebugUtilsMessengerEXT gdmf_debug_messenger = VK_NULL_HANDLE;
static int gdmf_vulkan_debug_messenger_create(void);
static void gdmf_vulkan_debug_messenger_destroy(void);
#endif

int gdmf_vulkan_instance_create(void) {
    uint32_t loader_version = VK_API_VERSION_1_0;
    PFN_vkEnumerateInstanceVersion enumerate_version =
        (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceVersion");

    if (enumerate_version) { enumerate_version(&loader_version); }

    printf("[Vulkan] Loader reports %u.%u; GDMF requires 1.0\n",
        VK_API_VERSION_MAJOR(loader_version),
        VK_API_VERSION_MINOR(loader_version));

    uint32_t available_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &available_extension_count, NULL);
    VkExtensionProperties* available_extensions = malloc(available_extension_count * sizeof(VkExtensionProperties));
    if (!available_extensions) { return -1; }
    vkEnumerateInstanceExtensionProperties(NULL, &available_extension_count, available_extensions);

    const char* required_extensions[] = { "VK_KHR_surface", gdmf_platform_surface_extension() };
    const uint32_t required_extension_count = sizeof(required_extensions) / sizeof(required_extensions[0]);

    for (uint32_t required_index = 0; required_index < required_extension_count; required_index++) {
        bool found = false;

        for (uint32_t extension_index = 0; extension_index < available_extension_count; extension_index++) {
            if (strcmp(required_extensions[required_index], available_extensions[extension_index].extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            printf("[Vulkan] Missing required extension: %s\n", required_extensions[required_index]);

            free(available_extensions);
            return -1;
        }
    }

    const char* enabled_extensions[6];
    uint32_t    enabled_extension_count = 0;
    VkInstanceCreateFlags instance_flags = 0;
    for (uint32_t extension_index = 0; extension_index < required_extension_count; extension_index++)
        enabled_extensions[enabled_extension_count++] = required_extensions[extension_index];

#ifdef VK_KHR_portability_enumeration
    for (uint32_t extension_index = 0; extension_index < available_extension_count; extension_index++) {
        if (strcmp(available_extensions[extension_index].extensionName,
                   VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
            enabled_extensions[enabled_extension_count++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
            instance_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            break;
        }
    }
#endif
    for (uint32_t extension_index = 0; extension_index < available_extension_count; extension_index++) {
        if (strcmp(available_extensions[extension_index].extensionName,
                   VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0) {
            enabled_extensions[enabled_extension_count++] =
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
            break;
        }
    }

#ifdef DEBUG
    bool debug_utils_available = false;
    for (uint32_t extension_index = 0; extension_index < available_extension_count; extension_index++) {
        if (strcmp(available_extensions[extension_index].extensionName, "VK_EXT_debug_utils") == 0) {
            debug_utils_available = true;
            break;
        }
    }
    if (debug_utils_available) {
        enabled_extensions[enabled_extension_count++] = "VK_EXT_debug_utils";
    } else {
        printf("[Vulkan] VK_EXT_debug_utils not available; debug messenger disabled\n");
    }

#endif

    free(available_extensions);

    const char* enabled_layers[1];
    uint32_t    enabled_layer_count = 0;

#ifdef DEBUG
    uint32_t available_layer_count = 0;
    vkEnumerateInstanceLayerProperties(&available_layer_count, NULL);
    VkLayerProperties* available_layers = malloc(available_layer_count * sizeof(VkLayerProperties));
    if (available_layers) {
        vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers);
        for (uint32_t layer_index = 0; layer_index < available_layer_count; layer_index++) {
            if (strcmp(available_layers[layer_index].layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                enabled_layers[enabled_layer_count++] = "VK_LAYER_KHRONOS_validation";
                break;
            }
        }
        free(available_layers);
    }
    if (enabled_layer_count > 0) {
        printf("[Vulkan] Validation layer enabled\n");
    } else {
        printf("[Vulkan] VK_LAYER_KHRONOS_validation not available\n");
    }

#endif

    VkApplicationInfo application_info = {
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = "Fuselage",
        .applicationVersion = VK_MAKE_VERSION(0, 3, 0),
        .pEngineName        = "Fuselage GDMF",
        .engineVersion      = VK_MAKE_VERSION(0, 3, 0),
        .apiVersion         = VK_API_VERSION_1_0
    };

    VkInstanceCreateInfo create_info = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .flags                   = instance_flags,
        .pApplicationInfo        = &application_info,
        .enabledExtensionCount   = enabled_extension_count,
        .ppEnabledExtensionNames = enabled_extensions,
        .enabledLayerCount       = enabled_layer_count,
        .ppEnabledLayerNames     = enabled_layer_count ? enabled_layers : NULL
    };

    VkInstance created_instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&create_info, NULL, &created_instance) != VK_SUCCESS) {
        printf("[Vulkan] vkCreateInstance failed\n");

        return -1;
    }
    gdmf_instance = created_instance;
    printf("[Vulkan] Instance created (API %u.%u)\n",
        VK_API_VERSION_MAJOR(application_info.apiVersion),
        VK_API_VERSION_MINOR(application_info.apiVersion));

#ifdef DEBUG
    gdmf_vulkan_debug_messenger_create();
#endif

    return 0;
}


#ifdef DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL gdmf_vulkan_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT     severity,
    VkDebugUtilsMessageTypeFlagsEXT            message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void*                                       user_data) {
    (void)message_types; (void)user_data;
    const char* prefix =
        (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)   ? "[Vulkan ERROR]"   :
        (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "[Vulkan WARNING]" :
                                                                         "[Vulkan INFO]";

    printf("%s %s\n", prefix, callback_data->pMessage);

    return VK_FALSE;
}

static int gdmf_vulkan_debug_messenger_create(void) {
    PFN_vkCreateDebugUtilsMessengerEXT create_messenger =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            gdmf_instance, "vkCreateDebugUtilsMessengerEXT");

    if (!create_messenger) {
        printf("[Vulkan] vkCreateDebugUtilsMessengerEXT not available\n");

        return -1;
    }

    VkDebugUtilsMessengerCreateInfoEXT create_info = {
        .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = gdmf_vulkan_debug_callback
    };

    VkDebugUtilsMessengerEXT created_messenger = VK_NULL_HANDLE;
    if (create_messenger(gdmf_instance, &create_info, NULL, &created_messenger) != VK_SUCCESS) {
        printf("[Vulkan] Debug messenger creation failed\n");

        return -1;
    }
    gdmf_debug_messenger = created_messenger;

    printf("[Vulkan] Debug messenger active\n");

    return 0;
}

static void gdmf_vulkan_debug_messenger_destroy(void) {
    if (gdmf_debug_messenger == VK_NULL_HANDLE) { return; }
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            gdmf_instance, "vkDestroyDebugUtilsMessengerEXT");
    if (destroy_messenger) { destroy_messenger(gdmf_instance, gdmf_debug_messenger, NULL); }
    gdmf_debug_messenger = VK_NULL_HANDLE;

    return;
}


#endif

VkInstance gdmf_vulkan_instance_get(void) {
    return gdmf_instance;
}

void gdmf_vulkan_instance_destroy(void) {
#ifdef DEBUG
    gdmf_vulkan_debug_messenger_destroy();
#endif
    if (gdmf_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(gdmf_instance, NULL);
        gdmf_instance = VK_NULL_HANDLE;
    }
}
