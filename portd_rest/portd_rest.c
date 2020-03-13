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
//#define DBG1

static int json_object_member_get_type(const JSON_Object *object, const char *name);
static REST_HTTP_STATUS s2e_rest_get(const char *uri, char *input_data, int32_t input_data_size);
static REST_HTTP_STATUS s2e_rest_patch(const char *uri, char *input_data, int32_t input_data_size);

unsigned int string_hash(void *string);
int do_err_rest_rsp(int ret);
int json_filter_from_json(JSON_Value **dst_json, JSON_Value *ori_json, char *query, int idx);
JSON_Value* json_filter_from_file(const char *file, char *query);

static int json_object_member_get_type(const JSON_Object *object, const char *name)
{
	JSON_Value *val = json_object_get_value(object, name);
	return json_value_get_type(val);
}

int do_err_rest_rsp(int ret)
{
	char *serialized_string = NULL;

	switch (ret) {
	case REST_HTTP_STATUS_NOT_FOUND:
		serialized_string = json_serialize_to_string_pretty(
							json_parse_string("{\"error\":{\"message\":\"Fail to get data in config\"}}")
						);
		break;
	case REST_HTTP_STATUS_BAD_REQUEST:
		serialized_string = json_serialize_to_string_pretty(
							json_parse_string("{\"error\":{\"message\":\"Member not exists\"}}")
						);	
		break;
	default:
		break;
	}
	rest_write(serialized_string, strlen(serialized_string));		

	return 0;
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

int json_filter_from_json(JSON_Value **dst_json, JSON_Value *ori_json, char *query, int idx)
{
	JSON_Value *dst_val, *ori_val;
	JSON_Array *dst_array, *ori_array;
	JSON_Value *tmp_val, *dup_val;
	JSON_Object *obj;
	int i, j, cnt, type, filter_cnt, append = 1;
	struct yuarel_param params[5];	
	char *serialized_string = NULL;

	filter_cnt = yuarel_parse_query(query, '&', params, 5); 
	/* Parse the filter attribute, max attribute is five */

	ori_val = ori_json;
	ori_array = json_array(ori_val);
	cnt = json_array_get_count(ori_array);
	dst_val = json_value_init_array();
	dst_array = json_value_get_array(dst_val);
	printf("Joy idx=%d\r\n", idx);
	for (i = 0; i < cnt; i++) { /* Check all object inside array */
		printf("i = %d\r\n", i);
		if (idx != 0 && idx != (i + 1)) {
			continue;
		}
		append = 1;
		obj = json_array_get_object(ori_array, i);
		printf("Line=%d\r\n", __LINE__);
		for (j = 0; j < filter_cnt; j++) { /* Check all filter attribute for the specific object */
			if (!json_object_has_value(obj, params[j].key)) {
				printf("Line=%d\r\n", __LINE__);
				return REST_HTTP_STATUS_NOT_FOUND;
			}
			printf("Line=%d\r\n", __LINE__);
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
				append = 0;
				break;
			}		
		}		
		if (append == 1) { /* Pass all the condition of filter, so appand this object to the array */
			tmp_val = json_object_get_wrapping_value(obj);
			dup_val = json_value_deep_copy(tmp_val);
			json_array_append_value(dst_array, dup_val);
		}
	}
	/* Create the json format and send REST response back */
	/* serialized_string = json_serialize_to_string_pretty(dst_val);
	rest_write(serialized_string, strlen(serialized_string));
	json_free_serialized_string(serialized_string);
	json_value_free(dst_val);
	*/
	*dst_json = dst_val;
	return REST_HTTP_STATUS_OK;
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
	int p, number, idx, ret;
	char *serialized_string = NULL;
	nng_url *         url = NULL;
	JSON_Value *user_data, *dst_data, *dst_json;
	void *dst_data2;
	char              rest_addr[128];
	char*		fakeurl = "http://localhost:404";
	char 			cmp_uri[128], tmp_path[128];
	char *parts[5];
	struct yuarel yurl;
	struct yuarel_param params[5];	
	
	sprintf(rest_addr, "%s%s", fakeurl, uri);
	printf("%s\r\n", rest_addr);

	if (-1 == yuarel_parse(&yurl, rest_addr)) {
		printf("Could not parse url!\n");
		return 1;
	}
	strcpy(tmp_path, yurl.path);
	number = yuarel_split_path(tmp_path, parts, 5);
#if DBG1
	printf("Struct values:\n");
	printf("\tscheme:\t\t%s\n", yurl.scheme);
	printf("\thost:\t\t%s\n", yurl.host);
	printf("\tport:\t\t%d\n", yurl.port);
	printf("\tpath:\t\t%s\n", yurl.path);
	printf("\tquery:\t\t%s\n", yurl.query);
	printf("\tfragment:\t%s\n", yurl.fragment);
#endif	
	if ((number == 2) && (string_hash(yurl.path) == string_hash("s2e_opm/config"))) {
		user_data = json_parse_file("s2e_opm.json");
		ret = json_filter_from_json(&dst_json, user_data, yurl.query, 0);
		if (ret == REST_HTTP_STATUS_OK) {
			serialized_string = json_serialize_to_string(dst_json);
			rest_write(serialized_string, strlen(serialized_string));
#ifdef DBG1
			serialized_string = json_serialize_to_string_pretty(dst_json);
			puts(serialized_string);
#endif
			json_free_serialized_string(serialized_string);
			json_value_free(dst_json);
		}
		json_value_free(user_data);
	} else if (number == 3) {
		idx = atoi(parts[number - 1]);
		sprintf(cmp_uri, "s2e_opm/config/%d", idx);
		if (idx > 0 && idx < 3) {
			if (string_hash(yurl.path) == string_hash(cmp_uri)) {
				user_data = json_parse_file("s2e_opm.json");
				ret = json_filter_from_json(&dst_json, user_data, yurl.query, idx);
				if (ret == REST_HTTP_STATUS_OK) {
					serialized_string = json_serialize_to_string(dst_json);
					rest_write(serialized_string, strlen(serialized_string));
#ifdef DBG1
					serialized_string = json_serialize_to_string_pretty(dst_json);
					puts(serialized_string);
#endif
					json_free_serialized_string(serialized_string);
					json_value_free(dst_json);
				}
				json_value_free(user_data);
			}
		} else {
			return REST_HTTP_STATUS_NOT_FOUND;
		}
	} else {
		ret = REST_HTTP_STATUS_BAD_REQUEST;
	}

	if (ret != REST_HTTP_STATUS_OK) {
		do_err_rest_rsp(ret);
		return ret;
	}
	return REST_HTTP_STATUS_OK;				

}

static REST_HTTP_STATUS s2e_rest_patch(const char *uri, char *input_data, int32_t input_data_size)
{
	char *tmp_str = NULL, *usr_obj_name;
	JSON_Value *usr_data, *ori_val, *rsp_val;
	JSON_Array *ori_array;
	JSON_Value_Type type;
	JSON_Object *obj;
	int array_cnt, i, j, obj_cnt;
	printf("s2e_rest_patch-uri=%s, input_data=%s, size=%d\r\n", uri, input_data, input_data_size);

	/* Get configuration json-obj */
	ori_val = json_parse_file("s2e_opm.json");
	ori_array = json_value_get_array(ori_val);
	array_cnt = json_array_get_count(ori_array);	
#if 1
	tmp_str = json_serialize_to_string_pretty(ori_val);
	printf("s2e_rest_patch1-%s, %d-%s\r\n", __func__, __LINE__, tmp_str);
#endif
	usr_data = json_parse_string(input_data);
	if (usr_data == NULL) {
		printf("Error %s-%d\r\n", __func__, __LINE__);
		return REST_HTTP_STATUS_BAD_REQUEST;
	}
#if 1	
	tmp_str = json_serialize_to_string_pretty(usr_data);
	printf("s2e_rest_patch2-%s, %d-%s\r\n", __func__, __LINE__, tmp_str);
#endif
	type = json_value_get_type(usr_data);
	if (type == JSONObject) {
		obj_cnt = json_object_get_count(json_object(usr_data));
#if 1
		printf("%s-%d, type = %x, obj_cnt = %d\r\n", __func__, __LINE__, type, obj_cnt);
#endif
	} else {
		printf("Error %s-%d\r\n", __func__, __LINE__);
		return REST_HTTP_STATUS_BAD_REQUEST;
	}

	for (i = 0; i < array_cnt; i++) {
		obj = json_array_get_object(ori_array, i);
		for (j = 0; j < obj_cnt; j++) {
			usr_obj_name = json_object_get_name(json_object(usr_data), j);
			type = json_object_member_get_type(obj, usr_obj_name);
			switch (type) {
			case JSONString:
				json_object_set_string(obj, 
					usr_obj_name, 
					json_object_get_string(json_object(usr_data), usr_obj_name));

				break;
			case JSONNumber:
				json_object_set_number(obj, 
					usr_obj_name, 
					json_object_get_number(json_object(usr_data), usr_obj_name));
				
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
	}
	json_serialize_to_file_pretty(ori_val, "s2e_opm.json");
#if 1
	rsp_val = json_object_get_wrapping_value(obj);
	tmp_str = json_serialize_to_string_pretty(rsp_val);
	printf("s2e_rest_patch3-%s, %d-%s\r\n", __func__, __LINE__, tmp_str);
	rest_write(tmp_str, strlen(tmp_str));
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

	while ((opt = getopt(argc, argv, "i:p:n:s:")) != -1) {
		switch (opt) {
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

	if (ip == NULL || port == -1 || module_name == NULL) {
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
	if ((id = rest_init(module_name)) < 0) {
		printf("(%s:%d)init rest fail (%d).\n", __func__, __LINE__, id);
		return -1;
	}

	/* Register your REST call back */
	if ((ret = rest_cb_register(id, "/config", REST_OP_GET, s2e_rest_get)) < 0) {
		printf("(%s:%d)register rest callback fail (%d).\n", __func__, __LINE__, ret);
		return -1;
	}

	if ((ret = rest_cb_register(id, "/config", REST_OP_PATCH, s2e_rest_patch)) < 0) {
		printf("(%s:%d)register rest callback fail (%d).\n", __func__, __LINE__, ret);
		return -1;
	}
	
	/* Start web server */
	if ((ret = http_server_start(port, url, size)) != HTTP_SERVER_OK)
		printf("(%s:%d)HTTP server start fail (%d).\n", __func__, __LINE__, ret);

	while (1)
		sleep(1);

	/* stop server & unregister rest callback function */
	http_server_stop();
	rest_cb_unregister(id);

	return 0;
}
