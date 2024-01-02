/*
 * canbus_bootloader.h
 *
 *  Created on: Dec 19, 2023
 *      Author: sum
 */

#ifndef SRC_CANBUS_BOOTLOADER_H_
#define SRC_CANBUS_BOOTLOADER_H_

/*
 * Frame Details (bytes 0-7 of CAN frame):
 * byte 0-1: packet index
 * byte 2-7: data
 */

#define PACKET_MAX_SIZE 8

// max data size supported: ~393kb
#define CAN_IDX_BYTES 2
#define CAN_DATA_BYTES 6

#endif /* SRC_CANBUS_BOOTLOADER_H_ */
