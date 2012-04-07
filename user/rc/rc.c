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
 * Copyright 2004, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: rc.c,v 1.1.1.1 2007/01/25 12:52:21 jiahao_jhou Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <sys/klog.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <net/if_arp.h>
#include <dirent.h>
#include <sys/mount.h>
#include <sys/vfs.h>

#include <rc.h>
#include <rc_event.h>
#include <shutils.h>
#include <nvram/typedefs.h>
#include <nvram/bcmnvram.h>
#include <nvparse.h>
#include "rtl8367m.h"
//#include "ar8316.h"
#include <rtxxxx.h>
#include <ralink.h>

#include <fcntl.h>
#include <linux/autoconf.h>
#include "../../config/autoconf.h"

static void restore_defaults(void);
static void sysinit(void);
static void rc_signal(int sig);
extern void ipdown_main_sp(int sig);

int remove_usb_mass(char *product);
void usbtpt(int argc, char *argv[]);
int start_telnetd();
void print_sw_mode();

extern struct nvram_tuple router_defaults[];

static int noconsole = 0;

static const char *const environment[] = {
	"HOME=/",
	"PATH=/sbin:/usr/sbin:/bin:/usr/bin",
	"SHELL=/bin/sh",
	"USER=root",
	NULL
};

static void
restore_defaults(void)
{
	system("insmod nvram_linux.o");

#ifdef CONFIG_SENTRY5
#include "rcs5.h"
#else
#define RC1_START() 
#define RC1_STOP()  
#define RC7_START()
#define RC7_STOP()
#define LINUX_OVERRIDES() 
#define EXTRA_RESTORE_DEFAULTS() 
#endif

	nvram_set("NVRAMMAGIC", "");

	struct nvram_tuple *t, *u;
	int restore_defaults, i;

	/* Restore defaults if told to or OS has changed */
	restore_defaults = !nvram_match("restore_defaults", "0");

	if (restore_defaults) {
		fprintf(stderr, "\n## Restoring defaults... ##\n");
		logmessage(LOGNAME, "Restoring defaults...");
	}

	/* Restore defaults */
	for (t = router_defaults; t->name; t++) {
		if (restore_defaults || !nvram_get(t->name)) {
			{
				nvram_set(t->name, t->value);
			}
		}
	}

	/* Commit values */
	if (restore_defaults) {
		/* default value of vlan */
		nvram_commit();		
		fprintf(stderr, "done\n");
	}

	klogctl(8, NULL, atoi(nvram_safe_get("console_loglevel")));
}

static void
set_wan0_vars(void)
{
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	
	/* check if there are any connections configured */
	for (unit = 0; unit < MAX_NVPARSE; unit ++) {
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		if (nvram_get(strcat_r(prefix, "unit", tmp)))
			break;
	}
	/* automatically configure wan0_ if no connections found */
	if (unit >= MAX_NVPARSE) {
		struct nvram_tuple *t;
		char *v;

		/* Write through to wan0_ variable set */
		snprintf(prefix, sizeof(prefix), "wan%d_", 0);
		for (t = router_defaults; t->name; t ++) {
			if (!strncmp(t->name, "wan_", 4)) {
				if (nvram_get(strcat_r(prefix, &t->name[4], tmp)))
					continue;
				v = nvram_get(t->name);
				nvram_set(tmp, v ? v : t->value);
			}
		}
		nvram_set(strcat_r(prefix, "unit", tmp), "0");
		nvram_set(strcat_r(prefix, "desc", tmp), "Default Connection");
		nvram_set(strcat_r(prefix, "primary", tmp), "1");
	}
}

static void
sysinit(void)
{
	time_t tm = 0;

	umask(0000);

	system("mount -a");
	system("dev_init.sh");

	/* for user space nvram utility */
//	system("mknod /dev/nvram c 228 0");	// move to dev_init.sh

	/* for switch ioctl */
	system("mknod /dev/rtl8367m c 206 0");
//	system("mknod /dev/ar8316 c 207 0");

	mkdir("/tmp/var", 0777);
	mkdir("/var/lock", 0777);
	mkdir("/var/locks", 0777);
	mkdir("/var/private", 0777);
	mkdir("/var/lib", 0777);
	mkdir("/var/log", 0777);
	mkdir("/var/run", 0777);
	mkdir("/var/tmp", 0777);
//	mkdir("/tmp/samba", 0777);			// for old samba
//	mkdir("/tmp/samba/private", 0777);		// for old samba
//	mkdir("/tmp/samba/var", 0777);			// for old samba
//	mkdir("/tmp/samba/var/locks", 0777);		// for old samba
//	mkdir("/tmp/samba/lib", 0777);			// for old samba
	mkdir("/var/spool", 0777);			//Jerry5 2009.07	
	mkdir("/var/lib/misc",0777);			//Jerry5 2009.07

#ifdef PRINTER_SUPPORT
	mkdir("/var/state", 0777);
	mkdir("/var/state/parport", 0777);
	mkdir("/var/state/parport/svr_statue", 0777);
#endif	
	mkdir("/tmp/harddisk", 0777);

	mkdir("/tmp/rc_notification", 0777);		// 2008.10 magic
	mkdir("/tmp/rc_action_incomplete", 0777);	// 2008.10 magic

	/* extra settings */
	symlink("/tmp", "/shares");

	/* Setup console */
	if (console_init())
		noconsole = 1;

	chdir("/");
	setsid();

	{
		const char *const *e;
		/* Make sure environs is set to something sane */
		for (e = environment; *e; e++)
			putenv((char *) *e);
	}

	/* Set a sane date */
	stime(&tm);

	system("echo 65536 > /proc/sys/fs/file-max");			// default: 11644
/*
	system("echo 192 > /proc/sys/net/core/somaxconn");		// default: 128
	system("echo 1000 > /proc/sys/net/core/netdev_max_backlog");	// default: 1000
	system("echo 192 > /proc/sys/net/ipv4/tcp_max_syn_backlog");	// default: 128
	system("echo 65536 > /proc/sys/net/ipv4/tcp_max_tw_buckets");	// default: 8192
*/
	system("echo \"1024 65535\" > /proc/sys/net/ipv4/ip_local_port_range");
/*
	system("echo 1 > /proc/sys/net/ipv4/tcp_no_metrics_save");
	system("echo 0 > /proc/sys/net/ipv4/tcp_timestamps");
	system("echo 0 > /proc/sys/net/ipv4/conf/all/send_redirects");
	system("echo 0 > /proc/sys/net/ipv4/conf/default/send_redirects");
	system("echo 0 > /proc/sys/net/ipv4/conf/all/accept_redirects");
	system("echo 0 > /proc/sys/net/ipv4/conf/default/accept_redirects");
	system("echo 1 > /proc/sys/net/ipv4/conf/all/rp_filter");
	system("echo 1 > /proc/sys/net/ipv4/conf/default/rp_filter");
*/
	system("echo 180 > /proc/sys/net/ipv4/netfilter/ip_conntrack_tcp_timeout_established");
	system("echo 90 > /proc/sys/net/ipv4/netfilter/ip_conntrack_udp_timeout");
//	system("echo 60 > /proc/sys/net/ipv4/netfilter/ip_conntrack_tcp_timeout_last_ack");
#if (!defined(W7_LOGO) && !defined(WIFI_LOGO))
//	system("echo 300000 > /proc/sys/net/nf_conntrack_max");
#endif
	system("echo 100 > /proc/sys/net/ipv4/icmp_ratelimit");
	system("echo 32768 > /proc/sys/net/ipv4/route/max_size");
/*
	system("echo 262144 > /proc/sys/net/core/rmem_default");
	system("echo 262144 > /proc/sys/net/core/rmem_max");
	system("echo 262144 > /proc/sys/net/core/wmem_default");
	system("echo 262144 > /proc/sys/net/core/wmem_max");
	system("echo \"4096 87380 8388608\" > /proc/sys/net/ipv4/tcp_rmem");
	system("echo \"4096 65536 8388608\" > /proc/sys/net/ipv4/tcp_wmem");
	system("echo \"4096 4096 4096\" > /proc/sys/net/ipv4/tcp_mem");
*/
}

static void
insmod(void)
{
	char buf[PATH_MAX];
	struct utsname name;

//	system("insmod -q bridge");
//	system("insmod -q mii");
//	system("insmod -q raeth");

	system("insmod -q usbcore");
	system("mount -t usbfs none /proc/bus/usb");
	system("insmod -q ehci-hcd");
	system("insmod -q ohci-hcd");

//	if (nvram_match("wl_radio_x", "1"))
		system("insmod -q rt2860v2_ap");
//	if (nvram_match("rt_radio_x", "1"))
	{
		system("insmod -q RT3090_ap_util");
		system("insmod -q RT3090_ap");
		system("insmod -q RT3090_ap_net");
	}

#if 0
#ifdef USB_SUPPORT
#ifdef LANGUAGE_TW
	system("insmod -q nls_cp950.o");
	system("insmod -q nls_big5.o");
	system("insmod -q nls_cp936.o");
	system("insmod -q nls_gb2312.o");
	system("insmod -q nls_utf8.o");
#endif
#ifdef LANGUAGE_CN
	system("insmod -q nls_cp936.o");
	system("insmod -q nls_gb2312.o");
	system("insmod -q nls_cp950.o");
	system("insmod -q nls_big5.o");
	system("insmod -q nls_utf8.o");
#endif
#ifdef LANGUAGE_KR
	system("insmod -q nls_cp949.o");
	system("insmod -q nls_euc-kr.o");
	system("insmod -q nls_utf8.o");
#endif
#ifdef LANGUAGE_JP
	system("insmod -q nls_cp932.o");
	system("insmod -q nls_euc-jp.o");
	system("insmod -q nls_sjis.o");
	system("insmod -q nls_utf8.o");
#endif
#endif
#endif

//	if (!nvram_match("wan0_proto", "3g"))
	{
		system("insmod -q /usr/lib/ufsd.ko");
	}
}

/* States */
enum {
	RESTART,
	STOP,
	START,
	TIMER,
	IDLE,
	SERVICE,
	HOTPLUG,
	RECOVER,
};

static int state = START;
static int signalled = -1;

/* Signal handling */
static void
rc_signal(int sig)
{
	if (state == IDLE) {	
		if (sig == SIGHUP) {
			signalled = RESTART;
		}
		else if (sig == SIGUSR2) {
			signalled = START;
		}
		else if (sig == SIGINT) {
			signalled = STOP;
		}
		else if (sig == SIGALRM) {
			signalled = TIMER;
		}
		else if (sig == SIGUSR1) {
			signalled = SERVICE;
		}
		else if (sig == SIGTTIN) {
			signalled = HOTPLUG;
		}
	}
}

/* Get the timezone from NVRAM and set the timezone in the kernel
 * and export the TZ variable 
 */
void
set_timezone(void)
{
	time_t now;
	struct tm gm, local;
	struct timezone tz;
	struct timeval *tvp = NULL;

	/* Export TZ variable for the time libraries to 
	 * use.
	 */
	time_zone_x_mapping();
	setenv("TZ", nvram_get("time_zone_x"), 1);

	/* Update kernel timezone */
	time(&now);
	gmtime_r(&now, &gm);
	localtime_r(&now, &local);
	tz.tz_minuteswest = (mktime(&gm) - mktime(&local)) / 60;
	settimeofday(tvp, &tz);
#if 0
#define	RC_BUILDTIME	1252636574
	{
		struct timeval tv = {RC_BUILDTIME, 0};

		time(&now);
		if (now < RC_BUILDTIME)
			settimeofday(&tv, &tz);
	}
#endif
}

/* Timer procedure */
int
do_timer(void)
{
	int interval = atoi(nvram_safe_get("timer_interval"));

#ifdef ASUS_EXT
	set_timezone();
	return 0;
#endif
	if (interval == 0)
		return 0;

	/* Report stats */
	if (!nvram_match("stats_server", "")) {
		char *stats_argv[] = { "stats", nvram_get("stats_server"), NULL };
		_eval(stats_argv, NULL, 5, NULL);
	}

	/* Sync time */
//	stop_ntpc();
//	start_ntpc();
	refresh_ntpc();

	set_timezone();
	alarm(interval);
	return 0;
}

int 
stop_watchdog()
{
	if (pids("watchdog"))
		system("killall watchdog");

	return 0;
}

int 
start_watchdog()
{
	char *watchdog_argv[] = {"watchdog", NULL};
	pid_t whpid;

	return _eval(watchdog_argv, NULL, 0, &whpid);
}

int
start_linkstatus_monitor()
{
        char *linkstatus_monitor_argv[] = {"linkstatus_monitor", NULL};
        pid_t lspid;

        return _eval(linkstatus_monitor_argv, NULL, 0, &lspid);
}

int
start_detect_internet()
{
        char *detect_internet_argv[] = {"detect_internet", NULL};
        pid_t dipid;

	if (!nvram_match("wan_route_x", "IP_Routed"))
		return 1;

	kill_pidfile_s("/var/run/detect_internet.pid", SIGTERM);
        return _eval(detect_internet_argv, NULL, 0, &dipid);
}

int
stop_usbled()
{
	if (pids("usbled"))
	{
		nvram_set("no_usb_led", "1");
		system("killall -SIGALRM usbled");
	}

	return 0;
}

int
start_usbled()
{
	char *usbled_argv[] = {"usbled", NULL};
	pid_t whpid;

	nvram_set("no_usb_led", "0");

	if (pids("usbled"))
	{
		nvram_set("no_usb_led", "0");
		system("killall -SIGALRM usbled");
		return 0;
	}

	return _eval(usbled_argv, NULL, 0, &whpid);
}

#if 0
int 
start_apcli_monitor()
{
	char *apcli_monitor_argv[] = {"apcli_monitor", NULL};
	pid_t ampid;

	return _eval(apcli_monitor_argv, NULL, 0, &ampid);
}

int 
start_ping_keep_alive()
{
	char *ping_keep_alive_argv[] = {"ping_keep_alive", NULL};
	pid_t pid;

	return _eval(ping_keep_alive_argv, NULL, 0, &pid);
}

int
start_usdsvr_broadcast()
{
	char *usdsvr_broadcast_argv[] = {"usdsvr_broadcast", NULL};
	pid_t ubpid;

	return _eval(usdsvr_broadcast_argv, NULL, 0, &ubpid);
}

int
start_usdsvr_unicast()
{
	char *usdsvr_unicast_argv[] = {"usdsvr_unicast", NULL};
	pid_t uupid;

	return _eval(usdsvr_unicast_argv, NULL, 0, &uupid);
}
#endif
#if (!defined(W7_LOGO) && !defined(WIFI_LOGO))
int 
stop_pspfix()					// psp fix
{
	if (pids("pspfix"))
		system("killall pspfix");

	return 0;
}

int 
start_pspfix()					// psp fix
{
	char *pspfix_argv[] = {"pspfix", NULL};
	pid_t whpid;

	return _eval(pspfix_argv, NULL, 0, &whpid);
}
#endif

#if 0
// oleg patch ~
static void
early_defaults(void)
{
       int stbport;

       if (nvram_match("wan_route_x", "IP_Bridged")) {
	       if (nvram_match("boardtype", "0x48E") && nvram_match("boardnum", "45"))	
	       {
		       nvram_set("vlan0ports", "0 1 2 3 4 5*");
		       nvram_set("vlan1ports", "5u");
	       }
       } else
       { /* router mode, use vlans */
	       /* Adjust switch config to bridge STB LAN port with WAN port */
	       stbport = atoi(nvram_safe_get("wan_stb_x"));

	       /* Check existing config for validity */
	       if (stbport < 0 || stbport > 5)
		       stbport = 0;

	       /* predefined config for WL520gu, WL520gc -- check boardtype for others */
	       /* there is no easy way to do LANx to real port number mapping, so we use array */
	       if (nvram_match("boardtype", "0x48E") && nvram_match("boardnum", "45"))
	       {
		       /* why don't you use different boardnum??? */
		       if (nvram_match("productid","WL500gpv2"))
		       {
			       /* todo: adjust port mapping */
			       nvram_set("vlan0ports", "0 1 2 3 5*");
			       nvram_set("vlan1ports", "4 5u");
		       } else {
			       static char *vlan0ports[] = { "1 2 3 4 5*",
				       "2 3 4 5*", "1 3 4 5*", "1 2 4 5*", "1 2 3 5*", "1 2 5*" };
			       static char *vlan1ports[] = { "0 5u",
				       "1 0 5u", "2 0 5u", "3 0 5u", "4 0 5u", "3 4 0 5u" };


			       nvram_set("vlan0ports", vlan0ports[stbport]);
			       nvram_set("vlan1ports", vlan1ports[stbport]);
		       }
	       }
       }
}
// ~ oleg patch
#endif

#ifdef QOS
void restart_qos()
{
	char net_name[32];
	FILE *fp=NULL;
	int qos_userspec_app_en = 0;
	int rulenum, idx_class;
	int try_count = 0;

	/* Get interface name */
	if (nvram_match("wan0_proto", "pppoe") || nvram_match("wan0_proto","pptp") || nvram_match("wan0_proto","l2tp"))
		strcpy (net_name, nvram_safe_get("wan0_pppoe_ifname"));
	else	
		strcpy (net_name, nvram_safe_get("wan0_ifname"));

	/* Add class for User specify, 10:20(high), 10:40(middle), 10:60(low)*/
	rulenum = atoi(nvram_safe_get("qos_rulenum_x"));
	idx_class = 0;
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
		 (rulenum && qos_userspec_app_en)	)
	{
/*
		nvram_set("qos_ubw", "0");
//		if (!(!nvram_match("qos_manual_ubw","0") && !nvram_match("qos_manual_ubw","")))
			while ((nvram_match("qos_ubw", "0") || nvram_match("qos_ubw", "0kbit")) && (try_count++ < 3))
				qos_get_wan_rate();
*/
		if (	(nvram_match("qos_manual_ubw","0") || nvram_match("qos_manual_ubw","")) &&
			(nvram_match("qos_ubw","0") || nvram_match("qos_ubw",""))
		)
		{
			fprintf(stderr, "no wan rate! skip qos setting!\n");
			goto Speedtest_Init_failed;
		}

		if (	nvram_match("sw_mode_ex", "1") &&
			!nvram_match("wan0_proto", "pptp") &&
			!nvram_match("wan0_proto", "l2tp") &&
			is_hwnat_loaded() 
		)
			system("rmmod hw_nat");

		nvram_set("qos_enable", "1");
		track_set("1");

		fprintf(stderr, "rebuild QoS rules...\n");
		Speedtest_Init();
	}
	else
	{
Speedtest_Init_failed:
		fprintf(stderr, "clear QoS rules...\n");

		track_set("0");

		if (nvram_match("qos_enable", "1"))
		{
			nvram_set("qos_enable", "0");

			/* Reset all qdisc first */
			doSystem("tc qdisc del dev %s root htb", net_name);
			system("tc qdisc del dev br0 root htb");

			/* Clean iptables*/
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
			!nvram_match("wan0_proto", "pptp") &&
			!nvram_match("wan0_proto", "l2tp") &&
			!is_hwnat_loaded() 
		)
			system("insmod -q hw_nat.ko");
	}
}
#endif

// 2008.08 magic {
static void handle_notifications(void) {
	DIR *directory = opendir("/tmp/rc_notification");
	
	printf("handle_notifications() start\n");
	
	state = IDLE;
	
	if (directory == NULL)
		return;
	
	while (TRUE) {
		struct dirent *entry;
		char *full_name;
		FILE *test_fp;

		entry = readdir(directory);
		if (entry == NULL)
			break;
		if (strcmp(entry->d_name, ".") == 0)
			continue;
		if (strcmp(entry->d_name, "..") == 0)
			continue;

		/* Remove the marker file. */
		full_name = (char *)(malloc(strlen(entry->d_name) + 100));
		if (full_name == NULL)
		{
			fprintf(stderr,
					"Error: Failed trying to allocate %lu bytes of memory for "
					"the full name of an rc notification marker file.\n",
					(unsigned long)(strlen(entry->d_name) + 100));
			break;
		}
		sprintf(full_name, "/tmp/rc_notification/%s", entry->d_name);
		remove(full_name);
		
//		printf("Flag : %s\n", entry->d_name);

		/* Take the appropriate action. */
		if (strcmp(entry->d_name, "restart_reboot") == 0)
		{
			fprintf(stderr, "rc rebooting the system.\n");
//			if (nvram_match("wan0_proto", "3g") && (strlen(nvram_safe_get("usb_path1")) > 0))
			if (strlen(nvram_safe_get("usb_path1")) > 0)
			{
				printf("[rc] hanble_notifications ejusb");
				system("ejusb");
				if (nvram_match("wan0_proto", "3g"))
					sleep(10);
				else
					sleep(3);
			}
			else
				sleep(1);	// wait httpd sends the page to the browser.

			system("reboot");
			return;
		}
		else if (strcmp(entry->d_name, "restart_networking") == 0)
		{
			fprintf(stderr, "rc restarting networking.\n");
			
#ifdef WEB_REDIRECT
			printf("--- SERVICE: Wait to kill wanduck ---\n");
			stop_wanduck();
			
			signalled = RESTART;
#endif
			return;
		}
		else if (strcmp(entry->d_name, "restart_cifs") == 0)
		{
			printf("rc restarting ftp\n");		// tmp test
			stop_ftp();
			sleep(1);
			run_ftpsamba();
		}
		else if (strcmp(entry->d_name, "restart_ddns") == 0 && nvram_match("wan_route_x", "IP_Routed"))
		{
			fprintf(stderr, "rc restarting DDNS.\n");
			stop_ddns();
			
			if (nvram_match("ddns_enable_x", "1")) {
				start_ddns();
				
				if (nvram_match("ddns_server_x", "WWW.ASUS.COM")
						&& strstr(nvram_safe_get("ddns_hostname_x"), ".asuscomm.com") != NULL) {
					// because the computer_name is followed by DDNS's hostname.
//					if (nvram_match("samba_running", "1")) {
					if (pids("smbd")) {
						stop_samba();
						sleep(1);
						run_samba();
					}
					
//					if (nvram_match("ftp_running", "1")) {
					if (pids("vsftpd")) {
						stop_ftp();
						sleep(1);
						run_ftp();
					}
					
//					if (nvram_match("dms_running", "1")) {
					if (pids("ushare")) {
						stop_dms();
#ifdef CONFIG_USER_MTDAAPD
						stop_mt_daapd();
#endif
						sleep(1);
						run_dms();
					}
				}
			}
		}
		else if (strcmp(entry->d_name, "restart_httpd") == 0)
		{
			fprintf(stderr, "rc restarting HTTPD.\n");
			stop_httpd();
			nvram_unset("login_ip");
			nvram_unset("login_timestamp");
			start_httpd();
		}
		else if (strcmp(entry->d_name, "restart_dns") == 0)
		{
			fprintf(stderr, "rc restarting DNS.\n");
//			stop_dns();
//			start_dns();
			restart_dns();
		}
		else if (strcmp(entry->d_name, "restart_dhcpd") == 0)
		{
			fprintf(stderr, "rc restarting DHCPD.\n");
			restart_dhcpd();
		}
		else if (strcmp(entry->d_name, "restart_upnp") == 0)
		{
			stop_upnp();
			if (nvram_match("upnp_enable", "1")) {
				fprintf(stderr, "rc restarting UPNP.\n");
				nvram_set("upnp_enable_ex", "1");
				start_upnp();
			}
		}
                else if (strcmp(entry->d_name, "restart_dms") == 0)
                {
                        stop_dms();
#ifdef CONFIG_USER_MTDAAPD
			stop_mt_daapd();
#endif
                        if (nvram_match("apps_dms", "1")) {
                                fprintf(stderr, "rc restarting DMS.\n");
                                nvram_set("apps_dmsx", "1");
                                start_dms();
#ifdef CONFIG_USER_MTDAAPD
				start_mt_daapd();
#endif
                        }
                }
#ifdef QOS
		else if (strcmp(entry->d_name, "restart_qos") == 0)
		{
			printf("rc restarting QOS.\n");
			
			restart_qos();
		}
#endif
		else if (strcmp(entry->d_name, "restart_syslog") == 0)
		{
			fprintf(stderr, "rc restarting syslogd.\n");
#ifdef ASUS_EXT  
	stop_logger();
	start_logger();
#endif

		}
		else if (strcmp(entry->d_name, "restart_firewall") == 0)
		{
			char wan_ifname[16];
			char *wan_proto = nvram_safe_get("wan_proto");
			
			fprintf(stderr, "rc restarting firewall.\n");
			/*if (!nvram_match("wan_status_t", "Connected"))
				continue;*/
			
			memset(wan_ifname, 0, 16);
			strncpy(wan_ifname, nvram_safe_get("wan_ifname_t"), 16);
			if (strlen(wan_ifname) == 0) {
				if (!strcmp(wan_proto, "pppoe")
						|| !strcmp(wan_proto, "pptp")
						|| !strcmp(wan_proto, "l2tp"))
					strcpy(wan_ifname, "ppp0");
				else
					strcpy(wan_ifname, "eth3");
			}
			
			start_firewall();
			
#ifdef NOIPTABLES
			start_firewall2(wan_ifname);
#else
			fprintf(stderr, "rc restarting IPTABLES firewall.\n");
			start_firewall_ex(wan_ifname, nvram_safe_get("wan0_ipaddr"), "br0", nvram_safe_get("lan_ipaddr"));
#endif
			
#ifndef ASUS_EXT
			/* Start connection dependent firewall */
			start_firewall2(wan_ifname);
#endif
		}
		else if (strcmp(entry->d_name, "restart_ntpc") == 0)
		{
			fprintf(stderr, "rc restarting ntpc.\n");
//			stop_ntpc();
//			start_ntpc();
			refresh_ntpc();
		}
		else if (strcmp(entry->d_name, "rebuild_cifs_config_and_password") ==
				 0)
		{
			fprintf(stderr, "rc rebuilding CIFS config and password databases.\n");
//			regen_passwd_files(); /* Must be called before regen_cifs_config_file(). */
//			regen_cifs_config_file();
		}
		else if (strcmp(entry->d_name, "ddns_update") == 0)
		{
			fprintf(stderr, "rc updating ez-ipupdate for ddns changes.\n");
//			update_ddns_changes();
		}
		else if (strcmp(entry->d_name, "restart_time") == 0)
		{
			fprintf(stderr, "rc restarting time.\n");

			do_timer();
			
#ifdef ASUS_EXT  
			stop_logger();
			start_logger();
#endif
			
//			stop_ntpc();
//			start_ntpc();
			refresh_ntpc();
		}
#ifdef WSC
		else if (!strcmp(entry->d_name, "restart_wps"))
		{
			fprintf(stderr, "rc restart_wps\n");
			;	// do nothing
		}
#endif
#if 0
		else if (!strcmp(entry->d_name, "restart_apcli"))
		{
			if (nvram_match("apcli_workaround", "0"))
			{
				nvram_set("apcli_workaround", "2");

				fprintf(stderr, "rc restarting apcli_monitor.\n");

				system("brctl addif br0 apcli0");
				kill_pidfile_s("/var/run/apcli_monitor.pid", SIGTSTP);
			}
		}
#endif
		else
		{
			fprintf(stderr,
					"WARNING: rc notified of unrecognized event `%s'.\n",
					entry->d_name);
		}

		/*
		 * If there hasn't been another request for the same event made since
		 * we started, we can safely remove the ``action incomplete'' marker.
		 * Otherwise, we leave the marker because we'll go through here again
		 * for this even and mark it complete only after we've completed it
		 * without getting another request for the same event while handling
		 * it.
		 */
		test_fp = fopen(full_name, "r");
		if (test_fp != NULL)
		{
			fclose(test_fp);
		}
		else
		{
			/* Remove the marker file. */
			sprintf(full_name, "/tmp/rc_action_incomplete/%s", entry->d_name);
			remove(full_name);
		}

		free(full_name);
	} 
	
	printf("handle_notifications() end, state : %d\n", state);
	closedir(directory);
}

int if_mounted_s()
{
	FILE *fp_m = fopen("/proc/mounts", "r");
	char buf[120];
	int ret = 0;

	memset(buf, 0, sizeof(buf));
	while (fgets(buf, sizeof(buf), fp_m))
	{
		if (strstr(buf, "/media/AiDisk_"))
		{
			ret = 1;
			break;
		}
		memset(buf, 0, sizeof(buf));
	}

	fclose(fp_m);
	return ret;
}

int if_mounted()
{
	FILE *fp_m = fopen("/proc/mounts", "r");
	FILE *fp_p = fopen("/proc/partitions", "r");
	char buf[120];
	int mounted_num = 0, blocks_cnt, valid_partnum = 0;
	#define MAXT 6
	char *p, *tokens[MAXT], *last;
	int i, ret = 0;

	printf("chk if_mounted\n");			// tmp test
	memset(buf, 0, sizeof(buf));
	while (fgets(buf, sizeof(buf), fp_m))
	{
		if (strstr(buf, "/media/AiDisk_"))
		{
			++mounted_num;
		}
		memset(buf, 0, sizeof(buf));
	}
	printf("mounted_num = %d\n", mounted_num);	// tmp test

	memset(buf, 0, sizeof(buf));
	while (fgets(buf, sizeof(buf), fp_p))
	{
		for (i=0, (p = strtok_r(buf, " ", &last)); p; (p = strtok_r(NULL, " ", &last)), ++i)
		{
		   if (i < MAXT - 1)
			   tokens[i] = p;
		}
		tokens[i] = NULL;

		if (((p = strstr(tokens[3], "sd"))!=NULL) && !((*(p+3)=='1')&&((*(p+4)==' ')||(*(p+4)=='\0')||(*(p+4)=='\t')||(*(p+4)=='\n'))))	// ignore chk sdx1
		{
			blocks_cnt = atoi(tokens[2]);
//			printf("chk %s blocks num: %d\n", tokens[3], blocks_cnt);	// tmp test
			if (blocks_cnt > 5)	// 5 bytes is just a chk number
				++valid_partnum;
		}
		memset(buf, 0, sizeof(buf));
	}
	printf("valid partnum = %d\n", valid_partnum);					// tmp test

	if ((mounted_num == valid_partnum) && (mounted_num > 0))
		ret = 1;
	else
		ret = 0;

	fclose(fp_m);
	fclose(fp_p);
	return ret;
}

void convert_misc_values()
{
	nvram_set("run_sh", "off");
//	nvram_set("rmem", "0");
	nvram_set("upnp_enable_ex", nvram_safe_get("upnp_enable"));
	nvram_set("lan_proto_ex", nvram_safe_get("lan_proto_x"));

	if (!strcmp(nvram_safe_get("wl_ssid"), ""))
		nvram_set("wl_ssid", "ASUS_5G");

	if (!strcmp(nvram_safe_get("rt_ssid"), ""))
		nvram_set("rt_ssid", "ASUS");
		

//	if (strcmp(nvram_safe_get("wl_ssid"), nvram_safe_get("wl_ssid2"))) {
		char buff[100];
		memset(buff, 0, 100);
		char_to_ascii(buff, nvram_safe_get("wl_ssid"));
		nvram_set("wl_ssid2", buff);
//	}

//	if (strcmp(nvram_safe_get("rt_ssid"), nvram_safe_get("rt_ssid2"))) {
		char buff2[100];
		memset(buff2, 0, 100);
		char_to_ascii(buff2, nvram_safe_get("rt_ssid"));
		nvram_set("rt_ssid2", buff2);
//	}

	if (!strcmp(nvram_safe_get("wl_gmode"), ""))
		nvram_set("wl_gmode", "2");

	if (!strcmp(nvram_safe_get("rt_gmode"), ""))
		nvram_set("rt_gmode", "2");

	nvram_set("lan_ipaddr_t", "");
	nvram_set("lan_netmask_t", "");
	nvram_set("lan_gateway_t", "");
	nvram_set("lan_dns_t", "");

	nvram_set("wan_ipaddr_t", "");
	nvram_set("wan_netmask_t", "");
	nvram_set("wan_gateway_t", "");
	nvram_set("wan_dns_t", "");

#if defined (W7_LOGO) || defined (WIFI_LOGO)
	nvram_set("wan_proto", "static");
	nvram_set("wan0_proto", "static");
	nvram_set("wan_ipaddr", "17.1.1.1");
	nvram_set("wan0_ipaddr", "17.1.1.1");
	nvram_set("wanx_ipaddr", "17.1.1.1");
	nvram_set("wan_ipaddr_t", "17.1.1.1");
	nvram_set("wan_gateway", "17.1.1.1");
	nvram_set("wan0_gateway", "17.1.1.1");
	nvram_set("wanx_gateway", "17.1.1.1");
	nvram_set("wan_gateway_t", "17.1.1.1");
	nvram_set("wan_netmask", "255.0.0.0");
	nvram_set("wan0_netmask", "255.0.0.0");
	nvram_set("wanx_netmask", "255.0.0.0");
	nvram_set("wan_netmask_t", "255.0.0.0");
#endif

	/* Setup wan0 variables if necessary */
	set_wan0_vars();
}

int get_dev_info(int *dev_class, char *product_id, char *vendor_id, char *prod_id);

//unsigned int mem_out_count = 0;

extern char usb_path1[];
extern char usb_path1_old[];
extern char usb_path2[];
extern char usb_path2_old[];

void
get_usb_path1_info()
{
	char infostr[10];

	memset(infostr, 0x0, 10);
	sprintf(infostr, "%s %s", nvram_safe_get("usb_path1_vid"), nvram_safe_get("usb_path1_pid"));

	if (!nvram_match("usb_path1_vid", "") && !nvram_match("usb_path1_pid", ""))
		puts(infostr);
	else
		puts("");
}

void
get_usb_path2_info()
{
	char infostr[10];

	memset(infostr, 0x0, 10);
	sprintf(infostr, "%s %s", nvram_safe_get("usb_path2_vid"), nvram_safe_get("usb_path2_pid"));

	if (!nvram_match("usb_path2_vid", "") && !nvram_match("usb_path2_pid", ""))
		puts(infostr);
	else
		puts("");
}

/* Main loop */
static void
main_loop(void)
{
	sigset_t sigset;
	pid_t shell_pid = 0;
	char *usb_cur_state;
	int i;
	char chkbuf[12];
	FILE *fp;
	char *wan_proto_type;
	int val;
	int bus_plugged, dev_class;
	char product_id[20];
	char productID[20], prid[10], veid[10];
#ifdef U2EC
	int u2ec_fifo;
#endif

	/* Basic initialization */
	sysinit();

	/* Setup signal handlers */
	signal_init();
	signal(SIGHUP, rc_signal);
	signal(SIGUSR1, rc_signal);
	signal(SIGUSR2, rc_signal);
	signal(SIGINT, rc_signal);
	signal(SIGALRM, rc_signal);
	signal(SIGTTIN, rc_signal);	// usb storage
	signal(SIGTSTP, ipdown_main_sp);// for ppp ip-down
	sigemptyset(&sigset);

	/* Restore defaults if necessary */
	restore_defaults();

/*
	ra_gpio_init();
	ra_gpio_read_int(&val);
	printf("sw_mode val is %x\n", val);			// tmp test

	if ((val & (1 << 9)) && (nvram_match("wan_proto", "3g")))
	{
		printf("[rc] router-mode set 3g flag\n");	// tmp test
		track_set("201");
		write_genconn();
	} 
	else if ((!(val & (1 << 9))) && (nvram_match("wan_proto", "3g")))
	{
		nvram_set("wan0_proto", "dhcp");
		printf("auto change wan_proto from 3g to dhcp");// tmp test
	}
*/

	getsyspara();

/*
	if (val & (1 << 13))	// force radio on under repeater mode
	{
		printf("[rc] Force turn radio on\n");		// tmp test
		nvram_set("wl_radio_x", "1");
	}
	else							// tmp test
	{
		printf("[rc Chk] [%s][%s]\n", nvram_safe_get("sw_mode"), nvram_safe_get("sw_mode_ex"));	
	}
*/

	init_switch_mode();
	convert_asus_values(0);
	convert_misc_values();

	memset(usb_path1, 0x0, 16);
	memset(usb_path1_old, 0x0, 16);
	memset(usb_path2, 0x0, 16);
	memset(usb_path2_old, 0x0, 16);

/*
	if (nvram_match("wan_proto", "3g"))
	{
		track_set("201");
		write_genconn();
	}
*/

	gen_ralink_config();
	gen_ralink_config_rt();
	insmod();

	/* Loop forever */
	for (;;) {
		switch (state) {
		case RECOVER:
//			check_all_tasks();
			printf("rc main: enter RECOVER state\n");

			state = IDLE;
			break;
		case SERVICE:
			printf("rc SERVICE\n");						// tmp test

			track_set("500");

/*
//			if (event_code == EVT_MEM_OUT)					// mem out
			if ((event_code == EVT_MEM_OUT) || nvram_match("rmem", "1"))	// tmp test
			{
				nvram_set("rmem", "0");					// tmp test
				if ((fp=fopen("/proc/sys/net/ipv4/netfilter/ip_conntrack_max", "w+")))
				{
					fputs("4096", fp);
					fclose(fp);
				}
				++mem_out_count;
				memset(chkbuf, 0, sizeof(chkbuf));
				sprintf(chkbuf, "%d", mem_out_count);
				nvram_set("event_mem_out", chkbuf);
				logmessage(LOGNAME, "Out of memory!");
				state = RECOVER;
				break;
			}
*/

			if (nvram_get("rc_service") != NULL) {	// for original process
				service_handle();
				state = IDLE;
			}
			else{	// for new process
				handle_notifications();
			}
			
			nvram_set("success_start_service", "1");	// 2008.05 James. For judging if the system is ready.
			break;
		case HOTPLUG:
//			if (!nvram_match("asus_mfg", "0"))
//				break;
/*
			if (nvram_match("ignore_plug", "1"))
			{
				printf("[rc debug] ignore hot-plug event\n");
				state = IDLE;
				break;
			}
*/
			usb_cur_state = nvram_safe_get("usb_dev_state");
			printf("\n## rc recv HOTPLUG:%s\n", usb_cur_state);	// tmp test

			if (!nvram_match("wan_proto", "3g"))
				track_set("501");
			else
				track_set("202");

			switch(event_code) {
			case USB_PLUG_ON:
				printf("#[rc] USB PLUG ON\n");			// tmp test
				if (strcmp(usb_cur_state, "on") == 0)		// ignore extra SIGTTIN 
				{
					printf("ignore SIGTTIN (on)\n");	// tmp test
					break;
				}

				memset(product_id, 0, sizeof(product_id));
				printf("chk bus_plugged\n");			// tmp test
				bus_plugged = get_dev_info(&dev_class, productID, veid, prid);
				printf("bus_plggued = %d, dev_class = %d\n", bus_plugged, dev_class);	// tmp test

				start_usbled();

				if (dev_class == 0x35)   // USB_CLS_3GDEV
				{
					nvram_set("usb_path1", "HSDPA");
				}
				else
				{
					for (i=0; i<15; ++i)			// check if mounted
					{
						if (!if_mounted())
							sleep(1);
						else
							break;
					}
					if (i == 15)
					{
						printf("scsi mount fail\n");	// tmp test
//						nvram_set("mount_late", "1");
					}
				}

				nvram_set("usb_dev_state", "on");
				hotplug_usb();
				stop_usbled();
				nvram_set("usb_dev_state", "off");
				break;
			case USB_PLUG_OFF:
				printf("\n### USB PLUG OFF ###\n");		// tmp test
				if (strcmp(usb_cur_state, "on") == 0)		// ignore extra SIGTTIN 
				{
					printf("ignore SIGTTIN (off)\n");	// tmp test
					break;
				}
/*
				if (nvram_match("wan_proto", "3g"))
				{
					printf("ignore it on 3g mode\n");	// tmp test
					break;
				}
*/
/*
				if (strcmp(usb_cur_state, "off") == 0)		// ignore extra SIGTTIN 
				{
					printf("ignore SIGTTIN (on)\n");	// tmp test
					break;
				}
*/
/*
				for (i=0; i<15; ++i)	// check if mounted
				{
					if (if_mounted_s())
						sleep(1);
					else
						break;
				}
				if (i == 15)
					printf("scsi umount fail\n");		// tmp test
*/
//				nvram_set("usb_dev_state", "off");
//				nvram_set("usb_mass_path", "none");
//				nvram_set("mount_late", "0");

				nvram_set("usb_dev_state", "on");
				hotplug_usb();
				nvram_set("usb_dev_state", "off");

				break;
			case USB_PRT_PLUG_ON:
				printf("\n### PRT PLUG ON ###\n");		// tmp test
//				if (strcmp(usb_cur_state, "on") == 0)		// ignore extra SIGTTIN 
//				{
//					printf("ignore SIGTTIN (prt_on)\n");	// tmp test
//					break;
//				}
//				nvram_set("usb_dev_state", "on");
//				hotplug_usb();
#ifdef U2EC
				u2ec_fifo = open("/var/u2ec_fifo", O_WRONLY|O_NONBLOCK);
				write(u2ec_fifo, "a", 1);
				close(u2ec_fifo);
#endif
				break;
			case USB_PRT_PLUG_OFF:
				printf("\n### PRT PLUG OFF ###\n");		// tmp test
//				if (strcmp(usb_cur_state, "off") == 0)		// ignore extra SIGTTIN 
//				{
//					printf("ignore SIGTTIN (prt_off)\n");	// tmp test
//					break;
//				}
//				nvram_set("usb_dev_state", "off");
//				nvram_set("usb_mnt_first_path", "");
//				hotplug_usb();
#ifdef U2EC
				u2ec_fifo = open("/var/u2ec_fifo", O_WRONLY|O_NONBLOCK);
				write(u2ec_fifo, "r", 1);
				close(u2ec_fifo);
#endif
				break;
#if 0
			case USB_3G_PLUG_ON:
				printf("#[rc] USB_3G_PLUG ON\n");		// tmp test
				if (strcmp(usb_cur_state, "on") == 0)		// ignore extra SIGTTIN 
				{
					printf("ignore SIGTTIN (3g_on)\n");	// tmp test
					break;
				}
				else if (nvram_match("run_sh", "on"))
				{
					printf("[rc s3g] 3g script is now running\n");
					break;
				}
				rc_start_3g();
				break;
			case USB_3G_PLUG_OFF:
				printf("#[rc] USB_3G_PLUG OFF\n");		// tmp test
/*
				track_set("203");				// get event code
				if (event_code == USB_HUB_RE_ENABLE)
				{	
					printf("ignore hub_re_enable (3g_off)\n");		// tmp test
					break;
				}
				bus_plugged = get_dev_info(&dev_class, productID, veid, prid);
				if (bus_plugged > 0)
				{
					printf("usbdev still on, ignore plug-off (3g_off)\n");	// tmp test
					break;
				}
				if (strcmp(usb_cur_state, "off") == 0)		// ignore extra SIGTTIN 
				{
					printf("ignore SIGTTIN (3g_off)\n");	// tmp test
					break;
				}
*/
				if (nvram_match("run_sh", "on"))
				{
					nvram_set("usb3g", "re");
					printf("[rc s3g] 3g script is now running\n");
					break;
				}
				stop_3g();
				break;
#endif
			default:
				printf("SIGTTIN: do nothing\n");		// tmp test
				break;
			}
			state = IDLE;
			break;
		case RESTART:
			printf("rc RESTART\n");						// tmp test
			/* Fall through */
		case STOP:	// when will it occur?
			printf("rc STOP\n");						// tmp test
#ifdef ASUS_EXT
			stop_misc();
#endif
			stop_services();

			stop_wan();

			stop_lan();

//			if (nvram_match("wan0_proto", "3g") && (strlen(nvram_safe_get("usb_path1")) > 0))
			if (strlen(nvram_safe_get("usb_path1")) > 0)
			{
				printf("[rc] main_loop ejusb");
				system("ejusb");
				if (nvram_match("wan0_proto", "3g"))
					sleep(10);
				else
					sleep(3);
			}

			if (state == STOP) {
				state = IDLE;
				break;
			}
			/* Fall through */
		case START:
			printf("\n[rc] START\n");

			set_timezone();

			start_linkstatus_monitor();
			config_loopback();
			vconfig();

//			goto RC_END;

			start_lan();
			default_filter_setting();
#if (!defined(W7_LOGO) && !defined(WIFI_LOGO))
#ifdef WEB_REDIRECT
			if (!nvram_match("wanduck_down", "1") && nvram_match("wan_nat_x", "1")/* && !nvram_match("wan0_proto", "static")*/)
			{
				printf("--- START: Wait to start wanduck ---\n");
				redirect_setting();
				start_wanduck();
        		}
#endif
#endif

			start_wan();
			start_dhcpd();

			wan_proto_type = nvram_safe_get("wan0_proto");
			if (wan_proto_type && (!strcmp(wan_proto_type, "pptp") || !strcmp(wan_proto_type, "l2tp")))	// delay run
				sleep(5);

			start_services();

#if (!defined(W7_LOGO) && !defined(WIFI_LOGO))
			if (nvram_match("wan_route_x", "IP_Bridged"))	/* 0712 ASUS */
			{
				sleep(1);
				printf("start detectWan\n");	// tmp test
				system("detectWan &");
			}
#endif

//			if (nvram_match("wan_route_x", "IP_Routed") && nvram_match("wan0_proto", "dhcp"))
//				system("start_mac_clone &");
RC_END:
			/* Fall through */
		case TIMER:
			do_timer();
			/* Fall through */
		case IDLE:
			state = IDLE;
			/* Wait for user input or state change */
			while (signalled == -1) {
				if (!noconsole && (!shell_pid || kill(shell_pid, 0) != 0))
				{
					shell_pid = run_shell(0, 1);
				}
				else
				{
					sigsuspend(&sigset);
				}
			}
			state = signalled;
			signalled = -1;
			break;
		default:
			return;
		}
	}
}

int
main(int argc, char **argv)
{
	char *base = strrchr(argv[0], '/');
	int i=0;
	
	base = base ? base + 1 : argv[0];

#if 0
	printf("\n\n########## CALL RC: ########## : ");	// tmp test
	for (i=0; i<argc; ++i)
	{
		printf("[%s] ", argv[i]);
	}
	printf("\n");						// tmp test
//
#endif

	/* init */
	if (!strcmp(base, "init")) {
		main_loop();
		return 0;
	}

	/* Set TZ for all rc programs */
        setenv("TZ", nvram_safe_get("time_zone_x"), 1);

	logmessage(LOGNAME, "%s starts", base);

	/* erase [device] */
	if (!strcmp(base, "erase")) {
		if (argv[1])
			return mtd_erase(argv[1]);
		else {
			fprintf(stderr, "usage: erase [device]\n");
			return EINVAL;
		}
	}
	else if (!strcmp(base, "nvram_restore")) {
		restore_defaults();
		return 0;
	}
	else if (!strcmp(base, "nvram_erase") || !strcmp(base, "ATE_Set_RestoreDefault")) {
		system("erase /dev/mtd1");
		return 0;
	}
#if (!defined(W7_LOGO) && !defined(WIFI_LOGO))
	else if (!strcmp(base, "pspfix"))	// psp fix
	{
		pspfix();
		return 0;
	}
#endif
	/* invoke watchdog */
	else if (!strcmp(base, "watchdog")) {
		return (watchdog_main());
	}

	/* stats [ url ] */
//	else if (!strcmp(base, "stats")) {	// disable for tmp
//		return http_stats(argv[1] ? : nvram_safe_get("stats_server"));
//	}
#ifndef FLASH2M
	/* write [path] [device] */
	else if (!strcmp(base, "write")) {
		if (argc >= 3)
			return mtd_write(argv[1], argv[2]);
		else {
			fprintf(stderr, "usage: write [path] [device]\n");
			return EINVAL;
		}
	}
#endif
	/* udhcpc [ deconfig bound renew ] */
	else if (!strcmp(base, "udhcpc"))
		return udhcpc_main(argc, argv);
#ifdef ASUS_EXT
	/* hotplug [event] */
	else if (!strcmp(base, "hotplug_usb_mass"))      // added by Jiahao for WL500gP
	{
		return hotplug_usb_mass("");
	}
	else if (!strcmp(base, "hotplug"))
	{
		fprintf(stderr, "call hotplug_usb_test()...\n");
		return hotplug_usb_test();
	}
#ifdef DLM
	else if (!strcmp(base, "create_swap_file"))
		return create_swap_file(argv[1]);
	else if (!strcmp(base, "run_apps"))
		return run_apps();
	else if (!strcmp(base, "run_ftpsamba"))	 // added by Jiahao for WL500gP
	{
		nvram_set("usb_storage_busy", "1");     // 2007.12 James.
		run_ftpsamba();
		nvram_set("usb_storage_busy", "0");     // 2007.12 James.

		return 0;
	}
	else if (!strcmp(base, "run_dms")) {
		nvram_set("usb_storage_busy", "1");
		run_dms();
		nvram_set("usb_storage_busy", "0");

		return 0;
	}
	else if (!strcmp(base, "run_samba")) {
		nvram_set("usb_storage_busy", "1");
		run_samba();
		nvram_set("usb_storage_busy", "0");

		return 0;
	}
	else if (!strcmp(base, "run_ftp")) {
		nvram_set("usb_storage_busy", "1");
		run_ftp();
		nvram_set("usb_storage_busy", "0");

		return 0;
	}
	else if (!strcmp(base, "stop_ftp")) {
		nvram_set("usb_storage_busy", "1");
		stop_ftp();
		nvram_set("usb_storage_busy", "0");

		return 0;
	}
	else if (!strcmp(base, "stop_dms")) {
		nvram_set("usb_storage_busy", "1");
		stop_dms();
#ifdef CONFIG_USER_MTDAAPD
		stop_mt_daapd();
#endif
		nvram_set("usb_storage_busy", "0");

		return 0;
	}
	else if (!strcmp(base, "stop_samba")) {
		nvram_set("usb_storage_busy", "1");
		stop_samba();
		nvram_set("usb_storage_busy", "0");

		return 0;
	}
	else if (!strcmp(base, "stop_ftpsamba")) {
		nvram_set("usb_storage_busy", "1");
		stop_ftpsamba();
		nvram_set("usb_storage_busy", "0");

		return 0;
	}
	else if (!strcmp(base, "check_proc_mounts_parts")) {
		pre_chk_mount();
		chk_partitions(USB_PLUG_ON);

		return 0;
	}
#endif
	/* ddns update ok */
	else if (!strcmp(base, "stopservice")) {
		if (argc >= 2)
			return (stop_service_main(atoi(argv[1])));
		else return (stop_service_main(0));
	}
	/* ddns update ok */
	else if (!strcmp(base, "ddns_updated")) 
	{
		return ddns_updated_main();
	}
	/* ddns update ok */
	else if (!strcmp(base, "start_ddns")) 
	{
		return start_ddns();
	}
	/* run ntp client */
	else if (!strcmp(base, "ntp")) {
		return (ntp_main());
	}
	else if (!strcmp(base, "refresh_ntpc")) {
		refresh_ntpc();
	}
#if 0
	else if (!strcmp(base, "gpiotest")) {
		return (gpio_main(/*atoi(argv[1])*/));
	}
#endif
	else if (!strcmp(base, "gpio_write")) {
		gpio_write(atoi(argv[1]));
		return 0;
	}
	else if (!strcmp(base, "radioctrl")) {
		if (argc >= 1)
			return (radio_main(atoi(argv[1])));
		else return EINVAL;
	}
	else if (!strcmp(base, "radioctrl_rt")) {
		if (argc >= 1)
			return (radio_main_rt(atoi(argv[1])));
		else return EINVAL;
	}
#ifdef BTN_SETUP
	/* invoke ots(one touch setup) */
	else if (!strcmp(base, "ots")) {	// no need. use WPS.
		return (ots_main());
	}
#endif
#ifndef FLASH2M
	/* udhcpc_ex [ deconfig bound renew ], for lan only */
	else if (!strcmp(base, "landhcpc"))
		return udhcpc_ex_main(argc, argv);
#endif
	/* rc [stop|start|restart ] */
	else if (!strcmp(base, "rc")) {
		if (argv[1]) {
			if (strncmp(argv[1], "start", 5) == 0)
				return kill(1, SIGUSR2);
			else if (strncmp(argv[1], "stop", 4) == 0)
			{
				printf("[rc stop]\n");
//				if (nvram_match("wan0_proto", "3g") && (strlen(nvram_safe_get("usb_path1")) > 0))
				if (strlen(nvram_safe_get("usb_path1")) > 0)
				{
					printf("[rc] main stop ejusb");
					system("ejusb");
					if (nvram_match("wan0_proto", "3g"))
						sleep(10);
					else
						sleep(3);
				}

				return kill(1, SIGINT);
			}
			else if (strncmp(argv[1], "restart", 7) == 0)
			{
				printf("[rc restart]\n");
				if (nvram_match("wan0_proto", "3g") && (strlen(nvram_safe_get("usb_path1")) > 0))
				if (strlen(nvram_safe_get("usb_path1")) > 0)
				{
					printf("[rc] main restart ejusb");
					system("ejusb");
					if (nvram_match("wan0_proto", "3g"))
						sleep(10);
					else
						sleep(3);
				}

				return kill(1, SIGHUP);
			}
		} else {
			fprintf(stderr, "usage: rc [start|stop|restart]\n");
			return EINVAL;
		}
	}
#endif // ASUS_EXT
	else if (!strcmp(base, "getMAC") || !strcmp(base, "getMAC_5G") || !strcmp(base, "ATE_Get_MacAddr_5G")) {
		return getMAC();
	}
	else if (!strcmp(base, "setMAC") || !strcmp(base, "setMAC_5G") || !strcmp(base, "ATE_Set_MacAddr_5G")) {
		if (argc == 2)
			return setMAC(argv[1]);
		else
			return EINVAL;
	}
	else if (!strcmp(base, "getMAC_2G") || !strcmp(base, "ATE_Get_MacAddr_2G")) {
		return getMAC_2G();
	}
	else if (!strcmp(base, "setMAC_2G") || !strcmp(base, "ATE_Set_MacAddr_2G")) {
		if (argc == 2)
			return setMAC_2G(argv[1]);
		else
			return EINVAL;
	}
	else if (!strcmp(base, "FWRITE")) {
		if (argc == 3)
			return FWRITE(argv[1], argv[2]);
		else
		return EINVAL;
	}
	else if (!strcmp(base, "getCountryCode") || !strcmp(base, "ATE_Get_RegulationDomain")) {
		return getCountryCode();
	}
	else if (!strcmp(base, "setCountryCode") || !strcmp(base, "ATE_Set_RegulationDomain")) {
		if (argc == 2)
			return setCountryCode(argv[1]);
		else
			return EINVAL;
	}
	else if (!strcmp(base, "gen_ralink_config")) {
		return gen_ralink_config();
	}
	else if (!strcmp(base, "gen_ralink_config_rt")) {
		return gen_ralink_config_rt();
	}
	else if (!strcmp(base, "getPIN") || !strcmp(base, "ATE_Get_PINCode")) {
		return getPIN();
	}
	else if (!strcmp(base, "setPIN") || !strcmp(base, "ATE_Set_PINCode")) {
		if (argc == 2)
			return setPIN(argv[1]);
		else
			return EINVAL;
	}
	else if (!strcmp(base, "ATE_Set_StartATEMode")) {
		nvram_set("asus_mfg", "1");
		puts("1");
		return 0;
	}
	else if (!strcmp(base, "ATE_Set_AllLedOn")) {
		LED_on_all();
		puts("1");
		return 0;
	}
	else if (!strcmp(base, "ATE_Get_FWVersion")) {
		puts(nvram_safe_get("firmver"));
		return 0;
	}
	else if (!strcmp(base, "ATE_Get_BootLoaderVersion")) {
		puts(nvram_safe_get("blver"));
		return 0;
	}
	else if (!strcmp(base, "ATE_Get_ResetButtonStatus")) {
		puts(nvram_safe_get("btn_rst"));
		return 0;
	}
	else if (!strcmp(base, "ATE_Get_WpsButtonStatus")) {
		puts(nvram_safe_get("btn_ez"));
		return 0;
	}
	else if (!strcmp(base, "ATE_Get_Usb2p0_Port1_Infor")) {
		get_usb_path1_info();
		return 0;
	}
	else if (!strcmp(base, "ATE_Get_Usb2p0_Port2_Infor")) {
		get_usb_path2_info();
		return 0;
	}
	else if (!strcmp(base, "getSSID")) {
		return getSSID();
	}
	else if (!strcmp(base, "getChannel")) {
		return getChannel();
	}
	else if (!strcmp(base, "getChannel_2g")) {
		return getChannel_2G();
	}
	else if (!strcmp(base, "getSiteSurvey")) {
		return getSiteSurvey();
	}
	else if (!strcmp(base, "getBSSID")) {
		return getBSSID();
	}
	else if (!strcmp(base, "getBootV")) {
		return getBootVer();
	}
	else if (!strcmp(base, "asuscfe") || !strcmp(base, "asuscfe_5g")) {
		if (argc == 2)
			return asuscfe(argv[1], WIF5G);
		else
			return EINVAL;
	}
	else if (!strcmp(base, "asuscfe_2g")) {
		if (argc == 2)
			return asuscfe(argv[1], WIF2G);
		else
			return EINVAL;
	}
	else if (!strcmp(base, "wps_pin")) {
		if (nvram_match("wps_band", "0"))
		{
		if (argc == 2)
			return wps_pin(atoi(argv[1]));
		else if (argc == 1)
			return wps_pin(0);
		else
			return EINVAL;
		}
		else
		{
		if (argc == 2)
			return wps_pin_2g(atoi(argv[1]));
		else if (argc == 1)
			return wps_pin_2g(0);
		else
			return EINVAL;
		}
	}
	else if (!strcmp(base, "wps_pbc")) {
		if (nvram_match("wps_band", "0"))
			return wps_pbc();
		else
			return wps_pbc_2g();
	}
	else if (!strcmp(base, "wps_oob")) {
		if (nvram_match("wps_band", "0"))
			wps_oob();
		else
			wps_oob_2g();
		return 0;
	}
	else if (!strcmp(base, "wps_start")) {
		if (nvram_match("wps_band", "0"))
			return start_wsc();
		else
			return start_wsc_2g();
	}
	else if (!strcmp(base, "wps_stop")) {
		if (nvram_match("wps_band", "0"))
			return stop_wsc();
		else
			return stop_wsc_2g();
	}
/*
//	else if (!strcmp(base, "startWan")) {
	else if (!strcmp(base, "start_wan")) {
		start_wan();
		return 0;
	}
	else if (!strcmp(base, "stop_wan")) {
		stop_wan();
		return 0;
	}
*/
	else if (!strcmp(base, "start_mac_clone")) {
		start_mac_clone();
		return 0;
	}
	/* ppp */
	else if (!strcmp(base, "ip-up"))
		return ipup_main(argc, argv);
	else if (!strcmp(base, "ip-down"))
		return ipdown_main(argc, argv);
	else if (!strcmp(base, "wan-up"))
		return ipup_main(argc, argv);
	else if (!strcmp(base, "wan-down"))
		return ipdown_main(argc, argv);
	/* restore default */
	else if (!strcmp(base, "restore"))	// no need
	{
		if (argc==2)
		{
			int step = atoi(argv[1]);
			if (step>=1)
			{
//				nvram_set("vlan_enable", "1");
				restore_defaults();
			}
			/* Setup wan0 variables if necessary */
			if (step>=2)
				set_wan0_vars();
			if (step>=3)
				RC1_START();
			if (step>=4)
				start_lan();
		}
		return 0;
	}
#ifdef ASUS_EXT
#ifdef QOS
	else if (!strcmp(base, "speedtest"))
	{
		qos_get_wan_rate();
		
		return 0;
	}
#endif
	else if (!strcmp(base, "restart_dns"))
	{
//		stop_dns();
//		start_dns();
		restart_dns();
		
		return 0;
	}
	else if (!strcmp(base, "restart_networkmap"))
	{
		stop_networkmap();
		unlink("/tmp/static_ip.inf");
		start_networkmap();

		return 0;
	}
	else if (!strcmp(base, "convert_asus_values"))
	{
		convert_asus_values(1);
		
		return 0;
	}
	else if (!strcmp(base, "umount2"))
	{
		umount2(argv[1], 0x00000002);	// MNT_DETACH
		return 0;
	}
	else if (!strcmp(base, "ejusb"))
	{
		if (argc == 1)
		{
			remove_usb_mass("1");
			remove_usb_mass("2");
		}
		else
			remove_usb_mass(argv[1]);
//		stop_usb();
		return 0;
	}
	else if (!strcmp(base, "pids"))
	{
		pids_main(argv[1]);
		return 0;
	}
	else if (!strcmp(base, "start_telnetd"))
	{
		start_telnetd();
		return 0;
	}
	else if (!strcmp(base, "start_ots"))
	{
		start_ots();
		return 0;
	}
	else if (!strcmp(base, "start_ntp"))
	{
		stop_ntpc();
		start_ntpc();
		return 0;
	}
	else if (!strcmp(base, "get_sw"))
	{
		printf("sw mode is %s", nvram_safe_get("sw_mode"));
		return 0;
	}
	else if (!strcmp(base, "tracktest"))
	{
		track_set(argv[1]);
		return 0;
	}
/*
	else if (!strcmp(base, "chkalltask"))
	{
		check_all_tasks();
		return 0;
	}
*/
	else if (!strcmp(base, "ledoff"))
	{
		LED_CONTROL(LED_POWER, LED_OFF);
		return 0;
	}
	else if (!strcmp(base, "ledon"))
	{
		LED_CONTROL(LED_POWER, LED_ON);
		return 0;
	}
	else if (!strcmp(base, "start3g"))
	{
		rc_start_3g();
		return 0;
	}
	else if (!strcmp(base, "stop3g"))
	{
		stop_3g();
		return 0;
	}
/*
	else if (!strcmp(base, "aeject"))
	{
		if (!argv[1])
		{
			printf("no dev specified\n");
			return 0;
		}

		do_eject(argv[1]);
		return 0;
	}
*/
#ifdef USBTPT
	else if (!strcmp(base, "utpt"))
	{
		usbtpt(argc, argv);
		return 0;
	}
#endif
	else if (!strcmp(base, "usb_path_nvram"))
	{
		usb_path_nvram(argv[1], argv[2]);
		return 0;
	}
	else if (!strcmp(base, "8367m"))
	{
		config8367m(argv[1], argv[2]);
		return 0;
	}
	else if (!strcmp(base, "dumparptable"))
	{
		dumparptable();
		return 0;
	}
/*
        else if (!strcmp(base, "ar8316"))
        {
                if (argc == 2)
                        config8316(argv[1]);

                return 0;
        }
*/
	else if (!strcmp(base, "getWANStatus"))
	{
		int retVal;
		retVal = rtl8367m_wanPort_phyStatus();
		printf("WAN port status %d\n", retVal);
		return 0;
	}
	else if (!strcmp(base, "linkstatus_monitor")) {
		return linkstatus_monitor();
	}
	else if (!strcmp(base, "arpping")) {
		arpping();
		return 0;
	}
	else if (!strcmp(base, "detect_internet")) {
		detect_internet();
		return 0;
	}
	else if (!strcmp(base, "stainfo")) {
		return stainfo();
	}
	else if (!strcmp(base, "stainfo_2g")) {
		return stainfo_2g();
	}
	else if (!strcmp(base, "getstat")) {
		return getstat();
	}
	else if (!strcmp(base, "getstat_2g")) {
		return getstat_2g();
	}
	else if (!strcmp(base, "getrssi") || !strcmp(base, "getrssi_5g")) {
		return getrssi();
	}
	else if (!strcmp(base, "getrssi_2g")) {
		return getrssi_2g();
	}
	else if (!strcmp(base, "gettxbfcal")) {
		return gettxbfcal();
	}
        else if (!strcmp(base, "rtl8367m_LanPort_linkUp")) {
                return rtl8367m_LanPort_linkUp();
        }
        else if (!strcmp(base, "rtl8367m_LanPort_linkDown")) {
                return rtl8367m_LanPort_linkDown();
        }
        else if (!strcmp(base, "rtl8367m_AllPort_linkUp")) {
                return rtl8367m_AllPort_linkUp();
        }
	else if (!strcmp(base, "rtl8367m_AllPort_linkDown")) {
		return rtl8367m_AllPort_linkDown();
	}
	else if (!strcmp(base, "print_num_of_connections")) {
		print_num_of_connections();
		return 0;
	}
	else if (!strcmp(base, "print_wan_ip")) {
		print_wan_ip();
		return 0;
	}
	else if (!strcmp(base, "usbled")) {
		return (usbled_main());
	}
	else if (!strcmp(base, "start_usbled")) {
		return (start_usbled());
	}
	else if (!strcmp(base, "stop_usbled")) {
		return (stop_usbled());
	}
	else if (!strcmp(base, "reset_to_defaults")) {
		system("erase /dev/mtd1");
		sys_exit();
		return 0;
	}
	else if (!strcmp(base, "phyState") || !strcmp(base, "ATE_Get_WanLanStatus")) {
		rtl8367m_AllPort_phyState();
		return 0;
	}
#endif
	return EINVAL;
}

