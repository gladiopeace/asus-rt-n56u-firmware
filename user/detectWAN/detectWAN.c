/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/sockios.h>
#include <netinet/if_ether.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <nvram/bcmnvram.h>
#include <unistd.h>

#define ETHER_ADDR_STR_LEN	18
#define MAC_BCAST_ADDR		(uint8_t *) "\xff\xff\xff\xff\xff\xff"
#define WAN_IF			"eth3"
#define LAN_IF			"br0"
//#define DEBUG		1

char *get_lan_ipaddr();
char *get_wan_ipaddr();
char *gateway_ip;

#include<syslog.h>
#include<stdarg.h>
void logmessage(char *logheader, char *fmt, ...)
{
	va_list args;
	char buf[512];

	va_start(args, fmt);

	vsnprintf(buf, sizeof(buf), fmt, args);
	openlog(logheader, 0, 0);
	syslog(0, buf);
	closelog();
	va_end(args);
}

void
chk_udhcpc()
{
	if ((nvram_match("wan_route_x", "IP_Routed") && nvram_match("wan0_proto", "dhcp") && !nvram_match("manually_disconnect_wan", "1"))
			|| nvram_match("wan_route_x", "IP_Bridged")
			)
	{
#ifdef DEBUG
        	printf("try to refresh udhcpc\n");      // tmp test
#endif
		if (nvram_match("sw_mode", "1")) {
			system("killall -SIGTERM wanduck");
			sleep(3);
		}

		if (	(nvram_match("wan_route_x", "IP_Routed") && !strcmp(get_wan_ipaddr(), "0.0.0.0"))/* ||
			(nvram_match("wan_route_x", "IP_Bridged") && !strcmp(get_lan_ipaddr(), "0.0.0.0"))*/
		)
		{
			logmessage("detectWAN", "perform DHCP release");
			system("killall -SIGUSR2 udhcpc");	// J++			
		}

		logmessage("detectWAN", "perform DHCP renew");
		system("killall -SIGUSR1 udhcpc");

		if (nvram_match("sw_mode", "1")) {
			system("wanduck&");
		}

		if (nvram_match("wan_route_x", "IP_Bridged"))
		{
			logmessage("detectWAN", "link down LAN ports");
			system("rtl8367m_AllPort_linkDown");
			logmessage("detectWAN", "radio off");
			if (nvram_match("wl_radio_x", "1"))
				system("radioctrl 0");
			if (nvram_match("rt_radio_x", "1"))
				system("radioctrl_rt 0");
			sleep(10);
			logmessage("detectWAN", "link up LAN ports");
			system("rtl8367m_AllPort_linkUp");
			sleep(5);
			logmessage("detectWAN", "radio on");
			if (nvram_match("wl_radio_x", "1"))
				system("radioctrl 1");
			if (nvram_match("rt_radio_x", "1"))
				system("radioctrl_rt 1");
		}
	}
}

char *
get_lan_ipaddr()
{
	int s;
	struct ifreq ifr;
	struct sockaddr_in *inaddr;
	struct in_addr ip_addr;

	/* Retrieve IP info */
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		return strdup("0.0.0.0");

	strncpy(ifr.ifr_name, LAN_IF, IFNAMSIZ);
	inaddr = (struct sockaddr_in *)&ifr.ifr_addr;
	inet_aton("0.0.0.0", &inaddr->sin_addr);	

	/* Get IP address */
	ioctl(s, SIOCGIFADDR, &ifr);
	close(s);	

	ip_addr = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr;
#ifdef DEBUG
	fprintf(stderr, "current LAN IP address: %s\n", inet_ntoa(ip_addr));
#endif
	return inet_ntoa(ip_addr);
}

char *
get_wan_ipaddr()
{
	int s;
	struct ifreq ifr;
	struct sockaddr_in *inaddr;
	struct in_addr ip_addr;

	if (nvram_match("wan_route_x", "IP_Bridged"))
		return strdup("0.0.0.0");

	/* Retrieve IP info */
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		return strdup("0.0.0.0");

	if (nvram_match("wan0_proto", "dhcp") || nvram_match("wan0_proto", "static"))
		strncpy(ifr.ifr_name, WAN_IF, IFNAMSIZ);
	else
		strncpy(ifr.ifr_name, "ppp0", IFNAMSIZ);
	inaddr = (struct sockaddr_in *)&ifr.ifr_addr;
	inet_aton("0.0.0.0", &inaddr->sin_addr);	

	/* Get IP address */
	ioctl(s, SIOCGIFADDR, &ifr);
	close(s);	

	ip_addr = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr;
#ifdef DEBUG
	fprintf(stderr, "current WAN IP address: %s\n", inet_ntoa(ip_addr));
#endif
	return inet_ntoa(ip_addr);
}

struct arpMsg {
	/* Ethernet header */
	uint8_t  h_dest[6];			/* destination ether addr */
	uint8_t  h_source[6];			/* source ether addr */
	uint16_t h_proto;			/* packet type ID field */

	/* ARP packet */
	uint16_t htype;				/* hardware type (must be ARPHRD_ETHER) */
	uint16_t ptype;				/* protocol type (must be ETH_P_IP) */
	uint8_t  hlen;				/* hardware address length (must be 6) */
	uint8_t  plen;				/* protocol address length (must be 4) */
	uint16_t operation;			/* ARP opcode */
	uint8_t  sHaddr[6];			/* sender's hardware address */
	uint8_t  sInaddr[4];			/* sender's IP address */
	uint8_t  tHaddr[6];			/* target's hardware address */
	uint8_t  tInaddr[4];			/* target's IP address */
	uint8_t  pad[18];			/* pad for min. Ethernet payload (60 bytes) */
} ATTRIBUTE_PACKED;

/* args:	yiaddr - what IP to ping
 *		ip - our ip
 *		mac - our arp address
 *		interface - interface to use
 * retn:	1 addr free
 *		0 addr used
 *		-1 error
 */
                                                                                                              
static const int one = 1;

int setsockopt_broadcast(int fd)
{
    return setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
}

long uptime(void)
{
    struct sysinfo info;
    sysinfo(&info);
    return info.uptime;
}

/* FIXME: match response against chaddr */
int arpping(/*uint32_t yiaddr, uint32_t ip, uint8_t *mac, char *interface*/)
{
	uint32_t yiaddr;
	uint32_t ip;
	uint8_t mac[6];
	char wanmac[18];
	char tmp[3];
	int i, ret;
	char DEV[8];

	if (nvram_match("wan_route_x", "IP_Routed"))
	{
		inet_aton(nvram_safe_get("wan_gateway_t"), &yiaddr);
		inet_aton(get_wan_ipaddr(), &ip);
		strcpy(wanmac, nvram_safe_get("wan0_hwaddr"));	// WAN MAC address
	}
	else
	{
		inet_aton(nvram_safe_get("lan_gateway_t"), &yiaddr);
		inet_aton(get_lan_ipaddr(), &ip);
		strcpy(wanmac, nvram_safe_get("lan_hwaddr"));	// br0 MAC address
	}

        wanmac[17]=0;
        for(i=0;i<6;i++)
        {
                tmp[2]=0;
                strncpy(tmp, wanmac+i*3, 2);
                mac[i]=strtol(tmp, (char **)NULL, 16);
        }

	int	timeout = 2;
	int	s;			/* socket */
	int	rv = 0;			/* return value */
	struct sockaddr addr;		/* for interface name */
	struct arpMsg	arp;
	fd_set		fdset;
	struct timeval	tm;
	time_t		prevTime;


	s = socket(PF_PACKET, SOCK_PACKET, htons(ETH_P_ARP));
	if (s == -1) {
#ifdef DEBUG
		fprintf(stderr, "cannot create raw socket\n");
#endif
		return 0;
	}

	if (setsockopt_broadcast(s) == -1) {
#ifdef DEBUG		
		fprintf(stderr, "cannot setsocketopt on raw socket\n");
#endif
		close(s);
		return 0;
	}

	/* send arp request */
	memset(&arp, 0, sizeof(arp));
	memcpy(arp.h_dest, MAC_BCAST_ADDR, 6);		/* MAC DA */
	memcpy(arp.h_source, mac, 6);			/* MAC SA */
	arp.h_proto = htons(ETH_P_ARP);			/* protocol type (Ethernet) */
	arp.htype = htons(ARPHRD_ETHER);		/* hardware type */
	arp.ptype = htons(ETH_P_IP);			/* protocol type (ARP message) */
	arp.hlen = 6;					/* hardware address length */
	arp.plen = 4;					/* protocol address length */
	arp.operation = htons(ARPOP_REQUEST);		/* ARP op code */
	memcpy(arp.sInaddr, &ip, sizeof(ip));		/* source IP address */
	memcpy(arp.sHaddr, mac, 6);			/* source hardware address */
	memcpy(arp.tInaddr, &yiaddr, sizeof(yiaddr));	/* target IP address */

	memset(&addr, 0, sizeof(addr));
	memset(DEV, 0, sizeof(DEV));
	if (nvram_match("wan_route_x", "IP_Routed"))
		strcpy(DEV, WAN_IF);
	else
		strcpy(DEV, LAN_IF);
	strncpy(addr.sa_data, DEV, sizeof(addr.sa_data));

	if (setsockopt(s, SOL_SOCKET, SO_BINDTODEVICE, DEV, IFNAMSIZ) != 0)	// J++
        {
#ifdef DEBUG
                fprintf(stderr, "setsockopt error: %s\n", DEV);
                perror("setsockopt set:");
#endif
        }

	ret = sendto(s, &arp, sizeof(arp), 0, &addr, sizeof(addr));

        if (setsockopt(s, SOL_SOCKET, SO_BINDTODEVICE, "", IFNAMSIZ) != 0)	// J++
        {
#ifdef DEBUG
                fprintf(stderr, "setsockopt error: %s\n", "");
                perror("setsockopt reset:");
#endif
        }

	if (ret < 0)
	{
		sleep(1);
		return 0;
	}

	/* wait arp reply, and check it */
	tm.tv_usec = 0;
	prevTime = uptime();
	while (timeout > 0) {
		FD_ZERO(&fdset);
		FD_SET(s, &fdset);
		tm.tv_sec = timeout;
		if (select(s + 1, &fdset, (fd_set *) NULL, (fd_set *) NULL, &tm) < 0) {
#ifdef DEBUG
			fprintf(stderr, "error on ARPING request\n");
#endif
			if (errno != EINTR) rv = 0;
		} else if (FD_ISSET(s, &fdset)) {
			if (recv(s, &arp, sizeof(arp), 0) < 0 ) rv = 0;
			if (arp.operation == htons(ARPOP_REPLY) &&
			    memcmp(arp.tHaddr, mac, 6) == 0 &&
			    *((uint32_t *) arp.sInaddr) == yiaddr) {
#ifdef DEBUG
				fprintf(stderr, "Valid arp reply from [%02X:%02X:%02X:%02X:%02X:%02X]\n",
					(unsigned char)arp.sHaddr[0],
					(unsigned char)arp.sHaddr[1],
					(unsigned char)arp.sHaddr[2],
					(unsigned char)arp.sHaddr[3],
					(unsigned char)arp.sHaddr[4],
					(unsigned char)arp.sHaddr[5]);
#endif
				close(s);
				rv = 1;
				return 1;
			}
		}
		timeout -= uptime() - prevTime;
		prevTime = uptime();
	}

	close(s);
#ifdef DEBUG
	fprintf(stderr, "%salid arp reply\n", rv ? "V" : "No v");
#endif
	return rv;
}

int is_phyconnected()
{
	if (nvram_match("wan_route_x", "IP_Routed"))
	{
		if (nvram_match("link_wan", "1"))
			return 1;
		else
			return 0;
	}
	else
	{
		if (nvram_match("link_lan", "1"))
			return 1;
		else
			return 0;
	}
}

#define MAX_ARP_RETRY 3

int detectWAN_arp()
{
	int count;

	while (1)
	{
		count = 0;

		while (count < MAX_ARP_RETRY)
		{
			if (!is_phyconnected())
			{
				count++;
				sleep(1);
			}
			else if (arpping())
			{
				break;
			}
			else
			{
				count++;
			}
		}

		if (is_phyconnected() && (count == MAX_ARP_RETRY))
		{
			chk_udhcpc();
		}

		sleep(15);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;

	for(;;)
	{
		if (nvram_match("wan_route_x", "IP_Routed"))
			gateway_ip = nvram_safe_get("wan_gateway_t");
		else
			gateway_ip = nvram_safe_get("lan_gateway_t");

		/* if not valid gateway, poll for it at first */
		if (!gateway_ip || ((gateway_ip)&&(strlen(gateway_ip)<7)) || ((gateway_ip)&&(strncmp(gateway_ip, "0.0.0.0", 7)==0)))
		{
			sleep(15);
		}
		/* valid gateway for now */
		else
		{
			break;
		}
	}

	ret = detectWAN_arp();

	if (ret < 0)
		printf("Failure!\n");
	else
		printf("Success!\n");
	
	return 0;
}