#ifndef NETWORK_NAT_H
#define NETWORK_NAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern int network_nat_init(void);
extern void network_nat_reset(void);

extern void network_nat_poll(void);

extern uint32_t network_nat_tx(uint32_t errbuf, uint32_t mbufs, uint32_t dest, uint32_t src, uint32_t frametype);
extern uint32_t network_nat_rx(uint32_t errbuf, uint32_t mbuf, uint32_t rxhdr, uint32_t *dataavail);
extern void network_nat_setirqstatus(uint32_t address);

extern void network_nat_forward_add(PortForwardRule rule);
extern void network_nat_forward_remove(PortForwardRule rule);
extern void network_nat_forward_edit(PortForwardRule old_rule, PortForwardRule new_rule);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif
