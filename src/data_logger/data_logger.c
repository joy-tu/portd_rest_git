/*
 * @brief DE API.
 *
 * @copyright
 * Copyright (C) MOXA Inc. All rights reserved.
 * This software is distributed under the terms of the
 * MOXA License.  See the file COPYING-MOXA for details.
 *
 * @date    2019/06/12
 * @author  Center Liang
 */

#include <stdio.h>
#include <string.h>

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>
#include <http_server.h>
#include <data_logger.h>
#include <parson.h>

/*****************************************************************************
 * The following define is temporarily placed here,
 * please put to appropriate file.
 ****************************************************************************/

#define DATA_LOGGER_SERVER_HOST     "http://127.0.0.1:12345/logger/queue"

#define DATA_LOGGER_QUERY_ARG_MAX_LEN   200

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

typedef enum
{
    LOGGER_QUERY_SYMBOL_AND = 0,
    LOGGER_QUERY_SYMBOL_QUESTION,

} LOGGER_QUERY_SYMBOL_IDX;

static const char *query_symbol_str[] = {"&", "?"};

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/


/*****************************************************************************
 * Private functions
 ****************************************************************************/

static char *logger_data_url_malloc(int32_t uri_len)
{
    char *url = malloc(strlen(DATA_LOGGER_SERVER_HOST) + uri_len + DATA_LOGGER_QUERY_ARG_MAX_LEN);

    return url;
}

void logger_data_url_free(char *full_url)
{
    if (full_url)
        free(full_url);
}

static LOGGER_QUERY_SYMBOL_IDX logger_data_find_url_query_symbol(char* uri)
{
    char *found = strchr(uri, '?');

    if (found)
        return LOGGER_QUERY_SYMBOL_AND;
    else
        return LOGGER_QUERY_SYMBOL_QUESTION;
}

static LOGGER_RET logger_data_chk_ret(HTTP_STATUS code)
{
    LOGGER_RET ret = HTTP_STATUS_OK;

    switch (code)
    {
        case HTTP_STATUS_OK:
            ret = LOGGER_DATA_OK;
            break;

        case HTTP_STATUS_NOT_FOUND:
            ret = LOGGER_DATA_NOT_FOUND;
            break;

        default:
            ret = LOGGER_DATA_OP_FAIL;
            break;
    }

    return ret;
}

static LOGGER_RET logger_data_get_id_list(int64_t *data_id, int32_t data_cnt, char *inbuf)
{
    int32_t i, ret = LOGGER_DATA_OK, cnt;
    JSON_Array*rootArr;
    JSON_Value *rootVal;
    JSON_Object *arrObj;
    JSON_Object *rootObj;

    const char *entity_id = NULL;
    char *jsonptr = NULL;
    JSON_Value_Type type;

    if (inbuf == NULL)
        return LOGGER_DATA_OP_FAIL;

//printf("inBuf : '%s'\n", inbuf);

    if ((rootVal = json_parse_string(inbuf)) == NULL)
    {
        printf("%s(%d) logger insert json string invalid\n",  __func__, __LINE__);
        return LOGGER_DATA_OP_FAIL;
    }

    type = json_value_get_type(rootVal);

    if (type == JSONArray)
    {
        if ((rootArr = json_value_get_array(rootVal)) == NULL)
        {
            printf("%s(%d) logger check json type invalid.\n", __func__, __LINE__);
            ret = LOGGER_DATA_OP_FAIL;
            goto get_id_list_done;
        }

        cnt = json_array_get_count(rootArr);
    }
    else if (type == JSONObject)
    {
        if ((rootObj = json_value_get_object(rootVal)) == NULL)
        {
            printf("%s(%d) logger check json type invalid.\n", __func__, __LINE__);
            ret = LOGGER_DATA_OP_FAIL;
            goto get_id_list_done;
        }

        cnt = 1;
    }

    for (i = 0; i < cnt; i++)
    {
        if (type == JSONArray)
        {
            arrObj = json_array_get_object(rootArr, i);

            entity_id = json_object_get_string(arrObj, "__logid__");
        }
        else if (type == JSONObject)
        {
            entity_id = json_object_get_string(rootObj, "__logid__");
        }

        if (entity_id != NULL)
        {
            if (data_id != NULL)
                data_id[i] = atoll(entity_id);

//printf("data_id[%d] %lld\n",i, data_id[i]);
        }
        else
        {
            printf("%s(%d) logger get data id fail\n",  __func__, __LINE__);
            ret = LOGGER_DATA_OP_FAIL;
            goto get_id_list_done;
        }

        if (type == JSONArray)
            json_object_remove(arrObj, "__logid__");
        else if (type == JSONObject)
            json_object_remove(rootObj, "__logid__");

        if (i + 1 == data_cnt)
            break;

    }

    jsonptr = json_serialize_to_string(rootVal);
    strcpy(inbuf, jsonptr);
    json_free_serialized_string(jsonptr);

//printf("inbuf remove id : '%s'\n", inbuf);

get_id_list_done:

    json_value_free(rootVal);

    return ret;
}

static LOGGER_RET logger_data_get_id(int64_t *data_id, char *inbuf)
{
    int32_t i;
    int32_t len;

    if (data_id == NULL)
        return LOGGER_DATA_OK;

    if (inbuf == NULL)
        return LOGGER_DATA_OP_FAIL;

    len = strlen(inbuf);

    inbuf[len - 1] = '\0';

    for (i = len - 1; i >= 0; i--)
    {
        if (inbuf[i] == ':')
        {
//printf("inbuf[i+1] = '%s'\n", &inbuf[i+1]);
            sscanf(&inbuf[i + 1], "\"%lld\"", data_id);
//printf("data_id = %lld\n", *data_id);
        }

        if (inbuf[i] == ',')
        {
            inbuf[i]   = '}';
            inbuf[i + 1] = '\0';
//printf("inbuf = '%s'\n", inbuf);
            return LOGGER_DATA_OK;
        }
    }

    return LOGGER_DATA_OP_FAIL;
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

LOGGER_RET logger_data_enq(char *uri, char *jsonStr, uint32_t len)
{
    HTTP_STATUS code;
    char *full_url = NULL;

    if (uri == NULL || jsonStr == NULL || len <= 0)
    {
        printf("%s(%d) data logger arg invalid\n",  __func__, __LINE__);
        return LOGGER_DATA_ARG_INVALID;
    }

    if ((full_url = logger_data_url_malloc(strlen(uri))) == NULL)
    {
        printf("%s(%d) data logger malloc fail\n",  __func__, __LINE__);
        return LOGGER_DATA_MALLOC_FAIL;
    }

    sprintf(full_url, "%s/%s", DATA_LOGGER_SERVER_HOST, uri);

    code = http_req_send(full_url, HTTP_REQ_OP_POST, "application/json", jsonStr, len, NULL, 0);

    logger_data_url_free(full_url);

    return logger_data_chk_ret(code);
}

LOGGER_RET logger_data_deq(char *uri, int64_t *data_id, LOGGER_DATA_OP op_flag, char **jsonStr, uint32_t *len)
{
    HTTP_STATUS code;
    char *full_url = NULL;
    uint keep;
    int64_t id;

    if (uri == NULL || jsonStr == NULL || len == NULL)
    {
        printf("%s(%d) data logger arg invalid\n",  __func__, __LINE__);
        return LOGGER_DATA_ARG_INVALID;
    }

    if ((full_url = logger_data_url_malloc(strlen(uri))) == NULL)
    {
        printf("%s(%d) data logger malloc fail\n",  __func__, __LINE__);
        return LOGGER_DATA_MALLOC_FAIL;
    }

    if (op_flag == LOGGER_DATA_OP_KEEP_DATA)
        keep = 1;
    else
        keep = 0;

    uint32_t idx = logger_data_find_url_query_symbol(uri);

    sprintf(full_url, "%s/%s%skeep=%u", DATA_LOGGER_SERVER_HOST, uri, query_symbol_str[idx], keep);
    code = http_req_send(full_url, HTTP_REQ_OP_GET, "application/json", NULL, 0, jsonStr, len);
//printf("full_url = '%s'\n", full_url);
    logger_data_url_free(full_url);

    if (code == HTTP_STATUS_OK)
    {
        if (logger_data_get_id(&id, *jsonStr) != LOGGER_DATA_OK)
        {
            printf("%s(%d) data logger get data id fail\n",  __func__, __LINE__);
            return LOGGER_DATA_OP_FAIL;
        }

        if (data_id != NULL)
            *data_id = id;
    }

    return logger_data_chk_ret(code);
}

LOGGER_RET logger_data_multi_deq(char *uri, int64_t *data_id_list, uint32_t data_cnt, LOGGER_DATA_OP op_flag, char **jsonStr, uint32_t *len)
{
    HTTP_STATUS code;
    char *full_url = NULL;
    uint keep;

    if (uri == NULL || jsonStr == NULL || len == NULL || data_cnt == 0)
    {
        printf("%s(%d) data logger arg invalid\n",  __func__, __LINE__);
        return LOGGER_DATA_ARG_INVALID;
    }

    if ((full_url = logger_data_url_malloc(strlen(uri))) == NULL)
    {
        printf("%s(%d) data logger malloc fail\n",  __func__, __LINE__);
        return LOGGER_DATA_MALLOC_FAIL;
    }

    if (op_flag == LOGGER_DATA_OP_KEEP_DATA)
        keep = 1;
    else
        keep = 0;

    uint32_t idx = logger_data_find_url_query_symbol(uri);

    sprintf(full_url, "%s/%s%skeep=%u&cnt=%u", DATA_LOGGER_SERVER_HOST, uri, query_symbol_str[idx], keep, data_cnt);
    code = http_req_send(full_url, HTTP_REQ_OP_GET, "application/json", NULL, 0, jsonStr, len);
//printf("full_url = '%s'\n", full_url);
    logger_data_url_free(full_url);

    if (code == HTTP_STATUS_OK)
    {
        if (logger_data_get_id_list(data_id_list, data_cnt, *jsonStr) != LOGGER_DATA_OK)
        {
            printf("%s(%d) data logger get data id fail\n",  __func__, __LINE__);
            return LOGGER_DATA_OP_FAIL;
        }
    }

    return logger_data_chk_ret(code);
}

LOGGER_RET logger_data_remove(char *uri, int64_t data_id, LOGGER_DATA_REMOVE op_flag)
{
    HTTP_STATUS code;
    char *full_url = NULL;
    int64_t id;

    if (uri == NULL)
    {
        printf("%s(%d) data logger arg invalid\n",  __func__, __LINE__);
        return LOGGER_DATA_ARG_INVALID;
    }

    if ((full_url = logger_data_url_malloc(strlen(uri))) == NULL)
    {
        printf("%s(%d) data logger malloc fail\n",  __func__, __LINE__);
        return LOGGER_DATA_MALLOC_FAIL;
    }

    if (op_flag == LOGGER_DATA_REMOVE_BY_ID)
        id = data_id;
    else if (op_flag == LOGGER_DATA_REMOVE_ALL)
        id = -1;
    else
    {
        printf("%s(%d) data logger arg invalid\n",  __func__, __LINE__);
        return LOGGER_DATA_ARG_INVALID;
    }

    uint32_t idx = logger_data_find_url_query_symbol(uri);

    sprintf(full_url, "%s/%s%sid=%lld", DATA_LOGGER_SERVER_HOST, uri, query_symbol_str[idx], id);

    code = http_req_send(full_url, HTTP_REQ_OP_DELETE, "application/json", NULL, 0, NULL, 0);

    logger_data_url_free(full_url);

    return logger_data_chk_ret(code);
}

LOGGER_RET logger_data_get(char *uri, int64_t data_id, char **jsonStr, uint32_t *len)
{
    HTTP_STATUS code;
    char *full_url = NULL;
    int64_t log_id;

    if (uri == NULL || jsonStr == NULL || len == NULL)
    {
        printf("%s(%d) data logger arg invalid\n",  __func__, __LINE__);
        return LOGGER_DATA_ARG_INVALID;
    }

    if ((full_url = logger_data_url_malloc(strlen(uri))) == NULL)
    {
        printf("%s(%d) data logger malloc fail\n",  __func__, __LINE__);
        return LOGGER_DATA_MALLOC_FAIL;
    }

    uint32_t idx = logger_data_find_url_query_symbol(uri);

    sprintf(full_url, "%s/%s%sid=%lld&keep=1", DATA_LOGGER_SERVER_HOST, uri, query_symbol_str[idx], data_id);
//printf("full_url = '%s'\n", full_url);
    code = http_req_send(full_url, HTTP_REQ_OP_GET, "application/json", NULL, 0, jsonStr, len);

    logger_data_url_free(full_url);

    if (code == HTTP_STATUS_OK)
    {
        if (logger_data_get_id(&log_id, *jsonStr) != LOGGER_DATA_OK)
        {
            printf("%s(%d) data logger get data id fail\n",  __func__, __LINE__);
            return LOGGER_DATA_OP_FAIL;
        }

        if (log_id != data_id)
        {
            printf("%s(%d) data logger data id invalid\n",  __func__, __LINE__);
            return LOGGER_DATA_OP_FAIL;
        }
    }

    return logger_data_chk_ret(code);
}

LOGGER_RET logger_data_get_next(char *uri, int64_t last_id, int64_t *data_id, char **jsonStr, uint32_t *len)
{
    HTTP_STATUS code;
    char *full_url = NULL;

    if (uri == NULL || jsonStr == NULL || len == NULL)
    {
        printf("%s(%d) data logger arg invalid\n",  __func__, __LINE__);
        return LOGGER_DATA_ARG_INVALID;
    }

    if ((full_url = logger_data_url_malloc(strlen(uri))) == NULL)
    {
        printf("%s(%d) data logger malloc fail\n",  __func__, __LINE__);
        return LOGGER_DATA_MALLOC_FAIL;
    }

    uint32_t idx = logger_data_find_url_query_symbol(uri);

    sprintf(full_url, "%s/%s%sid=%lld&keep=1&next=1", DATA_LOGGER_SERVER_HOST, uri, query_symbol_str[idx], last_id);
//printf("next full_url = '%s'\n", full_url);
    code = http_req_send(full_url, HTTP_REQ_OP_GET, "application/json", NULL, 0, jsonStr, len);

    logger_data_url_free(full_url);

    if (code == HTTP_STATUS_OK)
    {
        if (logger_data_get_id(data_id, *jsonStr) != LOGGER_DATA_OK)
        {
            printf("%s(%d) data logger get log id fail\n",  __func__, __LINE__);
            return LOGGER_DATA_OP_FAIL;
        }
    }

    return logger_data_chk_ret(code);
}

