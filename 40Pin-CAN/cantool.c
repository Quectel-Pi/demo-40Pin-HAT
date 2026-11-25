/*
 * cantool - 最小 SocketCAN 收发工具（M2 40Pin CAN 测试用）
 *
 * 用法：
 *   cantool send <iface> <count> <id> <interval_ms>
 *   cantool recv <iface> <count> <timeout_ms>
 *
 * 与 M2_can-loopback.sh 的约定：
 *   - send：发经典 CAN 帧，8 字节数据 00 <序号> A2 A3 A4 A5 A6 A7，
 *     帧间隔 interval_ms，全部发出后返回 0；
 *   - recv：每收一帧打印一行（含 id=0xXXX，供脚本兜底计数），
 *     收满 count 帧或 timeout_ms 超时后退出。
 *
 * 编译（静态，aarch64，可运行于 Android /data/local/tmp）：
 *   aarch64-linux-gnu-gcc -static -O2 -Wall -o prebuilt/cantool cantool.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <poll.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>

#include <linux/can.h>
#include <linux/can/raw.h>

static int open_can(const char *ifname)
{
	int s;
	struct ifreq ifr;
	struct sockaddr_can addr;

	s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (s < 0) {
		perror("socket");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
	if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
		perror("SIOCGIFINDEX");
		close(s);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		close(s);
		return -1;
	}
	return s;
}

static int do_send(const char *ifname, int count, unsigned long id, int interval_ms)
{
	int s, i;

	if (id > CAN_SFF_MASK) {
		fprintf(stderr, "仅支持标准帧 ID（<= 0x7FF）\n");
		return 1;
	}

	s = open_can(ifname);
	if (s < 0)
		return 1;

	for (i = 0; i < count; i++) {
		struct can_frame f;

		memset(&f, 0, sizeof(f));
		f.can_id = id;
		f.can_dlc = 8;
		f.data[0] = 0x00;
		f.data[1] = i & 0xFF;
		f.data[2] = 0xA2;
		f.data[3] = 0xA3;
		f.data[4] = 0xA4;
		f.data[5] = 0xA5;
		f.data[6] = 0xA6;
		f.data[7] = 0xA7;

		if (write(s, &f, sizeof(f)) != sizeof(f)) {
			perror("write");
			close(s);
			return 1;
		}
		if (interval_ms > 0 && i + 1 < count)
			usleep(interval_ms * 1000);
	}

	close(s);
	return 0;
}

static long long now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int do_recv(const char *ifname, int count, int timeout_ms)
{
	int s, n = 0;
	long long deadline = now_ms() + timeout_ms;

	s = open_can(ifname);
	if (s < 0)
		return 1;

	while (n < count) {
		long long remain = deadline - now_ms();
		struct pollfd pfd = { .fd = s, .events = POLLIN };
		struct can_frame f;
		int ret, i;

		if (remain <= 0)
			break;
		ret = poll(&pfd, 1, (int)remain);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			perror("poll");
			break;
		}
		if (ret == 0)
			break;

		if (read(s, &f, sizeof(f)) != sizeof(f))
			continue;

		printf("%s id=0x%03X len=%d data=", ifname,
		       (unsigned int)(f.can_id & CAN_SFF_MASK), f.can_dlc);
		for (i = 0; i < f.can_dlc; i++)
			printf("%02X%s", f.data[i], i + 1 < f.can_dlc ? " " : "");
		printf("\n");
		fflush(stdout);
		n++;
	}

	close(s);
	return n > 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
	if (argc == 6 && strcmp(argv[1], "send") == 0)
		return do_send(argv[2], atoi(argv[3]),
			       strtoul(argv[4], NULL, 0), atoi(argv[5]));

	if (argc == 5 && strcmp(argv[1], "recv") == 0)
		return do_recv(argv[2], atoi(argv[3]), atoi(argv[4]));

	fprintf(stderr,
		"用法：\n"
		"  cantool send <iface> <count> <id> <interval_ms>\n"
		"  cantool recv <iface> <count> <timeout_ms>\n");
	return 1;
}
