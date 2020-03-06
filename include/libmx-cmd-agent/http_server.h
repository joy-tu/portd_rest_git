#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H
#include <stdint.h>
#include <stdlib.h>

#define HTTP_SERVER_OK                  0
#define HTTP_SERVER_INVALID_PARAM       -1
#define HTTP_SERVER_START_FAIL          -2
#define HTTP_SERVER_MEMORY_UNAVAILABLE  -3
#define HTTP_SERVER_PROCESS_FAIL        -4

#define MAX_URL_PATH_LEN    128

/**
 *  \brief  Start a http server in background.
 *  \param[in]  port        the TCP port of this http server
 *  \param[in]  url         the url of this http server, with prefix "http://"
 *  \param[in]  http_size   the MAX limit body size of this http server
 *  \retval     HTTP_SERVER_OK
 *  \retval     HTTP_SERVER_MEMORY_UNAVAILABLE
 *  \retval     HTTP_SERVER_INVALID_PARAM
 *
 *  This function start a http server in background
 */
int http_server_start(uint16_t port, char* url, uint32_t http_size);

/**
 *  \brief  Stop the http server.
 *  \param[in]  port        the TCP port of this http server
 *  \param[in]  url         the url of this http server, with prefix "http://"
 *  \param[in]  http_size   the MAX limit body size of this http server
 *  \retval     HTTP_SERVER_OK
 *  \retval     HTTP_SERVER_MEMORY_UNAVAILABLE
 *  \retval     HTTP_SERVER_INVALID_PARAM
 *
 *  This function release all the resources allocated by http server
 */
void http_server_stop(void);

typedef enum {

	HTTP_STATUS_CONTINUE                 = 100,
	HTTP_STATUS_SWITCHING                = 101,
	HTTP_STATUS_PROCESSING               = 102,
	HTTP_STATUS_OK                       = 200,
	HTTP_STATUS_CREATED                  = 201,
	HTTP_STATUS_ACCEPTED                 = 202,
	HTTP_STATUS_NOT_AUTHORITATIVE        = 203,
	HTTP_STATUS_NO_CONTENT               = 204,
	HTTP_STATUS_RESET_CONTENT            = 205,
	HTTP_STATUS_PARTIAL_CONTENT          = 206,
	HTTP_STATUS_MULTI_STATUS             = 207,
	HTTP_STATUS_ALREADY_REPORTED         = 208,
	HTTP_STATUS_IM_USED                  = 226,
	HTTP_STATUS_MULTIPLE_CHOICES         = 300,
	HTTP_STATUS_STATUS_MOVED_PERMANENTLY = 301,
	HTTP_STATUS_FOUND                    = 302,
	HTTP_STATUS_SEE_OTHER                = 303,
	HTTP_STATUS_NOT_MODIFIED             = 304,
	HTTP_STATUS_USE_PROXY                = 305,
	HTTP_STATUS_TEMPORARY_REDIRECT       = 307,
	HTTP_STATUS_PERMANENT_REDIRECT       = 308,
	HTTP_STATUS_BAD_REQUEST              = 400,
	HTTP_STATUS_UNAUTHORIZED             = 401,
	HTTP_STATUS_PAYMENT_REQUIRED         = 402,
	HTTP_STATUS_FORBIDDEN                = 403,
	HTTP_STATUS_NOT_FOUND                = 404,
	HTTP_STATUS_METHOD_NOT_ALLOWED       = 405,
	HTTP_STATUS_NOT_ACCEPTABLE           = 406,
	HTTP_STATUS_PROXY_AUTH_REQUIRED      = 407,
	HTTP_STATUS_REQUEST_TIMEOUT          = 408,
	HTTP_STATUS_CONFLICT                 = 409,
	HTTP_STATUS_GONE                     = 410,
	HTTP_STATUS_LENGTH_REQUIRED          = 411,
	HTTP_STATUS_PRECONDITION_FAILED      = 412,
	HTTP_STATUS_PAYLOAD_TOO_LARGE        = 413,
	HTTP_STATUS_ENTITY_TOO_LONG          = 414,
	HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE   = 415,
	HTTP_STATUS_RANGE_NOT_SATISFIABLE    = 416,
	HTTP_STATUS_EXPECTATION_FAILED       = 417,
	HTTP_STATUS_TEAPOT                   = 418,
	HTTP_STATUS_UNPROCESSABLE_ENTITY     = 422,
	HTTP_STATUS_LOCKED                   = 423,
	HTTP_STATUS_FAILED_DEPENDENCY        = 424,
	HTTP_STATUS_UPGRADE_REQUIRED         = 426,
	HTTP_STATUS_PRECONDITION_REQUIRED    = 428,
	HTTP_STATUS_TOO_MANY_REQUESTS        = 429,
	HTTP_STATUS_HEADERS_TOO_LARGE        = 431,
	HTTP_STATUS_UNAVAIL_LEGAL_REASONS    = 451,
	HTTP_STATUS_INTERNAL_SERVER_ERROR    = 500,
	HTTP_STATUS_NOT_IMPLEMENTED          = 501,
	HTTP_STATUS_BAD_GATEWAY              = 502,
	HTTP_STATUS_SERVICE_UNAVAILABLE      = 503,
	HTTP_STATUS_GATEWAY_TIMEOUT          = 504,
	HTTP_STATUS_HTTP_VERSION_NOT_SUPP    = 505,
	HTTP_STATUS_VARIANT_ALSO_NEGOTIATES  = 506,
	HTTP_STATUS_INSUFFICIENT_STORAGE     = 507,
	HTTP_STATUS_LOOP_DETECTED            = 508,
	HTTP_STATUS_NOT_EXTENDED             = 510,
	HTTP_STATUS_NETWORK_AUTH_REQUIRED    = 511,

} HTTP_STATUS;


typedef enum
{
    HTTP_REQ_OP_GET = 0,
	HTTP_REQ_OP_POST,
	HTTP_REQ_OP_DELETE,
	HTTP_REQ_OP_PUT,
	HTTP_REQ_OP_PATCH
	
} HTTP_REQ_OP;


HTTP_STATUS http_req_send(char *full_url, HTTP_REQ_OP req_op, char *content_type, char *in_buf, uint32_t in_buf_len, char **out_buf, uint32_t *out_buf_len);
const char *http_get_mx_api_token(void);

#endif //HTTP_SERVER_H
