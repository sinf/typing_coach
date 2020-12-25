#pragma once

typedef char Filepath[1024];

void find_config_dir();

__attribute__((format(printf,2,3)))
void get_path(Filepath p, const char *fmt, ...);


