/*  Copyright (C) MOXA Inc. All rights reserved.

    This software is distributed under the terms of the
    MOXA License.  See the file COPYING-MOXA for details.
*/

#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <stdlib.h>
#include <stdint.h>

typedef enum
{
    LOGGER_DATA_OK            = 0,
	LOGGER_DATA_NOT_FOUND 	  = -1,
	LOGGER_DATA_OP_FAIL       = -2,
    LOGGER_DATA_MALLOC_FAIL   = -3,
    LOGGER_DATA_ARG_INVALID   = -4,
	
} LOGGER_RET;

typedef enum
{
    LOGGER_DATA_OP_DEQUEUE_DATA = 0,
	LOGGER_DATA_OP_KEEP_DATA,
} LOGGER_DATA_OP;

typedef enum
{
    LOGGER_DATA_REMOVE_BY_ID = 0,
	LOGGER_DATA_REMOVE_ALL,
} LOGGER_DATA_REMOVE;

LOGGER_RET logger_data_enq(char *uri, char *jsonStr, uint32_t len);
LOGGER_RET logger_data_deq(char *uri, int64_t *data_id, LOGGER_DATA_OP op_flag, char **jsonStr, uint32_t *len);
LOGGER_RET logger_data_multi_deq(char *uri, int64_t *data_id_list, uint32_t data_cnt, LOGGER_DATA_OP op_flag, char **jsonStr, uint32_t *len);
LOGGER_RET logger_data_remove(char *uri, int64_t data_id, LOGGER_DATA_REMOVE op_flag);
LOGGER_RET logger_data_get(char *uri, int64_t data_id, char **jsonStr, uint32_t *len);
LOGGER_RET logger_data_get_next(char *uri, int64_t last_id, int64_t *data_id, char **jsonStr, uint32_t *len);

#endif /* __LOGGER_H__ */
