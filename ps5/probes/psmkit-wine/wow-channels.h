/* Battle.net game table shared by launch admission and direct start.
 * The wow_* names are historical: a "channel" is one supported game, and its
 * folder is the game id. Only the games listed here are admitted. */
#ifndef FOREVER_WOW_CHANNELS_H
#define FOREVER_WOW_CHANNELS_H
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#define WOW_PATH_BYTES 1100
#define WOW_FOLDER_BYTES 256
#define WOW_PRODUCT_BYTES 128
struct wow_channel
{
    char folder[WOW_FOLDER_BYTES], product[WOW_PRODUCT_BYTES];
    char windows_cwd[WOW_PATH_BYTES], unix_cwd[WOW_PATH_BYTES];
    char executable[WOW_FOLDER_BYTES], unix_loader[WOW_PATH_BYTES];
};
typedef int (*wow_channel_fn)(const struct wow_channel *, void *);
static inline int wow_regular(const char *path)
{
    struct stat st;
    return !stat(path, &st) && S_ISREG(st.st_mode);
}
static inline int wow_join(char *out, size_t size, const char *directory, const char *name)
{
    int n = snprintf(out, size, "%s/%s", directory, name);
    return n < 0 || (size_t)n >= size ? -ENAMETOOLONG : 0;
}
struct wow_game { const char *id, *root, *executable, *uid, *label; };
static const struct wow_game wow_games[] = {
    {"d2r", "Diablo II Resurrected", "D2R.exe", "osi", "Diablo II: Resurrected"},
    {"d4", "Diablo IV", "Diablo IV.exe", "fenris", "Diablo IV"},
};
#define WOW_GAME_COUNT (sizeof(wow_games) / sizeof(wow_games[0]))
static inline const struct wow_game *wow_game_by_executable(const char *name)
{
    for (size_t i = 0; i < WOW_GAME_COUNT; ++i)
        if (!strcasecmp(name, wow_games[i].executable)) return &wow_games[i];
    return NULL;
}
static inline const struct wow_game *wow_game_by_id(const char *id)
{
    for (size_t i = 0; i < WOW_GAME_COUNT; ++i)
        if (!strcasecmp(id, wow_games[i].id)) return &wow_games[i];
    return NULL;
}
static inline int wow_executable_name(const char *name) { return wow_game_by_executable(name) != NULL; }
static inline int wow_folder_name(const char *name) { return wow_game_by_id(name) != NULL; }
static inline int wow_uid_valid(const char *uid)
{
    for (size_t i = 0; i < WOW_GAME_COUNT; ++i)
        if (!strcasecmp(uid, wow_games[i].uid)) return 1;
    return 0;
}
static inline int wow_channel_parse(const char *image, struct wow_channel *channel)
{
    char path[WOW_PATH_BYTES], *exe, *root;
    const struct wow_game *game;
    size_t n = strlen(image);
    if (n >= sizeof(path) || n < 3 || (image[0] != 'C' && image[0] != 'c' && image[0] != 'W' && image[0] != 'w') ||
        image[1] != ':' || (image[2] != '/' && image[2] != '\\')) return -EINVAL;
    memcpy(path, image, n + 1);
    for (size_t i = 2; i < n; ++i) if (path[i] == '\\') path[i] = '/';
    if (strstr(path, "/../") || strstr(path, "/./") || strstr(path, "//")) return -EINVAL;
    exe = strrchr(path, '/');
    if (!exe || !(game = wow_game_by_executable(exe + 1))) return -EINVAL;
    *exe++ = 0; root = strrchr(path, '/');
    if (!root || strcasecmp(root + 1, game->root)) return -EINVAL;
    memset(channel, 0, sizeof(*channel));
    snprintf(channel->folder, sizeof(channel->folder), "%s", game->id);
    snprintf(channel->product, sizeof(channel->product), "%s", game->uid);
    snprintf(channel->executable, sizeof(channel->executable), "%s", exe);
    n = (size_t)(exe - path - 1);
    memcpy(channel->windows_cwd, image, n); channel->windows_cwd[n] = 0;
    return 0;
}
static inline int wow_product_valid(const char *text)
{
    size_t n = strlen(text);
    if (!n || n >= WOW_PRODUCT_BYTES) return 0;
    for (size_t i = 0; i < n; ++i)
        if (!((text[i] >= 'a' && text[i] <= 'z') || (text[i] >= 'A' && text[i] <= 'Z') ||
              (text[i] >= '0' && text[i] <= '9') || text[i] == '_' || text[i] == '-')) return 0;
    return 1;
}
static inline int wow_ascii_equal(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b)
    {
        unsigned char left = *a, right = *b;
        if (left >= 'A' && left <= 'Z') left += 'a' - 'A';
        if (right >= 'A' && right <= 'Z') right += 'a' - 'A';
        if (left != right) return 0;
    }
    return !*a && !*b;
}
/* Windows spelling is insensitive to ASCII case, but two native case
 * variants are ambiguous. The output uses on-disk case. Diablo games have no
 * loader DLL; loader receives the executable so callers keep one contract. */
static inline int wow_channel_resolve_files(const char *cwd, const char *requested,
                                            char *executable, char *loader, void *context)
{
    char path[WOW_PATH_BYTES];
    unsigned matches = 0;
    struct dirent *entry;
    DIR *directory;
    int error = 0;
    (void)context;
    if (!wow_executable_name(requested)) return -EINVAL;
    if (!(directory = opendir(cwd))) return -errno;
    errno = 0;
    while ((entry = readdir(directory)))
        if (wow_ascii_equal(entry->d_name, requested))
        {
            if (++matches > 1) error = -EEXIST;
            else snprintf(executable, WOW_FOLDER_BYTES, "%s", entry->d_name);
        }
    if (errno && !error) error = -errno;
    closedir(directory);
    if (error) return error;
    if (matches != 1) return -ENOENT;
    if (wow_join(path, sizeof(path), cwd, executable)) return -ENAMETOOLONG;
    if (!wow_regular(path)) return -ENOENT;
    if (loader) snprintf(loader, WOW_FOLDER_BYTES, "%s", executable);
    return 0;
}

static inline int wow_channel_read(const char *prefix, const struct wow_game *game,
                                   struct wow_channel *channel)
{
    char relative[WOW_PATH_BYTES], executable[WOW_FOLDER_BYTES];
    int n, error;
    memset(channel, 0, sizeof(*channel));
    strcpy(channel->folder, game->id);
    strcpy(channel->product, game->uid);
    n = snprintf(relative, sizeof(relative), "drive_c/Program Files (x86)/%s", game->root);
    if (n < 0 || (size_t)n >= sizeof(relative) ||
        wow_join(channel->unix_cwd, sizeof(channel->unix_cwd), prefix, relative)) return -ENAMETOOLONG;
    n = snprintf(channel->windows_cwd, sizeof(channel->windows_cwd), "C:\\Program Files (x86)\\%s", game->root);
    if (n < 0 || (size_t)n >= sizeof(channel->windows_cwd)) return -ENAMETOOLONG;
    error = wow_channel_resolve_files(channel->unix_cwd, game->executable, executable, NULL, NULL);
    if (error == -ENOTDIR) error = -ENOENT;
    if (error) return error;
    strcpy(channel->executable, executable);
    /* A non-empty unix_loader marks an installed, launchable game. */
    return wow_join(channel->unix_loader, sizeof(channel->unix_loader), channel->unix_cwd, executable);
}
static inline int wow_channels_scan(const char *prefix, wow_channel_fn visit, void *context)
{
    struct wow_channel channel;
    int error;
    for (size_t i = 0; i < WOW_GAME_COUNT; ++i)
    {
        error = wow_channel_read(prefix, &wow_games[i], &channel);
        if (error == -ENOENT || error == -EEXIST) continue;
        if (error || (error = visit(&channel, context))) return error;
    }
    return 0;
}
struct wow_selection { const char *name; struct wow_channel *channel; int found; };
static inline int wow_select_visit(const struct wow_channel *channel, void *context)
{
    struct wow_selection *selection = context;
    if (selection->found || (selection->name[0] && strcasecmp(channel->folder, selection->name))) return 0;
    *selection->channel = *channel; selection->found = 1;
    return 0;
}
/* An empty name picks the first installed game in table order. */
static inline int wow_channel_select(const char *prefix, const char *name, struct wow_channel *channel)
{
    struct wow_selection selection = {name, channel, 0};
    int error;
    if (*name && !wow_folder_name(name)) return -EINVAL;
    error = wow_channels_scan(prefix, wow_select_visit, &selection);
    return error ? error : selection.found ? 0 : -ENOENT;
}
static inline const char *wow_channel_launch_uid(const struct wow_channel *channel)
{
    return channel->product;
}
static inline const char *wow_channel_label(const struct wow_channel *channel)
{
    const struct wow_game *game = wow_game_by_id(channel->folder);
    return game ? game->label : channel->folder;
}
#endif
