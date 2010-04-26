#ifndef NETWORK_H
#define NETWORK_H

/* RPCemu networking */

/* Functions provided by each host platform's network code */

// r0: Reason code in r0
// r1: Pointer to buffer for any error string
// r2-r5: Reason code dependent
// Returns 0 in r0 on success, non 0 otherwise and error buffer filled in
void networkswi(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3,
                uint32_t r4, uint32_t r5, uint32_t *retr0, uint32_t *retr1);

void initnetwork(void);
void network_reset(void);

/* Functions shared between each platform, in network.c */
void memcpytohost(void *dest, uint32_t src, uint32_t len);
void memcpyfromhost(uint32_t dest, const void *source, uint32_t len);
void strcpyfromhost(uint32_t dest, const char *source);

/* Structures and variables shared between each host platform's network code */

/* Structures to represent the RISC OS view of things */
struct pkthdr {
    uint32_t len;
    uint32_t rcvif;
};

struct mbuf {
    uint32_t m_next;
    uint32_t m_list;
    uint32_t m_off;
    uint32_t m_len;
    uint32_t m_inioff;
    uint32_t m_inilen;
    uint8_t m_type;
    uint8_t m_sys1;
    uint8_t m_sys2;
    uint8_t m_flags;
    struct pkthdr m_pkthdr;
};

struct rx_hdr {
    uint32_t rx_ptr;
    uint32_t rx_tag;
    uint8_t rx_src_addr[6];
    uint8_t _spad[2];
    uint8_t rx_dst_addr[6];
    uint8_t _dpad[2];
    uint32_t rx_frame_type;
    uint32_t rx_error_level;
    uint32_t rx_cksum;
};

#endif
