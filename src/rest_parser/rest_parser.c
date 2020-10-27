/*
 * @brief RESTful API engine.
 *
 * @copyright
 * Copyright (C) MOXA Inc. All rights reserved.
 * This software is distributed under the terms of the
 * MOXA License.  See the file COPYING-MOXA for details.
 *
 * @date    2018/01/08
 * @author  Center Liang
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <rest.h>
#include <pthread.h>
#include "parson.h"

/*****************************************************************************
 * The following define is temporarily placed here,
 * please put to appropriate file.
 ****************************************************************************/

#define MAX_REST_INFO_CNT   5
#define MAX_REST_CB_CNT     100

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

typedef struct REST_API_CB_t
{
    char *uri;
    char *uri_arr;
    int32_t uri_level;

    rest_cb_t post_handle;
    rest_cb_t get_handle;
    rest_cb_t put_handle;
    rest_cb_t patch_handle;
    rest_cb_t delete_handle;

    struct REST_API_CB_t* next;

} REST_API_CB, *REST_API_CB_p;

typedef struct REST_API_INFO_t
{
    char* uri_prefix;
    REST_API_CB *cb;

} REST_API_INFO, *REST_API_INFO_p;

static REST_API_INFO rest_api_info[MAX_REST_INFO_CNT];
static int32_t rest_api_info_cnt = 0;

static char **rest_output_data;
static uint32_t *rest_output_data_size = 0;
static uint32_t rest_output_data_idx = 0;
REST_OUTPUT_INFO *rest_output_data_info;

static char *query_buf;
static int32_t query_buf_size = 0;

static char **restapi_list;
static int32_t restapi_list_size = 0;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

static void store_uri_query_val(char* uri)
{
	char *str = NULL;

	str = strchr(uri, '?');

	if(str != NULL)
	{
		int idx;
		
		idx = (int)(str - uri);
		query_buf = &uri[idx+1];
		query_buf_size  = strlen(query_buf);
//printf("query_buf = '%s'\n", query_buf);
	}
    else
    {
        query_buf      = NULL;
		query_buf_size = 0;
    }
}

static void store_rest_list(char **apiNameList, int32_t list_size)
{
    if (apiNameList != NULL)
    {
        restapi_list      = apiNameList;
        restapi_list_size = list_size;
    }
}

static int32_t init_uri(char *uri, char list[][REST_MAX_URL_NAME_LEN])
{
    char formBuf[REST_MAX_URL_NAME_LEN];
    int nameListCnt = 0;
    char *ptr;
	
    strncpy(formBuf, uri, sizeof(formBuf));

    if((ptr = strchr(formBuf, '?')) != NULL)
        *ptr = '\0';
	
//printf("formBuf = %s\n", formBuf);
    ptr = strtok(formBuf, "/");

	if (ptr == NULL)
		return REST_URL_NOTDEFINE;

	if (strlen(ptr) > REST_MAX_URL_NAME_LEN)
    	return REST_PARAM_FAIL;

    strcpy(list[0], ptr);

    while (ptr != NULL)
    {
//printf ("found '%s'\n", list[nameListCnt]);
        nameListCnt++;

        ptr = strtok(NULL, "/");

        if (ptr != NULL)
        {
            if (strlen(ptr) > REST_MAX_URL_NAME_LEN)
                return REST_PARAM_FAIL;

            strcpy(list[nameListCnt], ptr);
        }

        if (nameListCnt >= REST_MAX_URL_LEVEL)
            break;
    }

	if(nameListCnt > 0)
	{
		char *str = NULL;

		str = strchr(list[nameListCnt-1], '?');
//printf("11 list[nameListCnt-1] = '%s'\n", list[nameListCnt-1]);
		if(str != NULL)
		{
			int idx;
			
			idx = (int)(str - list[nameListCnt-1]);
			list[nameListCnt-1][idx] = '\0';
		}
//printf("22 list[nameListCnt-1] = '%s'\n", list[nameListCnt-1]);
	}
//printf("nameListCnt = %d\n", nameListCnt);
    return nameListCnt;
}

static REST_RET check_uri(char list[][REST_MAX_URL_NAME_LEN], int32_t *item_idx, REST_API_INFO *api_info, int32_t max_item_num)
{
    int32_t i;

    for (i = 0; i < max_item_num; i++)
    {
        if (strcmp(list[0], (char*)api_info[i].uri_prefix) == 0)
        {
            //printf("find uri\n");
            *item_idx = i;
            break;
        }
    }

    if (i == max_item_num)
    {
        printf("Cant't find URL\n");
        return REST_URL_NOTDEFINE;
    }

    return REST_OK;
}

static REST_API_CB_p search_cb(char list[][REST_MAX_URL_NAME_LEN], int32_t list_cnt, REST_API_INFO *api_info)
{
    REST_API_CB_p cb = api_info->cb;
    int i;
//printf("list_cnt = %d\n", list_cnt);
    if (cb == NULL || list_cnt <= 1)
        return NULL;

    while (cb)
    {
//printf(" %d, %d\n", list_cnt, cb->uri_level);
		if((list_cnt-1) >= cb->uri_level)
		{
	        for (i = 0; i < cb->uri_level; i++)
	        {
	        	if(*(cb->uri_arr + (i * REST_MAX_URL_NAME_LEN)) == '#')
	        	{
	//printf("find #\n");
					return cb;
	        	}

				if(*(cb->uri_arr + (i * REST_MAX_URL_NAME_LEN)) == '+')
	        	{
	//printf("find +\n");
					continue;
	        	}
	 				
	//printf("(%s), (%s) %d\n", list[i+1], cb->uri_arr + (i*MAX_URL_NAME_LEN), strcmp(list[i + 1], cb->uri_arr + (i * MAX_URL_NAME_LEN)));
				
				if (strcmp(list[i + 1], cb->uri_arr + (i * REST_MAX_URL_NAME_LEN)) != 0)
	                break;
	        }
	//printf("i = %d, level = %d\n", i , cb->uri_level);
	        if (i == cb->uri_level)
	            return cb;
		}

        cb = cb->next;
    }

    return NULL;
}

REST_RET rest_mem_cpy(char *buf, int32_t buf_size)
{

	if(buf == NULL || buf_size <= 0 ||
	   rest_output_data == NULL || rest_output_data_size == NULL || rest_output_data_info == NULL)
		return REST_OP_INVALID;
	
	if(*rest_output_data == NULL)
	{
		char *ptr = malloc(buf_size);

		if(ptr == NULL)
		{
	        printf("(%s:%d)rest malloc buf fail.\n", __FILE__, __LINE__);
	        return REST_MALLOC_FAIL;
    	}

		*rest_output_data = ptr;
		*rest_output_data_size = buf_size;
		rest_output_data_idx = 0;
	}
	
	if((rest_output_data_idx + buf_size) > *rest_output_data_size)
	{
		char *ptr = realloc(*rest_output_data, rest_output_data_idx + buf_size);

		if(ptr == NULL)
		{
	        printf("(%s:%d)rest malloc buf fail.\n", __FILE__, __LINE__);
	        return REST_MALLOC_FAIL;
    	}

		if(ptr != *rest_output_data)
		{
			*rest_output_data = ptr;
		}
		
		*rest_output_data_size = rest_output_data_idx + buf_size;
	}

	memcpy(*rest_output_data + rest_output_data_idx, buf, buf_size);
	rest_output_data_idx += buf_size;

	return REST_OK;
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

REST_RET rest_find_uri_query_val(char *q_name, char **q_val)
{
    char tmp[query_buf_size+1];
    char * pch;

	if (q_name == NULL || q_val == NULL)
		return REST_PARAM_FAIL;

	if (query_buf == NULL)
		return REST_QUERY_NOT_FOUND;

    strcpy(tmp, query_buf);

    pch = strtok(tmp, "=");

    while (pch)
    {
        if (strcmp(pch, q_name) == 0)
        {
            pch = strtok(NULL, "&");
			
			if((*q_val = malloc(strlen(pch) + 1)) == NULL)
			{
				printf("%s(%d) rest find uri query value malloc fail.\n",  __func__, __LINE__);
				return REST_QUERY_NOT_FOUND;
			}
			
            strcpy(*q_val, pch);
            return REST_OK;
        }
        else
        {

            pch = strtok(NULL, "&");

            if (pch != NULL)
            {
                pch = strtok(NULL, "=");
            }
            else
                break;

        }
    }

	return REST_QUERY_NOT_FOUND;
}

REST_RET rest_find_uri_name(int32_t uri_idx, char **name)
{
	if (name == NULL)
		return REST_PARAM_FAIL;
//printf("%d %d \n", uri_idx, restapi_list_size);
	if(uri_idx >= restapi_list_size)
		return REST_QUERY_NOT_FOUND;
	
    char *name_api = (char*)restapi_list;
	char *sub_uri = name_api + (uri_idx * REST_MAX_URL_NAME_LEN);

	if((*name = malloc(strlen(sub_uri))) == NULL)
	{
		printf("%s(%d) rest find uri query value malloc fail.\n",  __func__, __LINE__);
		return REST_QUERY_NOT_FOUND;
	}

    strcpy(*name, sub_uri);
    
	return REST_OK;
}

REST_RET rest_write(char *data, int32_t data_size)
{
	REST_RET ret = REST_OK;
	
	if(data == NULL || data_size <= 0)
		return REST_OP_INVALID;

	if((ret = rest_mem_cpy(data, data_size)) != REST_OK)
		return ret;
	
	rest_output_data_info->output_data_len  = rest_output_data_idx;
	rest_output_data_info->output_data_type = REST_DATA_TYPE_JSON;
	rest_output_data_info->output_data_desc = NULL;
	
	return ret;
}

REST_RET rest_write_file(char* file_name, char *data, int32_t data_size)
{
	REST_RET ret = REST_OK;
		
	if(data == NULL || data_size <= 0 || file_name == NULL)
		return REST_OP_INVALID;

	if((ret = rest_mem_cpy(data, data_size)) != REST_OK)
		return ret;
	
	rest_output_data_info->output_data_len  = rest_output_data_idx;
	rest_output_data_info->output_data_type = REST_DATA_TYPE_FILE;

	if(rest_output_data_info->output_data_desc == NULL)
	{
		char *ptr = malloc(strlen(file_name));

		if(ptr == NULL)
			return REST_MALLOC_FAIL;

		strcpy(ptr, file_name);
		
		rest_output_data_info->output_data_desc = ptr;
	}
	
	return ret;
}

REST_RET rest_cb_register(int32_t id, char *uri, int32_t op, rest_cb_t handle)
{
	if (id >= MAX_REST_INFO_CNT || uri == NULL)
		return REST_PARAM_FAIL;

	pthread_mutex_lock(&mutex);
	
    int ret = REST_OK, cnt = 0;
    REST_API_CB_p cb = rest_api_info[id].cb;
    REST_API_CB_p last_cb, tmp_cb = NULL;
    char apiNameList[REST_MAX_URL_LEVEL][REST_MAX_URL_NAME_LEN];

    last_cb = cb;

    while (cb && cnt < MAX_REST_CB_CNT)
    {
        if (strcmp(cb->uri, uri) == 0)
        {
            tmp_cb = cb;
            break;
        }

        last_cb = cb;
        cb      = cb->next;
        cnt++;
    }

    if (tmp_cb == NULL)
    {
        tmp_cb = malloc(sizeof(REST_API_CB));

		if(tmp_cb == NULL)
		{
			ret = REST_MALLOC_FAIL;
			goto rest_regtister_done;
		}

        if (rest_api_info[id].cb == NULL)
            rest_api_info[id].cb = tmp_cb;
        else
            last_cb->next = tmp_cb;

        tmp_cb->next = NULL;
        tmp_cb->uri  = uri;
        tmp_cb->uri_level = init_uri(uri, apiNameList);
//printf("tmp_cb->uri_level = %d\n", tmp_cb->uri_level);
        tmp_cb->uri_arr   = malloc(REST_MAX_URL_LEVEL + REST_MAX_URL_NAME_LEN);
		
		if(tmp_cb->uri_arr == NULL)
		{
			if(tmp_cb)
				free(tmp_cb);
			
			ret = REST_MALLOC_FAIL;
			goto rest_regtister_done;
		}
		
        memcpy(tmp_cb->uri_arr, apiNameList, REST_MAX_URL_LEVEL + REST_MAX_URL_NAME_LEN);
    }

    switch (op)
    {
        case REST_OP_POST:
            tmp_cb->post_handle = handle;
            break;

        case REST_OP_GET:
            tmp_cb->get_handle = handle;
            break;

        case REST_OP_PUT:
            tmp_cb->put_handle = handle;
            break;

        case REST_OP_PATCH:
            tmp_cb->patch_handle = handle;
            break;

        case REST_OP_DELETE:
            tmp_cb->delete_handle = handle;
            break;

        default:
            ret = REST_OP_INVALID;
            break;
    }

	
rest_regtister_done:
	
	pthread_mutex_unlock(&mutex);
    return ret;
}

REST_RET rest_cb_unregister(int32_t id)
{
	REST_API_CB *cb = NULL;
	REST_API_CB *tmp_cb = NULL; 
	
	if(id >= MAX_REST_INFO_CNT)
		return REST_PARAM_FAIL;
	
	pthread_mutex_lock(&mutex);
	
	if(rest_api_info_cnt == 0)
		return REST_OP_INVALID;

	rest_api_info[id].uri_prefix = NULL;

	cb = rest_api_info[id].cb;

	while(cb)
	{
		if(cb->uri_arr)
			free(cb->uri_arr);
		
		tmp_cb = cb;
		cb = cb->next;

		free(tmp_cb);
	}
	
	rest_api_info[id].cb = NULL;
	rest_api_info_cnt--;

	pthread_mutex_unlock(&mutex);

	return REST_OK;
}

REST_RET rest_init(char *uri_prefix)
{
	int i, idx = -1, ret = REST_OK;
	
	if (uri_prefix == NULL)
		return REST_PARAM_FAIL;
	
	pthread_mutex_lock(&mutex);
	
    if (rest_api_info_cnt >= MAX_REST_INFO_CNT)
    {
    	ret = REST_INIT_EXCEED;
        goto rest_init_done;
    }
	
	if(rest_api_info_cnt == 0)
	{
		memset(rest_api_info, 0x0, sizeof(rest_api_info));
	
		for(i = 0; i < MAX_REST_INFO_CNT; i++)
		{
			rest_api_info[i].uri_prefix = NULL;
			rest_api_info[i].cb         = NULL;
		}
	}

	for(i = 0; i < MAX_REST_INFO_CNT; i++)
	{
		if(rest_api_info[i].uri_prefix == NULL)
		{
			rest_api_info[rest_api_info_cnt].uri_prefix = uri_prefix;
			rest_api_info_cnt++;
			idx = i;
			break;
		}
	}
	
	if(idx < 0)
    	ret = REST_INIT_EXCEED;
	else
		ret = idx;
	
rest_init_done:

	pthread_mutex_unlock(&mutex);
	return ret;
}

int32_t rest_parser_handler(int32_t op, char *uri,
                            char *input_data, uint32_t input_data_size,
                            REST_OUTPUT_INFO *output_data_info,
                            char **output_data, uint32_t *output_data_size)
{
    char apiNameList[REST_MAX_URL_LEVEL][REST_MAX_URL_NAME_LEN];
    int32_t cnt = 0, info_idx = 0, ret = REST_OK;
    REST_API_CB_p cb;

	pthread_mutex_lock(&mutex);

    if (rest_api_info_cnt == 0)
    {
        ret = REST_HTTP_STATUS_NOT_FOUND;
		goto rest_parser_done;
    }

    if ((cnt = init_uri(uri, apiNameList)) <= 0)
    {
        ret = REST_HTTP_STATUS_NOT_FOUND;
		goto rest_parser_done;
    }

    if (check_uri(apiNameList, &info_idx, &rest_api_info[0], rest_api_info_cnt) != REST_OK)
    {
        ret = REST_HTTP_STATUS_NOT_FOUND;
		goto rest_parser_done;
    }

    if ((cb = search_cb(apiNameList, cnt, &rest_api_info[info_idx])) == NULL)
    {
        ret = REST_HTTP_STATUS_NOT_FOUND;
		goto rest_parser_done;
    }

    store_uri_query_val(uri);
    store_rest_list((char**)apiNameList, cnt);
	
	rest_output_data      = output_data;
	rest_output_data_size = output_data_size;
	rest_output_data_info = output_data_info;
	rest_output_data_idx  = 0;

	output_data_info->output_data_len = 0;
	output_data_info->output_data_type = REST_DATA_TYPE_NULL;
	output_data_info->output_data_desc = NULL;
	
    switch (op)
    {
        case REST_OP_POST:

            if (cb->post_handle)
                ret = cb->post_handle(uri, input_data, input_data_size);
            else
                ret = REST_HTTP_STATUS_NOT_FOUND;
//printf("ret = %d\n", ret);
            break;

        case REST_OP_GET:

            if (cb->get_handle)
                ret = cb->get_handle(uri, input_data, input_data_size);
            else
                ret = REST_HTTP_STATUS_NOT_FOUND;

            break;

        case REST_OP_PATCH:

            if (cb->patch_handle)
                ret = cb->patch_handle(uri, input_data, input_data_size);
            else
                ret = REST_HTTP_STATUS_NOT_FOUND;

            break;

        case REST_OP_PUT:

            if (cb->put_handle)
                ret = cb->put_handle(uri, input_data, input_data_size);
            else
                ret = REST_HTTP_STATUS_NOT_FOUND;

            break;

        case REST_OP_DELETE:

            if (cb->delete_handle)
                ret = cb->delete_handle(uri, input_data, input_data_size);
            else
                ret = REST_HTTP_STATUS_NOT_FOUND;

            break;

        default:
            ret = REST_HTTP_STATUS_NOT_FOUND;
            break;
    }

rest_parser_done:
	
	pthread_mutex_unlock(&mutex);
	
    return ret;
}




