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
 *
 * Network services
 *
 * Copyright 2004, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: network.c,v 1.1.1.1 2007/01/25 12:52:21 jiahao_jhou Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <signal.h>
#include <dirent.h>
typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;

typedef u_int64_t __u64;
typedef u_int32_t __u32;
typedef u_int16_t __u16;
typedef u_int8_t __u8;

typedef unsigned char   bool;   // 1204 ham

#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <nvram/bcmnvram.h>
#include <netconf.h>
#include <shutils.h>
#include <wlutils.h>
#include <nvparse.h>
#include <rc.h>
#include <nvram/bcmutils.h>
#include <etioctl.h>
#include <bcmparams.h>
#include <net/route.h>
#include <stdarg.h>
#include "ralink.h"

#ifdef ASUS_EXT
#ifndef FLASH2M
void lan_up(char *lan_ifname);
#endif
#endif

#define	MAX_MAC_NUM	64
static int mac_num;
static char mac_clone[MAX_MAC_NUM][18];

void
config_loopback(void)
{
	/* Bring up loopback interface */
	ifconfig("lo", IFUP, "127.0.0.1", "255.0.0.0");

	/* Add to routing table */
	route_add("lo", 0, "127.0.0.0", "0.0.0.0", "255.0.0.0");
}

int
ifconfig(char *name, int flags, char *addr, char *netmask)
{
	int s;
	struct ifreq ifr;
	struct in_addr in_addr, in_netmask, in_broadaddr;

	/* Open a raw socket to the kernel */
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		goto err;

	/* Set interface name */
	strncpy(ifr.ifr_name, name, IFNAMSIZ);

	/* Set interface flags */
	ifr.ifr_flags = flags;
	if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0)
		goto err;

	/* Set IP address */
	if (addr) {
		inet_aton(addr, &in_addr);
		sin_addr(&ifr.ifr_addr).s_addr = in_addr.s_addr;
		ifr.ifr_addr.sa_family = AF_INET;
		if (ioctl(s, SIOCSIFADDR, &ifr) < 0)
			goto err;
	}
	/* Set IP netmask and broadcast */
	if (addr && netmask) {
		inet_aton(netmask, &in_netmask);
		sin_addr(&ifr.ifr_netmask).s_addr = in_netmask.s_addr;
		ifr.ifr_netmask.sa_family = AF_INET;
		if (ioctl(s, SIOCSIFNETMASK, &ifr) < 0)
			goto err;

		in_broadaddr.s_addr = (in_addr.s_addr & in_netmask.s_addr) | ~in_netmask.s_addr;
		sin_addr(&ifr.ifr_broadaddr).s_addr = in_broadaddr.s_addr;
		ifr.ifr_broadaddr.sa_family = AF_INET;
		if (ioctl(s, SIOCSIFBRDADDR, &ifr) < 0)
			goto err;
	}

	close(s);

	return 0;

 err:
	close(s);
	perror(name);
	return errno;
}

static int
route_manip(int cmd, char *name, int metric, char *dst, char *gateway, char *genmask)
{
	int s;
	struct rtentry rt;

	/* Open a raw socket to the kernel */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		goto err;
	}

	/* Fill in rtentry */
	memset(&rt, 0, sizeof(rt));
	if (dst) {
		inet_aton(dst, &sin_addr(&rt.rt_dst));
	}
	if (gateway) {
		inet_aton(gateway, &sin_addr(&rt.rt_gateway));
	}
	if (genmask) {	
		inet_aton(genmask, &sin_addr(&rt.rt_genmask));
	}
	rt.rt_metric = metric;
	rt.rt_flags = RTF_UP;
	if (sin_addr(&rt.rt_gateway).s_addr) {
		rt.rt_flags |= RTF_GATEWAY;
	}
	if (sin_addr(&rt.rt_genmask).s_addr == INADDR_BROADCAST) {
		rt.rt_flags |= RTF_HOST;
	}
	rt.rt_dev = name;

	/* Force address family to AF_INET */
	rt.rt_dst.sa_family = AF_INET;
	rt.rt_gateway.sa_family = AF_INET;
	rt.rt_genmask.sa_family = AF_INET;

	if (ioctl(s, cmd, &rt) < 0) {
		goto err;
	}

	close(s);
	return 0;

 err:
	close(s);
	perror(name);
	return errno;
}

int
route_add(char *name, int metric, char *dst, char *gateway, char *genmask)
{
	return route_manip(SIOCADDRT, name, metric, dst, gateway, genmask);
}

int
route_del(char *name, int metric, char *dst, char *gateway, char *genmask)
{
	return route_manip(SIOCDELRT, name, metric, dst, gateway, genmask);
}

static int
add_routes(char *prefix, char *var, char *ifname)
{
	char word[80], *next;
	char *ipaddr, *netmask, *gateway, *metric;
	char tmp[100];

	foreach(word, nvram_safe_get(strcat_r(prefix, var, tmp)), next) {

		netmask = word;
		ipaddr = strsep(&netmask, ":");
		if (!ipaddr || !netmask)
			continue;
		gateway = netmask;
		netmask = strsep(&gateway, ":");
		if (!netmask || !gateway)
			continue;
		metric = gateway;
		gateway = strsep(&metric, ":");
		if (!gateway || !metric)
			continue;
		if (inet_addr_(gateway) == INADDR_ANY) 			// oleg patch
			gateway = nvram_safe_get("wanx_gateway");	// oleg patch

		//route_add(ifname, atoi(metric) + 1, ipaddr, gateway, netmask);
		route_add(ifname, 0, ipaddr, gateway, netmask);
	}

	return 0;
}

static void	// oleg patch , add 
add_wanx_routes(char *prefix, char *ifname, int metric)
{
	char *routes, *msroutes, *tmp;
	char buf[30];

	char ipaddr[] = "255.255.255.255";
	char gateway[] = "255.255.255.255";
	char netmask[] = "255.255.255.255";

	if (!nvram_match("dr_enable_x", "1"))
		return;

	/* routes */
	routes = strdup(nvram_safe_get(strcat_r(prefix, "routes", buf)));
	for (tmp = routes; tmp && *tmp; )
	{
		char *ipaddr = strsep(&tmp, " ");
		char *gateway = strsep(&tmp, " ");
		if (gateway) {
			route_add(ifname, metric + 1, ipaddr, gateway, netmask);
		}
	}
	free(routes);
	
	/* ms routes */
	for (msroutes = nvram_safe_get(strcat_r(prefix, "msroutes", buf)); msroutes && isdigit(*msroutes); )
	{
		/* read net length */
		int bit, bits = strtol(msroutes, &msroutes, 10);
		struct in_addr ip, gw, mask;
		
		if (bits < 1 || bits > 32 || *msroutes != ' ')
			break;
		mask.s_addr = htonl(0xffffffff << (32 - bits));

		/* read network address */
		for (ip.s_addr = 0, bit = 24; bit > (24 - bits); bit -= 8)
		{
			if (*msroutes++ != ' ' || !isdigit(*msroutes))
				goto bad_data;

			ip.s_addr |= htonl(strtol(msroutes, &msroutes, 10) << bit);
		}
		
		/* read gateway */
		for (gw.s_addr = 0, bit = 24; bit >= 0 && *msroutes; bit -= 8)
		{
			if (*msroutes++ != ' ' || !isdigit(*msroutes))
				goto bad_data;

			gw.s_addr |= htonl(strtol(msroutes, &msroutes, 10) << bit);
		}
		
		/* clear bits per RFC */
		ip.s_addr &= mask.s_addr;
		
		strcpy(ipaddr, inet_ntoa(ip));
		strcpy(gateway, inet_ntoa(gw));
		strcpy(netmask, inet_ntoa(mask));
		
		route_add(ifname, metric + 1, ipaddr, gateway, netmask);
		
		if (*msroutes == ' ')
			msroutes++;
	}
bad_data:
	return;
}

static int
del_routes(char *prefix, char *var, char *ifname)
{
	char word[80], *next;
	char *ipaddr, *netmask, *gateway, *metric;
	char tmp[100];
	
	foreach(word, nvram_safe_get(strcat_r(prefix, var, tmp)), next) {
		dprintf("add %s\n", word);
		
		netmask = word;
		ipaddr = strsep(&netmask, ":");
		if (!ipaddr || !netmask)
			continue;
		gateway = netmask;
		netmask = strsep(&gateway, ":");
		if (!netmask || !gateway)
			continue;

		metric = gateway;
		gateway = strsep(&metric, ":");
		if (!gateway || !metric)
			continue;

		if (inet_addr_(gateway) == INADDR_ANY) 	// oleg patch
			gateway = nvram_safe_get("wanx_gateway");

		dprintf("add %s\n", ifname);
		
		route_del(ifname, atoi(metric) + 1, ipaddr, gateway, netmask);
	}

	return 0;
}

void	// oleg patch , add
start_igmpproxy(char *wan_ifname)
{
	static char *igmpproxy_conf = "/tmp/igmpproxy.conf";
	struct stat st_buf;
	FILE *fp;
	char *altnet = nvram_safe_get("mr_altnet_x");
	char *altnet_mask;

	if (atoi(nvram_safe_get("udpxy_enable_x")))
		eval("/usr/sbin/udpxy", "-a", nvram_get("lan_ifname") ? : "br0",
			"-m", wan_ifname, "-p", nvram_get("udpxy_enable_x"));

	if (!nvram_match("mr_enable_x", "1"))
		return;

	printf("start igmpproxy [%s]\n", wan_ifname);   // tmp test

	if ((fp = fopen(igmpproxy_conf, "w")) == NULL) {
		perror(igmpproxy_conf);
		return;
	}

	if (altnet && strlen(altnet) > 0)
		altnet_mask = altnet;
	else
		altnet_mask = "0.0.0.0/0";
	printf("start igmpproxy: altnet_mask = %s\n", altnet_mask);	// tmp test
	fprintf(fp, "# automagically generated from web settings\n"
		"quickleave\n\n"
		"phyint %s upstream  ratelimit 0  threshold 1\n"
		"\taltnet %s\n\n"
		"phyint %s downstream  ratelimit 0  threshold 1\n\n"
		"phyint ppp0 disabled\n\n",
		wan_ifname, 
		altnet_mask, 
		nvram_safe_get("lan_ifname") ? : "br0");

	fclose(fp);

	doSystem("/bin/igmpproxy -c %s", igmpproxy_conf);
}


static int
add_lan_routes(char *lan_ifname)
{
	return add_routes("lan_", "route", lan_ifname);
}

static int
del_lan_routes(char *lan_ifname)
{
	return del_routes("lan_", "route", lan_ifname);
}

int 
is_ap_mode()
{
	if ((nvram_match("wan_nat_x", "0")) && (nvram_match("wan_route_x", "IP_Bridged")))
		return 1;
	else
		return 0;
}

int 
is_3g_mode()
{
	if ((nvram_match("wan_proto", "3g")))	
		return 1;
	else
		return 0;
}

int 
start_udhcpc()
{
	if ((nvram_match("router_disable", "1")) && (nvram_match("lan_proto_ex", "1")))
	{
		char *dhcp_argv[] = { "udhcpc",
			"-i", "br0",
			"-p", "/var/run/udhcpc_lan.pid",
			"-s", "/tmp/landhcpc",
			NULL
		};
		pid_t pid;

		_eval(dhcp_argv, NULL, 0, &pid);
	}
	else if (nvram_match("wan0_proto", "dhcp"))
	{
		char *dhcp_argv[] = { "udhcpc",
			"-i", "eth3",
			"-p", "/var/run/udhcpc0.pid",
			"-s", "/tmp/udhcpc",
			NULL
		};
		pid_t pid;

		_eval(dhcp_argv, NULL, 0, &pid);
	}
}

int 
stop_udhcpc()
{
	if (pids("udhcpc"))
		system("killall -SIGTERM udhcpc");

	return 0;
}

void 
vconfig()
{
	int stbport;
	int controlrate_unknown_unicast;
	int controlrate_unknown_multicast;
	int controlrate_multicast;
	int controlrate_broadcast;

	doSystem("ifconfig eth2 hw ether %s", nvram_safe_get("lan_hwaddr"));
	ifconfig("eth2", IFUP, NULL, NULL);

//	if ((!is_ap_mode()) && (!is_3g_mode())) {
		if (!nvram_match("wan_hwaddr", ""))
			doSystem("ifconfig eth3 hw ether %s", nvram_safe_get("wan_hwaddr"));
		else
			doSystem("ifconfig eth3 hw ether %s", nvram_safe_get("lan_hwaddr"));
//	}
	ifconfig("eth3", IFUP, NULL, NULL);

	ifconfig(WIF, IFUP, NULL, NULL);
	if (nvram_match("wl_radio_x", "0"))
		radio_main(0);
#if (!defined(W7_LOGO) && !defined(WIFI_LOGO))
#ifdef MR
	else if (nvram_match("HT_BW", "1"))
#else
	else if (nvram_match("HT_BW", "2"))
#endif
	{
		int channel = get_channel();
		if (channel)
		{
//			doSystem("iwpriv %s set Channel=%d", WIF, channel);
			doSystem("iwpriv %s set HtBw=%d", WIF, 1);
		}
	}
#endif
//	if (nvram_match("wl_txbf", "1"))
//		doSystem("iwpriv %s set ETxBfEnCond=1", WIF);	// TxBF

	ifconfig(WIF2G, IFUP, NULL, NULL);
	if (nvram_match("rt_radio_x", "0"))
		radio_main_rt(0);
#if (!defined(W7_LOGO) && !defined(WIFI_LOGO))
#ifdef MR
	else if (nvram_match("rt_HT_BW", "1"))
#else
	else if (nvram_match("rt_HT_BW", "2"))
#endif
	{
		int channel_2g = get_channel_2G();
		if (channel_2g)
		{
//			doSystem("iwpriv %s set Channel=%d", WIF2G, channel_2g);
			doSystem("iwpriv %s set HtBw=%d", WIF2G, 1);
		}
	}
#endif

#if 0
	if (nvram_match("sw_mode_ex", "2") && !nvram_match("sta_ssid", ""))
		ifconfig("apcli0", IFUP, NULL, NULL);
#endif

	system("brctl addbr br0");
	system("brctl setfd br0 0.1");
	system("brctl sethello br0 0.1");
#if (!defined(W7_LOGO) && !defined(WIFI_LOGO))
	if (nvram_match("lan_stp", "0") || is_ap_mode() || is_3g_mode())
		system("brctl stp br0 0");
	else
		system("brctl stp br0 1");
#else
	system("brctl stp br0 1");
#endif
	if ((!is_ap_mode()) && (!is_3g_mode()))
	{
		stbport = atoi(nvram_safe_get("wan_stb_x"));

		if (stbport < 0 || stbport > 6)
			stbport = 0;

		switch(stbport)
		{
			case 1:	// LLLWW
				system("8367m 8 1");
				break;
			case 2:	// LLWLW
				system("8367m 8 2");
				break;
			case 3:	// LWLLW
				system("8367m 8 3");
				break;
			case 4:	// WLLLW
				system("8367m 8 4");
				break;
			case 5:	// WWLLW
				system("8367m 8 5");
				break;
			case 6: // LLWWW
				system("8367m 8 6");
				break;
			default:// LLLLW
//				system("8367m 8 0");
				break;
		}

		system("brctl addif br0 eth2");
	}
	else
	{
		system("brctl addif br0 eth2");
		system("brctl addif br0 eth3");
	}

	/* unknown unicast storm control */
	if (!nvram_get("controlrate_unknown_unicast"))
		controlrate_unknown_unicast = 16;
	else
		controlrate_unknown_unicast = atoi(nvram_get("controlrate_unknown_unicast"));
	if (controlrate_unknown_unicast < 0 || controlrate_unknown_unicast > 1024)
		controlrate_unknown_unicast = 0;
	if (controlrate_unknown_unicast)
		doSystem("8367m 22 %d", controlrate_unknown_unicast);

	/* unknown multicast storm control */
	if (!nvram_get("controlrate_unknown_multicast"))
		controlrate_unknown_multicast = 20;
	else
		controlrate_unknown_multicast = atoi(nvram_get("controlrate_unknown_multicast"));
	if (controlrate_unknown_multicast < 0 || controlrate_unknown_multicast > 1024)
		controlrate_unknown_multicast = 0;
	if (controlrate_unknown_multicast)
		doSystem("8367m 23 %d", controlrate_unknown_multicast);

	/* multicast storm control */
	if (!nvram_get("controlrate_multicast"))
		controlrate_multicast = 20;
	else
		controlrate_multicast = atoi(nvram_get("controlrate_multicast"));
	if (controlrate_multicast < 0 || controlrate_multicast > 1024)
		controlrate_multicast = 0;
	if (controlrate_multicast)
		doSystem("8367m 24 %d", controlrate_multicast);

	/* broadcast storm control */
	if (!nvram_get("controlrate_broadcast"))
		controlrate_broadcast = 16;
	else
		controlrate_broadcast = atoi(nvram_get("controlrate_broadcast"));
	if (controlrate_broadcast < 0 || controlrate_broadcast > 1024)
		controlrate_broadcast = 0;
	if (controlrate_broadcast)
		doSystem("8367m 25 %d", controlrate_broadcast);

	rtl8367m_AllPort_linkUp();
	kill_pidfile_s("/var/run/linkstatus_monitor.pid", SIGALRM);

	if (!nvram_match("wl_mode_x", "1"))
		doSystem("brctl addif br0 %s", WIF);
	if (!nvram_match("rt_mode_x", "1"))
		doSystem("brctl addif br0 %s", WIF2G);

	if (!nvram_match("wl_mode_x", "0") && !nvram_match("sw_mode_ex", "2"))
	{
		ifconfig("wds0", IFUP, NULL, NULL);
		ifconfig("wds1", IFUP, NULL, NULL);
		ifconfig("wds2", IFUP, NULL, NULL);
		ifconfig("wds3", IFUP, NULL, NULL);

		system("brctl addif br0 wds0");
		system("brctl addif br0 wds1");
		system("brctl addif br0 wds2");
		system("brctl addif br0 wds3");
	}

	if (!nvram_match("rt_mode_x", "0") && !nvram_match("sw_mode_ex", "2"))
	{
		ifconfig("wdsi0", IFUP, NULL, NULL);
		ifconfig("wdsi1", IFUP, NULL, NULL);
		ifconfig("wdsi2", IFUP, NULL, NULL);
		ifconfig("wdsi3", IFUP, NULL, NULL);

		system("brctl addif br0 wdsi0");
		system("brctl addif br0 wdsi1");
		system("brctl addif br0 wdsi2");
		system("brctl addif br0 wdsi3");
	}
#if (!defined(W7_LOGO) && !defined(WIFI_LOGO))
//	if (nvram_match("IgmpSnEnable", "1"))
	if (atoi(nvram_safe_get("wl_mrate")))
		doSystem("iwpriv %s set IgmpSnEnable=1", WIF);

//	if (nvram_match("rt_IgmpSnEnable", "1"))
	if (atoi(nvram_safe_get("rt_mrate")))
		doSystem("iwpriv %s set IgmpSnEnable=1", WIF2G);
#endif

	printf("[rc] set lan_if as %s/%s\n", nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"));
	doSystem("ifconfig br0 hw ether %s", nvram_safe_get("lan_hwaddr"));
	ifconfig("br0", IFUP, nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"));

	/* clean up... */
	nvram_unset("wan0_hwaddr_x");
}

void
start_lan(void)
{
	char *lan_ifname = nvram_safe_get("lan_ifname");
	char br0_ifnames[255];
	char name[80], *next;
	char tmpstr[48];
	int i, j;
	int s;
	struct ifreq ifr;

	if (nvram_match("lan_ipaddr", ""))
	{
		//nvram_set("lan_ipaddr", "192.168.1.220");
		nvram_set("lan_ipaddr", "192.168.2.1");
		nvram_set("lan_netmask", "255.255.255.0");
	}
	else if (nvram_match("lan_netmask", ""))
		nvram_set("lan_netmask", "255.255.255.0");

#ifdef ASUS_EXT
#ifndef FLASH2M
	/* 
	* Configure DHCP connection. The DHCP client will run 
	* 'udhcpc bound'/'udhcpc deconfig' upon finishing IP address 
	* renew and release.
	*/
	if (nvram_match("router_disable", "1"))
	{
/*
		if (nvram_match("sw_mode_ex", "2"))
		{
			nvram_unset("lan_ipaddr_new");
			nvram_unset("lan_netmask_new");
			nvram_unset("lan_gateway_new");
			nvram_unset("lan_dns_new");
			nvram_unset("lan_wins_new");
			nvram_unset("lan_domain_new");
			nvram_unset("lan_lease_new");
			nvram_unset("lan_ifname_new");
			nvram_unset("lan_udhcpstate_new");

			nvram_unset("lan_ipaddr_now");
			nvram_unset("lan_netmask_now");
			nvram_unset("lan_gateway_now");
			nvram_unset("lan_dns_now");
			nvram_unset("lan_wins_now");
			nvram_unset("lan_domain_now");
			nvram_unset("lan_lease_now");
		}
*/
		if (nvram_match("lan_proto_ex", "1"))
		{
			char *dhcp_argv[] = { "udhcpc",
					      "-i", "br0",
					      "-p", "/var/run/udhcpc_lan.pid",
					      "-s", "/tmp/landhcpc",
					      NULL
			};
			pid_t pid;

			/* Bring up and configure LAN interface */
			ifconfig(lan_ifname, IFUP,
				nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"));

			symlink("/sbin/rc", "/tmp/landhcpc");

			/* Start dhcp daemon */
			_eval(dhcp_argv, NULL, 0, &pid);
		}
		else
		{
			/* Bring up and configure LAN interface */
			ifconfig(lan_ifname, IFUP,
				nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"));
			lan_up(lan_ifname);

			update_lan_status(1);
		}
	}
	else
#endif // FLASH2M
	{
		/* Bring up and configure LAN interface */
		ifconfig(lan_ifname, IFUP,
		 	nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"));
		/* Install lan specific static routes */
		add_lan_routes(lan_ifname);

		update_lan_status(1);
	}
#else
	/* Bring up and configure LAN interface */
	ifconfig(lan_ifname, IFUP,
		 nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"));

	/* Install lan specific static routes */
	add_lan_routes(lan_ifname);
#endif // ASUS_EXT

#ifndef ASUS_EXT
	/* Start syslogd if either log_ipaddr or log_ram_enable is set */
	if (!nvram_match("log_ipaddr", "") || nvram_match("log_ram_enable", "1")) {
		char *argv[] = {
			"syslogd",
			NULL, 		/* -C */
			NULL, NULL,	/* -R host */
			NULL
		};
		int pid;
		int argc = 1;
		
		if (nvram_match("log_ram_enable", "1")) {
			argv[argc++] = "-C";
		}
		else if (!nvram_match("log_ram_enable", "0")) {
			nvram_set("log_ram_enable", "0");
		}
				
		if (!nvram_match("log_ipaddr", "")) {
			argv[argc++] = "-R";
			argv[argc++] = nvram_safe_get("log_ipaddr");
		}

		fprintf(stderr, "start syslogd\n");
		_eval(argv, NULL, 0, &pid);
	}
#endif // ASUS_EXT
}

void
stop_lan(void)
{
	char *lan_ifname = nvram_safe_get("lan_ifname");
	char name[80], *next;

	dprintf("%s\n", lan_ifname);

	/* Stop the syslogd daemon */
	if (pids("syslogd"))
		system("killall syslogd");

	/* Remove static routes */
	del_lan_routes(lan_ifname);

	/* Bring down LAN interface */
	ifconfig(lan_ifname, 0, NULL, NULL);

	/* Bring down bridged interfaces */
	if (strncmp(lan_ifname, "br", 2) == 0) {
#ifdef ASUS_EXT
		foreach(name, nvram_safe_get("lan_ifnames_t"), next) {
#else
		foreach(name, nvram_safe_get("lan_ifnames"), next) {
#endif
			doSystem("wlconf %s down", name);
			ifconfig(name, 0, NULL, NULL);
			doSystem("brctl delif %s %s", lan_ifname, name);
		}
		doSystem("brctl delbr %s", lan_ifname);
	}
	/* Bring down specific interface */
	else if (strcmp(lan_ifname, ""))
		doSystem("wlconf %s down", lan_ifname);
	
	dprintf("done\n");
}

static int
wan_prefix(char *ifname, char *prefix)
{
	int unit;
	
	if ((unit = wan_ifunit(ifname)) < 0)
		return -1;

	sprintf(prefix, "wan%d_", unit);
	//sprintf(prefix, "wan_", unit);
	return 0;
}

static int
add_wan_routes(char *wan_ifname)
{
	char prefix[] = "wanXXXXXXXXXX_";

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return -1;

	return add_routes(prefix, "route", wan_ifname);
}

static int
del_wan_routes(char *wan_ifname)
{
	char prefix[] = "wanXXXXXXXXXX_";

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return -1;

	return del_routes(prefix, "route", wan_ifname);
}

int
start_3g_process()
{
	char cmdname[128];

	printf("[rc] start 3g: %s\n", nvram_safe_get("run_sh"));	// tmp test
	if (strcmp(nvram_safe_get("wan0_proto"), "3g") != 0)
		return -1;

	if (nvram_match("auto3g", "0"))
	{
		printf("[rc] do not start 3g proces automatically\n");
	}
	else if (nvram_match("run_sh", "on"))
	{
		printf("[rc s3g] 3g script is now running\n");
		return -1;
	}
	else
	{
		memset(cmdname, 0, sizeof(cmdname));
		sprintf(cmdname, "3g.sh %s %s %s %s &", nvram_safe_get("d3g"), nvram_safe_get("dev_vid"), nvram_safe_get("dev_pid"), nvram_safe_get("wan_3g_pin"));
		printf("[rc] start 3g process:(%s), %s\n", nvram_safe_get("d3g"), cmdname);	// tmp test
		system(cmdname);
	}
	return 0;
}

int
stop_3g()
{
	printf("[rc] stop 3g\n");	// tmp test
 
	track_set("203");

	nvram_set("usb_dev_state", "off");
	nvram_set("usb_mnt_first_path", "");
	nvram_set("usb_path1", "");
	nvram_set("wan0_ipaddr", "");
	nvram_set("usb3g", "");

	if (pids("pppd"))
		system("killall -SIGKILL pppd");
	sleep(1);
	if (pids("pppd"))
		system("killall -SIGKILL pppd");

	//system("killall comgt");

	return 0;
}

int enable_qos()
{
#if defined (W7_LOGO) || defined (WIFI_LOGO)
	return 0;
#endif
	int qos_userspec_app_en = 0;
	int rulenum = atoi(nvram_safe_get("qos_rulenum_x")), idx_class = 0;

	/* Add class for User specify, 10:20(high), 10:40(middle), 10:60(low)*/
	if (rulenum) {
		for (idx_class=0; idx_class < rulenum; idx_class++)
		{
			if (atoi(Ch_conv("qos_prio_x", idx_class)) == 1)
			{
				qos_userspec_app_en = 1;
				break;
			}
			else if (atoi(Ch_conv("qos_prio_x", idx_class)) == 6)
			{
				qos_userspec_app_en = 1;
				break;
			}
		}
	}

	if (	(nvram_match("qos_tos_prio", "1") ||
		 nvram_match("qos_pshack_prio", "1") ||
		 nvram_match("qos_service_enable", "1") ||
		 nvram_match("qos_shortpkt_prio", "1")	) ||
		(!nvram_match("qos_manual_ubw","0") && !nvram_match("qos_manual_ubw","")) ||
		(rulenum && qos_userspec_app_en)
	)
	{
		fprintf(stderr, "found QoS rulues\n");
		return 1;
	}
	else
	{
		fprintf(stderr, "no QoS rulues\n");
		return 0;
	}
}

int is_hwnat_loaded()
{
        DIR *dir_to_open = NULL;

        dir_to_open = opendir("/sys/module/hw_nat");
        if (dir_to_open)
        {
                closedir(dir_to_open);
                return 1;
        }
                return 0;
}

void
start_wan(void)
{
	char *wan_ifname;
	char *wan_proto;
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char eabuf[32];
	int s;
	struct ifreq ifr;
	pid_t pid;
	char name[80], *next;

	/* check if we need to setup WAN */
	if (nvram_match("router_disable", "1"))
		return;

	if (	nvram_match("sw_mode_ex", "1") &&
//		nvram_match("mr_enable_x", "0") &&
		(nvram_match("wl_radio_x", "0") || nvram_match("wl_mrate", "0")) &&
		(nvram_match("rt_radio_x", "0") || nvram_match("rt_mrate", "0")) &&
		//!nvram_match("wan0_proto", "pptp") &&
		//!nvram_match("wan0_proto", "l2tp") &&
		!enable_qos() &&
		!is_hwnat_loaded()
	)
		system("insmod -q hw_nat.ko");

#ifdef ASUS_EXT
	update_wan_status(0);
	/* start connection independent firewall */
	start_firewall();
#else
	/* start connection independent firewall */
	start_firewall();
#endif

	/* Create links */
	mkdir("/tmp/ppp", 0777);
	mkdir("/tmp/ppp/peers", 0777);
	symlink("/sbin/rc", "/tmp/ppp/ip-up");
	symlink("/sbin/rc", "/tmp/ppp/ip-down");	
	symlink("/sbin/rc", "/tmp/udhcpc");
//	symlink("/dev/null", "/tmp/ppp/connect-errors");

	/* Start each configured and enabled wan connection and its undelying i/f */
	for (unit = 0; unit < MAX_NVPARSE; unit ++) 
	{
#ifdef ASUS_EXT // Only multiple pppoe is allowed 
		if (unit > 0 && !nvram_match("wan_proto", "pppoe")) break;
#endif
		if (unit > 2) break;

		snprintf(prefix, sizeof(prefix), "wan%d_", unit);

		/* make sure the connection exists and is enabled */ 
		wan_ifname = nvram_get(strcat_r(prefix, "ifname", tmp));
		if (!wan_ifname) {
			continue;
		}
		wan_proto = nvram_get(strcat_r(prefix, "proto", tmp));
		if (!wan_proto || !strcmp(wan_proto, "disabled"))
			continue;

		fprintf(stderr, "%s(): wan_ifname=%s, wan_proto=%s\n", __FUNCTION__, wan_ifname, wan_proto);

		/* Set i/f hardware address before bringing it up */
		if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
			continue;

		strncpy(ifr.ifr_name, wan_ifname, IFNAMSIZ);

		/* Since WAN interface may be already turned up (by vlan.c),
		   if WAN hardware address is specified (and different than the current one),
		   we need to make it down for synchronizing hwaddr. */
		if (ioctl(s, SIOCGIFHWADDR, &ifr)) {
			close(s);
			continue;
		}

		if (nvram_match("wan_proto", "dhcp") && nvram_match("mac_clone_en", "1"))
			nvram_set(strcat_r(prefix, "hwaddr", tmp), nvram_safe_get("cl0macaddr"));
/*
		fprintf(stderr, "$$ xxx_hwaddr (%s): %s\n", strcat_r(prefix, "hwaddr", tmp),
			nvram_safe_get(strcat_r(prefix, "hwaddr", tmp)));
*/
		ether_atoe(nvram_safe_get(strcat_r(prefix, "hwaddr", tmp)), eabuf);
/*
		int i;
		fprintf(stderr, "$$ eabuf: ");
		for (i=0; i<ETHER_ADDR_LEN; ++i)
			fprintf(stderr, "%02X%s", eabuf[i] & 0xff, i < 5 ? ":" : "");
		fprintf(stderr, "\n");
		fprintf(stderr, "$$ ifr_hwaddr_sa_data: ");
		for (i=0; i<ETHER_ADDR_LEN; ++i)
			fprintf(stderr, "%02X%s", ifr.ifr_hwaddr.sa_data[i] & 0xff, i < 5 ? ":" : "");
		fprintf(stderr, "\n");
*/
		if (/*!nvram_match("wan_hwaddr", "") && */(bcmp(eabuf, ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN)))
		{
			/* current hardware address is different than user specified */
			ifconfig(wan_ifname, 0, NULL, NULL);
		}

		/* Configure i/f only once, specially for wireless i/f shared by multiple connections */
		if (ioctl(s, SIOCGIFFLAGS, &ifr)) {
			close(s);
			continue;
		}

		if (!(ifr.ifr_flags & IFF_UP)) {
//			fprintf(stderr, "** wan_ifname: %s is NOT UP\n", wan_ifname);

			/* Sync connection nvram address and i/f hardware address */
			memset(ifr.ifr_hwaddr.sa_data, 0, ETHER_ADDR_LEN);

			if (nvram_match("wan_proto", "dhcp") && nvram_match("mac_clone_en", "1"))
			{
				ether_atoe(nvram_safe_get("cl0macaddr"), ifr.ifr_hwaddr.sa_data);
				ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
				ioctl(s, SIOCSIFHWADDR, &ifr);
			}
			else if (nvram_match(strcat_r(prefix, "hwaddr", tmp), "") ||
			    !ether_atoe(nvram_safe_get(strcat_r(prefix, "hwaddr", tmp)), ifr.ifr_hwaddr.sa_data) ||
			    !memcmp(ifr.ifr_hwaddr.sa_data, "\0\0\0\0\0\0", ETHER_ADDR_LEN)) {
/*
				fprintf(stderr, "** chk hwaddr:[%s]<%s>\n", strcat_r(prefix, "hwaddr", tmp),
					nvram_safe_get(strcat_r(prefix, "hwaddr", tmp)));
				fprintf(stderr, "** atoe result:%d\n", ether_atoe(nvram_safe_get(strcat_r(prefix, "hwaddr", tmp)),
					ifr.ifr_hwaddr.sa_data));
				fprintf(stderr, "** ifr_sa_data not empty:[%d]\n", 
					memcmp(ifr.ifr_hwaddr.sa_data, "\0\0\0\0\0\0", ETHER_ADDR_LEN));
*/
				if (ioctl(s, SIOCGIFHWADDR, &ifr)) {
					fprintf(stderr, "ioctl fail. continue\n");
					close(s);
					continue;
				}
				nvram_set(strcat_r(prefix, "hwaddr", tmp), ether_etoa(ifr.ifr_hwaddr.sa_data, eabuf));
			}
			else {
				ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
				ioctl(s, SIOCSIFHWADDR, &ifr);
			}

			/* Bring up i/f */
			ifconfig(wan_ifname, IFUP, NULL, NULL);
		}

		close(s);

#ifdef ASUS_EXT
		if (unit == 0) 
		{		
			FILE *fp;

//			setup_ethernet(nvram_safe_get("wan_ifname"));	// do nothing
			start_pppoe_relay(nvram_safe_get("wan_ifname"));

			/* Enable Forwarding */
			if ((fp = fopen("/proc/sys/net/ipv4/ip_forward", "r+"))) {
				fputc('1', fp);
				fclose(fp);
			} else
			{	
				perror("/proc/sys/net/ipv4/ip_forward");
			}
		}

		/* 
		* Configure PPPoE connection. The PPPoE client will run 
		* ip-up/ip-down scripts upon link's connect/disconnect.
		*/
/* oleg patch mark off
#ifdef RPPPPOE
		if (nvram_match("wan_proto", "pppoe") || (nvram_match("wan_proto", "pptp")
#ifdef DHCP_PPTP
&& !nvram_match(strcat_r(prefix, "pppoe_gateway", tmp), "")
#endif
))
*/
		if (strcmp(wan_proto, "pppoe") == 0 || strcmp(wan_proto, "pptp") == 0 ||
		    strcmp(wan_proto, "l2tp") == 0) 	// oleg patch
		{
//			int demand = atoi(nvram_safe_get(strcat_r(prefix, "pppoe_idletime", tmp)));
			int demand = atoi(nvram_safe_get(strcat_r(prefix, "pppoe_idletime", tmp))) &&
			strcmp(wan_proto, "l2tp") /* L2TP does not support idling */;	// oleg patch

			/* update demand option */
			nvram_set(strcat_r(prefix, "pppoe_demand", tmp), demand ? "1" : "0");
			/* Bring up  WAN interface */
			ifconfig(wan_ifname, IFUP, 
				nvram_safe_get(strcat_r(prefix, "pppoe_ipaddr", tmp)),
				nvram_safe_get(strcat_r(prefix, "pppoe_netmask", tmp)));

//			if (strcmp(wan_proto, "pppoe") != 0)	/* pptp, l2tp, pppoe too */
//			{
				/* launch dhcp client and wait for lease forawhile */
				if (nvram_match(strcat_r(prefix, "pppoe_ipaddr", tmp), "0.0.0.0")) 
				{
					char *wan_hostname = nvram_safe_get(strcat_r(prefix, "hostname", tmp));
					char *dhcp_argv[] = { "udhcpc",
					     "-i", wan_ifname,
					     "-p", (sprintf(tmp, "/var/run/udhcpc%d.pid", unit), tmp),
					     "-s", "/tmp/udhcpc",
					     "-b",
					     wan_hostname && *wan_hostname ? "-H" : NULL,
					     wan_hostname && *wan_hostname ? wan_hostname : NULL,
					     NULL
						};
					printf("[rc] start dhcp daemon\n");	// tmp test
					/* Start dhcp daemon */
					_eval(dhcp_argv, NULL, 0, NULL);
		 		} else {
					printf("[rc] start 2\n");	// tmp test
		 			/* do not use safe_get here, values are optional */
					/* start firewall */
					start_firewall_ex(nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp)),
					"0.0.0.0", "br0", nvram_safe_get("lan_ipaddr"));

					/* setup static wan routes via physical device */
					add_routes("wan_", "route", wan_ifname);
					/* and set default route if specified with metric 1 */
					if (inet_addr_(nvram_safe_get(strcat_r(prefix, "pppoe_gateway", tmp))) &&
				   	!nvram_match("wan_heartbeat_x", ""))
						route_add(wan_ifname, 2, "0.0.0.0", 
					nvram_safe_get(strcat_r(prefix, "pppoe_gateway", tmp)), "0.0.0.0");
					/* start multicast router */
					start_igmpproxy(wan_ifname);
				}
//			}
// ~ oleg patch

			/* launch pppoe client daemon */
			start_pppd(prefix);

			/* ppp interface name is referenced from this point on */
			wan_ifname = nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp));

			/* Pretend that the WAN interface is up */
			if (nvram_match(strcat_r(prefix, "pppoe_demand", tmp), "1")) 
			{
				int timeout = 5;
				/* Wait for pppx to be created */
				while (ifconfig(wan_ifname, IFUP, NULL, NULL) && timeout--)
					sleep(1);

				/* Retrieve IP info */
				if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
					continue;
				strncpy(ifr.ifr_name, wan_ifname, IFNAMSIZ);

				/* Set temporary IP address */
				if (ioctl(s, SIOCGIFADDR, &ifr))
					perror(wan_ifname);

				nvram_set(strcat_r(prefix, "ipaddr", tmp), inet_ntoa(sin_addr(&ifr.ifr_addr)));
				nvram_set(strcat_r(prefix, "netmask", tmp), "255.255.255.255");

				/* Set temporary P-t-P address */
				if (ioctl(s, SIOCGIFDSTADDR, &ifr))
					perror(wan_ifname);
				nvram_set(strcat_r(prefix, "gateway", tmp), inet_ntoa(sin_addr(&ifr.ifr_dstaddr)));

				close(s);

				/* 
				* Preset routes so that traffic can be sent to proper pppx even before 
				* the link is brought up.
				*/

				preset_wan_routes(wan_ifname);
			}
#ifdef ASUS_EXT
			nvram_set("wan_ifname_t", wan_ifname);
#endif
		}
//#endif // RPPPPOE
#endif
		/* 
		* Configure DHCP connection. The DHCP client will run 
		* 'udhcpc bound'/'udhcpc deconfig' upon finishing IP address 
		* renew and release.
		*/
		else if (strcmp(wan_proto, "dhcp") == 0 ||
			 strcmp(wan_proto, "bigpond") == 0 
//#ifdef DHCP_PPTP	// oleg patch mark off
//|| (strcmp(wan_proto,"pptp")==0 && nvram_match(strcat_r(prefix, "pppoe_gateway", tmp), ""))
//#endif
		) {
//			char *wan_hostname = nvram_get(strcat_r(prefix, "hostname", tmp));
			char *wan_hostname;
			if (!nvram_match("computer_name", ""))
				wan_hostname = nvram_get("computer_name");
			else
				wan_hostname = nvram_get("productid");

			char *dhcp_argv[] = { "udhcpc",
					      "-i", wan_ifname,
					      "-p", (sprintf(tmp, "/var/run/udhcpc%d.pid", unit), tmp),
					      "-s", "/tmp/udhcpc",
					      wan_hostname && *wan_hostname ? "-H" : NULL,
					      wan_hostname && *wan_hostname ? wan_hostname : NULL,
					      NULL
			};
			/* Start dhcp daemon */

			/* J++ debug */
#if 0
			int dhcpc_argc;
			for (dhcpc_argc = 0; dhcp_argv[dhcpc_argc] != NULL; dhcpc_argc++)
				fprintf(stderr, "%s ", dhcp_argv[dhcpc_argc]);
			fprintf(stderr, "\n");
#endif
			/* J++ debug */

//			if ( !(!nvram_match("wl_mode_EX", "ap") && nvram_match("no_profile", "1")) )
			_eval(dhcp_argv, NULL, 0, &pid);
#ifdef ASUS_EXT
			wanmessage("Can not get IP from server");
			nvram_set("wan_ifname_t", wan_ifname);
#endif
		}
		/* Configure static IP connection. */
		else if ((strcmp(wan_proto, "static") == 0) || (strcmp(wan_proto, "Static") == 0)) {
			/* Assign static IP address to i/f */
			ifconfig(wan_ifname, IFUP,
				 nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)), 
				 nvram_safe_get(strcat_r(prefix, "netmask", tmp)));
			/* We are done configuration */
			wan_up(wan_ifname);
#ifdef ASUS_EXT
			//nvram_set("wan_ifname_t", wan_ifname);
			nvram_set("wan_ifname_t", "eth3");
#endif
		}
//#ifdef DHCP_PPTP
// 		if (strcmp(wan_proto,"pptp")==0 && nvram_match(strcat_r(prefix, "pppoe_gateway", tmp), ""))
// 		{
//			char *wan_hostname = nvram_safe_get(strcat_r(prefix, "hostname", tmp));
//			char *dhcp_argv[] = { "udhcpc",
//					      "-i", wan_ifname,
//					      "-p", (sprintf(tmp, "/var/run/udhcpc%d.pid", unit), tmp),
//					      "-s", "/tmp/udhcpc",
//					      wan_hostname && *wan_hostname ? "-H" : NULL,
//					      wan_hostname && *wan_hostname ? wan_hostname : NULL,
//					      NULL
//			};
//			/* Start dhcp daemon */
//			_eval(dhcp_argv, NULL, 0, &pid);
//#ifdef ASUS_EXT
//			wanmessage("Can not get IP from server");
//			nvram_set("wan_ifname_t", wan_ifname);
//#endif
//		}
//#endif

#ifndef ASUS_EXT
		/* Start connection dependent firewall */
		start_firewall2(wan_ifname);
#endif

		dprintf("%s %s\n",
			nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)),
			nvram_safe_get(strcat_r(prefix, "netmask", tmp)));
	}

	/* Report stats */
	if (!nvram_match("stats_server", "")) {
		char *stats_argv[] = { "stats", nvram_safe_get("stats_server"), NULL };
		_eval(stats_argv, NULL, 5, NULL);
	}
}

void
stop_wan(void)
{
	char name[80], *next;

	if (	nvram_match("sw_mode_ex", "1") &&
		//!nvram_match("wan0_proto", "pptp") &&
		//!nvram_match("wan0_proto", "l2tp") &&
		is_hwnat_loaded()
	)
		system("rmmod hw_nat");

	if (pids("stats"))
		system("killall stats");
	if (pids("ntpclient"))
		system("killall ntpclient");

	/* Shutdown and kill all possible tasks */
	if (pids("ip-up"))
		system("killall ip-up");
	if (pids("ip-down"))
		system("killall ip-down");
	if (pids("l2tpd"))
		system("killall l2tpd");	// oleg patch
	if (pids("pppd"))
		system("killall pppd");
	if (pids("pptp"))
		system("killall pptp");
	if (pids("udhcpc"))
	{
		logmessage("stop_wan()", "perform DHCP release");
		doSystem("killall -%d udhcpc", SIGUSR2);
	}
	if (pids("udhcpc"))
		system("killall -SIGTERM udhcpc");
	if (pids("igmpproxy"))
		system("killall igmpproxy");	// oleg patch

	/* Bring down WAN interfaces */
	foreach(name, nvram_safe_get("wan_ifnames"), next)
	{
		ifconfig(name, 0, NULL, NULL);
	}

	/* Remove dynamically created links */
	unlink("/tmp/udhcpc");
	
	unlink("/tmp/ppp/ip-up");
	unlink("/tmp/ppp/ip-down");
	//unlink("/tmp/ppp/options");	// oleg patch mark off
	rmdir("/tmp/ppp");

#ifdef ASUS_EXT
	update_wan_status(0);
#endif
}

void 
stop_wan_ppp()
{
	printf(" stop wan ppp \n");	// tmp test
	//system("killall -SIGSTOP pppd");
	if (pids("l2tpd"))
		system("killall l2tpd");
	if (pids("pppd"))
		system("killall pppd");
	if (pids("pptp"))
		system("killall pptp");
	sleep(3);
	if (pids("pppd"))
		system("killall -SIGKILL pppd");

	system("ifconfig ppp0 down");
	nvram_set("wan_status_t", "Disconnected");
}

void 
restart_wan_ppp()	/* pptp, l2tp */
{
	printf(" restart wan ppp \n");	// tmp test
	//system("killall -SIGCONT pppd");

	if (pids("udhcpc"))
		system("killall -SIGTERM udhcpc");
	if (pids("l2tpd"))
		system("killall l2tpd");
	if (pids("pppd"))
		system("killall pppd");
	if (pids("pppoe-relay"))
		system("killall pppoe-relay");
	sleep(3);
	if (pids("pppd"))
		system("killall -SIGKILL pppd");
	sleep(1);
	start_wan();

	nvram_set("wan_status_t", "Connected");
}

void
stop_wan2(void)
{
	char name[80], *next;

	if (nvram_match("wan0_proto", "3g"))
	{
		stop_3g();
		return;
	}

	if (pids("stats"))
		system("killall stats");
	if (pids("ntpclient"))
		system("killall ntpclient");

	/* Shutdown and kill all possible tasks */
	if (pids("ip-up"))
		system("killall ip-up");
	if (pids("ip-down"))
		system("killall ip-down");
	if (pids("l2tpd"))
		system("killall l2tpd");	// oleg patch
	if (pids("pppd"))
		system("killall pppd");
	if (pids("pptp"))
		system("killall pptp");
	if (pids("udhcpc"))
	{
		logmessage("stop_wan2()", "perform DHCP release");
		doSystem("killall -%d udhcpc", SIGUSR2);
	}
	if (pids("udhcpc"))
		system("killall udhcpc");
	if (pids("igmpproxy"))
		system("killall igmpproxy");	// oleg patch

	/* Remove dynamically created links */
	unlink("/tmp/udhcpc");
	
	unlink("/tmp/ppp/ip-up");
	unlink("/tmp/ppp/ip-down");
	//unlink("/tmp/ppp/options");	// oleg patch mark off
	rmdir("/tmp/ppp");

#ifdef ASUS_EXT
	if (!nvram_match("wan_ifname_t", "")) wan_down(nvram_safe_get("wan_ifname_t"));
#endif
}

int chk_flag = 0;

int	// oleg patch add
update_resolvconf(void)
{
	FILE *fp;
	char word[256], *next;
	int i, dns_count = 0;
	char *chk_dns;
	char *use_resolv = NULL;

	printf("\n### update resolvconf:%d\n", chk_flag);	// tmp test
	while (strcmp(nvram_safe_get("update_resolv"), "used") == 0)
		continue;

	//stop_dns();
	if (!nvram_match("wan_dnsenable_x", "1") || nvram_match("wan0_proto", "static"))	// ham 0415
	{
		/* Write resolv.conf with upstream nameservers */
		//start_dns();
		restart_dns();

		//printf("chk return :[%s][%s]\n", nvram_safe_get("wan_dnsenable_x"), nvram_safe_get("wan0_proto"));	// tmp test
		return 0;
	}

	/* check if auto dns enabled */
	if (!nvram_match("wan_dnsenable_x", "1"))
	{
		printf("chk return 2:[%s]\n", nvram_safe_get("wan_dnsenable_x"));	// tmp test
		return 0;
	}

	//printf("open resolvconf\n");	// tmp test
	nvram_set("update_resolv", "used");

	if (!(fp = fopen("/tmp/resolv.conf", "w+"))) {
		nvram_set("update_resolv", "free");
		perror("/tmp/resolv.conf");
		return errno;
	}

	// dns patch check 0524
	if(nvram_match("wan_dnsenable_x", "1"))
	{
		fprintf(fp, "nameserver 8.8.8.8\n");			// add for incompatible cable modem user
		++dns_count;
	}

	foreach(word, ((strlen(nvram_safe_get("wan_dns_t")) > 0) ? nvram_safe_get("wan_dns_t"):
		nvram_safe_get("wanx_dns")), next)
	{
		fprintf(fp, "nameserver %s\n", word);
		++dns_count;
		nvram_set("auto_dhcp_dns", word);
		printf("[rc] auto set dhcp dns option:%s\n", word);	// tmp test
	}
	if(dns_count < 3)
	{
		if(dns_count < 2)
			fprintf(fp, "nameserver 156.154.70.1\n");	// add for incompatible cable modem user
		fprintf(fp, "nameserver 8.8.4.4\n");			// add for incompatible cable modem user
	}

	fclose(fp);

	dns_count = 0;
	/* write also /etc/resolv.conf */
	if (!(fp = fopen("/etc/resolv.conf", "w+"))) {
		nvram_set("update_resolv", "free");
		perror("/etc/resolv.conf");
		return errno;
	}

	if(nvram_match("wan_dnsenable_x", "1"))
	{
		fprintf(fp, "nameserver 8.8.8.8\n");			// add for incompatible cable modem user
		++dns_count;
	}

	foreach(word, ((strlen(nvram_safe_get("wan_dns_t")) > 0) ? nvram_safe_get("wan_dns_t"):
		nvram_safe_get("wanx_dns")), next)
	{
		fprintf(fp, "nameserver %s\n", word);
		++dns_count;
	}
	if(dns_count < 3)
	{
		if(dns_count < 2)
			fprintf(fp, "nameserver 156.154.70.1\n");	// add for incompatible cable modem user
		fprintf(fp, "nameserver 8.8.4.4\n");			// add for incompatible cable modem user
	}

	fclose(fp);

	nvram_set("update_resolv", "free");

	//start_dns();
	restart_dns();

	// dns patch check 0524 end

	return 0;
}

void
wan_up(char *wan_ifname)	// oleg patch, replace
{
	fprintf(stderr, "%s(%s)\n", __FUNCTION__, wan_ifname);

	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char *wan_proto, *gateway;

// 2010.09 James. {
	{
		printf("rc: Send SIGUSR1 to wanduck.\n");
		kill_pidfile_s("/var/run/wanduck.pid", SIGUSR1);
		printf("rc: Send SIGUSR2 to wanduck.\n");
		kill_pidfile_s("/var/run/wanduck.pid", SIGUSR2);
	}
// 2010.09 James. }

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
	{
		printf("wan up chk\n");	// tmp test
		/* called for dhcp+ppp */
		if (!nvram_match("wan0_ifname", wan_ifname))
		{
//			nvram_set("wanup_mem_cric", "0");
			return;
		}

		/* re-start firewall with old ppp0 address or 0.0.0.0 */
		start_firewall_ex("ppp0", nvram_safe_get("wan0_ipaddr"),
			"br0", nvram_safe_get("lan_ipaddr"));

		/* setup static wan routes via physical device */
		add_routes("wan_", "route", wan_ifname);
//#ifdef DHCPROUTE
		/* and one supplied via DHCP */
		add_wanx_routes("wanx_", wan_ifname, 0);
//#endif
		gateway = inet_addr_(nvram_safe_get("wan_gateway")) != INADDR_ANY ?
			nvram_safe_get("wan_gateway") : nvram_safe_get("wanx_gateway");

		/* and default route with metric 1 */
		if (inet_addr_(gateway) != INADDR_ANY)
		{
			char word[100], *next;

			route_add(wan_ifname, 2, "0.0.0.0", gateway, "0.0.0.0");

			/* ... and to dns servers as well for demand ppp to work */
			if (nvram_match("wan_dnsenable_x", "1"))
				foreach(word, nvram_safe_get("wanx_dns"), next)
			{
				in_addr_t mask = inet_addr(nvram_safe_get("wanx_netmask"));
				if ((inet_addr(word) & mask) != (inet_addr(nvram_safe_get("wanx_ipaddr")) & mask))
					route_add(wan_ifname, 2, word, gateway, "255.255.255.255");
			}
		}

//#ifdef MULTICAST
		/* start multicast router */
		start_igmpproxy(wan_ifname);
//#endif
		update_resolvconf();

//		nvram_set("wanup_mem_cric", "0");
		return;
	}

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	dprintf("%s %s\n", wan_ifname, wan_proto);

	/* Set default route to gateway if specified */
	if (nvram_match(strcat_r(prefix, "primary", tmp), "1"))
	{
		if (strcmp(wan_proto, "dhcp") == 0 || strcmp(wan_proto, "static") == 0)
		{
			/* the gateway is in the local network */
			route_add(wan_ifname, 0, nvram_safe_get(strcat_r(prefix, "gateway", tmp)),
				NULL, "255.255.255.255");
		}
		/* default route via default gateway */
		route_add(wan_ifname, 0, "0.0.0.0",
			nvram_safe_get(strcat_r(prefix, "gateway", tmp)), "0.0.0.0");
		/* hack: avoid routing cycles, when both peer and server has the same IP */
		if (strcmp(wan_proto, "pptp") == 0 || strcmp(wan_proto, "l2tp") == 0) {
			/* delete gateway route as it's no longer needed */
			route_del(wan_ifname, 0, nvram_safe_get(strcat_r(prefix, "gateway", tmp)),
				"0.0.0.0", "255.255.255.255");
		}
	}

	/* Install interface dependent static routes */
	add_wan_routes(wan_ifname);

	/* setup static wan routes via physical device */
	if (strcmp(wan_proto, "dhcp") == 0 || strcmp(wan_proto, "static") == 0)
	{
		nvram_set("wanx_gateway", nvram_safe_get(strcat_r(prefix, "gateway", tmp)));
		add_routes("wan_", "route", wan_ifname);
	}

//#ifdef  DHCPROUTE
	/* and one supplied via DHCP */
	if (strcmp(wan_proto, "dhcp") == 0)
		add_wanx_routes(prefix, wan_ifname, 0);
//#endif

	/* Add dns servers to resolv.conf */
	update_resolvconf();

	/* Sync time */
#ifdef ASUS_EXT
	update_wan_status(1);

	start_firewall_ex(wan_ifname, nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)),
		"br0", nvram_safe_get("lan_ipaddr"));

	if (strcmp(wan_proto, "bigpond")==0)
	{
		stop_bpalogin();
		start_bpalogin();
	}
#endif
#ifdef CDMA
	if ((strcmp(wan_proto, "cdma")==0))
	{
		nvram_set("cdma_down", "2");
	}
#endif

	/* start multicast router */
	if (strcmp(wan_proto, "dhcp") == 0 ||
		//strcmp(wan_proto, "bigpond") == 0 ||
		strcmp(wan_proto, "static") == 0)
	{
		start_igmpproxy(wan_ifname);
	}
#if (!defined(W7_LOGO) && !defined(WIFI_LOGO))
	int try_count = 0;
	nvram_set("qos_ubw", "0");
//	if (!(!nvram_match("qos_manual_ubw","0") && !nvram_match("qos_manual_ubw","")))
	while ((nvram_match("qos_ubw", "0") || nvram_match("qos_ubw", "0kbit")) && (try_count++ < 2))
		qos_get_wan_rate();
#endif
//2008.10 magic{
#ifdef QOS
	int qos_userspec_app_en = 0;
	int rulenum = atoi(nvram_safe_get("qos_rulenum_x")), idx_class = 0;
	/* Add class for User specify, 10:20(high), 10:40(middle), 10:60(low)*/
	if (rulenum) {
		for (idx_class=0; idx_class < rulenum; idx_class++)
		{
			if (atoi(Ch_conv("qos_prio_x", idx_class)) == 1)
			{
				qos_userspec_app_en = 1;
				break;
			}
			else if (atoi(Ch_conv("qos_prio_x", idx_class)) == 6)
			{
				qos_userspec_app_en = 1;
				break;
			}
		}
	}

//	if (nvram_match("qos_global_enable", "1") || nvram_match("qos_userdef_enable", "1"))
	if (	(nvram_match("qos_tos_prio", "1") ||
		 nvram_match("qos_pshack_prio", "1") ||
		 nvram_match("qos_service_enable", "1") ||
		 nvram_match("qos_shortpkt_prio", "1")	) ||
		 (!nvram_match("qos_manual_ubw","0") && !nvram_match("qos_manual_ubw","")) ||
		 (rulenum && qos_userspec_app_en)
		 /* || nvram_match("qos_dfragment_enable", "1")*/)
	{
		if (	(nvram_match("qos_manual_ubw","0") || nvram_match("qos_manual_ubw","")) &&
			(nvram_match("qos_ubw","0") || nvram_match("qos_ubw",""))
		)
		{
			fprintf(stderr, "wan_up: no wan rate! skip qos setting!\n");
			goto Speedtest_Init_failed_wan_up;
		}

		nvram_set("qos_enable", "1");
		track_set("1");

		if (	nvram_match("sw_mode_ex", "1") &&
			//!nvram_match("wan0_proto", "pptp") &&
			//!nvram_match("wan0_proto", "l2tp") &&
			is_hwnat_loaded()
		)
			system("rmmod hw_nat");

		fprintf(stderr, "wan_up rebuild QoS rules...\n");
		Speedtest_Init();
	}
	else
	{
Speedtest_Init_failed_wan_up:
		fprintf(stderr, "wan_up clear QoS rules...\n");

		track_set("0");

		if (nvram_match("qos_enable", "1"))
		{
			nvram_set("qos_enable", "0");		

			/* Reset all qdisc first */
			doSystem("tc qdisc del dev %s root htb", wan_ifname);
			system("tc qdisc del dev br0 root htb");

			/* Clean iptables*/
			FILE *fp;
			if ((fp=fopen("/tmp/mangle_rules", "w"))==NULL) 
			{
				nvram_set("qos_file"," /tmp/mangle_rules file does not exist.");
				if ((fp = fopen("/tmp/mangle_rules", "w+")) == NULL)
					return ;
			}
			fprintf(fp, "*mangle\n");
			fprintf(fp, "-F\n");
			fprintf(fp, "COMMIT\n\n");
			fclose(fp);
			system("iptables-restore /tmp/mangle_rules");
		}

		start_dfragment_standalone();

		if (	nvram_match("sw_mode_ex", "1") &&
//			nvram_match("mr_enable_x", "0") &&
			(nvram_match("wl_radio_x", "0") || nvram_match("wl_mrate", "0")) &&
			(nvram_match("rt_radio_x", "0") || nvram_match("rt_mrate", "0")) &&
			//!nvram_match("wan0_proto", "pptp") &&
			//!nvram_match("wan0_proto", "l2tp") &&
			!is_hwnat_loaded()
		)
			system("insmod -q hw_nat.ko");
	}
#endif
//2008.10 magic}

	if (nvram_match("upnp_started", "1"))
	{
		stop_upnp();
		start_upnp();
	}

	start_ddns();

	stop_ntpc();
	start_ntpc();

#if (!defined(W7_LOGO) && !defined(WIFI_LOGO))
	if (nvram_match("wan0_proto", "dhcp"))	// 0712 ASUS
	{
//		if (pids("detectWan"))
//			system("killall detectWan");

		if (!pids("detectWan"))
		{
			printf("start detectWan\n");	// tmp test
			system("detectWan &");
		}
	}
#endif
}

void
wan_down(char *wan_ifname)
{
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char *wan_proto;

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return;

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	
	//dprintf("%s %s\n", wan_ifname, wan_proto);

	/* Remove default route to gateway if specified */
	if (nvram_match(strcat_r(prefix, "primary", tmp), "1"))
		route_del(wan_ifname, 0, "0.0.0.0", 
			nvram_safe_get(strcat_r(prefix, "gateway", tmp)),
			"0.0.0.0");

	/* Remove interface dependent static routes */
	del_wan_routes(wan_ifname);

	/* Update resolv.conf */
	/* Update resolv.conf -- leave as is if no dns servers left for demand to work */
	if (*nvram_safe_get("wanx_dns"))	// oleg patch
		nvram_unset(strcat_r(prefix, "dns", tmp));
	update_resolvconf();

	if (strcmp(wan_proto, "static")==0)
		ifconfig(wan_ifname, IFUP, NULL, NULL);

#ifdef ASUS_EXT
	update_wan_status(0);

	if (strcmp(wan_proto, "bigpond")==0) stop_bpalogin();
#endif

#ifdef CDMA
	if ((strcmp(wan_proto, "cdma")==0))
	{
		stop_cdma();
		nvram_set("cdma_down", "1");
	}
#endif

	// cleanup
	nvram_set("wan_ipaddr_t", "");
}

#ifdef ASUS_EXT
#ifndef FLASH2M
void
lan_up(char *lan_ifname)
{
	FILE *fp;
	char word[100], *next;
	char line[100];

	/* Set default route to gateway if specified */
	route_add(lan_ifname, 0, "0.0.0.0", 
			nvram_safe_get("lan_gateway"),
			"0.0.0.0");

	/* Open resolv.conf to read */
	if (!(fp = fopen("/tmp/resolv.conf", "w"))) {
		perror("/tmp/resolv.conf");
		return;
	}

	if (!nvram_match("lan_gateway", ""))
		fprintf(fp, "nameserver %s\n", nvram_safe_get("lan_gateway"));

	foreach(word, nvram_safe_get("lan_dns"), next)
	{
		fprintf(fp, "nameserver %s\n", word);
	}
	fclose(fp);

	// also for /etc/resolv.conf
	if (!(fp = fopen("/etc/resolv.conf", "w+"))) {
		perror("/etc/resolv.conf");
		return;
	}

	if (!nvram_match("lan_gateway", ""))
		fprintf(fp, "nameserver %s\n", nvram_safe_get("lan_gateway"));

	foreach(word, nvram_safe_get("lan_dns"), next)
	{
		fprintf(fp, "nameserver %s\n", word);
	}
	fclose(fp);

	/* Sync time */
	stop_ntpc();
	start_ntpc();
}

void
lan_down(char *lan_ifname)
{
	/* Remove default route to gateway if specified */
	route_del(lan_ifname, 0, "0.0.0.0", 
			nvram_safe_get("lan_gateway"),
			"0.0.0.0");

	/* remove resolv.conf */
	unlink("/tmp/resolv.conf");
	unlink("/etc/resolv.conf");
}


void
lan_up_ex(char *lan_ifname)
{
	FILE *fp;
	char word[100], *next;
	char line[100];

	/* Set default route to gateway if specified */
	route_add(lan_ifname, 0, "0.0.0.0", 
			nvram_safe_get("lan_gateway_t"),
			"0.0.0.0");

	/* Open resolv.conf to read */
	if (!(fp = fopen("/tmp/resolv.conf", "w"))) {
		perror("/tmp/resolv.conf");
		return;
	}

	if (!nvram_match("lan_gateway_t", ""))
		fprintf(fp, "nameserver %s\n", nvram_safe_get("lan_gateway_t"));

	foreach(word, nvram_safe_get("lan_dns_t"), next)
	{
		fprintf(fp, "nameserver %s\n", word);
	}
	fclose(fp);

	/* also for /etc/resolv.conf */
	if (!(fp = fopen("/etc/resolv.conf", "w+"))) {
		perror("/etc/resolv.conf");
		return;
	}

	if (!nvram_match("lan_gateway_t", ""))
		fprintf(fp, "nameserver %s\n", nvram_safe_get("lan_gateway_t"));

	foreach(word, nvram_safe_get("lan_dns_t"), next)
	{
		fprintf(fp, "nameserver %s\n", word);
	}
	fclose(fp);

#if (!defined(W7_LOGO) && !defined(WIFI_LOGO))
	if (!pids("detectWan"))
	{
		printf("start detectWan\n");
		system("detectWan &");
	}
#endif

	/* Sync time */
	stop_ntpc();
	start_ntpc();
	//update_lan_status(1);
}

void
lan_down_ex(char *lan_ifname)
{
	/* Remove default route to gateway if specified */
	route_del(lan_ifname, 0, "0.0.0.0", 
			nvram_safe_get("lan_gateway_t"),
			"0.0.0.0");

	/* remove resolv.conf */
	unlink("/tmp/resolv.conf");
	unlink("/etc/resolv.conf");

	update_lan_status(0);
}
#endif
#endif

int
wan_ifunit(char *wan_ifname)
{
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";

	if ((unit = ppp_ifunit(wan_ifname)) >= 0) {
		return unit;
	} else {
		for (unit = 0; unit < MAX_NVPARSE; unit ++) {
			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			if (nvram_match(strcat_r(prefix, "ifname", tmp), wan_ifname) &&
			    (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") ||
			     nvram_match(strcat_r(prefix, "proto", tmp), "bigpond") ||
//#ifdef DHCP_PPTP	// oleg patch mark off
//			     nvram_match(strcat_r(prefix, "proto", tmp), "pptp") ||
//#endif
#ifdef CDMA
			     nvram_match(strcat_r(prefix, "proto", tmp), "cdma") ||
#endif
			     nvram_match(strcat_r(prefix, "proto", tmp), "static")))
				return unit;
		}
	}
	return -1;
}

int
preset_wan_routes(char *wan_ifname)
{
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";

	printf("preset wan routes [%s]\n", wan_ifname);

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return -1;

	/* Set default route to gateway if specified */
	if (nvram_match(strcat_r(prefix, "primary", tmp), "1"))
	{
		route_add(wan_ifname, 0, "0.0.0.0", "0.0.0.0", "0.0.0.0");
	}

	/* Install interface dependent static routes */
	add_wan_routes(wan_ifname);
	return 0;
}

int
wan_primary_ifunit(void)
{
	int unit;
	
	for (unit = 0; unit < MAX_NVPARSE; unit ++) {
		char tmp[100], prefix[] = "wanXXXXXXXXXX_";
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		//snprintf(prefix, sizeof(prefix), "wan_", unit);
		if (nvram_match(strcat_r(prefix, "primary", tmp), "1"))
			return unit;
	}

	return 0;
}

void dumparptable()
{
	char buf[256];
	char ip_entry[32], hw_type[8], flags[8], hw_address[32], mask[32], device[8];
	char macbuf[36];

	FILE *fp = fopen("/proc/net/arp", "r");
	if (!fp) {
		fprintf(stderr, "no proc fs mounted!\n");
		return;
	}

	mac_num = 0;

//	while (fgets(buf, 256, fp) && (mac_num < MAX_MAC_NUM - 1)) {
	while (fgets(buf, 256, fp) && (mac_num < MAX_MAC_NUM - 2)) {
		sscanf(buf, "%s %s %s %s %s %s", ip_entry, hw_type, flags, hw_address, mask, device);

		if (!strcmp(device, "br0"))
		{
			strcpy(mac_clone[mac_num++], hw_address);
//			fprintf(stderr, "%d %s\n", mac_num, mac_clone[mac_num - 1]);
		}
	}
	fclose(fp);

	mac_conv("wan_hwaddr_x", -1, macbuf);
	if (!nvram_match("wan_hwaddr_x", "") && strcasecmp(macbuf, "FF:FF:FF:FF:FF:FF"))
		strcpy(mac_clone[mac_num++], macbuf);
//	else
		strcpy(mac_clone[mac_num++], nvram_safe_get("il1macaddr"));

	if (mac_num)
	{
		fprintf(stderr, "num of mac: %d\n", mac_num);
		int i;
		for (i = 0; i < mac_num; i++)
			fprintf(stderr, "mac to clone: %s\n", mac_clone[i]);
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

	strncpy(ifr.ifr_name, "br0", IFNAMSIZ);
	inaddr = (struct sockaddr_in *)&ifr.ifr_addr;
	inet_aton("0.0.0.0", &inaddr->sin_addr);	

	/* Get IP address */
	ioctl(s, SIOCGIFADDR, &ifr);
	close(s);	

	ip_addr = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr;
//	fprintf(stderr, "current LAN IP address: %s\n", inet_ntoa(ip_addr));
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
		strncpy(ifr.ifr_name, "eth3", IFNAMSIZ);
	else
		strncpy(ifr.ifr_name, "ppp0", IFNAMSIZ);
	inaddr = (struct sockaddr_in *)&ifr.ifr_addr;
	inet_aton("0.0.0.0", &inaddr->sin_addr);	

	/* Get IP address */
	ioctl(s, SIOCGIFADDR, &ifr);
	close(s);	

	ip_addr = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr;
//	fprintf(stderr, "current WAN IP address: %s\n", inet_ntoa(ip_addr));
	return inet_ntoa(ip_addr);
}

void
print_wan_ip()
{
	int s;
	struct ifreq ifr;
	struct sockaddr_in *inaddr;
	struct in_addr ip_addr;

	if (nvram_match("wan_route_x", "IP_Bridged"))
//		return 0;
		return;

	/* Retrieve IP info */
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
//		return 0;
		return;

	if (nvram_match("wan0_proto", "dhcp") || nvram_match("wan0_proto", "static"))
		strncpy(ifr.ifr_name, "eth3", IFNAMSIZ);
	else
		strncpy(ifr.ifr_name, "ppp0", IFNAMSIZ);
	inaddr = (struct sockaddr_in *)&ifr.ifr_addr;
	inet_aton("0.0.0.0", &inaddr->sin_addr);	

	/* Get IP address */
	ioctl(s, SIOCGIFADDR, &ifr);
	close(s);	

	ip_addr = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr;
	fprintf(stderr, "current WAN IP address: %s\n", inet_ntoa(ip_addr));
//	if (strcmp("0.0.0.0", inet_ntoa(ip_addr)))
//		return 1;
//	else
//		return 0;
}

int
has_wan_ip()
{
	int s;
	struct ifreq ifr;
	struct sockaddr_in *inaddr;
	struct in_addr ip_addr;

	if (nvram_match("wan_route_x", "IP_Bridged"))
		return 0;

	/* Retrieve IP info */
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		return 0;

	if (nvram_match("wan0_proto", "dhcp") || nvram_match("wan0_proto", "static"))
		strncpy(ifr.ifr_name, "eth3", IFNAMSIZ);
	else
		strncpy(ifr.ifr_name, "ppp0", IFNAMSIZ);
	inaddr = (struct sockaddr_in *)&ifr.ifr_addr;
	inet_aton("0.0.0.0", &inaddr->sin_addr);	

	/* Get IP address */
	ioctl(s, SIOCGIFADDR, &ifr);
	close(s);	

	ip_addr = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr;
//	fprintf(stderr, "current WAN IP address: %s\n", inet_ntoa(ip_addr));
	if (strcmp("0.0.0.0", inet_ntoa(ip_addr)))
		return 1;
	else
		return 0;
}

// 2010.09 James modified. {
int
got_wan_ip()
{
	/*int s;
	struct ifreq ifr;
	struct in_addr in_addr;

	if (nvram_match("wan_route_x", "IP_Bridged"))
		return 0;

	// Retrieve IP info
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		return 0;

	if (nvram_match("wan0_proto", "dhcp") || nvram_match("wan0_proto", "static"))
		strncpy(ifr.ifr_name, "eth3", IFNAMSIZ);
	else
		strncpy(ifr.ifr_name, "ppp0", IFNAMSIZ);
	inet_aton("0.0.0.0", &in_addr);
	sin_addr(&ifr.ifr_addr).s_addr = in_addr.s_addr;

	// Get IP address
	ioctl(s, SIOCGIFADDR, &ifr);
	close(s);

//	fprintf(stderr, "current WAN IP address: %s\n", inet_ntoa(sin_addr(&ifr.ifr_addr)));

	if ((strcmp("0.0.0.0", inet_ntoa(sin_addr(&ifr.ifr_addr)))))//*/
	char *wan_ip = nvram_safe_get("wan_ipaddr_t");
	if (strcmp("", wan_ip) && strcmp("0.0.0.0", wan_ip))
		return 1;
	else
		return 0;
}

void
start_mac_clone()
{
	int had_try = 0;

	if (!nvram_match("wan_proto", "dhcp"))
		return;

	//sleep(15);
	nvram_set("done_auto_mac", "0");

	while (!got_wan_ip() && !had_try)
	{
		if (is_phyconnected() == 0)
		{
			sleep(5);
			continue;
		}

		dumparptable();

		if (mac_num > 1)
		{
			nvram_set("mac_clone_en", "1");
			int i;
			for (i = 0; i < mac_num; i++)
			{
				nvram_set("cl0macaddr", mac_clone[i]);

				stop_wan();
				start_wan();
				sleep(15);

				if (got_wan_ip())
				{
					char buf[13];
					memset(buf, 0, 13);
					mac_conv2("cl0macaddr", -1, buf);
					nvram_set("wan_hwaddr_x", buf);
					fprintf(stderr, "stop mac cloning!\n");
					break;
				}

				if(i == mac_num-1)
					had_try = 1;
			}
			nvram_set("mac_clone_en", "0");
		}

		//sleep(5);
	}

	nvram_set("done_auto_mac", "1");
	nvram_commit_safe();
}
// 2010.09 James modified. }

int
ppp0_as_default_route()
{
	int i, n, found;
	FILE *f;
	unsigned int dest, mask;
	char buf[256], device[256];

	n = 0;
	found = 0;
	mask = 0;
	device[0] = '\0';

	if (f = fopen("/proc/net/route", "r"))
	{
		while (fgets(buf, sizeof(buf), f) != NULL)
		{
			if (++n == 1 && strncmp(buf, "Iface", 5) == 0)
				continue;

			i = sscanf(buf, "%255s %x %*s %*s %*s %*s %*s %x",
						device, &dest, &mask);

			if (i != 3)
				break;

			if (device[0] != '\0' && dest == 0 && mask == 0)
			{
				found = 1;
				break;
			}
		}

		fclose(f);

		if (found && !strcmp("ppp0", device))
			return 1;
		else
			return 0;
	}

	return 0;
}

long print_num_of_connections()
{
	char buf[256];
	char entries[16], others[256];
	long num_of_entries;

	FILE *fp = fopen("/proc/net/stat/nf_conntrack", "r");
	if (!fp) {
		fprintf(stderr, "no connection!\n");
		return;
	}

	fgets(buf, 256, fp);
	fgets(buf, 256, fp);
	fclose(fp);

	memset(entries, 0x0, 16);
	sscanf(buf, "%s %s", entries, others);
	num_of_entries = strtoul(entries, NULL, 16);

	fprintf(stderr, "connection count: %ld\n", num_of_entries);
	return num_of_entries;
}

int
found_default_route()
{
	int i, n, found;
	FILE *f;
	unsigned int dest, mask;
	char buf[256], device[256];
	char wanif[8];
        
	n = 0;
	found = 0;
	mask = 0;
	device[0] = '\0';

	if (f = fopen("/proc/net/route", "r"))
	{
		while (fgets(buf, sizeof(buf), f) != NULL)
		{
			if (++n == 1 && strncmp(buf, "Iface", 5) == 0)
				continue;

			i = sscanf(buf, "%255s %x %*s %*s %*s %*s %*s %x",
						device, &dest, &mask);

			if (i != 3)
			{
//				fprintf(stderr, "junk in buffer");
				break;
			}

			if (device[0] != '\0' && dest == 0 && mask == 0)
			{
//				fprintf(stderr, "default route dev: %s\n", device);
				found = 1;
				break;
			}
		}

		fclose(f);

		if (nvram_match("wan0_proto", "dhcp") || nvram_match("wan0_proto", "static"))
			strcpy(wanif, "eth3");
		else
			strcpy(wanif, "ppp0");

		if (found && !strcmp(wanif, device))
		{
//			fprintf(stderr, "got default route!\n");
			return 1;
		}
		else
		{
			fprintf(stderr, "no default route!\n");
			return 0;
		}
	}

	fprintf(stderr, "no default route!!!\n");
	return 0;
}

