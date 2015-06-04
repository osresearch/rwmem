/*
 * DirectHW.c - userspace part for DirectHW
 *
 * Copyright © 2008-2010 coresystems GmbH <info@coresystems.de>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <AvailabilityMacros.h>
#include <IOKit/IOKitLib.h>
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <unistd.h>
#include <errno.h>
#include "DirectHW.h"

#ifndef MAP_FAILED
#define MAP_FAILED	((void *)-1)
#endif

/* define WANT_OLD_API for support of OSX 10.4 and earlier */
#undef WANT_OLD_API

/* define DEBUG to print Framework debugging information */
static const int debug;

#define     err_get_system(err) (((err)>>26)&0x3f)
#define     err_get_sub(err)    (((err)>>14)&0xfff)
#define     err_get_code(err)   ((err)&0x3fff)

enum {
	kReadIO,
	kWriteIO,
	kPrepareMap,
	kReadMSR,
	kWriteMSR,
	kNumberOfMethods
};

typedef struct {
	UInt32 offset;
	UInt32 width;
	UInt32 data;
} iomem_t;

typedef struct {
	UInt64 addr;
	UInt64 size;
} map_t;

typedef struct {
	UInt32 core;
	UInt32 index;
	UInt32 hi;
	UInt32 lo;
} msrcmd_t;

static io_connect_t connect = -1;
static io_service_t iokit_uc;

static int darwin_init(void)
{
	kern_return_t err;

	/* Note the actual security happens in the kernel module.
	 * This check is just candy to be able to get nicer output
	 */
	if (getuid() != 0) {
		/* Fun's reserved for root */
		errno = EPERM;
		return -1;
	}

	/* Get the DirectHW driver service */
	iokit_uc = IOServiceGetMatchingService(kIOMasterPortDefault,
					IOServiceMatching("DirectHWService"));

	if (!iokit_uc) {
		fprintf(stderr, "DirectHW.kext not loaded.\n");
		errno = ENOSYS;
		return -1;
	}

	/* Create an instance */
	err = IOServiceOpen(iokit_uc, mach_task_self(), 0, &connect);

	/* Should not go further if error with service open */
	if (err != KERN_SUCCESS) {
		fprintf(stderr, "Could not create DirectHW instance.\n");
		errno = ENOSYS;
		return -1;
	}

	return 0;
}

static void darwin_cleanup(void)
{
	IOServiceClose(connect);
}

static int darwin_ioread(int pos, unsigned char * buf, int len)
{

	kern_return_t err;
	size_t dataInLen = sizeof(iomem_t);
	size_t dataOutLen = sizeof(iomem_t);
	iomem_t in;
	iomem_t out;
	UInt32 tmpdata;

	in.width = len;
	in.offset = pos;

	if (len > 4)
		return 1;

#if !defined(__LP64__) && defined(WANT_OLD_API)
	/* Check if OSX 10.5 API is available */
	if (IOConnectCallStructMethod != NULL) {
#endif
		err = IOConnectCallStructMethod(connect, kReadIO, &in, dataInLen, &out, &dataOutLen);
#if !defined(__LP64__) && defined(WANT_OLD_API)
	} else {
		/* Use old API */
		err = IOConnectMethodStructureIStructureO(connect, kReadIO, dataInLen, &dataOutLen, &in, &out);
	}
#endif

	if (err != KERN_SUCCESS)
		return 1;

	tmpdata = out.data;

	switch (len) {
	case 1:
		memcpy(buf, &tmpdata, 1);
		break;
	case 2:
		memcpy(buf, &tmpdata, 2);
		break;
	case 4:
		memcpy(buf, &tmpdata, 4);
		break;
	}

	return 0;
}

static int darwin_iowrite(int pos, unsigned char * buf, int len)
{
	kern_return_t err;
	size_t dataInLen = sizeof(iomem_t);
	size_t dataOutLen = sizeof(iomem_t);
	iomem_t in;
	iomem_t out;

	in.width = len;
	in.offset = pos;
	memcpy(&in.data, buf, len);

	if (len > 4)
		return 1;

#if !defined(__LP64__) && defined(WANT_OLD_API)
	/* Check if OSX 10.5 API is available */
	if (IOConnectCallStructMethod != NULL) {
#endif
		err = IOConnectCallStructMethod(connect, kWriteIO, &in, dataInLen, &out, &dataOutLen);
#if !defined(__LP64__) && defined(WANT_OLD_API)
	} else {
		/* Use old API */
		err = IOConnectMethodStructureIStructureO(connect, kWriteIO, dataInLen, &dataOutLen, &in, &out);
	}
#endif

	if (err != KERN_SUCCESS)
		return 1;

	return 0;
}


/* Compatibility interface */

unsigned char inb(unsigned short addr)
{
	unsigned char ret;
	darwin_ioread(addr, &ret, 1);
	return ret;
}

unsigned short inw(unsigned short addr)
{
	unsigned short ret;
	darwin_ioread(addr, (unsigned char *)&ret, 2);
	return ret;
}

unsigned int inl(unsigned short addr)
{
	unsigned int ret;
	darwin_ioread(addr, (unsigned char *)&ret, 4);
	return ret;
}

void outb(unsigned char val, unsigned short addr)
{
	darwin_iowrite(addr, &val, 1);
}

void outw(unsigned short val, unsigned short addr)
{
	darwin_iowrite(addr, (unsigned char *)&val, 2);
}

void outl(unsigned int val, unsigned short addr)
{
	darwin_iowrite(addr, (unsigned char *)&val, 4);
}

int iopl(int level __attribute__((unused)))
{
	atexit(darwin_cleanup);
	return darwin_init();
}

void *map_physical(uint64_t phys_addr, size_t len)
{
	kern_return_t err;
#if __LP64__
	mach_vm_address_t addr;
	mach_vm_size_t size;
#else
        vm_address_t addr;
        vm_size_t size;
#endif
	size_t dataInLen = sizeof(map_t);
	size_t dataOutLen = sizeof(map_t);
	map_t in, out;

	in.addr = phys_addr;
	in.size = len;

	if (debug)
		fprintf(stderr, "map_phys: phys %08"PRIx64", %08zx\n", phys_addr, len);

#if !defined(__LP64__) && defined(WANT_OLD_API)
	/* Check if OSX 10.5 API is available */
	if (IOConnectCallStructMethod != NULL) {
#endif
		err = IOConnectCallStructMethod(connect, kPrepareMap, &in, dataInLen, &out, &dataOutLen);
#if !defined(__LP64__) && defined(WANT_OLD_API)
	} else {
		/* Use old API */
		err = IOConnectMethodStructureIStructureO(connect, kPrepareMap, dataInLen, &dataOutLen, &in, &out);
	}
#endif

	if (err != KERN_SUCCESS) {
		fprintf(stderr, "\nError(kPrepareMap): system 0x%x subsystem 0x%x code 0x%x ",
				err_get_system(err), err_get_sub(err), err_get_code(err));

		fprintf(stderr, "physical 0x%08"PRIx64"[0x%zx]\n", phys_addr, len);

		switch (err_get_code(err)) {
		case 0x2c2: fprintf(stderr, "Invalid argument.\n"); errno = EINVAL; break;
		case 0x2cd: fprintf(stderr, "Device not open.\n"); errno = ENOENT; break;
		}

		return MAP_FAILED;
	}

        err = IOConnectMapMemory(connect, 0, mach_task_self(),
			&addr, &size, kIOMapAnywhere | kIOMapInhibitCache);

	/* Now this is odd; The above connect seems to be unfinished at the
	 * time the function returns. So wait a little bit, or the calling
	 * program will just segfault. Bummer. Who knows a better solution?
	 */
	usleep(1000);

	if (err != KERN_SUCCESS) {
		fprintf(stderr, "\nError(IOConnectMapMemory): system 0x%x subsystem 0x%x code 0x%x ",
				err_get_system(err), err_get_sub(err), err_get_code(err));

		fprintf(stderr, "physical 0x%08"PRIx64"[0x%zx]\n", phys_addr, len);

		switch (err_get_code(err)) {
		case 0x2c2: fprintf(stderr, "Invalid argument.\n"); errno = EINVAL; break;
		case 0x2cd: fprintf(stderr, "Device not open.\n"); errno = ENOENT; break;
		}

		return MAP_FAILED;
	}

	if (debug)
		fprintf(stderr, "map_phys: virt %08"PRIx64", %08zx\n", addr, (size_t) size);

        return (void *)addr;
}

void unmap_physical(void *virt_addr __attribute__((unused)), size_t len __attribute__((unused)))
{
	// Nut'n Honey
}

static int current_logical_cpu = 0;

msr_t rdmsr(int addr)
{
	kern_return_t err;
	size_t dataInLen = sizeof(msrcmd_t);
	size_t dataOutLen = sizeof(msrcmd_t);
	msrcmd_t in, out;
	msr_t ret = { INVALID_MSR_HI, INVALID_MSR_LO };

	in.core = current_logical_cpu;
	in.index = addr;

#if !defined(__LP64__) && defined(WANT_OLD_API)
	/* Check if OSX 10.5 API is available */
	if (IOConnectCallStructMethod != NULL) {
#endif
		err = IOConnectCallStructMethod(connect, kReadMSR, &in, dataInLen, &out, &dataOutLen);
#if !defined(__LP64__) && defined(WANT_OLD_API)
	} else {
		/* Use old API */
		err = IOConnectMethodStructureIStructureO(connect, kReadMSR, dataInLen, &dataOutLen, &in, &out);
	}
#endif

	if (err != KERN_SUCCESS)
		return ret;

	ret.lo = out.lo;
	ret.hi = out.hi;

	return ret;
}

int wrmsr(int addr, msr_t msr)
{
	kern_return_t err;
	size_t dataInLen = sizeof(msrcmd_t);
	size_t dataOutLen = sizeof(msrcmd_t);
	msrcmd_t in, out;

	in.core = current_logical_cpu;
	in.index = addr;
	in.lo = msr.lo;
	in.hi = msr.hi;

#if !defined(__LP64__) && defined(WANT_OLD_API)
	/* Check if OSX 10.5 API is available */
	if (IOConnectCallStructMethod != NULL) {
#endif
		err = IOConnectCallStructMethod(connect, kWriteMSR, &in, dataInLen, &out, &dataOutLen);
#if !defined(__LP64__) && defined(WANT_OLD_API)
	} else {
		/* Use old API */
		err = IOConnectMethodStructureIStructureO(connect, kWriteMSR, dataInLen, &dataOutLen, &in, &out);
	}
#endif

	if (err != KERN_SUCCESS)
		return 1;

	return 0;
}

int logical_cpu_select(int cpu)
{
	current_logical_cpu = cpu;
	return 0;
}

