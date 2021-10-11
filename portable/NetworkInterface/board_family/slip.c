// Created by Indraneel on 9/10/21

// Reference : https://github.com/lobaro/util-slip

#include "slip.h"

/**
 * @brief for each byte in the packet, send the appropriate character
	 sequence
 * 
 * */
int slip_encode(const uint8_t* p, int len, uint8_t* d) {
	/* for each byte in the packet, send the appropriate character
	 * sequence
	 */
    // local backup of data during encoding
    memcpy(p,d,len*sizeof(uint8_t));

    int tot_len = len;

    /* send an initial END character to flush out any data that may
	 * have accumulated in the receiver due to line noise
	 */
    *d++ = SLIP_END;
    tot_len++;

	while (len--) {
		switch (*p) {
		/* if it's the same code as an END character, we send a
		 * special two character code so as not to make the
		 * receiver think we sent an END
		 */
		case SLIP_END:
            *d++ = SLIP_ESC;
            tot_len++;
            *d++ = SLIP_ESC_END;
            tot_len++;
			break;

			/* if it's the same code as an ESC character,
			 * we send a special two character code so as not
			 * to make the receiver think we sent an ESC
			 */
		case SLIP_ESC:
            *d++ = SLIP_ESC;
            tot_len++;
            *d++ = SLIP_ESC_ESC;
            tot_len++;
			break;
			/* otherwise, we just send the character
			 */
		default:
            *d++ = *p;
		}
		p++;
	}

    /* tell the receiver that we're done sending the packet
	 */
    *d++ = SLIP_END;
    tot_len++;

    return tot_len;
}


/* RECV_PACKET: reads a packet from buf into the buffer located at "p".
 *      If more than len bytes are received, the packet will
 *      be truncated.
 *      Returns the number of bytes stored in the buffer.
 */
int slip_read_packet(uint8_t *buf, uint8_t *p, int len) {
	char c;
	int data_len = 0;
    int it=0;

	if (len == 0) {
		return 0;
	}

	/* sit in a loop reading bytes until we put together
	 * a whole packet.
	 * Make sure not to copy them into the packet if we
	 * run out of room.
	 */
	 while (it<len) {
		/* get a character to process
		 */
        c = *(buf+it);
        it++;

		/* handle bytestuffing if necessary
		 */
		switch (c) {

		/* if it's an END character then we're done with
		 * the packet
		 */
		case SLIP_END:
            // Do nothing
			break;

			/* if it's the same code as an ESC character, wait
			 * and get another character and then figure out
			 * what to store in the packet based on that.
			 */
		case SLIP_ESC:
            if(it<len)
            {
                c = *(buf+it);
                it++;
                /* if "c" is not one of these two, then we
                * have a protocol violation.  The best bet
                * seems to be to leave the byte alone and
                * just stuff it into the packet
                */
                switch (c) {
                case SLIP_ESC_END:
                    c = SLIP_END;
                    break;
                case SLIP_ESC_ESC:
                    c = SLIP_ESC;
                    break;
                }
                // Store the character
                *(p+data_len) = c;
                data_len++;

            }
			break;
		default:
			// Store the character
			*(p+data_len) = c;
            data_len++;
		}
	}

    return data_len;
}
