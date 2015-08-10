/** \file
 * Read arbitrary physical memory.
 *
 * This is not as dangerous as wrmem, but you should still be careful!
 * For instance, attempting to read from SMRAM will cause an immediate
 * kernel panic.
 *
 * (c) 2015 Trammell Hudson
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include "DirectHW.h"

static char
printable(
 	uint8_t c
)
{
	if (isprint(c))
		return (char) c;
	return '.';
}


static void
hexdump(
	const uintptr_t base_offset,
	const uint8_t * const buf,
	const size_t len
)
{
	const size_t width = 16;

	for (size_t offset = 0 ; offset < len ; offset += width)
	{
		printf("%08"PRIxPTR":", base_offset + offset);
		for (size_t i = 0 ; i < width ; i++)
		{
			if (i + offset < len)
				printf(" %02x", buf[offset+i]);
			else
				printf("   ");
		}

		printf("  ");

		for (size_t i = 0 ; i < width ; i++)
		{
			if (i + offset < len)
				printf("%c", printable(buf[offset+i]));
			else
				printf(" ");
		}

		printf("\n");
	}
}


/*
 * Copy four bytes at a time, even if it spills over.
 * This will read from the PCI space safely, unlike mempcy().
 */
static void
quad_memcpy(
	uint32_t * const out,
	const uint32_t * const in,
	size_t len
)
{
	for (size_t i = 0 ; i < len ; i += 4)
	{
		out[i/4] = in[i/4];
	}
}


int
main(
	int argc,
	char ** argv
)
{
	int do_ascii = 0;

	if (argc != 3 && argc != 4)
	{
		fprintf(stderr, "Usage: %s [-x] phys-address len\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (strcmp(argv[1], "-x") == 0)
	{
		do_ascii = 1;
		argv++;
	}

	const uintptr_t addr = strtoul(argv[1], NULL, 0);
	const size_t len = strtoul(argv[2], NULL, 0);

	// align to a page boundary
	const uintptr_t page_mask = 0xFFF;
	const uintptr_t page_offset = addr & page_mask;
	const uintptr_t map_addr = addr & ~page_mask;

	const size_t map_len = (len + page_offset + page_mask) & ~page_mask;

	if (iopl(0) < 0)
	{
		perror("iopl");
		return EXIT_FAILURE;
	}

	const uint8_t * const map_buf = map_physical(map_addr, map_len);
	if (map_buf == NULL)
	{
		perror("mmap");
		return EXIT_FAILURE;
	}

	const uint8_t * const buf = map_buf + page_offset;

	// because the PCIe space doesn't like being probed at anything
	// other than 4-bytes at a time, we force a copy of the region
	// into a local buffer.
	void * const out_buf = calloc(1, len);
	if (!out_buf)
	{
		perror("calloc");
		return EXIT_FAILURE;
	}

	quad_memcpy(out_buf, (const void*) buf, len);

	if (do_ascii)
	{
		hexdump(addr, out_buf, len);
	} else {
		for(size_t offset = 0 ; offset < len ; )
		{
			const ssize_t rc = write(
				STDOUT_FILENO,
				out_buf + offset,
				len - offset
			);

			if (rc <= 0)
			{
				perror("write");
				return EXIT_FAILURE;
			}

			offset += rc;
		}
	}

	return EXIT_SUCCESS;
}
