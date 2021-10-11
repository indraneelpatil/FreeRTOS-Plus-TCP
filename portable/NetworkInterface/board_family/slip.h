// Created by Indraneel on 9/10/21

// Reference : https://github.com/lobaro/util-slip

#ifndef SRC_UTIL_SLIP_SLIP_H_
#define SRC_UTIL_SLIP_SLIP_H_

#include <stdint.h>

/* SLIP special character codes
 */
#define SLIP_END             ((uint8_t)0300)    /* indicates end of packet */
#define SLIP_ESC             ((uint8_t)0333)    /* indicates byte stuffing */
#define SLIP_ESC_END         ((uint8_t)0334)    /* ESC ESC_END means END data byte */
#define SLIP_ESC_ESC         ((uint8_t)0335)    /* ESC ESC_ESC means ESC data byte */


/* slip_recv_packet: reads a packet from buf into the buffer
 * located at "p". If more than len bytes are received, the
 * packet will be truncated.
 * Returns the number of bytes stored in the buffer.
 * Returns 0 if the buffer does not contain a full packet.
 */
int slip_read_packet(uint8_t *buf, uint8_t *p, int len);
int slip_encode(const uint8_t* p, int len, uint8_t* d);


#endif /* SRC_UTIL_SLIP_SLIP_H_ */