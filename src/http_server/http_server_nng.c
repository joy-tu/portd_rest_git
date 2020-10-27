/*  Copyright (C) MOXA Inc. All rights reserved.

    This software is distributed under the terms of the
    MOXA License.  See the file COPYING-MOXA for details.
*/
/*
    reset_server.c

    A imbeded REST server that accept the client connection and call the API to the server application

    2019-04-01  Shian
        First created
*/
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>
#include <nng/supplemental/util/platform.h>

#include "http_server.h"
#include "rest.h"


#define HTTP_PROTOCOL_PREFIX_HIGH "HTTP://"
#define HTTP_PROTOCOL_PREFIX_LOW "http://"
#define HTTP_PROTOCOL_PREFIX_LEN 7

#define MAX_CONTENT_TYPE_LEN    128

#define HTTP_HEADER_CONTENT_DISPOSITION	"attachment; filename=\"%s\""
#define HTTP_DEFAULT_REP_BUF_SIZE    	100

typedef int (*REST_FUN_CB)(const char* method, const char* path, 
                           const char* rcv_content_type, char* content, 
                           size_t content_len, uint16_t *result_code, 
                           char* rep_content_type, 
                           char** rep_buf, uint32_t *reply_buf_size,
                           uint32_t *reply_len, char** rep_desc);

typedef struct __rest_handler
{
    //add your need here, currently we only need the call back...
    REST_FUN_CB rest_cb;
    uint32_t http_size_limit;
    const char *mx_api_token;
} _rest_handler;

_rest_handler rest_handler;

static const char *req_op_str[] = {"GET", "POST", "DELETE", "PUT", "PATCH"};

static char *data_rep = NULL;
static uint32_t data_rep_size = 0;

nng_http_server *server = NULL;

nng_http_handler *handler = NULL;

/**
 *  \brief  The call back for the rest server appplication.
 *  \param[in]    method               the HTTP method received from client, should not be NULL
 *  \param[in]    path                 the uri path received from client, should not be NULL
 *  \param[in]    rcv_content_type     the content type received from HTTP client(JSON, or ...)
 *  \param[in]    content              the content received from HTTP client
 *  \param[in]    content_len          the content length of this content
 *  \param[out]   result_code          the HTTP result code to the client(200, 404, 301...)
 *  \param[out]   rep_content_type     the HTTP reply content type to the client(application/json, text/html...)
 *  \param[out]   rep_buf              the content to reply to HTTP client
 *  \param[out]   reply_len            the length of the reply content
 *  \retval       RESULT_OK or REST_RESULT_INVALID_PARAM
 *
 *  This function for the server to implement application call back when recv a request from client.
 */
int req_cb(
    const char* method,
    const char* path,
    const char* rcv_content_type,
    char* content,
    size_t content_len,
    uint16_t *result_code,
    char* rep_content_type,
    char** rep_buf,
    uint32_t *reply_buf_size,
    uint32_t *reply_len,
    char** rep_desc
)
{
    int32_t op, ret = REST_OK;
	REST_OUTPUT_INFO output_data_info;
	
    if ((method == NULL) || (path == NULL))
        return HTTP_SERVER_INVALID_PARAM;

    if ((rep_buf == NULL) || (result_code == NULL))
        return HTTP_SERVER_INVALID_PARAM;

//printf("Recv a request, method %s, url %s, content %s\n", method, path, (content==NULL)?"NULL":content);

    if (strcmp(method, "POST") == 0)
        op = REST_OP_POST;
    else if (strcmp(method, "GET") == 0)
        op = REST_OP_GET;
    else if (strcmp(method, "PUT") == 0)
        op = REST_OP_PUT;
    else if (strcmp(method, "PATCH") == 0)
        op = REST_OP_PATCH;
    else if (strcmp(method, "DELETE") == 0)
        op = REST_OP_DELETE;
    else
        op = REST_OP_INVALID;


    //rep_buf[0] = '\0';

    ret = rest_parser_handler(op, (char *)path, 
    						  content, content_len,
    						  &output_data_info, rep_buf, reply_buf_size);

    

    if (output_data_info.output_data_type == REST_DATA_TYPE_FILE)
    { 
        strcpy(rep_content_type, "text/plain");
		*rep_desc    = output_data_info.output_data_desc;
		*reply_len   = output_data_info.output_data_len;
		*result_code = 200;
    }
	else if (output_data_info.output_data_type == REST_DATA_TYPE_JSON)
	{
		strcpy(rep_content_type, "application/json");
		
		if(output_data_info.output_data_len == 0)
		{
			strcpy(*rep_buf, "{}");
			*reply_len = strlen(*rep_buf);
		}
		else
			*reply_len = output_data_info.output_data_len;
		
		*result_code = ret;
	}
	else
	{
		strcpy(rep_content_type, "application/json");
		strcpy(*rep_buf, "{}");
		*reply_len = strlen(*rep_buf);
		*result_code = ret;	
	}
	
    return HTTP_SERVER_OK;
}

// handle the call back of the server
void req_handle(nng_aio *aio)
{
    nng_http_req *   req  = (nng_http_req *)nng_aio_get_input(aio, 0);
    nng_http_res *   rep;
    size_t           sz = 0;
    int              rv = 0;
    void*            data;
    uint16_t rst_code = NNG_HTTP_STATUS_OK;
    uint32_t reply_len = 0;
    char rep_content_type[MAX_CONTENT_TYPE_LEN];
	const char *req_content_type;
	char *data_desc = NULL;

    if (rest_handler.rest_cb == NULL)
    {
        printf("Error! no set any call back to handle the data!\n");
        return;
    }

    //allocate a response for the job session
    if (((rv = nng_http_res_alloc(&rep)) != 0))
    {
        nng_aio_finish(aio, rv);
        return;
    }

    //retrive the data size
    nng_http_req_get_data(req, &data, &sz);
	req_content_type = nng_http_req_get_header(req, "Content-type");

	/* get mx-api-token */
	rest_handler.mx_api_token = nng_http_req_get_header(req, "Authorization");

	if(req_content_type != NULL)
	{
		// parser multipart boundary
		if(strncmp(req_content_type, "multipart/form-data", strlen("multipart/form-data")) == 0)
		{		
			int32_t end_of_header = 0;
			char *head_line = strchr((char*)data, '\n');

			if(head_line != NULL)
			{
				head_line++;

				if((*head_line == '\r') && (*(head_line + 1) == '\n'))
				{
					end_of_header = 1;
					data = head_line + 2;  // two symbol '/r' + '/n'
				}
				
				while(!end_of_header)
				{						
					head_line = strchr(head_line, '\n');
				
					if(head_line == NULL)
						break;

					head_line++;

					if((*head_line == '\r') && (*(head_line + 1) == '\n'))
					{
						end_of_header = 1;
						data = head_line + 2; // two symbol '/r' + '/n'
					}
				}
				
				if(end_of_header)
				{
					int32_t i;
					char *ptr = (char*)data;

					for(i = (strlen((char*)data) - 2); i > 0; i--)
					{
						if((ptr[i] == '\n') && (ptr[i-1] == '\r'))
						{
							ptr[i-1] = '\0';
							break;
						}
					}

					if(i > 0)
						sz = (i - 1);
				}						
			}
		}
	}
	
    //call the call back the main module put in
    rv = rest_handler.rest_cb(nng_http_req_get_method(req), nng_http_req_get_uri(req), 
    						  req_content_type,
    						 (char*)data, sz, &rst_code, 
    						 rep_content_type, &data_rep, &data_rep_size, &reply_len, &data_desc);

    if (rv == HTTP_SERVER_OK)
    {
		if ((rv = nng_http_res_copy_data(rep, data_rep, reply_len)) != 0)
		{
	        printf("Error! set http payload fail!\n");
	        return;
	 	}
#if 0		
		if ((rv = nng_http_res_copy_data(rep, data_rep, reply_len)) != 0)
		{
	        printf("Error! set http payload fail!\n");
	        return;
	 	}	
#endif
		if(strlen(rep_content_type) != 0)
		{
	        if ((rv = nng_http_res_set_header(rep, "Content-type", rep_content_type)) != 0)
			{
		        printf("Error! set http header fail!\n");
		        return;
	   	 	}

			if(strcmp(rep_content_type, "text/plain") == 0)
			{
				if ((rv = nng_http_res_set_header(rep, "Content-Disposition", "attachment;")) != 0)
				{
			        printf("Error! set http header fail!\n");
			        return;
		   	 	}

				if(data_desc != NULL)
				{
					char *disp = malloc(strlen(HTTP_HEADER_CONTENT_DISPOSITION) + strlen(data_desc) + 1);

					if(disp == NULL)
						return;

					sprintf(disp, HTTP_HEADER_CONTENT_DISPOSITION, data_desc);

					rv = nng_http_res_set_header(rep, "Content-Disposition", disp);				
					
					if(data_desc)
						free(data_desc);

					if(disp)
						free(disp);

					if(rv != 0)
					{
				        printf("Error! set http header fail!\n");
				        return;
			   	 	}
				}
			}
		}

        if ((rv = nng_http_res_set_status(rep, rst_code)) != 0)
		{
	        printf("Error! set http status fail!\n");
	        return;
   	 	}	
    }
    else
    {     
        printf("Error! set http header fail!\n");
		nng_aio_finish(aio, rv);
        return;
    }

    nng_aio_set_output(aio, 0, rep);
    nng_aio_finish(aio, 0);
}

static int rest_start(char* server_ip, char* uri, uint16_t port)
{
    char              rest_addr[128];
    nng_url *         url = NULL;
    int               rv = HTTP_SERVER_OK;

    do
    {
        if ((server_ip == NULL) || (uri == NULL))
        {
            rv = HTTP_SERVER_INVALID_PARAM;
            printf("Empty URL!\n");
            break;
        }

        // The Url that nng will redirect to me
        snprintf(rest_addr, sizeof(rest_addr), "%s:%u", server_ip, port);

        if ((rv = nng_url_parse(&url, rest_addr)) != 0)
        {
            printf("[REST server]Error! parse url %s fail!\n", rest_addr);
            rv = HTTP_SERVER_INVALID_PARAM;
            break;
        }

        // Get a suitable HTTP server instance.  This creates one
        // if it doesn't already exist.
        if ((rv = nng_http_server_hold(&server, url)) != 0)
        {
            printf("[REST server]Error! start server instance fail!\n");
            rv = HTTP_SERVER_START_FAIL;
            break;
        }

        // The Url that nng will redirect to me
        snprintf(rest_addr, sizeof(rest_addr), "%s", uri);

        if ((rv = nng_http_handler_alloc(&handler, rest_addr, req_handle)) != 0)
        {
            printf("[REST server]Error! start server handle fail!\n");
            rv = HTTP_SERVER_START_FAIL;
            break;
        }

        if ((rv = nng_http_handler_set_tree(handler)) != 0)
        {
            printf("[REST server]Error! start server handle fail!\n");
            rv = HTTP_SERVER_START_FAIL;
            break;
        }

        //Use NULL that we want to receive all method
        if ((rv = nng_http_handler_set_method(handler, NULL)) != 0)
        {
            printf("[REST server]Error! set server method fail!\n");
            rv = HTTP_SERVER_START_FAIL;
            break;
        }

        // limit the http body size
        //if ((rv = nng_http_handler_collect_body(handler, true, rest_handler.http_size_limit)) != 0)
        if ((rv = nng_http_handler_collect_body(handler, true, 20 * 1024 * 1024)) != 0)
        {
            printf("[REST server]Error! collect server body buffer fail!\n");
            rv = HTTP_SERVER_MEMORY_UNAVAILABLE;
            break;
        }

        if ((rv = nng_http_server_add_handler(server, handler)) != 0)
        {
            printf("[REST server]Error! add server handle fail!\n");
            rv = HTTP_SERVER_START_FAIL;
            break;
        }

        if ((rv = nng_http_server_start(server)) != 0)
        {
            printf("[REST server]Error! start server fail!\n");
            rv = HTTP_SERVER_START_FAIL;
            break;
        }
    }
    while (0);

	if(url)
		nng_url_free(url);

    return rv;
}

int http_server_start(uint16_t port, char* url, uint32_t http_size) // http_size : size of byte
{
    int result = HTTP_SERVER_OK;

    if (url == NULL)
        return HTTP_SERVER_INVALID_PARAM;

    rest_handler.rest_cb = req_cb;
    rest_handler.http_size_limit = http_size;

    if ((strncmp(url, HTTP_PROTOCOL_PREFIX_HIGH, HTTP_PROTOCOL_PREFIX_LEN) != 0)
            && (strncmp(url, HTTP_PROTOCOL_PREFIX_LOW, HTTP_PROTOCOL_PREFIX_LEN) != 0))
    {
        result = HTTP_SERVER_INVALID_PARAM;
    }
    else
    {
        char server_ip[MAX_URL_PATH_LEN];
        char *ptr, *uri;

        //first, we reterive the ip length without http prefix
        memcpy(server_ip, url + HTTP_PROTOCOL_PREFIX_LEN, (strlen(url) - HTTP_PROTOCOL_PREFIX_LEN));
        ptr = strtok(server_ip, "/");

        printf("server_ip %s\n", (ptr == NULL) ? "NULL" : ptr);

        if (ptr == NULL)
        {
            result = HTTP_SERVER_INVALID_PARAM;
        }
        else
        {
            //Now we know the ip length we add back http prefix
            uint16_t server_ip_len = strlen(ptr);
            memset(server_ip, 0 , sizeof(server_ip));
            memcpy(server_ip, url, HTTP_PROTOCOL_PREFIX_LEN + server_ip_len);

            //Reterive the URI part
            uri = url + HTTP_PROTOCOL_PREFIX_LEN + server_ip_len;

            printf("start http rest server %s:%u, uri %s\n", server_ip, port, uri);
            result = rest_start(server_ip, uri, port);
        }
    }

	if(result == HTTP_SERVER_OK)
	{
		if(data_rep == NULL)
		{
			if((data_rep = malloc(HTTP_DEFAULT_REP_BUF_SIZE)) == NULL)
				return HTTP_SERVER_MEMORY_UNAVAILABLE;

			data_rep_size = HTTP_DEFAULT_REP_BUF_SIZE;			
		}
	}

    return result;
}

void http_server_stop(void)
{
    /* By nng manual :
       Any handlers that are registered with servers are automatically freed when the server itself is deallocated. 
    */
    if(server == NULL)
    {
        /* We have no server instance to release...*/
        return;
    }

    //we have to releae the handle first before we destory the server
    if(handler != NULL)
    {
        nng_http_server_del_handler(server, handler);
        nng_http_handler_free(handler);
    }

    nng_http_server_stop(server);
    nng_http_server_release(server);

    handler = NULL;
    server = NULL;

	if(data_rep != NULL)
		free(data_rep);

	data_rep = NULL;
	data_rep_size = 0;
}


static int32_t http_req_httpdo(nng_url *url, nng_http_req *req, nng_http_res *res, void **datap, size_t *sizep)
{
	int              rv;
	nng_aio *        aio  = NULL;
	nng_http_client *cli  = NULL;
	nng_http_conn *  h    = NULL;
	size_t           clen = 0;
	void *           data = NULL;
	const char *     ptr;

	if (((rv = nng_aio_alloc(&aio, NULL, NULL)) != 0) ||
	    ((rv = nng_http_client_alloc(&cli, url)) != 0)) {
		goto fail;
	}
	
	nng_http_client_connect(cli, aio);
	
	nng_aio_wait(aio);

	if ((rv = nng_aio_result(aio)) != 0) {
		goto fail;
	}

	h = nng_aio_get_output(aio, 0);

	nng_http_conn_write_req(h, req, aio);
	
	nng_aio_wait(aio);
	
	if ((rv = nng_aio_result(aio)) != 0) {
		goto fail;
	}
	
	nng_http_conn_read_res(h, res, aio);
	nng_aio_wait(aio);
	if ((rv = nng_aio_result(aio)) != 0) {
		goto fail;
	}

	clen = 0;
	if ((ptr = nng_http_res_get_header(res, "Content-Length")) != NULL) {
		clen = atoi(ptr);
	}

	if (clen > 0) {
		nng_iov iov;
		data        = nng_alloc(clen);
		iov.iov_buf = data;
		iov.iov_len = clen;
		nng_aio_set_iov(aio, 1, &iov);
		nng_http_conn_read_all(h, aio);
		nng_aio_wait(aio);
		if ((rv = nng_aio_result(aio)) != 0) {
			goto fail;
		}
	}

	*datap = data;
	*sizep = clen;

fail:
	if (aio != NULL) {
		nng_aio_free(aio);
	}
	if (h != NULL) {
		nng_http_conn_close(h);
	}
	if (cli != NULL) {
		nng_http_client_free(cli);
	}
//printf("rv(%d)\n", rv);
	return (rv);
}

HTTP_STATUS http_req_send(char *full_url, HTTP_REQ_OP req_op, char *content_type, char *in_buf, uint32_t in_buf_len, char **out_buf, uint32_t *out_buf_len)
{
	nng_url *        url;
	nng_http_req *   req;
	nng_http_res *   res;
	int ret = HTTP_STATUS_OK;
	char *rx_data = NULL;
	size_t rx_size = 0;

	if(full_url == NULL || content_type == NULL)
	{
		printf("%s(%d) http request parameter invalid\n",  __func__, __LINE__);	
		return HTTP_STATUS_BAD_REQUEST;
	}
	
//printf("full_url = '%s'\n", full_url);

	if(nng_url_parse(&url, full_url) != 0)
	{
		printf("%s(%d) http request malloc fail\n",  __func__, __LINE__);
		ret = HTTP_STATUS_BAD_REQUEST;
		goto HTTP_REQ_DONE;
	}

	if(nng_http_req_alloc(&req, url) != 0)
	{
		printf("%s(%d) http request malloc fail\n",  __func__, __LINE__);
		ret = HTTP_STATUS_BAD_REQUEST;
		goto HTTP_REQ_DONE;
	}

	if(nng_http_res_alloc(&res) != 0)
	{
		printf("%s(%d) http request malloc fail\n",  __func__, __LINE__);
		ret = HTTP_STATUS_BAD_REQUEST;
		goto HTTP_REQ_DONE;
	}

	if(in_buf != NULL && in_buf_len > 0)
		nng_http_req_set_data(req, in_buf, in_buf_len);

	if(nng_http_req_set_method(req, req_op_str[req_op]) != 0)
	{
		printf("%s(%d) http request set OP fail\n",  __func__, __LINE__ );
		ret = HTTP_STATUS_BAD_REQUEST;
		goto HTTP_REQ_DONE;
	}

	if(nng_http_req_set_header(req, "Content-Type", content_type) != 0)
	{
		printf("%s(%d) http request set header fail\n",  __func__, __LINE__ );
		ret = HTTP_STATUS_BAD_REQUEST;
		goto HTTP_REQ_DONE;
	}
	
	if(http_req_httpdo(url, req, res, (void **) &rx_data, &rx_size) != 0)
	{
		printf("%s(%d) http request send fail\n",  __func__, __LINE__ );
		ret = HTTP_STATUS_BAD_REQUEST;
		goto HTTP_REQ_DONE;
	}

	if(rx_size > 0 && out_buf != NULL && out_buf_len != NULL)
	{
//printf("len %d %d\n", rx_size, out_buf_len);
		if((*out_buf = malloc(rx_size + 1)) != NULL)
		{
			char *out_arr = *out_buf;
			
			memcpy(out_arr, rx_data, rx_size);
			out_arr[rx_size] = '\0';
			*out_buf_len = rx_size;
		}
		else
		{
			printf("%s(%d) http request buf malloc fail!!\n",  __func__, __LINE__);
			ret = HTTP_STATUS_BAD_REQUEST;
			goto HTTP_REQ_DONE;
		}
	}
//printf("http res code %d\n" , nng_http_res_get_status(res));
	if ((ret = nng_http_res_get_status(res)) != HTTP_STATUS_OK)
	{
		printf("%s(%d) http resp fail(%d %s)\n",  __func__, __LINE__ ,
	    nng_http_res_get_status(res),
	    nng_http_res_get_reason(res));		
		goto HTTP_REQ_DONE;
	}


HTTP_REQ_DONE:
	
	nng_http_req_free(req);
	nng_http_res_free(res);
	nng_url_free(url);
	
	return ret;
}

const char *http_get_mx_api_token(void)
{
	return rest_handler.mx_api_token;
}

