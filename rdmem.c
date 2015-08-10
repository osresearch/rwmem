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
#include <unistd.h>
#include "DirectHW.h"

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
	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s [-x] phys-address len\n", argv[0]);
		return EXIT_FAILURE;
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


	uint32_t out_buf[0x10000/4];

	size_t offset = 0;
	while (offset < len)
	{
		size_t chunk = sizeof(out_buf);
		if (chunk > len - offset)
			chunk = len - offset;

		const void * const virt_addr = buf + offset;
		const uintptr_t phys_addr = addr + offset;

		fprintf(stderr, "%016"PRIxPTR"\n", phys_addr);
		quad_memcpy(out_buf, virt_addr, chunk);

		ssize_t rc = write(STDOUT_FILENO, out_buf, chunk);
		if (rc <= 0)
		{
			perror("write");
			return EXIT_FAILURE;
		}

		offset += rc;
	}

	return EXIT_SUCCESS;
}
