/* File order sorting functions
 * This file is part of jdupes; see jdupes.c for license information */

#include <stdio.h>
#include <string.h>

#include <libjodycode.h>
#include "likely_unlikely.h"
#include "jdupes.h"


int sort_by_name_numeric(file_t *file1, file_t *file2)
{
#ifndef NO_USER_ORDER
	int index = file1->user_order - 1;
	if (file1->user_order == file2->user_order)
		return jc_numeric_strcmp(file1->d_name + paramprefix[index], file2->d_name + paramprefix[index], 0);
	else
#endif
		return jc_numeric_strcmp(file1->d_name, file2->d_name, 0);
}
