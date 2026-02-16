#include <stdlib.h>
#include <string.h>
#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#define NOB_EXPERIMENTAL_DELETE_OLD

#include "./thirdparty/nob.h"

#define BUILD_FOLDER		"build/"
#define SRC_FOLDER		"src/"
#define THIRDPARTY_FOLDER	"thirdparty/"

void build_tool_async(Cmd *cmd, Procs *procs, const char *bin_path, const char *src_path) {
    cmd_append(cmd, "gcc", "-Wall", "-Wextra","-ggdb", "-I"THIRDPARTY_FOLDER, "-march=native","-Ofast","-o", bin_path, src_path, SRC_FOLDER"bpe.c");
    da_append(procs, cmd_run_async_and_reset(cmd));
}


const char *tools[] = {
    "txt2bpe",
    "tkn_inspect",
    "bpe_inspect"

};

int main(int argc, char **argv) {

    NOB_GO_REBUILD_URSELF(argc, argv); // buid only if the nob.c has changed by checking the timestamp of nob.c and nob

    Cmd cmd = {0};
    Procs procs = {0};

    const char *program_name = shift(argv, argc); // take the argument of command

    if(!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1; // check the existence of the build folder

    // the command only has a program name which does not contain the parameters
    // buid the program using the parameters which is in the tools
    if(argc <= 0) {
	for(size_t i = 0; i < ARRAY_LEN(tools); ++i) {
	    const char *bin_path = temp_sprintf(BUILD_FOLDER"%s", tools[i]);
	    const char *src_path = temp_sprintf(SRC_FOLDER"%s.c", tools[i]);
	    build_tool_async(&cmd, &procs, bin_path, src_path);
	}
	if(!nob_procs_wait_and_reset(&procs)) return 1;
	return 0;
    }


    // specify the tool
    const char *tool_name = shift(argv, argc);

    for(size_t i = 0; i < ARRAY_LEN(tools); ++i) {
	// select the special tool
	if(strcmp(tool_name, tools[i]) == 0) {
	    // generate binary executable tool
	    const char *bin_path = temp_sprintf(BUILD_FOLDER"%s", tools[i]);
	    const char *src_path = temp_sprintf(SRC_FOLDER"%s.c", tools[i]);
	    build_tool_async(&cmd, &procs, bin_path, src_path);
	    if(!nob_procs_wait_and_reset(&procs)) return 1;

	    // run the executable tool
	    cmd_append(&cmd, bin_path);
	    da_append_many(&cmd, argv, argc);
	    if(!cmd_run_sync_and_reset(&cmd)) return 1;
	    return 0;
	}
    }

    nob_log(ERROR, "Unkown tool `%s`", tool_name);
    nob_log(ERROR, "Available tools:");
    for(size_t i = 0; i < ARRAY_LEN(tools); ++i) {
	nob_log(ERROR, "  %s", tools[i]);
    }
    
    return 0;
}
