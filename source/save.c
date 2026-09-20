/**
 * @file save.c
 */
#include "save.h"

#include "audio_utils.h"
#include "bitset.h"
#include "card.h"
#include "deck_rules.h"
#include "game.h"
#include "game/shop.h"
#include "game_variables.h"
#include "hand.h"
#include "joker.h"
#include "list.h"
#include "util.h"
#include "version.h"

#include <stdlib.h>
#include <string.h>

// See https://gbadev.net/gbadoc/memory.html for more details on SRAM
// A few important pieces of info:
//   - Read/Writes are limited to 8-bits words so they are done byte per byte
//   - Memory is filled with 1s by default
//     (at least in mgba, not sure about real HW)

#define HEADER_ADDRESS  0x0
#define OPTIONS_ADDRESS 0x10
#define GAME_ADDRESS    0x30

#define SAVE_SECTION_FLAG_NONE    0
#define SAVE_SECTION_FLAG_OPTIONS (1 << 0)
#define SAVE_SECTION_FLAG_GAME    (1 << 1)

#define CHECK_MAGIC     0x4C414247 // Spells GBAL, used to determine if the save data is junk
#define CHECK_HASH_SIZE 7
#define GIT_HASH_START  17 // starts after "GBALATRO-VERSION:" in the gbalatro_version var

#define SAVE_LABEL_SIZE 16
#define SAVE_GAME_FORMAT 9
#define SAVE_MIN_ANTE    1

// clang-format off
/**
 * @brief SaveHeader for validation checks to be packed and written to SRAM for validation.
 *         Defined in this discussion as follows: https://github.com/GBALATRO/balatro-gba/discussions/450
 *
 * word | Byte 0 | Byte 1 | Byte 2 | Byte 3 | name         | purpose
 * -----|--------|--------|--------|--------|--------------|------------------------------------------------------------------
 * 0    | 0x47   | 0x42   | 0x41   | 0x4C   | MAGIC        | Identify if proceeding data is valid and not junk, spells "GBAL"
 * 1    | Dirty  | H[0]   | H[1]   | H[2]   | GITHASH_LOW  | Dirty flag, followed by the first 3 bytes of shortened git hash H
 * 2    | H[3]   | H[4]   | H[5]   | H[6]   | GITHASH_HIGH | Last 4 bytes of shortened git hash H, with a dirty flag
 * 3    | SEC[0] | SEC[1] | SEC[2] | SEC[3] | VALID_SCTNS  | Identifies whether each section of the save data is valid or not
 */
// clang-format on
typedef struct SaveHeader
{
    u32 magic;
    u8 dirty;
    char githash[CHECK_HASH_SIZE];
    u32 valid_sections;
} SaveHeader;

/**
 * @brief Default value for the SaveHeader struct.
 */
static const SaveHeader SaveHeader_default = {
    .magic = CHECK_MAGIC,
    .dirty = false,
    .githash = "fffffff",
    .valid_sections = SAVE_SECTION_FLAG_NONE
};

// clang-format off
/**
 * @brief SaveOptions will only contain options data set in the Options Menu
 *
 * word | Byte 0 | Byte 1 | Byte 2 | Byte 3 | name         | purpose
 * -----|--------|--------|--------|--------|--------------|------------------------------------------------------------------
 * 0    | '-'    | ' '    | 'O'    | 'P'    | TAG          | Pretty tag to clearly visualize the Options section in a hex viewer
 * 1    | 'T'    | 'I'    | 'O'    | 'N'    | -            | Spells "- OPTIONS DATA -"
 * 2    | 'S'    | ' '    | 'D'    | 'A'    | -            | -
 * 3    | 'T'    | 'A'    | ' '    | '-'    | -            | -
 * 4    | SPEED  | CNTRST | READBL | MUSIC  | OPTN_VALUES  | All 5 option values, followed by some padding,
 * 5    | SOUND  | UNDEF  | UNDEF  | UNDEF  | -            | so that the next section starts at the beginning of the
 * 6    | UNDEF  | UNDEF  | UNDEF  | UNDEF  | -            | next 4-word row in a hex viewer
 * 7    | UNDEF  | UNDEF  | UNDEF  | UNDEF  | -            | -
 */
// clang-format on
typedef struct SaveOptions
{
    char tag_options[SAVE_LABEL_SIZE];
    u8 game_speed;
    u8 cards_high_contrast;
    u8 cards_more_readable;
    u8 music_volume;
    u8 sound_volume;
    s8 padding[11];
} SaveOptions;

/**
 * @brief Default value for the SaveOptions struct, with tags already set.
 */
// clang-format off
static const SaveOptions SaveOptions_default = {
    .tag_options = "- OPTIONS DATA -",
    .game_speed = GAME_SPEED_MIN,
    .cards_high_contrast = DEFAULT_HIGH_CONTRAST,
    .cards_more_readable = DEFAULT_MORE_READABLE,
    .music_volume = VOLUME_OPTION_MAX,
    .sound_volume = VOLUME_OPTION_MAX,
    .padding = {
        UNDEFINED, UNDEFINED, UNDEFINED,
        UNDEFINED, UNDEFINED, UNDEFINED,
        UNDEFINED, UNDEFINED, UNDEFINED,
        UNDEFINED, UNDEFINED
    }
};
// clang-format on

/**
 * @brief JokerObjectSaveData will hold the minimal amount of data necessary to reconstruct a Joker.
 *         The `id` is a u8 in the base Joker struct, but I made it a u32 here to keep
 *         a better aligment when looking at the save file in a hex viewer.
 */
typedef struct JokerObjectSaveData
{
    u32 id;
    u32 modifier;
    s32 scoring_state;
    s32 persistent_state;
} JokerObjectSaveData;

// clang-format off
/**
 * @brief SaveGame will contain the data about the current run to be saved to SRAM.
 *         GameVariables was used for this purpose at first, but some data needed to be shared but
 *         not saved, so it couldn't be dumped "as is" anymore and this struct had to be created.
 *
 * word | Byte 0 | Byte 1 | Byte 2 | Byte 3 | name         | purpose
 * -----|--------|--------|--------|--------|--------------|------------------------------------------------------------------
 * 0    | '-'    | 'I'    | 'N'    | 'T'    | TAG          | Spells "-INTERNAL DATA -"
 * 1    | 'E'    | 'R'    | 'N'    | 'A'    | -            | -
 * 2    | 'L'    | ' '    | 'D'    | 'A'    | -            | -
 * 3    | 'T'    | 'A'    | ' '    | '-'    | -            | -
 * 4    | T[0]   | T[1]   | T[2]   | T[3]   | GLOB TIMER   | The global timer used for animations thoughout the game
 * 5    | RNG[0] | RNG[1] | RNG[2] | RNG[3] | RNG INFO     | RNG Info struct, containing the seed used for RNG, either randomly shuffled or chosen by the player
 * 6    | RNG[4] | RNG[5] | RNG[6] | RNG[7] | -            | at game start, and the current position in the RNG sequence for the given seed, since the start of the run
 * 7    | RND[0] | RND[1] | RND[2] | RND[3] | ROUND        | What Round we are about to start
 * 8    | ANT[0] | ANT[1] | ANT[2] | ANT[3] | ANTE         | What Ante we are on
 * 9    | MNY[0] | MNY[1] | MNY[2] | MNY[3] | MONEY        | How much money we currently have left
 * 10   | UNDEF  | UNDEF  | UNDEF  | UNDEF  | PADDING      | Some padding
 * 11   | UNDEF  | UNDEF  | UNDEF  | UNDEF  | -            | -
 * 12   | '-'    | ' '    | 'O'    | 'W'    | TAG          | Spells "- OWNED JOKERS -"
 * 13   | 'N'    | 'E'    | 'D'    | ' '    | -            | -
 * 14   | 'J'    | 'O'    | 'K'    | 'E'    | -            | -
 * 15   | 'R'    | 'S'    | ' '    | '-'    | -            | -
 * 16   | ID[0]  | ID[1]  | ID[2]  | ID[3]  | JOKER DATA 0 | Minimal necessary data to reconstruct a JokerObject
 * 17   | STT[0] | STT[1] | STT[2] | STT[3] | -            | Contains the Joker's `id` and `persistent_state`
 * ...  | ...    | ...    | ...    | ...    | ...          | ...
 * ...  | ...    | ...    | ...    | ...    | ...          | ...
 * ??   | '_'    | 'E'    | 'N'    | 'D'    | END_TAG      | Spells "_END", marks the end of the savefile
 */
// clang-format on
typedef struct SaveGame
{
    char tag_internal[SAVE_LABEL_SIZE];
    s32 timer;
    RngInfo rng_info;
    int round;
    int ante;
    int money;
    u32 format_version;
    int hand_size;
    int deck_type;
    int current_blind;
    int next_boss_blind;
    int blinds_states[NUM_BLINDS_PER_ANTE];
    AlchemicalInventory alchemy;
    VoucherState vouchers;
    u16 hand_play_counts[ALCHEMICAL_HAND_TYPE_COUNT];
    int card_count;
    Card cards[MAX_DECK_SIZE];

    char tag_jokers[SAVE_LABEL_SIZE];
    JokerObjectSaveData jokers_data[MAX_JOKERS_HELD_SIZE];

    char tag_end[4];
} SaveGame;

/**
 * @brief Default value for the SaveGame struct, with tags already set.
 */
static const SaveGame SaveGame_default = {
    .tag_internal = "-INTERNAL DATA -",
    .timer = 0,
    .rng_info = {0, 0},
    .round = 0,
    .ante = 0,
    .money = 0,
    .format_version = SAVE_GAME_FORMAT,
    .hand_size = DEFAULT_HAND_SIZE,
    .deck_type = DECK_TYPE_RED,
    .current_blind = BLIND_TYPE_SMALL,
    .next_boss_blind = BLIND_TYPE_BOSS,
    .blinds_states = {
        BLIND_STATE_CURRENT,
        BLIND_STATE_UPCOMING,
        BLIND_STATE_UPCOMING
    },
    .alchemy = {
        .held = {
            ALCHEMICAL_INVALID_ID,
            ALCHEMICAL_INVALID_ID,
            ALCHEMICAL_INVALID_ID
        },
        .count = 0,
        .used_count = 0,
        .hand_levels = {}
    },
    .vouchers = {
        .owned_mask = 0,
        .offer_id = VOUCHER_INVALID_ID,
        .offer_ante = VOUCHER_INVALID_ID
    },
    .hand_play_counts = {},
    .card_count = 0,
    .cards = {},

    .tag_jokers = "- OWNED JOKERS -",
    .jokers_data = {},

    .tag_end = "_END"
};

_Static_assert(
    GAME_ADDRESS + sizeof(SaveGame) <= SRAM_SIZE,
    "SaveGame exceeds available GBA SRAM"
);

/**
 * @brief Write raw binary data to SRAM
 *
 * @param sram_base address written to in the SRAM
 * @param bytes pointer to the written data
 * @param size number of bytes written
 */
static inline void write_sram(u32 sram_base, const u8* bytes, u32 size)
{
    if (sram_base + size > SRAM_SIZE)
        return;

    for (u32 i = 0; i < size; i++)
    {
        sram_mem[sram_base + i] = bytes[i];
    }
}

/**
 * @brief Read raw binary data from SRAM
 *
 * @sa write_sram
 */
static inline void read_sram(u32 sram_base, u8* bytes, u32 size)
{
    if (sram_base + size > SRAM_SIZE)
        return;

    for (u32 i = 0; i < size; i++)
    {
        bytes[i] = sram_mem[sram_base + i];
    }
}

/**
 * @brief Checks the 7 chars of gbalatro_version after the "GBALATRO_VERSION" prefix
 *         representing the git hash of the code the build is based on.
 *
 * @returns true if the git hash of the ROM is equal to the hash saved in SRAM.
 *          false if they are different.
 */
static inline bool check_hash(const char* prefix)
{
    for (u32 i = 0; i < CHECK_HASH_SIZE; i++)
    {
        if (gbalatro_version[GIT_HASH_START + i] != prefix[i])
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief Determines if the current build is considered "dirty" aka has uncommitted changes.
 *         This works because the gbalatro_version string has "-dirty" added at the end if it's
 *         dirty.
 *
 * @returns true if version is dirty, false otherwise.
 */
static inline bool is_version_dirty(void)
{
    return strlen(gbalatro_version) > GIT_HASH_START + CHECK_HASH_SIZE;
}

/**
 * @brief Reads whether the save data exists and is valid.
 *
 * @param header pointer to the SaveHeader struct to fill
 * @returns true if the save data is valid, false if not
 */
static inline bool get_save_header(SaveHeader* header)
{
    read_sram(HEADER_ADDRESS, (u8*)header, sizeof(*header));
    return (header->magic == CHECK_MAGIC) && header->dirty == is_version_dirty() &&
           check_hash(header->githash);
}

/**
 * @brief Writes a magic number and ROM version info to SRAM to signal that the
 *         save data exists and allow the game to determine if it is compatible.
 *
 * This will read the SaveHeader first and check if the data is valid. If yes, the
 * `valid_sections` will be updated, if not, it will be overwritten and start from
 * `SAVE_SECTION_FLAG_NONE`.
 *
 * @param section_flag The section flag to be set to 1, corresponds to the
 *                      section we're writing to SRAM.
 */
static inline void set_save_header(u32 section_flag)
{
    SaveHeader header;

    // Check for valid data. If it's junk, set all sections as invalid, else keep the flags.
    // Then add the requested flag.
    if (!get_save_header(&header))
    {
        memcpy(&header, &SaveHeader_default, sizeof(SaveHeader_default));
    }

    header.valid_sections |= section_flag;

    header.dirty = is_version_dirty();
    memcpy(&(header.githash), gbalatro_version + GIT_HASH_START, CHECK_HASH_SIZE);

    write_sram(HEADER_ADDRESS, (const u8*)&header, sizeof(header));
}

void save_options(void)
{
    SaveOptions options = SaveOptions_default;

    options.game_speed = g_game_vars.game_speed;
    options.cards_high_contrast = get_cards_high_contrast();
    options.cards_more_readable = get_cards_more_readable();
    options.music_volume = g_game_vars.music_volume;
    options.sound_volume = g_game_vars.sound_volume;

    write_sram(OPTIONS_ADDRESS, (const u8*)&options, sizeof(options));
    set_save_header(SAVE_SECTION_FLAG_OPTIONS);
}

void load_options(void)
{
    SaveHeader header;
    SaveOptions options = SaveOptions_default;

    // If options data doesn't exist or is invalid, just don't read from
    // SRAM and apply the default values
    if (get_save_header(&header) && (header.valid_sections & SAVE_SECTION_FLAG_OPTIONS))
        read_sram(OPTIONS_ADDRESS, (u8*)&options, sizeof(options));

    /*
     * A valid header does not guarantee that the option payload survived.
     * In particular, game_speed is used as an array index by the Settings UI.
     * Clamp all persisted values before exposing them to runtime code.
     */
    if (options.game_speed < GAME_SPEED_MIN || options.game_speed > GAME_SPEED_MAX)
        options.game_speed = DEFAULT_GAME_SPEED;
    if (options.music_volume > VOLUME_OPTION_MAX)
        options.music_volume = DEFAULT_MUSIC_VOLUME;
    if (options.sound_volume > VOLUME_OPTION_MAX)
        options.sound_volume = DEFAULT_SOUND_VOLUME;
    if (options.cards_high_contrast > 1)
        options.cards_high_contrast = DEFAULT_HIGH_CONTRAST;
    if (options.cards_more_readable > 1)
        options.cards_more_readable = DEFAULT_MORE_READABLE;

    g_game_vars.game_speed = options.game_speed;
    g_game_vars.music_volume = options.music_volume;
    g_game_vars.sound_volume = options.sound_volume;

    set_volume(volume_module_step_to_val(g_game_vars.music_volume));
    set_cards_high_contrast(options.cards_high_contrast);
    set_cards_more_readable(options.cards_more_readable);
}

bool is_game_data_valid(void)
{
    SaveHeader header;
    if (!get_save_header(&header) || !(header.valid_sections & SAVE_SECTION_FLAG_GAME))
        return false;
    SaveGame game = SaveGame_default;
    read_sram(GAME_ADDRESS, (u8*)&game, sizeof(game));
    if (game.format_version != SAVE_GAME_FORMAT ||
        memcmp(game.tag_internal, SaveGame_default.tag_internal, SAVE_LABEL_SIZE) != 0 ||
        memcmp(game.tag_jokers, SaveGame_default.tag_jokers, SAVE_LABEL_SIZE) != 0 ||
        memcmp(game.tag_end, SaveGame_default.tag_end, sizeof(game.tag_end)) != 0 ||
        game.card_count <= 0 || game.card_count > MAX_DECK_SIZE ||
        game.rng_info.seed > MAX_BASE36 ||
        game.rng_info.step > RNG_MAX_RESTORE_STEPS ||
        game.timer < 0 || game.round < 0 ||
        game.round > MAX_ANTE * NUM_BLINDS_PER_ANTE ||
        game.ante < SAVE_MIN_ANTE || game.ante > MAX_ANTE ||
        game.money < 0 || game.hand_size < 1 || game.hand_size > MAX_HAND_SIZE ||
        game.deck_type < 0 || game.deck_type >= DECK_TYPE_MAX ||
        game.current_blind < BLIND_TYPE_SMALL ||
        game.current_blind >= BLIND_TYPE_MAX ||
        game.next_boss_blind < BLIND_TYPE_BOSS ||
        game.next_boss_blind >= BLIND_TYPE_MAX ||
        game.alchemy.count > ALCHEMICAL_HELD_LIMIT ||
        !voucher_state_is_valid(&game.vouchers) ||
        game.vouchers.offer_ante < VOUCHER_INVALID_ID ||
        game.vouchers.offer_ante > MAX_ANTE)
        return false;

    int expected_current_slot =
        game.current_blind == BLIND_TYPE_SMALL
            ? SMALL_BLIND
            : game.current_blind == BLIND_TYPE_BIG ? BIG_BLIND : BOSS_BLIND;
    int current_slots = 0;
    for (int i = 0; i < NUM_BLINDS_PER_ANTE; i++)
    {
        if (game.blinds_states[i] < BLIND_STATE_CURRENT ||
            game.blinds_states[i] >= BLIND_STATE_MAX)
            return false;
        current_slots += game.blinds_states[i] == BLIND_STATE_CURRENT;
    }
    /*
     * A resumable snapshot represents the blind-select screen after the shop.
     * More than one CURRENT marker, or a marker that disagrees with
     * current_blind, sends selection/render code down incompatible branches.
     */
    if (current_slots != 1 ||
        game.blinds_states[expected_current_slot] != BLIND_STATE_CURRENT)
        return false;
    for (int i = 0; i < expected_current_slot; i++)
        if (game.blinds_states[i] != BLIND_STATE_DEFEATED &&
            game.blinds_states[i] != BLIND_STATE_SKIPPED)
            return false;
    for (int i = expected_current_slot + 1; i < NUM_BLINDS_PER_ANTE; i++)
        if (game.blinds_states[i] != BLIND_STATE_UPCOMING)
            return false;
    for (int i = 0; i < game.alchemy.count; i++)
        if (alchemical_get_info((enum AlchemicalId)game.alchemy.held[i]) == NULL)
            return false;
    for (int i = game.alchemy.count; i < ALCHEMICAL_HELD_LIMIT; i++)
        if (game.alchemy.held[i] != ALCHEMICAL_INVALID_ID)
            return false;
    for (int i = 0; i < ALCHEMICAL_HAND_TYPE_COUNT; i++)
        if (game.alchemy.hand_levels[i] > 20)
            return false;
    for (int i = 0; i < game.card_count; i++)
        if (game.cards[i].suit >= NUM_SUITS || game.cards[i].rank >= NUM_RANKS ||
            game.cards[i].enhancement >= CARD_ENHANCEMENT_COUNT ||
            game.cards[i].edition >= CARD_EDITION_COUNT ||
            game.cards[i].seal >= CARD_SEAL_COUNT ||
            game.cards[i].alchemy_original_suit >= NUM_SUITS ||
            game.cards[i].alchemy_flags != 0 ||
            (game.cards[i].boss_flags & (u8)~CARD_BOSS_PLAYED_ANTE) != 0)
            return false;
    bool reached_joker_end = false;
    int saved_joker_count = 0;
    int negative_joker_count = 0;
    for (int i = 0; i < MAX_JOKERS_HELD_SIZE; i++)
    {
        JokerObjectSaveData data = game.jokers_data[i];
        if (data.id == (u32)UNDEFINED)
        {
            reached_joker_end = true;
            continue;
        }
        if (reached_joker_end)
            return false;
        if (data.id >= get_joker_registry_size() || data.modifier >= MAX_EDITIONS)
            return false;
        if (data.id == SELTZER_JOKER_ID &&
            (data.persistent_state < 1 || data.persistent_state > 10))
            return false;
        saved_joker_count++;
        negative_joker_count += data.modifier == NEGATIVE_EDITION;
    }
    int saved_joker_capacity = deck_get_joker_capacity(
        (enum DeckType)game.deck_type,
        voucher_get_joker_capacity(&game.vouchers, BASE_JOKERS_HELD_SIZE)
    );
    saved_joker_capacity =
        min(MAX_JOKERS_HELD_SIZE, saved_joker_capacity + negative_joker_count);
    if (saved_joker_count > saved_joker_capacity)
        return false;
    return true;
}

void save_game(void)
{
    SaveGame game = SaveGame_default;

    // Fixed data

    game.timer = g_game_vars.timer;
    game.rng_info = g_game_vars.rng_info;
    /*
     * Resume reconstructs libc's RNG sequence by replaying it.  Never write a
     * snapshot that the bounded restore path would later reject or hang on.
     * Runs beyond the replay budget resume from the nearest safe checkpoint.
     */
    game.rng_info.step = min(game.rng_info.step, RNG_MAX_RESTORE_STEPS);
    game.round = g_game_vars.round;
    game.ante = g_game_vars.ante;
    game.money = g_game_vars.money;
    game.format_version = SAVE_GAME_FORMAT;
    game.hand_size = g_game_vars.hand_size;
    game.deck_type = g_game_vars.deck;
    game.current_blind = g_game_vars.current_blind;
    game.next_boss_blind = g_game_vars.next_boss_blind;
    for (int i = 0; i < NUM_BLINDS_PER_ANTE; i++)
        game.blinds_states[i] = g_game_vars.blinds_states[i];
    game.alchemy = g_game_vars.alchemy;
    game.vouchers = g_game_vars.vouchers;
    memcpy(
        game.hand_play_counts,
        g_game_vars.hand_play_counts,
        sizeof(game.hand_play_counts)
    );
    game.card_count = game_export_deck(game.cards, MAX_DECK_SIZE);
    /*
     * A run with no exportable deck cannot be resumed. Keep the previous
     * valid SRAM snapshot instead of replacing it with an invalid save.
     */
    if (game.card_count <= 0 || game.card_count > MAX_DECK_SIZE)
        return;

    // Lists

    List* jokers_list = get_jokers_list();
    ListItr itr = list_itr_create(jokers_list);
    int i = 0;
    JokerObject* joker_object = NULL;
    while (i < MAX_JOKERS_HELD_SIZE &&
           (joker_object = list_itr_next(&itr)) != NULL)
    {
        if (joker_object->joker == NULL)
            continue;
        JokerObjectSaveData data = {
            (u32)joker_object->joker->id,
            (u32)joker_object->joker->modifier,
            joker_object->joker->scoring_state,
            joker_object->joker->persistent_state
        };
        game.jokers_data[i++] = data;
    }
    for (; i < MAX_JOKERS_HELD_SIZE; i++)
    {
        JokerObjectSaveData data = {UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED};
        game.jokers_data[i] = data;
    }

    write_sram(GAME_ADDRESS, (const u8*)&game, sizeof(game));
    set_save_header(SAVE_SECTION_FLAG_GAME);
}

bool load_game(void)
{
    SaveHeader header;

    if (!get_save_header(&header) ||
        !(header.valid_sections & SAVE_SECTION_FLAG_GAME) ||
        !is_game_data_valid())
        return false;

    SaveGame game = SaveGame_default;
    read_sram(GAME_ADDRESS, (u8*)&game, sizeof(game));

    if (game.format_version != SAVE_GAME_FORMAT ||
        !game_restore_deck(game.cards, game.card_count))
    {
        return false;
    }

    while (!list_is_empty(get_jokers_list()))
    {
        JokerObject* joker_object = list_get_at_idx(get_jokers_list(), 0);
        if (joker_object != NULL && joker_object->joker != NULL)
            game_shop_set_joker_avail(joker_object->joker->id, true);
        remove_owned_joker(0);
        joker_object_destroy(&joker_object);
    }

    for (int i = 0; i < MAX_JOKERS_HELD_SIZE; i++)
    {
        JokerObjectSaveData data = game.jokers_data[i];
        if (data.id == (u32)UNDEFINED)
            break;

        Joker* joker = joker_new_with_modifier((u8)data.id, (u8)data.modifier);
        if (joker == NULL)
            goto load_failed;
        joker->scoring_state = data.scoring_state;
        joker->persistent_state = data.persistent_state;

        JokerObject* joker_object = joker_object_new(joker);
        if (joker_object == NULL)
        {
            joker_destroy(&joker);
            goto load_failed;
        }
        if (!add_joker(joker_object))
        {
            joker_object_destroy(&joker_object);
            goto load_failed;
        }
        game_shop_set_joker_avail(joker->id, false);
    }

    /*
     * Commit scalar run state only after every fixed-pool object has been
     * reconstructed. Resume is therefore all-or-nothing instead of opening a
     * run with missing Jokers or half a deck.
     */
    g_game_vars.timer = game.timer;
    rng_restore(game.rng_info);
    g_game_vars.round = game.round;
    g_game_vars.ante = game.ante;
    g_game_vars.money = game.money;
    g_game_vars.hand_size = clamp(game.hand_size, 1, MAX_HAND_SIZE);
    g_game_vars.deck = clamp(game.deck_type, 0, DECK_TYPE_MAX - 1);
    g_game_vars.current_blind =
        clamp(game.current_blind, BLIND_TYPE_SMALL, BLIND_TYPE_MAX - 1);
    g_game_vars.next_boss_blind =
        clamp(game.next_boss_blind, BLIND_TYPE_BOSS, BLIND_TYPE_MAX - 1);
    for (int i = 0; i < NUM_BLINDS_PER_ANTE; i++)
        g_game_vars.blinds_states[i] =
            clamp(game.blinds_states[i], BLIND_STATE_CURRENT, BLIND_STATE_MAX - 1);

    alchemical_inventory_reset(&g_game_vars.alchemy);
    for (int i = 0; i < game.alchemy.count; i++)
        alchemical_inventory_add(&g_game_vars.alchemy, game.alchemy.held[i]);
    g_game_vars.alchemy.used_count = game.alchemy.used_count;
    for (int i = 0; i < ALCHEMICAL_HAND_TYPE_COUNT; i++)
        g_game_vars.alchemy.hand_levels[i] = game.alchemy.hand_levels[i];

    voucher_state_reset(&g_game_vars.vouchers);
    g_game_vars.vouchers = game.vouchers;
    memcpy(
        g_game_vars.hand_play_counts,
        game.hand_play_counts,
        sizeof(g_game_vars.hand_play_counts)
    );
    return true;

load_failed:
    while (!list_is_empty(get_jokers_list()))
    {
        JokerObject* joker_object = list_get_at_idx(get_jokers_list(), 0);
        if (joker_object != NULL && joker_object->joker != NULL)
            game_shop_set_joker_avail(joker_object->joker->id, true);
        remove_owned_joker(0);
        joker_object_destroy(&joker_object);
    }
    /*
     * A failed Resume must not leave its successfully reconstructed deck in
     * memory. Otherwise choosing New Run afterwards would mistake that deck
     * for a loaded run and skip new-run initialization.
     */
    game_clear_deck();
    return false;
}

void clear_game_save(void)
{
    SaveHeader header;
    if (!get_save_header(&header))
        return;
    header.valid_sections &= ~SAVE_SECTION_FLAG_GAME;
    write_sram(HEADER_ADDRESS, (const u8*)&header, sizeof(header));
}
