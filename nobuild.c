#include <stdbool.h>

#define NOBUILD_IMPLEMENTATION
#include "./nobuild.h"

#define GAME_EXE_NAME "Juelsminde Joust"
#define BUILD_DIR "build"

static Cstr_Array compiler_flags = {0};
static Cstr_Array linker_flags = {0};
static Cstr_Array compilation_units = {0};

#define CF(flag) (compiler_flags = cstr_array_append(compiler_flags, (flag)))
#define LD(flag) (linker_flags = cstr_array_append(linker_flags, (flag)))
#define MODULE(mod) (compilation_units = cstr_array_append(compilation_units, (mod)))

void common_cflags(void) {
    CF("clang");
    CF("-std=c99");
    CF("-o");
    CF(GAME_EXE_NAME);
    CF("-Wall");
    CF("-Wextra");
    CF("-pedantic");
    CF("-ftabstop=1");
}

void common_ldflags(void) {
    LD("-lraylib");
    LD("-lGL");
    LD("-lm");
    LD("-ldl");
    LD("-lrt");
    LD("-lX11");
}

void debug_build(void) {
    common_cflags();
    CF("-O0");
    CF("-ggdb");
    CF("-DDEBUG");
    common_ldflags();
}

void release_build(void) {
    common_cflags();
    CF("-O3");
    CF("-DNDEBUG");
    common_ldflags();
}

typedef struct Options {
    bool is_release_build;
    bool run;
} Options;

static Options parse_options(int argc, char **argv) {
    --argc, ++argv;
    
    Options opts = {0};

    for (int i = 0; i < argc; ++i) {
        char *optstr = argv[i];

        if (strcmp("release", optstr) == 0) opts.is_release_build = true;
        else if (strcmp("run", optstr) == 0) opts.run = true;
    }

    return opts;
}

int main(int argc, char **argv) {
    GO_REBUILD_URSELF(argc, argv);

    Options opts = parse_options(argc, argv);

    MODULE("jj_game.c");

    if (opts.is_release_build) {
        release_build();
    }
    else {
        debug_build();
    }

    Cstr_Array args = cstr_array_concat(compiler_flags, compilation_units);
    args = cstr_array_concat(args, linker_flags);

    Cmd compile_command = (Cmd) { .line = args };
    //cmd_show(compile_command);
    cmd_run_sync(compile_command);

    if (opts.run)
    {
        CMD("./" GAME_EXE_NAME);
    }
    return 0;
}

