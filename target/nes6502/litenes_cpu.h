#ifndef LITENES_CPU_H
#define LITENES_CPU_H

#include <stdio.h>
#include <stdio.h>

extern const char *cpu_op_name[256];                   
extern const char *cpu_op_address_mode[256]; 
extern FILE *g_fp;
void init_log(void);

#endif