/* Game table, path grammar, discovery and direct-start contracts. */
#include <dirent.h>
#include <errno.h>
static int fail_directory_eof;
static struct dirent *test_readdir(DIR *directory);
#define readdir test_readdir
#include "wow-channels.h"
#undef readdir
#include "forever-setup.h"
#include <assert.h>
#include <stdlib.h>
static char root[256];
static struct dirent *test_readdir(DIR *directory)
{
    struct dirent *entry = readdir(directory);
    if (!entry && fail_directory_eof) errno = EIO;
    return entry;
}
static void put(const char *relative, const char *text)
{
    char path[2048], *slash;
    assert(snprintf(path, sizeof(path), "%s/%s", root, relative) > 0);
    slash = strrchr(path, '/'); assert(slash); *slash = 0;
    assert(!forever_mkdirs(path)); *slash = '/';
    assert(!forever_write_file(path, text, strlen(text)));
}
static int collect(const struct wow_channel *channel, void *context)
{
    unsigned *count = context;
    assert(wow_folder_name(channel->folder) && channel->unix_loader[0]); ++*count; return 0;
}
int main(void)
{
    char pattern[] = "/tmp/forever-games-XXXXXX", prefix[1024], executable[256], loader[256];
    char d4[1100];
    struct wow_channel channel;
    unsigned count = 0;
    assert(mkdtemp(pattern)); snprintf(root, sizeof(root), "%s", pattern);
    snprintf(prefix, sizeof(prefix), "%s/prefix", root);
    snprintf(d4, sizeof(d4), "%s/drive_c/Program Files (x86)/Diablo IV", prefix);

    assert(wow_executable_name("D2R.exe")); assert(wow_executable_name("diablo iv.EXE"));
    assert(!wow_executable_name("Wow.exe")); assert(!wow_executable_name("Diablo IV Launcher.exe"));
    assert(wow_folder_name("d2r") && wow_folder_name("D4") && !wow_folder_name("_retail_"));
    assert(wow_uid_valid("osi") && wow_uid_valid("FENRIS") && !wow_uid_valid("wow"));

    assert(!wow_channel_parse("C:\\Program Files (x86)\\Diablo IV\\Diablo IV.exe", &channel));
    assert(!strcmp(channel.folder, "d4") && !strcmp(channel.product, "fenris"));
    assert(!strcmp(channel.windows_cwd, "C:\\Program Files (x86)\\Diablo IV"));
    assert(!wow_channel_parse("w:/Games/diablo ii resurrected/d2r.exe", &channel));
    assert(!strcmp(channel.folder, "d2r") && !strcmp(channel.executable, "d2r.exe"));
    assert(wow_channel_parse("D:\\Diablo IV\\Diablo IV.exe", &channel) < 0);
    assert(wow_channel_parse("C:\\Diablo II Resurrected\\Diablo IV.exe", &channel) < 0);
    assert(wow_channel_parse("C:\\Diablo IV\\nested\\Diablo IV.exe", &channel) < 0);
    assert(wow_channel_parse("C:\\Diablo IV\\..\\Diablo IV.exe", &channel) < 0);
    assert(wow_channel_parse("C:\\World of Warcraft\\_retail_\\Wow.exe", &channel) < 0);

    assert(wow_channel_select(prefix, "", &channel) == -ENOENT);
    put("prefix/drive_c/Program Files (x86)/Diablo IV/Diablo IV.exe", "MZ");
    assert(!wow_channels_scan(prefix, collect, &count) && count == 1);
    assert(!wow_channel_select(prefix, "", &channel) && !strcmp(channel.folder, "d4"));
    assert(!strcmp(channel.unix_cwd, d4) && !strcmp(channel.executable, "Diablo IV.exe"));
    assert(!strcmp(wow_channel_launch_uid(&channel), "fenris"));
    assert(!strcmp(wow_channel_label(&channel), "Diablo IV"));
    assert(wow_channel_select(prefix, "d2r", &channel) == -ENOENT);
    assert(wow_channel_select(prefix, "_retail_", &channel) == -EINVAL);

    put("prefix/drive_c/Program Files (x86)/Diablo II Resurrected/d2r.EXE", "MZ");
    count = 0;
    assert(!wow_channels_scan(prefix, collect, &count) && count == 2);
    assert(!wow_channel_select(prefix, "", &channel) && !strcmp(channel.folder, "d2r"));
    assert(!wow_channel_select(prefix, "D2R", &channel) && !strcmp(channel.executable, "d2r.EXE"));
    assert(!strcmp(channel.windows_cwd, "C:\\Program Files (x86)\\Diablo II Resurrected"));

    assert(!wow_channel_resolve_files(d4, "diablo iv.exe", executable, loader, NULL));
    assert(!strcmp(executable, "Diablo IV.exe") && !strcmp(loader, executable));
    assert(wow_channel_resolve_files(d4, "Wow.exe", executable, loader, NULL) == -EINVAL);
    fail_directory_eof = 1;
    assert(wow_channel_resolve_files(d4, "D2R.exe", executable, loader, NULL) == -EIO);
    fail_directory_eof = 0;

    puts("game table discovery, path grammar and direct-start selection PASS");
    return 0;
}
