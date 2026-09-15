/*
    Host test for `install.c` (not part of the build):
        cc -std=gnu11 -Wall -Wextra -o install_test install.c install_test.c && ./install_test /tmp/somewhere
*/

#include "install.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures;

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
            failures++;                                                         \
        }                                                                       \
    } while (0)

static void make_path(char *out, const char *root, const char *rel)
{
    snprintf(out, PATH_MAX, "%s/%s", root, rel);
}

static void write_file(const char *root, const char *rel, const char *content)
{
    char path[PATH_MAX];
    make_path(path, root, rel);
    mkdir(root, 0777);
    for (char *p = path + strlen(root) + 1; (p = strchr(p, '/')); p++) {
        *p = '\0';
        mkdir(path, 0777);
        *p = '/';
    }
    FILE *fp = fopen(path, "w");
    if (!fp) {
        perror(path);
        exit(2);
    }
    fputs(content, fp);
    fclose(fp);
}

static bool exists(const char *root, const char *rel)
{
    char path[PATH_MAX];
    struct stat st;
    make_path(path, root, rel);
    return stat(path, &st) == 0;
}

static bool has_content(const char *root, const char *rel, const char *expected)
{
    char path[PATH_MAX], buf[4096] = "";
    make_path(path, root, rel);
    FILE *fp = fopen(path, "r");
    if (!fp)
        return false;
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    return strcmp(buf, expected) == 0;
}

static unsigned progress_calls, progress_done, progress_total;

static void on_progress(unsigned done, unsigned total, void *userdata)
{
    CHECK(userdata == &progress_calls);
    progress_calls++;
    progress_done = done;
    progress_total = total;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s EMPTY_DIRECTORY\n", argv[0]);
        return 2;
    }
    const char *base = argv[1];
    char src[PATH_MAX], dst[PATH_MAX], fresh[PATH_MAX], empty[PATH_MAX], path[PATH_MAX];
    make_path(src, base, "romfs");
    make_path(dst, base, "sdcard");
    make_path(fresh, base, "fresh");
    make_path(empty, base, "empty");
    mkdir(empty, 0777);

    /* Embedded tree, identified by `.install-id`: one plugin of the previous build is gone. */
    write_file(src, ".install-id", "tree2\n");
    write_file(src, "git-rev", "v2\n");
    write_file(src, "reader.lua", "reader v2");
    write_file(src, "plugins/kept.koplugin/main.lua", "kept v2");

    /* Installed tree (same version, previous build), with files added by the user, and a tampered manifest. */
    write_file(dst, ".install-id", "tree1\n");
    write_file(dst, "git-rev", "v2\n");
    write_file(dst, "reader.lua", "reader v1");
    write_file(dst, "plugins/gone.koplugin/main.lua", "gone");
    write_file(dst, "plugins/gone.koplugin/sub/extra.lua", "gone too");
    write_file(dst, "plugins/kept.koplugin/main.lua", "kept v1");
    write_file(dst, "plugins/user.koplugin/main.lua", "user plugin");
    write_file(dst, "settings.reader.lua", "user settings");
    write_file(dst, ".install-manifest",
               ".install-id\ngit-rev\nreader.lua\n"
               "plugins/gone.koplugin/main.lua\nplugins/gone.koplugin/sub/extra.lua\n"
               "plugins/kept.koplugin/main.lua\n"
               "../outside.txt\n/etc/hosts\nplugins/../settings.reader.lua\n./settings.reader.lua\n"
               "plugins//user.koplugin/main.lua\nplugins\n\n");
    write_file(base, "outside.txt", "outside");

    /* Update. */
    CHECK(ko_install_needed(src, dst));
    CHECK(ko_install(src, dst, on_progress, &progress_calls));
    CHECK(!ko_install_needed(src, dst));
    CHECK(has_content(dst, ".install-id", "tree2\n"));
    CHECK(has_content(dst, "git-rev", "v2\n"));
    CHECK(has_content(dst, "reader.lua", "reader v2"));
    CHECK(has_content(dst, "plugins/kept.koplugin/main.lua", "kept v2"));
    CHECK(progress_calls == 3 && progress_done == 3 && progress_total == 3);
    /* Obsolete files are removed, along with the directories they leave empty. */
    CHECK(!exists(dst, "plugins/gone.koplugin/main.lua"));
    CHECK(!exists(dst, "plugins/gone.koplugin/sub/extra.lua"));
    CHECK(!exists(dst, "plugins/gone.koplugin"));
    CHECK(exists(dst, "plugins/kept.koplugin"));
    /* The user's files, and anything outside the installation, are left alone. */
    CHECK(has_content(dst, "plugins/user.koplugin/main.lua", "user plugin"));
    CHECK(has_content(dst, "settings.reader.lua", "user settings"));
    CHECK(has_content(base, "outside.txt", "outside"));
    CHECK(exists(dst, "plugins"));
    /* The manifest now lists this build's files. */
    CHECK(has_content(dst, ".install-manifest", ".install-id\ngit-rev\nplugins/kept.koplugin/main.lua\nreader.lua\n"));
    CHECK(!exists(dst, ".install-manifest.tmp"));

    /* Requested reinstall: the marker is consumed. */
    write_file(dst, ".reinstall", "");
    CHECK(ko_install_needed(src, dst));
    CHECK(!exists(dst, ".reinstall"));
    CHECK(!ko_install_needed(src, dst));

    /* Installation made by a build without `.install-id`: reinstalled. */
    make_path(path, dst, ".install-id");
    unlink(path);
    CHECK(ko_install_needed(src, dst));

    /* Fresh install, into a directory that doesn't exist yet, without progress callback. */
    CHECK(ko_install_needed(src, fresh));
    CHECK(ko_install(src, fresh, NULL, NULL));
    CHECK(has_content(fresh, "plugins/kept.koplugin/main.lua", "kept v2"));
    CHECK(has_content(fresh, ".install-id", "tree2\n"));
    CHECK(!ko_install_needed(src, fresh));

    /* Interrupted install (no marker): needed again, and resumed. */
    make_path(path, fresh, ".install-id");
    unlink(path);
    CHECK(ko_install_needed(src, fresh));
    CHECK(ko_install(src, fresh, NULL, NULL));
    CHECK(!ko_install_needed(src, fresh));

    /* A tree without marker is never installed. */
    CHECK(!ko_install_needed(empty, fresh));
    CHECK(!ko_install(empty, fresh, NULL, NULL));
    CHECK(has_content(fresh, ".install-id", "tree2\n"));

    /* A missing source fails cleanly. */
    make_path(path, base, "missing");
    CHECK(!ko_install(path, fresh, NULL, NULL));

    printf(failures ? "install_test: %d failure(s)\n" : "install_test: all checks passed\n", failures);
    return failures != 0;
}
