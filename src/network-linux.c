/* RPCemu networking */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_tun.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <linux/sockios.h>

#include "rpcemu.h"
#include "mem.h"
#include "podules.h"
#include "network.h"

/* The opened tunnel device */
static int tunfd = -1;

/* Max packet is 1500 bytes plus headers */
static unsigned char buffer[1522];

/**
 * Given a system username, lookup their uid and gid
 *
 * @param user username
 * @param uid
 * @param gid
 * @return 0 on failure (username not found), 1 otherwise
 */
static int
nametouidgid(const char *user, uid_t *uid, gid_t *gid)
{
    const struct passwd *pw = getpwnam(user);
  
    if (pw) {
        *uid = pw->pw_uid;
        if (gid) {
            *gid = pw->pw_gid;
        }
        return 1;
    }
    return 0;
}

static int dropprivileges(uid_t uid, gid_t gid)
{
    if (setgid(gid) != 0 || setuid(uid) != 0)
        return -1;
    else
        return 0;
}

/* Open and configure the tunnel device */
static int tun_alloc(void)
{
    struct ifreq ifr;
    struct sockaddr_in *addr;
    int fd;
    int sd;

    assert(config.network_type == NetworkType_EthernetBridging ||
           config.network_type == NetworkType_IPTunnelling);

    if (config.network_type == NetworkType_IPTunnelling && config.ipaddress == NULL) {
        error("IP address not configured");
        return -1;
    }
    
    if (config.network_type == NetworkType_EthernetBridging && config.bridgename == NULL) {
        error("Network Bridgename not configured");
        return -1;
    }

    if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
        error("Error opening /dev/net/tun device: %s", strerror(errno));
        return -1;
    }
    
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP; 
    
    if (ioctl(fd, TUNSETIFF, &ifr) < 0) {
        error("Error setting TAP on tunnel device: %s", strerror(errno));
        return -1;
    }

    ioctl(fd, TUNSETNOCSUM, 1);

    if ((sd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
        error("Error getting socket: %s", strerror(errno));
        return -1;
    }

    if (config.network_type == NetworkType_IPTunnelling) {
        addr = (struct sockaddr_in *)(&(ifr.ifr_addr));
        addr->sin_family = AF_INET;
        addr->sin_port = 0;
        inet_aton(config.ipaddress, &(addr->sin_addr));
        if (ioctl(sd, SIOCSIFADDR, &ifr) == -1) {
            error("Error assigning %s addr: %s",
                  ifr.ifr_name, strerror(errno));
            return -1;
        }
    } else if (config.network_type == NetworkType_EthernetBridging) {
        struct ifreq brifr;

        if (ioctl(sd, SIOCGIFINDEX, &ifr) == -1) {
            error("Error interface index for %s: %s",
                  ifr.ifr_name, strerror(errno));
            return -1;
        }
        strncpy(brifr.ifr_name, config.bridgename, IFNAMSIZ);
        brifr.ifr_name[IFNAMSIZ - 1] = '\0';
        brifr.ifr_ifindex = ifr.ifr_ifindex;
        if (ioctl(sd, SIOCBRADDIF, &brifr) == -1) {
            error("Error adding %s to bridge: %s",
                  ifr.ifr_name, strerror(errno));
            return -1;
        }
    }

    /* Get the current flags */
    if (ioctl(sd, SIOCGIFFLAGS, &ifr) == -1) {
        error("Error getting %s flags: %s", ifr.ifr_name, strerror(errno));
        return -1;
    }
    
    /* Turn on the UP flag */
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(sd, SIOCSIFFLAGS, &ifr) == -1) {
        error("Error setting %s flags: %s", ifr.ifr_name, strerror(errno));
        return -1;
    }

    if (config.macaddress != NULL) {
        /* Parse supplied MAC address */
        if (!network_macaddress_parse(config.macaddress, network_hwaddr)) {
            error("Unable to parse '%s' as a MAC address", config.macaddress);
            return -1;
        }
    } else {
        /* Get the hardware address */
        if (ioctl(sd, SIOCGIFHWADDR, &ifr) == -1) {
            error("Error getting %s hardware address: %s",
                  ifr.ifr_name, strerror(errno));
            return -1;
        }

        /* Calculate the emulated hardware address */
        network_hwaddr[0] = 0x02;
        network_hwaddr[1] = 0x00;
        network_hwaddr[2] = 0xA4;
        network_hwaddr[3] = ifr.ifr_hwaddr.sa_data[3];
        network_hwaddr[4] = ifr.ifr_hwaddr.sa_data[4];
        network_hwaddr[5] = ifr.ifr_hwaddr.sa_data[5];
    }

    close(sd);

    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        error("Error setting %s non-blocking: %s",
              ifr.ifr_name, strerror(errno));
        return -1;
    }

    return fd;
}

#define HEADERLEN 18

/* Transmit data
   errbuf - pointer to buffer to return error string
   mbufs - pointer to mbuf chain containing data to send
   dest - pointer to detination MAC address
   src - pointer to source MAC address, or 0 to use default

   returns errbuf on error, else zero
*/
uint32_t
network_plt_tx(uint32_t errbuf, uint32_t mbufs, uint32_t dest, uint32_t src, uint32_t frametype)
{
    unsigned char *buf = buffer;
    struct mbuf txb;
    int packetlength;
    int ret;
    int i;

    if (tunfd == -1) {
        strcpyfromhost(errbuf, "RPCEmu: Networking not available");
        return errbuf;
    }

    /* Ethernet packet is
       4 bytes preamble
       6 bytes destination MAC address
       6 bytes source MAC address
       2 bytes frame type (Ethernet II) or length (IEEE 802.3)
       up to 1500 bytes payload
    */

    for (i = 0; i < 4; i++) *buf++ = 0;

    memcpytohost(buf, dest, 6);
    buf += 6;

    if (src) {
        memcpytohost(buf, src, 6);
    } else {
        for (i = 0; i < 6; i++) {
            buf[i] = network_hwaddr[i];
        }
    }
    buf += 6;

    *buf++ = (frametype>>8) & 0xFF;
    *buf++ = frametype & 0xFF;

    packetlength = HEADERLEN;


    /* Copy the mbuf chain as the payload */
    while (mbufs) {
        memcpytohost(&txb, mbufs, sizeof(struct mbuf));
        packetlength += txb.m_len;
        if (packetlength > sizeof(buffer)) {
            strcpyfromhost(errbuf, "RPCEmu: Packet too large to send");
            return errbuf;
        }
        memcpytohost(buf, mbufs + txb.m_off, txb.m_len);
        buf += txb.m_len;
        mbufs = txb.m_next;
    }

    do {
        ret = write(tunfd, buffer, packetlength);
    } while ((ret == -1) && (errno == EAGAIN));
    if (ret == -1) {
        strcpyfromhost(errbuf, strerror(errno));
        return errbuf;
    }

    return 0;
}

uint32_t
network_plt_rx(uint32_t errbuf, uint32_t mbuf, uint32_t rxhdr, uint32_t *dataavail)
{
    struct mbuf rxb;
    struct rx_hdr hdr;
    int packetlength;

    *dataavail = 0;

    if (tunfd == -1) {
        strcpyfromhost(errbuf, "RPCEmu: Networking not available");
        return errbuf;
    }

    memset(&hdr, 0, sizeof(hdr));

    packetlength = read(tunfd, buffer, sizeof(buffer));
    if (packetlength == -1) {
        if (errno == EAGAIN) return 0;
        strcpyfromhost(errbuf, strerror(errno));
        return errbuf;
    }
        
    if (mbuf && packetlength > HEADERLEN) {
        unsigned char *payload = buffer + HEADERLEN;

        /* Fill in recieved header structure */
        memcpy(hdr.rx_dst_addr, buffer + 4, 6);
        memcpy(hdr.rx_src_addr, buffer + 10, 6);
        hdr.rx_frame_type = (buffer[16] << 8) | buffer[17];
        hdr.rx_error_level = 0;
        memcpyfromhost(rxhdr, &hdr, sizeof(hdr));

        packetlength -= HEADERLEN;
        
        memcpytohost(&rxb, mbuf, sizeof(rxb));

        if (packetlength > rxb.m_inilen) {
            strcpyfromhost(errbuf, "RPCEmu: Mbuf too small for received packet");
            return errbuf;
        } else {
            /* Copy payload in to the mbuf */
            rxb.m_off = rxb.m_inioff;
            memcpyfromhost(mbuf + rxb.m_off, payload, packetlength);
            rxb.m_len = packetlength;
            memcpyfromhost(mbuf, &rxb, sizeof(rxb));

            *dataavail = 1;
        }
    }

    return 0;
}

// Pointer to a word in RMA, used as the IRQ status register
static uint32_t irqstatus = 0;

static void
sig_io(int sig)
{
    NOT_USED(sig);

    mem_write8(irqstatus, 1);
    if (network_poduleinfo != NULL) {
        network_poduleinfo->irq = 1;
    }
    rethinkpoduleints();
}


void
network_plt_setirqstatus(uint32_t address)
{
    struct sigaction sa;
    int flags;

    memset(&sa, 0, sizeof(sa));

    if ((flags = fcntl(tunfd, F_GETFL)) == -1) {
        error("Error getting flags for network device: %s", strerror(errno));
        return;
    }

    if (irqstatus) {
        if (fcntl(tunfd, F_SETFL, flags & ~O_ASYNC) == -1) {
            error("Error disabling SIGIO for network device: %s",
                  strerror(errno));
            return;
        }

        sa.sa_handler = SIG_DFL;

        if (sigaction(SIGIO, &sa, NULL) == -1) {
            error("Error uninstalling SIGIO handler: %s", strerror(errno));
            return;
        }
    }

    irqstatus = address;

    if (irqstatus) {
        sa.sa_handler = sig_io;

        if (sigaction(SIGIO, &sa, NULL) == -1) {
            error("Error installing SIGIO handler: %s", strerror(errno));
            return;
        }

        if (fcntl(tunfd, F_SETFL, flags | O_ASYNC) == -1) {
            error("Error enabling SIGIO for network device: %s",
                  strerror(errno));
            return;
        }
    }
}

int
network_plt_init(void)
{ 
    tunfd = tun_alloc();

    /* After attempting to configure the network we no longer need root
       privileges, so drop them and refuse to carry on if we can't */

    /* At the moment we only support extended privilege gain via the setuid
       on the binary or running rpcemu with the sudo command, this should check
       for both of them */
    if (getuid() == 0 || geteuid() == 0) { /* Running as root */
        const char *user = config.username;
        uid_t uid = 0;
        gid_t gid = 0;

        /* Use configured username if available, otherwise see if
           we are running from a sudo command */
        if (user == NULL) {
            user = getenv("SUDO_USER");
        }

        if (user == NULL) {
            fatal("No username available to return to non privileged mode, "
                  "refusing to continue running as root");
        }

        if (!nametouidgid(user, &uid, &gid)) {
            fatal("Could not find username '%s' on the system, or error",
                  user);
        }

        if (dropprivileges(uid, gid) < 0) {
            fatal("Error dropping privileges: %s", strerror(errno));
        }

        rpclog("Networking: Dropping runtime privileges back to '%s'\n", user);
    }

    if (tunfd != -1) {
        return 1;
    } else {
        return 0;
    }
}

/**
 * Shutdown any running network components.
 *
 * Called on program shutdown and program reset after
 * configuration has changed.
 */
void
network_plt_reset(void)
{
	if (tunfd != -1) {
		close(tunfd);
		tunfd = -1;
	}
}
