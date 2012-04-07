#include <sys/time.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <nvram/bcmnvram.h>
#include <rtxxxx.h>
//#include <time.h>

void conntrack_and_dns_cache_cleanup()
{
	int i, num = 1;

	fprintf(stderr, "conntrack & dns cache cleanup\n");

	track_set("101");
	for (i = 0; i < num; i++)
	{
		fprintf(stderr, "**** clean ip_conntrack %d time(s). ****\n", i + 1);
		system("cat /proc/net/nf_conntrack 1>/dev/null 2>&1");		
	}
	track_set("100");

	system("rm -f /tmp/dproxy.cache");
	restart_dns();
}

static int count = 0;

void traceroute_monitor()
{
	if (nvram_match("no_internet_detect", "1"))
		count++;
	else
		count = 0;

	if (count > 3)
	{
		count = 0;
//		nvram_set("no_internet_detect", "0");

		if (pids("traceroute"))
			system("killall traceroute");	
	}
}

/*
#define LANPORT0_LINK_UP	0x0001
#define LANPORT1_LINK_UP	0x0002
#define LANPORT2_LINK_UP	0x0004
#define LANPORT3_LINK_UP	0x0008
unsigned char lanport0_link_up = 0;
unsigned char lanport1_link_up = 0;
unsigned char lanport2_link_up = 0;
unsigned char lanport3_link_up = 0;
*/
static unsigned int linkstatus_wan = 0;
static unsigned int linkstatus_lan = 0;
static unsigned int linkstatus_wan_old = 0;
static unsigned int linkstatus_lan_old = 0;
static int linkstatus_usb = 0;
static int linkstatus_usb_old = 0;
static unsigned int linkspeed_wan = 0;
char str_linkspeed_wan[2];

static unsigned int timer_speed = -1;

struct itimerval itv;

void
alarmtimer_ls(unsigned long sec, unsigned long usec)
{
	itv.it_value.tv_sec  = sec;
	itv.it_value.tv_usec = usec;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}

int
usb_status()
{
	if (nvram_match("no_usb_led", "1"))
		return 0;
	else if (nvram_invmatch("usb_path1", "") || nvram_invmatch("usb_path2", ""))
		return 1;
	else
		return 0;
}

void catch_sig_linkstatus(int sig)
{
//	time_t now;

	if (sig == SIGALRM)
	{
/*
		if (nvram_match("no_internet_detect", "1"))
		{
			now = time((time_t *)0);

			if (	((unsigned long)(now - strtoul(nvram_safe_get("detect_timestamp_wan"), NULL, 10)) > 5) &&
				pids("traceroute")	)
				system("killall traceroute");
		}
*/
		timer_speed = (timer_speed + 1) % 8;

		if (nvram_match("wan_route_x", "IP_Routed"))
			linkstatus_wan_old = linkstatus_wan;
		linkstatus_lan_old = linkstatus_lan;
		linkstatus_usb_old = linkstatus_usb;
		if (nvram_match("wan_route_x", "IP_Routed"))
		{
			linkstatus_wan = rtl8367m_wanPort_phyStatus();
			linkstatus_lan = rtl8367m_lanPorts_phyStatus();

			if (!timer_speed)
			{
				linkspeed_wan = rtl8367m_WanPort_phySpeed();
				sprintf(str_linkspeed_wan, "%d", linkspeed_wan);
				nvram_set("link_spd_wan", str_linkspeed_wan);
			}
		}
		else
			linkstatus_lan = rtl8367m_wanPort_phyStatus() || rtl8367m_lanPorts_phyStatus();
		linkstatus_usb = usb_status();
//		fprintf(stderr, "linkstatus_wan: %d\n", linkstatus_wan);
//		fprintf(stderr, "linkstatus_lan: %d\n", linkstatus_lan);

		if (nvram_match("wan_route_x", "IP_Routed") && (linkstatus_wan != linkstatus_wan_old))
		{
			if (linkstatus_wan)
			{
				nvram_set("link_wan", "1");
				LED_CONTROL(LED_WAN, LED_ON);

				if (pids("udhcpc"))
				{
				}
			}
			else
			{
				nvram_set("link_wan", "0");
				LED_CONTROL(LED_WAN, LED_OFF);

				if (pids("udhcpc"))
				{
//					if (strcasecmp(nvram_safe_get("wl_country_code"), "US"))
					{
						logmessage("linkstatus", "perform DHCP release");
						system("killall -SIGUSR2 udhcpc");

						conntrack_and_dns_cache_cleanup();
					}

					logmessage("linkstatus", "perform DHCP renew");
					system("killall -SIGUSR1 udhcpc");
				}
				else
					conntrack_and_dns_cache_cleanup();	
			}
		}

		if (linkstatus_lan != linkstatus_lan_old)
		{
/*
			if (linkstatus_lan & LANPORT0_LINK_UP)
				lanport0_link_up = 1;
			else
				lanport0_link_up = 0;

			if (linkstatus_lan & LANPORT1_LINK_UP)
				lanport1_link_up = 1;
			else
				lanport1_link_up = 0;

			if (linkstatus_lan & LANPORT2_LINK_UP)
				lanport2_link_up = 1;
			else
				lanport2_link_up = 0;

			if (linkstatus_lan & LANPORT3_LINK_UP)
				lanport3_link_up = 1;
			else
				lanport3_link_up = 0;

			fprintf(stderr, "LAN Link Status: %d, %d, %d, %d\n", lanport0_link_up, lanport1_link_up, lanport2_link_up, lanport3_link_up);
*/
			if (linkstatus_lan)
			{
				nvram_set("link_lan", "1");
				LED_CONTROL(LED_LAN, LED_ON);
			}
			else
			{
				nvram_set("link_lan", "0");
				LED_CONTROL(LED_LAN, LED_OFF);
			}
		}

		if (linkstatus_usb != linkstatus_usb_old)
		{
			if (linkstatus_usb)
			{
//				nvram_set("link_usb", "1");
				LED_CONTROL(LED_USB, LED_ON);
			}
			else
			{
//				nvram_set("link_usb", "0");
				LED_CONTROL(LED_USB, LED_OFF);
			}
		}

		if (nvram_match("wan_route_x", "IP_Routed"))
			traceroute_monitor();

//		alarm(1);
		alarmtimer_ls(1, 666 * 1000);
	}
}

void
LED_on_all()
{
	LED_CONTROL(LED_POWER, LED_ON);
	LED_CONTROL(LED_WAN, LED_ON);
	LED_CONTROL(LED_LAN, LED_ON);
	LED_CONTROL(LED_USB, LED_ON);	
}

int
linkstatus_monitor(int argc, char *argv[])
{
	FILE *fp;

	/* write pid */
	if ((fp=fopen("/var/run/linkstatus_monitor.pid", "w"))!=NULL)
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	nvram_set("link_wan", "0");
	nvram_set("link_lan", "0");
	nvram_set("link_spd_wan", "0");
#ifdef SR3
	system("8367m 11");	// for SR3 LAN LED
#endif
	ralink_gpio_init(LED_WAN, GPIO_DIR_OUT);
	ralink_gpio_init(LED_LAN, GPIO_DIR_OUT);
	ralink_gpio_init(LED_USB, GPIO_DIR_OUT);
	LED_CONTROL(LED_WAN, LED_OFF);
	LED_CONTROL(LED_LAN, LED_OFF);
	LED_CONTROL(LED_USB, LED_OFF);

	signal(SIGALRM, catch_sig_linkstatus);
//	alarm(1);

	while (1)
	{
		pause();
	}

	return 0;
}