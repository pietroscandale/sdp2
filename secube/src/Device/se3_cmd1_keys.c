/**
 *  \file se3_cmd1_keys.c
 *  \author Nicola Ferri
 *  \brief L1 handlers for key management operations
 */

#include "se3_cmd1_keys.h"
#include "se3_keys.h"

/** \brief insert, delete or update key
 *  
 *  key_edit : (op:ui16, id:ui32, validity:ui32, data-len:ui16, name-len:ui16, data[data-len], name[name-len]) => ()
 */
uint16_t L1d_key_edit(uint16_t req_size, const uint8_t* req, uint16_t* resp_size, uint8_t* resp)
{
    struct {
        uint16_t op;
        uint32_t id;
        uint32_t validity;
        uint16_t data_len;
        uint16_t name_len;
        const uint8_t* data;
        const uint8_t* name;
    } req_params;
    
    se3_flash_key key;
	bool equal;
    se3_flash_it it = { .addr = NULL };

    if (req_size < SE3_CMD1_KEY_EDIT_REQ_OFF_DATA_AND_NAME) {
        SE3_TRACE(("[L1d_key_edit] req size mismatch\n"));
        return SE3_ERR_PARAMS;
    }

    if (!se3c1.login.y) {
        SE3_TRACE(("[L1d_key_edit] not logged in\n"));
        return SE3_ERR_ACCESS;
    }

    SE3_GET16(req, SE3_CMD1_KEY_EDIT_REQ_OFF_OP, req_params.op);
    SE3_GET32(req, SE3_CMD1_KEY_EDIT_REQ_OFF_ID, req_params.id);
    SE3_GET32(req, SE3_CMD1_KEY_EDIT_REQ_OFF_VALIDITY, req_params.validity);
    SE3_GET16(req, SE3_CMD1_KEY_EDIT_REQ_OFF_DATA_LEN, req_params.data_len);
    SE3_GET16(req, SE3_CMD1_KEY_EDIT_REQ_OFF_NAME_LEN, req_params.name_len);
    req_params.data = req + SE3_CMD1_KEY_EDIT_REQ_OFF_DATA_AND_NAME;
    req_params.name = req + SE3_CMD1_KEY_EDIT_REQ_OFF_DATA_AND_NAME + req_params.data_len;
    
    // check params
    if ((req_params.data_len > SE3_KEY_DATA_MAX) || (req_params.name_len > SE3_KEY_NAME_MAX)) {
        return SE3_ERR_PARAMS;
    }

    key.id = req_params.id;
    key.data_size = req_params.data_len;
    key.name_size = req_params.name_len;
    key.validity = req_params.validity;
    key.data = (uint8_t*)req_params.data;
    key.name = (uint8_t*)req_params.name;

    se3_flash_it_init(&it);
    if (!se3_key_find(key.id, &it)) {
        it.addr = NULL;
    }

    switch (req_params.op) {
    case SE3_KEY_OP_INSERT:
        if (NULL != it.addr) {
            return SE3_ERR_RESOURCE;
        }
        if (!se3_key_new(&it, &key)) {
            SE3_TRACE(("[L1d_key_edit] se3_key_new failed\n"));
            return SE3_ERR_MEMORY;
        }
        break;
    case SE3_KEY_OP_DELETE:
        if (NULL == it.addr) {
            return SE3_ERR_RESOURCE;
        }
        if (!se3_flash_it_delete(&it)) {
            return SE3_ERR_HW;
        }
        break;
    case SE3_KEY_OP_UPSERT:
		equal = false;
        if (NULL != it.addr) {
            // do not replace if equal
			equal = se3_key_equal(&it, &key);
			if (!equal) {
				if (!se3_flash_it_delete(&it)) {
					return SE3_ERR_HW;
				}
			}
        }
        it.addr = NULL;
		if (!equal) {
			if (!se3_key_new(&it, &key)) {
				SE3_TRACE(("[L1d_key_edit] se3_key_new failed\n"));
				return SE3_ERR_MEMORY;
			}
		}
        break;
    default:
        SE3_TRACE(("[L1d_key_edit] invalid op\n"));
        return SE3_ERR_PARAMS;
    }
   
	return SE3_OK;
}



/** \brief list all keys in device
 *  
 *  key_list : (skip:ui16, nmax:ui16, salt[32]) => (count:ui16, keyinfo0, keyinfo1, ...)
 *      keyinfo: (id:ui32, validity:ui32, data-len:ui16, name-len:ui16, name[name-len], fingerprint[32])
 */
uint16_t L1d_key_list(uint16_t req_size, const uint8_t* req, uint16_t* resp_size, uint8_t* resp)
{
    struct {
        uint16_t skip;
        uint16_t nmax;
		uint8_t* salt;
    } req_params;
    struct {
        uint16_t count;
    } resp_params;

    se3_flash_key key;
    se3_flash_it it = { .addr = NULL };
    size_t size = 0;
    size_t key_info_size = 0;
    uint8_t* p;
    uint16_t skip;
    uint8_t tmp[SE3_KEY_NAME_MAX];
	uint8_t fingerprint[SE3_KEY_FINGERPRINT_SIZE];
	
    if (req_size != SE3_CMD1_KEY_LIST_REQ_SIZE) {
        SE3_TRACE(("[L1d_key_list] req size mismatch\n"));
        return SE3_ERR_PARAMS;
    }

    if (!se3c1.login.y) {
        SE3_TRACE(("[L1d_key_list] not logged in\n"));
        return SE3_ERR_ACCESS;
    }

    SE3_GET16(req, SE3_CMD1_KEY_LIST_REQ_OFF_SKIP, req_params.skip);
    SE3_GET16(req, SE3_CMD1_KEY_LIST_REQ_OFF_NMAX, req_params.nmax);
	req_params.salt = req + SE3_CMD1_KEY_LIST_REQ_OFF_SALT;

	/* ! will write key data to request buffer */
	key.data = (uint8_t*)req + ((SE3_CMD1_KEY_LIST_REQ_SIZE / 16) + 1) * 16;
    key.name = tmp;
    resp_params.count = 0;
    skip = req_params.skip;
    size = SE3_CMD1_KEY_LIST_RESP_OFF_KEYINFO;
    p = resp + SE3_CMD1_KEY_LIST_RESP_OFF_KEYINFO;
    while (se3_flash_it_next(&it)) {
        if (it.type == SE3_TYPE_KEY) {
            if (skip) {
                skip--;
                continue;
            }
            se3_key_read(&it, &key);
            key_info_size = SE3_CMD1_KEY_LIST_KEYINFO_OFF_NAME + key.name_size;
            if (size + key_info_size > SE3_RESP1_MAX_DATA) {
                break;
            }
			se3_key_fingerprint(&key, req_params.salt, fingerprint);
            SE3_SET32(p, SE3_CMD1_KEY_LIST_KEYINFO_OFF_ID, key.id);
            SE3_SET32(p, SE3_CMD1_KEY_LIST_KEYINFO_OFF_VALIDITY, key.validity);
            SE3_SET16(p, SE3_CMD1_KEY_LIST_KEYINFO_OFF_DATA_LEN, key.data_size);
            SE3_SET16(p, SE3_CMD1_KEY_LIST_KEYINFO_OFF_NAME_LEN, key.name_size);
			memcpy(p + SE3_CMD1_KEY_LIST_KEYINFO_OFF_FINGERPRINT, fingerprint, SE3_KEY_FINGERPRINT_SIZE);
            memcpy(p + SE3_CMD1_KEY_LIST_KEYINFO_OFF_NAME, key.name, key.name_size);
            p += key_info_size;
            size += key_info_size;
            (resp_params.count)++;
            if (resp_params.count >= req_params.nmax) {
                break;
            }
        }
    }
	memset(key.data, 0, SE3_KEY_DATA_MAX);

    SE3_SET16(resp, SE3_CMD1_KEY_LIST_RESP_OFF_COUNT, resp_params.count);
    *resp_size = (uint16_t)size;
	
    return SE3_OK;
}