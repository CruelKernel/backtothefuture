#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define BOOT_IMAGE_HEADER_V1_SIZE 1648
#define BOOT_IMAGE_HEADER_V2_SIZE 1660

typedef union __attribute__((packed)) {
	uint32_t version;
	struct {
		unsigned os_patch_level:11;
		unsigned os_version:21;
	};
	struct {
		unsigned month:4;
		unsigned year:7;
		unsigned c:7;
		unsigned b:7;
		unsigned a:7;
	};
} os_version_t;


static inline void check(bool cond, const char *message, ...)
{
	if (cond) {
		va_list args;
		va_start(args, message);
		vfprintf(stderr, message, args);
		va_end(args);
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char *argv[])
{
	int fd;
	uint32_t *addr;
	const char *file, *os_version, *os_patch_level;
	char *delim = NULL;
	int year, month;
	int a, b, c;
	os_version_t curv, newv;
	bool preserve_os_version = false;
	bool preserve_os_patch_level = false;

	check(argc != 4, "Usage: %s <file> <os_version|same> <os_patch_level|same>\n", argv[0]);
	file = argv[1];
	os_version = argv[2];
	os_patch_level = argv[3];

	if (!strcmp(os_version, "same")) {
		preserve_os_version = true;
	} else {
		// Format: a.b.c
		for (const char *p = os_version; *p != '\0'; ++p) {
			check(!(isdigit(*p) || *p == '.'),
				"Incorrect os_version '%s'. Format: a.b.c\n", os_version);
		}
		a = strtol(os_version, &delim, 10);
		check(*delim != '.', "Incorrect os_version '%s'. Format: a.b.c\n", os_version);
		b = strtol(delim + 1, &delim, 10);
		check(*delim != '.', "Incorrect os_version '%s'. Format: a.b.c\n", os_version);
		c = strtol(delim + 1, NULL, 10);
		check(!(0 <= a && a <= 127 &&
			0 <= b && b <= 127 &&
			0 <= c && c <= 127),
			"Incorrect os_version '%s'. Format: a.b.c\n", os_version);
	}


	if (!strcmp(os_patch_level, "same")) {
		preserve_os_patch_level = true;
	} else {
		// Format: YYYY-MM
		check(strlen(os_patch_level) != 7 ||
		      os_patch_level[4] != '-'    ||
		      !isdigit(os_patch_level[0]) ||
		      !isdigit(os_patch_level[1]) ||
		      !isdigit(os_patch_level[2]) ||
		      !isdigit(os_patch_level[3]) ||
		      !isdigit(os_patch_level[5]) ||
		      !isdigit(os_patch_level[6]),
			"Incorrent os_patch_level '%s'. Format: YYYY-MM\n", os_patch_level);

		year  = atoi(os_patch_level);
		month = atoi(os_patch_level + 5);

		check(!(2000 <= year && year <= 2127),
			"Incorrect year: %ld (2000 <= year <= 2127)\n", year);
		check(!(1 <= month && month <= 12),
			"Incorrect month: %ld (01 <= month <= 12)\n", month);
	}

	fd = open(file, O_RDWR);
	check(fd < 0, "open %s failed: %s\n", file, strerror(errno));

	addr = mmap(NULL, BOOT_IMAGE_HEADER_V1_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	check(addr == MAP_FAILED, "mmap %s failed: %s\n", file, strerror(errno));

	check(strncmp((const char *)addr, "ANDROID!", 8),
		"Incorrect magic number, not a boot file.\n");

	curv.version = *(addr + 11);
	printf("Current OS version:\t%u.%u.%u %u-%02u\n",
		curv.a, curv.b, curv.c,
		curv.year + 2000, curv.month);

	if (preserve_os_version) {
		newv.os_version = curv.os_version;
	} else {
		newv.a = a;
		newv.b = b;
		newv.c = c;
	}

	if (preserve_os_patch_level) {
		newv.os_patch_level = curv.os_patch_level;
	} else {
		newv.year  = (year - 2000);
		newv.month = month;
	}

	if (curv.version != newv.version) {
		printf("New OS version:\t\t%u.%u.%u %u-%02u\n",
			newv.a, newv.b, newv.c,
			newv.year + 2000, newv.month);

		if (curv.os_version > newv.os_version) {
			fprintf(stderr, "warn: new os_version is lower than current\n");
		}

		if (curv.os_patch_level > newv.os_patch_level) {
			fprintf(stderr, "warn: new os_patch_level version is lower than current\n");
		}

		*(addr + 11) = newv.version;
		check(msync(addr, BOOT_IMAGE_HEADER_V1_SIZE, MS_SYNC) < 0, "msync failed: %s\n", strerror(errno));
	} else {
		printf("The dates are the same. Nothing to be done.\n");
	}

	check(munmap(addr, BOOT_IMAGE_HEADER_V1_SIZE) < 0, "munmap failed: %s\n", strerror(errno));
	check(close(fd) < 0, "close failed: %s\n", strerror(errno));

	return 0;
}
