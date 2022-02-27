// configfile.c - handles loading and saving the configuration options
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "platform.h"
#include "configfile.h"
#include "cliopts.h"
#include "gfx/gfx_screen_config.h"
#include "gfx/gfx_window_manager_api.h"
#include "controller/controller_api.h"
#include "fs/fs.h"
#include "pc/mod_list.h"
#include "pc/network/ban_list.h"

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

enum ConfigOptionType {
    CONFIG_TYPE_BOOL,
    CONFIG_TYPE_UINT,
    CONFIG_TYPE_FLOAT,
    CONFIG_TYPE_BIND,
    CONFIG_TYPE_STRING,
};

struct ConfigOption {
    const char *name;
    enum ConfigOptionType type;
    union {
        bool *boolValue;
        unsigned int *uintValue;
        float* floatValue;
        char* stringValue;
    };
    int maxStringLength;
};

/*
 *Config options and default values
 */

// Video/audio stuff
ConfigWindow configWindow       = {
    .x = WAPI_WIN_CENTERPOS,
    .y = WAPI_WIN_CENTERPOS,
    .w = DESIRED_SCREEN_WIDTH,
    .h = DESIRED_SCREEN_HEIGHT,
    .vsync = 0,
    .reset = false,
    .fullscreen = false,
    .exiting_fullscreen = false,
    .settings_changed = false,
};
unsigned int configFiltering    = 1;          // 0=force nearest, 1=linear, (TODO) 2=three-point
unsigned int configMasterVolume = 30; // 0 - MAX_VOLUME
unsigned int configMusicVolume = MAX_VOLUME;
unsigned int configSfxVolume = MAX_VOLUME;
unsigned int configEnvVolume = MAX_VOLUME;

// Keyboard mappings (VK_ values, by default keyboard/gamepad/mouse)
unsigned int configKeyA[MAX_BINDS]          = { 0x0026,   0x1000,     0x1103     };
unsigned int configKeyB[MAX_BINDS]          = { 0x0033,   0x1001,     0x1101     };
unsigned int configKeyX[MAX_BINDS]          = { 0x0025,   0x1002,     VK_INVALID };
unsigned int configKeyY[MAX_BINDS]          = { 0x0032,   0x1003,     VK_INVALID };
unsigned int configKeyStart[MAX_BINDS]      = { 0x0039,   0x1006,     VK_INVALID };
unsigned int configKeyL[MAX_BINDS]          = { 0x002A,   0x1009,     0x1104     };
unsigned int configKeyR[MAX_BINDS]          = { 0x0036,   0x100A,     0x101B     };
unsigned int configKeyZ[MAX_BINDS]          = { 0x0025,   0x1007,     0x101A     };
unsigned int configKeyCUp[MAX_BINDS]        = { 0x0148,   VK_INVALID, VK_INVALID };
unsigned int configKeyCDown[MAX_BINDS]      = { 0x0150,   VK_INVALID, VK_INVALID };
unsigned int configKeyCLeft[MAX_BINDS]      = { 0x014B,   VK_INVALID, VK_INVALID };
unsigned int configKeyCRight[MAX_BINDS]     = { 0x014D,   VK_INVALID, VK_INVALID };
unsigned int configKeyStickUp[MAX_BINDS]    = { 0x0011,   VK_INVALID, VK_INVALID };
unsigned int configKeyStickDown[MAX_BINDS]  = { 0x001F,   VK_INVALID, VK_INVALID };
unsigned int configKeyStickLeft[MAX_BINDS]  = { 0x001E,   VK_INVALID, VK_INVALID };
unsigned int configKeyStickRight[MAX_BINDS] = { 0x0020,   VK_INVALID, VK_INVALID };
unsigned int configKeyChat[MAX_BINDS]       = { 0x001C,   VK_INVALID, VK_INVALID };
unsigned int configKeyPlayerList[MAX_BINDS] = { 0x000F,   0x1004,     VK_INVALID };
unsigned int configKeyDUp[MAX_BINDS]        = { 0x0147,   0x100b,     VK_INVALID };
unsigned int configKeyDDown[MAX_BINDS]      = { 0x014f,   0x100c,     VK_INVALID };
unsigned int configKeyDLeft[MAX_BINDS]      = { 0x0153,   0x100d,     VK_INVALID };
unsigned int configKeyDRight[MAX_BINDS]     = { 0x0151,   0x100e,     VK_INVALID };
unsigned int configStickDeadzone = 16; // 16*DEADZONE_STEP=4960 (the original default deadzone)
unsigned int configRumbleStrength = 50;
#ifdef EXTERNAL_DATA
bool configPrecacheRes = true;
#endif
#ifdef BETTERCAMERA
// BetterCamera settings
unsigned int configCameraXSens   = 10;
unsigned int configCameraYSens   = 10;
unsigned int configCameraAggr    = 0;
unsigned int configCameraPan     = 0;
unsigned int configCameraDegrade = 50; // 0 - 100%
bool         configCameraInvertX = false;
bool         configCameraInvertY = true;
bool         configEnableCamera  = true;
bool         configCameraAnalog  = true;
bool         configCameraMouse   = false;
#endif
bool         configSkipIntro     = 0;
bool         configShareLives    = 0;
bool         configEnableCheats  = 0;
bool         configBubbleDeath   = true;
unsigned int configAmountofPlayers   = 16;
bool         configHUD           = true;
#ifdef DISCORDRPC
bool         configDiscordRPC    = true;
#endif
// coop-specific
char         configJoinIp[MAX_CONFIG_STRING] = "";
unsigned int configJoinPort                      = DEFAULT_PORT;
unsigned int configHostPort                      = DEFAULT_PORT;
unsigned int configHostSaveSlot                  = 1;
unsigned int configPlayerInteraction             = 1;
unsigned int configPlayerKnockbackStrength       = 25;
bool         configStayInLevelAfterStar          = 0;
unsigned int configNetworkSystem                 = 0;
char         configPlayerName[MAX_PLAYER_STRING] = "";
unsigned int configPlayerModel                   = 0;
unsigned int configPlayerPalette                 = 0;
unsigned int config60Fps                         = 1;
unsigned int configDrawDistance                  = 5;

static const struct ConfigOption options[] = {
    {.name = "fullscreen",           .type = CONFIG_TYPE_BOOL, .boolValue = &configWindow.fullscreen},
    {.name = "window_x",             .type = CONFIG_TYPE_UINT, .uintValue = &configWindow.x},
    {.name = "window_y",             .type = CONFIG_TYPE_UINT, .uintValue = &configWindow.y},
    {.name = "window_w",             .type = CONFIG_TYPE_UINT, .uintValue = &configWindow.w},
    {.name = "window_h",             .type = CONFIG_TYPE_UINT, .uintValue = &configWindow.h},
    {.name = "vsync",                .type = CONFIG_TYPE_BOOL, .boolValue = &configWindow.vsync},
    {.name = "texture_filtering",    .type = CONFIG_TYPE_UINT, .uintValue = &configFiltering},
    {.name = "master_volume",        .type = CONFIG_TYPE_UINT, .uintValue = &configMasterVolume},
    {.name = "music_volume",         .type = CONFIG_TYPE_UINT, .uintValue = &configMusicVolume},
    {.name = "sfx_volume",           .type = CONFIG_TYPE_UINT, .uintValue = &configSfxVolume},
    {.name = "env_volume",           .type = CONFIG_TYPE_UINT, .uintValue = &configEnvVolume},
    {.name = "key_a",                .type = CONFIG_TYPE_BIND, .uintValue = configKeyA},
    {.name = "key_b",                .type = CONFIG_TYPE_BIND, .uintValue = configKeyB},
    {.name = "key_x",                .type = CONFIG_TYPE_BIND, .uintValue = configKeyX},
    {.name = "key_y",                .type = CONFIG_TYPE_BIND, .uintValue = configKeyY},
    {.name = "key_start",            .type = CONFIG_TYPE_BIND, .uintValue = configKeyStart},
    {.name = "key_l",                .type = CONFIG_TYPE_BIND, .uintValue = configKeyL},
    {.name = "key_r",                .type = CONFIG_TYPE_BIND, .uintValue = configKeyR},
    {.name = "key_z",                .type = CONFIG_TYPE_BIND, .uintValue = configKeyZ},
    {.name = "key_cup",              .type = CONFIG_TYPE_BIND, .uintValue = configKeyCUp},
    {.name = "key_cdown",            .type = CONFIG_TYPE_BIND, .uintValue = configKeyCDown},
    {.name = "key_cleft",            .type = CONFIG_TYPE_BIND, .uintValue = configKeyCLeft},
    {.name = "key_cright",           .type = CONFIG_TYPE_BIND, .uintValue = configKeyCRight},
    {.name = "key_stickup",          .type = CONFIG_TYPE_BIND, .uintValue = configKeyStickUp},
    {.name = "key_stickdown",        .type = CONFIG_TYPE_BIND, .uintValue = configKeyStickDown},
    {.name = "key_stickleft",        .type = CONFIG_TYPE_BIND, .uintValue = configKeyStickLeft},
    {.name = "key_stickright",       .type = CONFIG_TYPE_BIND, .uintValue = configKeyStickRight},
    {.name = "key_chat",             .type = CONFIG_TYPE_BIND, .uintValue = configKeyChat},
    {.name = "key_playerlist",       .type = CONFIG_TYPE_BIND, .uintValue = configKeyPlayerList},
    {.name = "key_dup",              .type = CONFIG_TYPE_BIND, .uintValue = configKeyDUp},
    {.name = "key_ddown",            .type = CONFIG_TYPE_BIND, .uintValue = configKeyDDown},
    {.name = "key_dleft",            .type = CONFIG_TYPE_BIND, .uintValue = configKeyDLeft},
    {.name = "key_dright",           .type = CONFIG_TYPE_BIND, .uintValue = configKeyDRight},
    {.name = "stick_deadzone",       .type = CONFIG_TYPE_UINT, .uintValue = &configStickDeadzone},
    {.name = "rumble_strength",      .type = CONFIG_TYPE_UINT, .uintValue = &configRumbleStrength},
    #ifdef EXTERNAL_DATA
    {.name = "precache",             .type = CONFIG_TYPE_BOOL, .boolValue = &configPrecacheRes},
    #endif
    #ifdef BETTERCAMERA
    {.name = "bettercam_enable",     .type = CONFIG_TYPE_BOOL, .boolValue = &configEnableCamera},
    {.name = "bettercam_analog",     .type = CONFIG_TYPE_BOOL, .boolValue = &configCameraAnalog},
    {.name = "bettercam_mouse_look", .type = CONFIG_TYPE_BOOL, .boolValue = &configCameraMouse},
    {.name = "bettercam_invertx",    .type = CONFIG_TYPE_BOOL, .boolValue = &configCameraInvertX},
    {.name = "bettercam_inverty",    .type = CONFIG_TYPE_BOOL, .boolValue = &configCameraInvertY},
    {.name = "bettercam_xsens",      .type = CONFIG_TYPE_UINT, .uintValue = &configCameraXSens},
    {.name = "bettercam_ysens",      .type = CONFIG_TYPE_UINT, .uintValue = &configCameraYSens},
    {.name = "bettercam_aggression", .type = CONFIG_TYPE_UINT, .uintValue = &configCameraAggr},
    {.name = "bettercam_pan_level",  .type = CONFIG_TYPE_UINT, .uintValue = &configCameraPan},
    {.name = "bettercam_degrade",    .type = CONFIG_TYPE_UINT, .uintValue = &configCameraDegrade},
    #endif
    {.name = "skip_intro",           .type = CONFIG_TYPE_BOOL, .boolValue = &configSkipIntro},
    {.name = "share_lives",          .type = CONFIG_TYPE_BOOL, .boolValue = &configShareLives},
    {.name = "enable_cheats",        .type = CONFIG_TYPE_BOOL, .boolValue = &configEnableCheats},
    {.name = "bubble_death",         .type = CONFIG_TYPE_BOOL, .boolValue = &configBubbleDeath},
    {.name = "amount_of_players",    .type = CONFIG_TYPE_UINT, .uintValue = &configAmountofPlayers},
    #ifdef DISCORDRPC
    {.name = "discordrpc_enable",    .type = CONFIG_TYPE_BOOL, .boolValue = &configDiscordRPC},
    #endif
    // coop-specific
    {.name = "coop_join_ip",                   .type = CONFIG_TYPE_STRING, .stringValue = (char*)&configJoinIp, .maxStringLength = MAX_CONFIG_STRING},
    {.name = "coop_join_port",                 .type = CONFIG_TYPE_UINT  , .uintValue   = &configJoinPort},
    {.name = "coop_host_port",                 .type = CONFIG_TYPE_UINT  , .uintValue   = &configHostPort},
    {.name = "coop_host_save_slot",            .type = CONFIG_TYPE_UINT  , .uintValue   = &configHostSaveSlot},
    {.name = "coop_player_interaction",        .type = CONFIG_TYPE_UINT  , .uintValue   = &configPlayerInteraction},
    {.name = "coop_player_knockback_strength", .type = CONFIG_TYPE_UINT  , .uintValue   = &configPlayerKnockbackStrength},
    {.name = "coop_stay_in_level_after_star",  .type = CONFIG_TYPE_UINT  , .boolValue   = &configStayInLevelAfterStar},
    {.name = "coop_network_system",            .type = CONFIG_TYPE_UINT  , .uintValue   = &configNetworkSystem},
    {.name = "coop_player_name",               .type = CONFIG_TYPE_STRING, .stringValue = (char*)&configPlayerName, .maxStringLength = MAX_PLAYER_STRING},
    {.name = "coop_player_model",              .type = CONFIG_TYPE_UINT  , .uintValue   = &configPlayerModel},
    {.name = "coop_player_palette",            .type = CONFIG_TYPE_UINT  , .uintValue   = &configPlayerPalette},
    {.name = "coop_60fps",                     .type = CONFIG_TYPE_UINT  , .uintValue   = &config60Fps},
    {.name = "coop_draw_distance",             .type = CONFIG_TYPE_UINT  , .uintValue   = &configDrawDistance},
};

// Reads an entire line from a file (excluding the newline character) and returns an allocated string
// Returns NULL if no lines could be read from the file
static char *read_file_line(fs_file_t *file) {
    char *buffer;
    size_t bufferSize = 8;
    size_t offset = 0; // offset in buffer to write

    buffer = malloc(bufferSize);
    while (1) {
        // Read a line from the file
        if (fs_readline(file, buffer + offset, bufferSize - offset) == NULL) {
            free(buffer);
            return NULL; // Nothing could be read.
        }
        offset = strlen(buffer);
        assert(offset > 0);

        // If a newline was found, remove the trailing newline and exit
        if (buffer[offset - 1] == '\n') {
            buffer[offset - 1] = '\0';
            break;
        }

        if (fs_eof(file)) // EOF was reached
            break;

        // If no newline or EOF was reached, then the whole line wasn't read.
        bufferSize *= 2; // Increase buffer size
        buffer = realloc(buffer, bufferSize);
        assert(buffer != NULL);
    }

    return buffer;
}

// Returns the position of the first non-whitespace character
static char *skip_whitespace(char *str) {
    while (isspace(*str))
        str++;
    return str;
}

// NULL-terminates the current whitespace-delimited word, and returns a pointer to the next word
static char *word_split(char *str) {
    // Precondition: str must not point to whitespace
    assert(!isspace(*str));

    // Find either the next whitespace char or end of string
    while (!isspace(*str) && *str != '\0')
        str++;
    if (*str == '\0') // End of string
        return str;

    // Terminate current word
    *(str++) = '\0';

    // Skip whitespace to next word
    return skip_whitespace(str);
}

// Splits a string into words, and stores the words into the 'tokens' array
// 'maxTokens' is the length of the 'tokens' array
// Returns the number of tokens parsed
static unsigned int tokenize_string(char *str, int maxTokens, char **tokens) {
    int count = 0;

    str = skip_whitespace(str);
    while (str[0] != '\0' && count < maxTokens) {
        tokens[count] = str;
        str = word_split(str);
        count++;
    }
    return count;
}

// Gets the config file path and caches it
const char *configfile_name(void) {
    return (gCLIOpts.ConfigFile[0]) ? gCLIOpts.ConfigFile : CONFIGFILE_DEFAULT;
}

// Loads the config file specified by 'filename'
void configfile_load(const char *filename) {
    fs_file_t *file;
    char *line;

    printf("Loading configuration from '%s'\n", filename);

    file = fs_open(filename);
    if (file == NULL) {
        // Create a new config file and save defaults
        printf("Config file '%s' not found. Creating it.\n", filename);
        configfile_save(filename);
        return;
    }

    // Go through each line in the file
    while ((line = read_file_line(file)) != NULL) {
        char *p = line;
        char *tokens[1 + MAX_BINDS];
        int numTokens;

        while (isspace(*p))
            p++;

        if (!*p || *p == '#') { // comment or empty line
            free(line);
            continue;
        }

        numTokens = tokenize_string(p, sizeof(tokens) / sizeof(tokens[0]), tokens);
        if (numTokens != 0) {
            if (numTokens >= 2) {
                const struct ConfigOption *option = NULL;

                // enable mods
                if (!strcmp(tokens[0], "enable-mod:")) {
                    for (unsigned int i = 0; i < gModTableLocal.entryCount; i++) {
                        struct ModListEntry* entry = &gModTableLocal.entries[i];
                        if (!strcmp(tokens[1], entry->name)) {
                            entry->enabled = true;
                            break;
                        }
                    }
                    free(line);
                    continue;
                }

                // ban list
                if (!strcmp(tokens[0], "ban:")) {
                    ban_list_add(tokens[1], true);
                    free(line);
                    continue;
                }

                for (unsigned int i = 0; i < ARRAY_LEN(options); i++) {
                    if (strcmp(tokens[0], options[i].name) == 0) {
                        option = &options[i];
                        break;
                    }
                }

                if (option == NULL)
                    printf("unknown option '%s'\n", tokens[0]);
                else {
                    switch (option->type) {
                        case CONFIG_TYPE_BOOL:
                            if (strcmp(tokens[1], "true") == 0)
                                *option->boolValue = true;
                            else
                                *option->boolValue = false;
                            break;
                        case CONFIG_TYPE_UINT:
                            sscanf(tokens[1], "%u", option->uintValue);
                            break;
                        case CONFIG_TYPE_BIND:
                            for (int i = 0; i < MAX_BINDS && i < numTokens - 1; ++i)
                                sscanf(tokens[i + 1], "%x", option->uintValue + i);
                            break;
                        case CONFIG_TYPE_FLOAT:
                            sscanf(tokens[1], "%f", option->floatValue);
                            break;
                        case CONFIG_TYPE_STRING:
                            memset(option->stringValue, '\0', option->maxStringLength);
                            snprintf(option->stringValue, option->maxStringLength, "%s", tokens[1]);
                            break;
                        default:
                            assert(0); // bad type
                    }
                    printf("option: '%s', value:", tokens[0]);
                    for (int i = 1; i < numTokens; ++i) printf(" '%s'", tokens[i]);
                    printf("\n");
                }
            } else
                puts("error: expected value");
        }
        free(line);
    }

    fs_close(file);

#ifndef DISCORD_SDK
    configNetworkSystem = 1;
#endif
}

// Writes the config file to 'filename'
void configfile_save(const char *filename) {
    FILE *file;

    printf("Saving configuration to '%s'\n", filename);

    file = fopen(fs_get_write_path(filename), "w");
    if (file == NULL) {
        // error
        return;
    }

    for (unsigned int i = 0; i < ARRAY_LEN(options); i++) {
        const struct ConfigOption *option = &options[i];

        switch (option->type) {
            case CONFIG_TYPE_BOOL:
                fprintf(file, "%s %s\n", option->name, *option->boolValue ? "true" : "false");
                break;
            case CONFIG_TYPE_UINT:
                fprintf(file, "%s %u\n", option->name, *option->uintValue);
                break;
            case CONFIG_TYPE_FLOAT:
                fprintf(file, "%s %f\n", option->name, *option->floatValue);
                break;
            case CONFIG_TYPE_BIND:
                fprintf(file, "%s ", option->name);
                for (int i = 0; i < MAX_BINDS; ++i)
                    fprintf(file, "%04x ", option->uintValue[i]);
                fprintf(file, "\n");
                break;
            case CONFIG_TYPE_STRING:
                fprintf(file, "%s %s\n", option->name, option->stringValue);
                break;
            default:
                assert(0); // unknown type
        }
    }

    // save enabled mods
    for (unsigned int i = 0; i < gModTableLocal.entryCount; i++) {
        struct ModListEntry* entry = &gModTableLocal.entries[i];
        if (entry == NULL) { continue; }
        if (!entry->enabled) { continue; }
        fprintf(file, "%s %s\n", "enable-mod:", entry->name);
    }

    // save ban list
    for (unsigned int i = 0; i < gBanCount; i++) {
        if (gBanAddresses == NULL) { break; }
        if (gBanAddresses[i] == NULL) { continue; }
        if (!gBanPerm[i]) { continue; }
        fprintf(file, "%s %s\n", "ban:", gBanAddresses[i]);
    }

    fclose(file);
}
