#include "persist.h"
#include "ini.h"

Filepath the_settings_path="";

// int opt_auto_space
#include "tm.h"
// int sb_continue_on_typo
#include "spambox.h"
// const char *database_path
#include "database.h"
// const char *debug_file_path
#include "debug.h"

static IniVar vars[] = {
	INI_STR("database", &database_path),
	INI_INT("auto_space", &opt_auto_space),
	INI_INT("continue_on_typo", &sb_continue_on_typo),
	INI_STR("debug_log", &debug_file_path),
	INI_END
};

void load_settings(void)
{
	ini_settings_read(vars, the_settings_path);
}

void save_settings(void)
{
	ini_settings_write(vars, the_settings_path);
}

