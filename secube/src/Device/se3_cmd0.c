/**
 *  \file se3_cmd0.c
 *  \author Nicola Ferri
 *  \brief L0 command handlers
 */

#include "se3_cmd0.h"
#include "se3_flash.h"

#define SE3_FLASH_SIGNATURE_ADDR  ((uint32_t)0x08020000)
#define SE3_FLASH_SIGNATURE_SIZE  ((size_t)0x40)


uint16_t L0d_echo(uint16_t req_size, const uint8_t* req, uint16_t* resp_size, uint8_t* resp)
{
    memcpy(resp, req, req_size);
    *resp_size = req_size;
    return SE3_OK;
}

uint16_t L0d_factory_init(uint16_t req_size, const uint8_t* req, uint16_t* resp_size, uint8_t* resp)
{
    enum {
        OFF_SERIAL = 0
    };
    const uint8_t* serial = req + OFF_SERIAL;
    se3_flash_it it;

	if (se3c0.serial.written) {
		return SE3_ERR_STATE;
	}

    se3_flash_it_init(&it);
    if (!se3_flash_it_new(&it, SE3_FLASH_TYPE_SERIAL, SE3_SERIAL_SIZE)) {
        return SE3_ERR_HW;
    }
    if (!se3_flash_it_write(&it, 0, serial, SE3_SERIAL_SIZE)) {
        return SE3_ERR_HW;
    }

    memcpy(se3c0.serial.data, serial, SE3_SERIAL_SIZE);
    se3c0.serial.written = true;
    return SE3_OK;
}


uint16_t L0d_bootmode_reset(uint16_t req_size, const uint8_t* req, uint16_t* resp_size, uint8_t* resp)
{

	if(!(se3_flash_bootmode_reset(SE3_FLASH_SIGNATURE_ADDR, SE3_FLASH_SIGNATURE_SIZE)))
		return SE3_ERR_HW;

	return SE3_OK;
}
