/* RPCemu networking */


// r0: Reason code in r0
// r1: Pointer to buffer for any error string
// r2-r5: Reason code dependent
// Returns 0 in r0 on success, non 0 otherwise and error buffer filled in
void networkswi(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r5, uint32_t *retr0, uint32_t *retr1);

void initnetwork(void);

