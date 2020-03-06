/*  Copyright (C) MOXA Inc. All rights reserved.

    This software is distributed under the terms of the
    MOXA License.  See the file COPYING-MOXA for details.
*/
/*
    portd_rest.c

    A web server that manipulate REST request and s2e_opm.json.

    2020-03-04  Joy
        First created
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <parson.h>
#include <http_server.h>
#include <rest.h>
#include <nng.h>
#include <yuarel.h>

#define MAX_BUF_SIZE 4096
#define DEFAULT_URL "http://%s/%s"

static int json_object_member_get_type(const JSON_Object *object, const char *name);

unsigned int string_hash(void *string);
JSON_Value* json_filter_from_json(JSON_Value *ori_json, char *query);
JSON_Value* json_filter_from_file(const char *file, char *query);

static int json_object_member_get_type(const JSON_Object *object, const char *name)
{
	JSON_Value *val = json_object_get_value(object, name);
	return json_value_get_type(val);
}

unsigned int string_hash(void *string)
{
	/* This is the djb2 string hash function */

	unsigned int result = 5381;
	unsigned char *p;

	p = (unsigned char *) string;

	while (*p != '\0') {
		result = (result << 5) + result + *p;
		++p;
	}

	return result;
}

JSON_Value* json_filter_from_json(JSON_Value *ori_json, char *query)
{
	JSON_Value *dst_val, *ori_val;
	JSON_Array *dst_array, *ori_array;
	JSON_Value *tmp_val, *dup_val;
	JSON_Object *obj;
	int i, j, cnt, type, filter_cnt, append = 1;
	struct yuarel_param params[5];	

	filter_cnt = yuarel_parse_query(query, '&', params, 5); 
	/* Parse the filter attribute, max attribute is five */

	ori_val = ori_json;
	ori_array = json_array(ori_val);
	cnt = json_array_get_count(ori_array);
	dst_val = json_value_init_array();
	dst_array = json_value_get_array(dst_val);
	for (i = 0; i < cnt; i++) { /* Check all object inside array */
		append = 1;
		obj = json_array_get_object(ori_array, i);
		for (j = 0; j < filter_cnt; j++) { /* Check all filter attribute for the specific object */
			if (append == 0)
				break;
			type = json_object_member_get_type(obj, params[j].key);
			switch (type) {
			case JSONString:
				if (!strcmp(json_object_get_string(obj, params[j].key), params[j].val)) {
					append = 1;
				} else {
					append = 0;
				}
				break;
			case JSONNumber:
				if (json_object_get_number(obj, params[j].key) == atoi(params[j].val)) {
					append = 1;
				} else {
					append = 0;
				}
				break;
			case JSONObject:
				break;
			case JSONArray:
				break;
			case JSONBoolean:
				break;
			default:
				break;
			}		
		}		
		if (append == 1) { /* Pass all the condition of filter, so appand this object to the array */
			tmp_val = json_object_get_wrapping_value(obj);
			dup_val = json_value_deep_copy(tmp_val);
			json_array_append_value(dst_array, dup_val);
		}
	}
	
	return dst_val;
}

JSON_Value* json_filter_from_file(const char *file, char *query)
{
	JSON_Value *dst_val, *ori_val = json_parse_file(file);
	JSON_Array *dst_array, *ori_array;
	JSON_Value *tmp_val, *dup_val;
	JSON_Object *obj;
	int i, j, cnt, type, filter_cnt, append = 1;
	struct yuarel_param params[5];	

	filter_cnt = yuarel_parse_query(query, '&', params, 5); 
	/* Parse the filter attribute, max attribute is five */
	
	ori_array = json_array(ori_val);
	cnt = json_array_get_count(ori_array);
	dst_val = json_value_init_array();
	dst_array = json_value_get_array(dst_val);
	for (i = 0; i < cnt; i++) { /* Check all object inside array */
		append = 1;
		obj = json_array_get_object(ori_array, i);
		for (j = 0; j < filter_cnt; j++) { /* Check all filter attribute for the specific object */
			if (append == 0)
				break;
			type = json_object_member_get_type(obj, params[j].key);
			switch (type) {
			case JSONString:
				if (!strcmp(json_object_get_string(obj, params[j].key), params[j].val)) {
					append = 1;
				} else {
					append = 0;
				}
				break;
			case JSONNumber:
				if (json_object_get_number(obj, params[j].key) == atoi(params[j].val)) {
					append = 1;
				} else {
					append = 0;
				}
				break;
			case JSONObject:
				break;
			case JSONArray:
				break;
			case JSONBoolean:
				break;
			default:
				break;
			}		
		}		
		if (append == 1) { /* Pass all the condition of filter, so appand this object to the array */
			tmp_val = json_object_get_wrapping_value(obj);
			dup_val = json_value_deep_copy(tmp_val);
			json_array_append_value(dst_array, dup_val);
		}
	}
	return dst_val;
}

static REST_HTTP_STATUS s2e_rest_get(const char *uri, char *input_data, int32_t input_data_size)
{
	int p, number, i;
	char *serialized_string = NULL;
	nng_url *         url = NULL;
	JSON_Value *user_data, *dst_data;
	char              rest_addr[128];
	char*			fakeurl = "http://localhost:404";
	struct yuarel yurl;
	char *parts[5];
	struct yuarel_param params[5];	
	
	printf("uri=%s, input_data=%s, size=%d\r\n", uri, input_data, input_data_size);

	sprintf(rest_addr, "%s%s", fakeurl, uri);
	printf("%s\r\n", rest_addr);

	if (-1 == yuarel_parse(&yurl, rest_addr)) {
		printf("Could not parse url!\n");
		return 1;
	}

	printf("Struct values:\n");
	printf("\tscheme:\t\t%s\n", yurl.scheme);
	printf("\thost:\t\t%s\n", yurl.host);
	printf("\tport:\t\t%d\n", yurl.port);
	printf("\tpath1:\t\t%s\n", yurl.path);
	printf("\tquery:\t\t%s\n", yurl.query);
	printf("\tfragment:\t%s\n", yurl.fragment);
#if 1
	if (string_hash(yurl.path) == string_hash("s2e_opm/config")) {
		user_data = json_parse_file("s2e_opm.json");
		dst_data = json_filter_from_json(user_data, yurl.query);
		serialized_string = json_serialize_to_string_pretty(dst_data);
		puts(serialized_string);
		rest_write(serialized_string, strlen(serialized_string));

		json_free_serialized_string(serialized_string);
		json_value_free(user_data);
		json_value_free(dst_data);
	}
#else
	switch (number) {
		case 2: //s2e_mode/xxx
			if (string_hash(parts[number - 1]) == string_hash("config")) {
				user_data = json_parse_file("wtrial.json");

				serialized_string = json_serialize_to_string_pretty(user_data);
					puts(serialized_string);
				rest_write(serialized_string, strlen(serialized_string));

				json_free_serialized_string(serialized_string);
				json_value_free(user_data);				
			}
			break;
		case 3:
			break;
		default:
			break;
	}
#endif
	return REST_HTTP_STATUS_OK;				

}

static void usage(char *str)
{
    fprintf(stderr, "Usage: %s [-ipns]\n", str);
    fprintf(stderr, "[-i IP]:          ip address\n");
    fprintf(stderr, "[-p port]:        port number\n");
    fprintf(stderr, "[-n module name]: module name\n");
	fprintf(stderr, "[-s size]:        maximum http body size\n");
}

int main(int argc, char **argv)
{
	int opt;
    int32_t ret, id;
    char *ip = NULL, *module_name = NULL;
    int port = -1, size = MAX_BUF_SIZE;
    char url[1024];

	while ((opt = getopt(argc, argv, "i:p:n:s:")) != -1)
    {
        switch (opt)
        {
            case 'n':
                module_name = optarg;
                break;

            case 'i':
                ip = optarg;
                break;

            case 'p':
                port = atoi(optarg);
                break;
				
			case 's':
                size = atoi(optarg);
                break;
				
            case '?':
            default:

                usage(argv[0]);
                exit(EXIT_FAILURE);

                break;
        }
    }

	if (ip == NULL || port == -1 || module_name == NULL)
    {
        usage(argv[0]);
        return -1;
    }

    printf("\n");
    printf("ip      '%s'\n", ip);
    printf("port    '%d'\n", port);
    printf("module  '%s'\n", module_name);
    printf("size    '%d'\n", size);

    sprintf(url, DEFAULT_URL, ip, module_name);

    /* initial the REST handler */
    if ((id = rest_init(module_name)) < 0)
    {
        printf("(%s:%d)init rest fail (%d).\n", __func__, __LINE__, id);
        return -1;
    }

    /* Register your REST call back */
    if ((ret = rest_cb_register(id, "/config", REST_OP_GET, s2e_rest_get)) < 0)
    {
        printf("(%s:%d)register rest callback fail (%d).\n", __func__, __LINE__, ret);
        return -1;
    }

	// Start web server
    if ((ret = http_server_start(port, url, size)) != HTTP_SERVER_OK)
        printf("(%s:%d)HTTP server start fail (%d).\n", __func__, __LINE__, ret);


    while (1)
        sleep(1);

    // stop server & unregister rest callback function
    http_server_stop();
    rest_cb_unregister(id);

    return 0;
}
