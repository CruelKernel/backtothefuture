#include <errno.h>
#include <err.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <linux/fs.h>

#define BOOT_MAGIC_SIZE 8
#define BOOT_IMAGE_HEADER_V1_SIZE 1648
#define BOOT_IMAGE_HEADER_V2_SIZE 1660

#define HEADER_VERSION_OFFSET (             \
	sizeof(uint8_t) * BOOT_MAGIC_SIZE + \
	8 * sizeof(uint32_t)                \
)
#define OS_VERSION_OFFSET_V0 (HEADER_VERSION_OFFSET + sizeof(uint32_t))
#define OS_VERSION_OFFSET_V1 OS_VERSION_OFFSET_V0
#define OS_VERSION_OFFSET_V2 OS_VERSION_OFFSET_V0
#define OS_VERSION_OFFSET_V3 (HEADER_VERSION_OFFSET - 6 * sizeof(uint32_t))

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

static bool check_error(bool print_errno, bool cond, const char *message, ...)
{
	if (!cond)
		return false;

	va_list args;
	va_start(args, message);
	vfprintf(stderr, message, args);
	va_end(args);
	if (print_errno)
		fprintf(stderr, ": %s", strerror(errno));
	fprintf(stderr, "\n");
	exit(EXIT_FAILURE);

	return true;
}

#define checkx(cond, ...) check_error(false, cond, __VA_ARGS__)
#define check(cond, ...) check_error(true, cond, __VA_ARGS__)

static bool warn_on(bool cond, const char *message, ...)
{
	if (!cond)
		return false;

	va_list args;
	va_start(args, message);
	vfprintf(stderr, message, args);
	va_end(args);
	fprintf(stderr, "\n");

	return true;
}

static inline uint32_t get_header_version(uint8_t *addr)
{
	return *(uint32_t *)(addr + HEADER_VERSION_OFFSET);
}

static inline os_version_t get_os_version(uint8_t *addr)
{
	os_version_t v;
	uint32_t hdr_ver = get_header_version(addr);

	switch (hdr_ver) {
		case 0:
		case 1:
		case 2:
			v.version = *(uint32_t *)(addr + OS_VERSION_OFFSET_V2);
			break;
		case 3:
			v.version = *(uint32_t *)(addr + OS_VERSION_OFFSET_V3);
			break;
		default:
			errx(1, "Unsupported header version %u", hdr_ver);
			break;
	}
	return v;
}

static inline void set_os_version(uint8_t *addr, os_version_t v)
{
	uint32_t hdr_ver = get_header_version(addr);

	switch (hdr_ver) {
		case 0:
		case 1:
		case 2:
			*(uint32_t *)(addr + OS_VERSION_OFFSET_V2) = v.version;
			break;
		case 3:
			*(uint32_t *)(addr + OS_VERSION_OFFSET_V3) = v.version;
			break;
		default:
			errx(1, "Unsupported header version %u", hdr_ver);
			break;
	}
}

static uint8_t *mmap_boot_image(const char *file, int flags)
{
	uint8_t *addr;
	uint32_t header_version;
	struct stat st;
	int mmap_flags = PROT_READ;
	size_t block_size;

	int fd = open(file, flags);
	check(fd < 0, "open %s failed", file);

	if (flags == O_RDWR)
		mmap_flags |= PROT_WRITE;

	check(fstat(fd, &st) < 0, "stat %s failed", file);
	checkx(!(S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)) &&
	       st.st_size < BOOT_IMAGE_HEADER_V2_SIZE,
	       "%s is too small", file);
	checkx(S_ISCHR(st.st_mode), "%s is not a block device", file);

	if (S_ISBLK(st.st_mode)) {
		check(ioctl(fd, BLKGETSIZE64, &block_size) < 0,
		      "%s can't determine block device size", file);
		checkx(block_size < BOOT_IMAGE_HEADER_V2_SIZE,
		       "%s is too small", file);
	}

	addr = mmap(NULL, BOOT_IMAGE_HEADER_V2_SIZE, mmap_flags,
		    MAP_SHARED, fd, 0);
	check(addr == MAP_FAILED, "mmap %s failed", file);
	check(close(fd) < 0, "close %s failed", file);

	checkx(strncmp((const char *)addr, "ANDROID!", 8),
	       "%s has incorrect magic number, not an android boot image",
		file
	);

	header_version = get_header_version(addr);
	checkx(header_version > 3,
	       "%s unsupported header version (%u)",
	       file, header_version
	);

	return addr;
}

int main(int argc, char *argv[])
{
	uint8_t *addr;
	const char *file, *os_version, *os_patch_level;
	char *delim = NULL;
	int year, month;
	int a, b, c;
	os_version_t curv, newv;
	bool preserve_os_version = false;
	bool preserve_os_patch_level = false;

	checkx(argc != 4,
	       "Usage: %s <file> <os_version|same> <os_patch_level|same>",
	       argv[0]);

	file = argv[1];
	os_version = argv[2];
	os_patch_level = argv[3];

	if (!strcmp(os_version, "same")) {
		preserve_os_version = true;
	} else {
		// Format: a.b.c
		for (const char *p = os_version; *p != '\0'; ++p) {
			checkx(!(isdigit(*p) || *p == '.'),
			       "Incorrect os_version '%s'. Format: a.b.c",
			       os_version);
		}
		a = strtol(os_version, &delim, 10);
		checkx(*delim != '.',
		       "Incorrect os_version '%s'. Format: a.b.c", os_version);
		b = strtol(delim + 1, &delim, 10);
		checkx(*delim != '.',
		       "Incorrect os_version '%s'. Format: a.b.c", os_version);
		c = strtol(delim + 1, NULL, 10);
		checkx(!(0 <= a && a <= 127 &&
			0 <= b && b <= 127 &&
			0 <= c && c <= 127),
		       "Incorrect os_version '%s'. Format: a.b.c", os_version);
	}

	if (!strcmp(os_patch_level, "same")) {
		preserve_os_patch_level = true;
	} else {
		// Format: YYYY-MM
		checkx(strlen(os_patch_level) != 7 ||
		       os_patch_level[4] != '-'    ||
		       !isdigit(os_patch_level[0]) ||
		       !isdigit(os_patch_level[1]) ||
		       !isdigit(os_patch_level[2]) ||
		       !isdigit(os_patch_level[3]) ||
		       !isdigit(os_patch_level[5]) ||
		       !isdigit(os_patch_level[6]),
		       "Incorrent os_patch_level '%s'. Format: YYYY-MM",
		       os_patch_level);

		year  = atoi(os_patch_level);
		month = atoi(os_patch_level + 5);

		checkx(!(2000 <= year && year <= 2127),
			"Incorrect year: %ld (2000 <= year <= 2127)", year);
		checkx(!(1 <= month && month <= 12),
			"Incorrect month: %ld (01 <= month <= 12)", month);
	}

	addr = mmap_boot_image(file, O_RDWR);

	curv = get_os_version(addr);
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

		warn_on(curv.os_version > newv.os_version,
			"warn: new os_version is lower than current"
		);

		warn_on(curv.os_patch_level > newv.os_patch_level,
			"warn: new os_patch_level version is lower than current"
		);

		set_os_version(addr, newv);

		check(msync(addr, BOOT_IMAGE_HEADER_V2_SIZE, MS_SYNC) < 0,
		      "msync failed");
	} else {
		printf("The dates are the same. Nothing to be done.\n");
	}

	check(munmap(addr, BOOT_IMAGE_HEADER_V2_SIZE) < 0, "munmap failed");

	return 0;
}
