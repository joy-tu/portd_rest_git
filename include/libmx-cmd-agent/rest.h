/*  Copyright (C) MOXA Inc. All rights reserved.

	This software is distributed under the terms of the
	MOXA License.  See the file COPYING-MOXA for details.
*/

#ifndef __REST_H__
#define __REST_H__
#include <stdint.h>

#define REST_MAX_URL_LEVEL       10
#define REST_MAX_URL_NAME_LEN    200

#define REST_OP_POST      1
#define REST_OP_GET       2
#define REST_OP_PUT       3
#define REST_OP_PATCH     4
#define REST_OP_DELETE    5

typedef enum {

	REST_HTTP_STATUS_CONTINUE                 = 100,
	REST_HTTP_STATUS_SWITCHING                = 101,
	REST_HTTP_STATUS_PROCESSING               = 102,
	REST_HTTP_STATUS_OK                       = 200,
	REST_HTTP_STATUS_CREATED                  = 201,
	REST_HTTP_STATUS_ACCEPTED                 = 202,
	REST_HTTP_STATUS_NOT_AUTHORITATIVE        = 203,
	REST_HTTP_STATUS_NO_CONTENT               = 204,
	REST_HTTP_STATUS_RESET_CONTENT            = 205,
	REST_HTTP_STATUS_PARTIAL_CONTENT          = 206,
	REST_HTTP_STATUS_MULTI_STATUS             = 207,
	REST_HTTP_STATUS_ALREADY_REPORTED         = 208,
	REST_HTTP_STATUS_IM_USED                  = 226,
	REST_HTTP_STATUS_MULTIPLE_CHOICES         = 300,
	REST_HTTP_STATUS_STATUS_MOVED_PERMANENTLY = 301,
	REST_HTTP_STATUS_FOUND                    = 302,
	REST_HTTP_STATUS_SEE_OTHER                = 303,
	REST_HTTP_STATUS_NOT_MODIFIED             = 304,
	REST_HTTP_STATUS_USE_PROXY                = 305,
	REST_HTTP_STATUS_TEMPORARY_REDIRECT       = 307,
	REST_HTTP_STATUS_PERMANENT_REDIRECT       = 308,
	REST_HTTP_STATUS_BAD_REQUEST              = 400,
	REST_HTTP_STATUS_UNAUTHORIZED             = 401,
	REST_HTTP_STATUS_PAYMENT_REQUIRED         = 402,
	REST_HTTP_STATUS_FORBIDDEN                = 403,
	REST_HTTP_STATUS_NOT_FOUND                = 404,
	REST_HTTP_STATUS_METHOD_NOT_ALLOWED       = 405,
	REST_HTTP_STATUS_NOT_ACCEPTABLE           = 406,
	REST_HTTP_STATUS_PROXY_AUTH_REQUIRED      = 407,
	REST_HTTP_STATUS_REQUEST_TIMEOUT          = 408,
	REST_HTTP_STATUS_CONFLICT                 = 409,
	REST_HTTP_STATUS_GONE                     = 410,
	REST_HTTP_STATUS_LENGTH_REQUIRED          = 411,
	REST_HTTP_STATUS_PRECONDITION_FAILED      = 412,
	REST_HTTP_STATUS_PAYLOAD_TOO_LARGE        = 413,
	REST_HTTP_STATUS_ENTITY_TOO_LONG          = 414,
	REST_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE   = 415,
	REST_HTTP_STATUS_RANGE_NOT_SATISFIABLE    = 416,
	REST_HTTP_STATUS_EXPECTATION_FAILED       = 417,
	REST_HTTP_STATUS_TEAPOT                   = 418,
	REST_HTTP_STATUS_UNPROCESSABLE_ENTITY     = 422,
	REST_HTTP_STATUS_LOCKED                   = 423,
	REST_HTTP_STATUS_FAILED_DEPENDENCY        = 424,
	REST_HTTP_STATUS_UPGRADE_REQUIRED         = 426,
	REST_HTTP_STATUS_PRECONDITION_REQUIRED    = 428,
	REST_HTTP_STATUS_TOO_MANY_REQUESTS        = 429,
	REST_HTTP_STATUS_HEADERS_TOO_LARGE        = 431,
	REST_HTTP_STATUS_UNAVAIL_LEGAL_REASONS    = 451,
	REST_HTTP_STATUS_INTERNAL_SERVER_ERROR    = 500,
	REST_HTTP_STATUS_NOT_IMPLEMENTED          = 501,
	REST_HTTP_STATUS_BAD_GATEWAY              = 502,
	REST_HTTP_STATUS_SERVICE_UNAVAILABLE      = 503,
	REST_HTTP_STATUS_GATEWAY_TIMEOUT          = 504,
	REST_HTTP_STATUS_HTTP_VERSION_NOT_SUPP    = 505,
	REST_HTTP_STATUS_VARIANT_ALSO_NEGOTIATES  = 506,
	REST_HTTP_STATUS_INSUFFICIENT_STORAGE     = 507,
	REST_HTTP_STATUS_LOOP_DETECTED            = 508,
	REST_HTTP_STATUS_NOT_EXTENDED             = 510,
	REST_HTTP_STATUS_NETWORK_AUTH_REQUIRED    = 511,
	
} REST_HTTP_STATUS;

typedef enum
{
	REST_OK    			 = 0,
	REST_OP_INVALID 	 = -1,
	REST_MALLOC_FAIL     = -2,
	REST_URL_NOTDEFINE   = -3,
	REST_PARAM_FAIL   	 = -4,
	REST_INIT_EXCEED     = -5,
	REST_QUERY_NOT_FOUND = -6,
	
} REST_RET;

typedef enum
{
	REST_DATA_TYPE_NULL = 1,
	REST_DATA_TYPE_JSON = 2,
	REST_DATA_TYPE_FILE = 3,

	REST_DATA_TYPE_NOT_SUPPORT = -1,
	
} REST_DATA_TYPE;

typedef struct REST_OUTPUT_INFO_t
{
	uint32_t output_data_len;
	uint32_t output_data_type;
	char *output_data_desc;

} REST_OUTPUT_INFO;

//typedef int32_t (*rest_cb_t)(const char *url, char *inBuf, int32_t inBufSize, char *outBuf, int32_t outbufSize);

typedef REST_HTTP_STATUS (*rest_cb_t)(const char *uri, char *input_data, int32_t input_data_size);

REST_RET rest_find_uri_query_val(char *q_name, char **q_val);
REST_RET rest_find_uri_name(int32_t uri_idx, char **name);

int32_t rest_init(char *url_prefix);

REST_RET rest_cb_register(int32_t id, char *uri, int32_t op, rest_cb_t handle);
REST_RET rest_cb_unregister(int32_t id);
REST_RET rest_write(char *data, int32_t data_size);
REST_RET rest_write_file(char* file_name, char *data, int32_t data_size);

int32_t rest_parser_handler(int32_t op, char *uri,
							char *input_data, uint32_t input_data_size,
							REST_OUTPUT_INFO *output_data_info, char **output_data, uint32_t *output_data_size);

#endif /* __REST_H__ */

