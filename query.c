/* Query match data
 * This file is part of jdupes; see jdupes.c for license information */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <libjodycode.h>
#include "jdupes.h"
#include "sizetree.h"
#include "query.h"


qstate_t *query_new_state(uint32_t options)
{
	qstate_t *qs = (qstate_t *)malloc(sizeof(qstate_t));
	if (qs == NULL) jc_oom("qstate_init_set");

	// Populate chains with sorting

	return qs;
}
