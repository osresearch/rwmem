/** \file
 * Read arbitrary physical memory.
 *
 * This is not as dangerous as wrmem, but you should still be careful!
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include "DirectHW.h"

int
main(
	int argc,
	char ** argv
)
{
	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s phys-address len\n", argv[0]);
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

	const uint8_t * const buf = map_physical(map_addr, map_len);
	if (buf == NULL)
	{
		perror("mmap");
		return EXIT_FAILURE;
	}


	size_t offset = 0;
	while (offset < len)
	{
		size_t chunk = 0x10000;
		if (chunk > len - offset)
			chunk = len - offset;

		const void * const virt_addr = buf + page_offset + offset;
		const uintptr_t phys_addr = map_addr + page_offset + offset;

		fprintf(stderr, "%016"PRIxPTR"\n", phys_addr);

		ssize_t rc = write(STDOUT_FILENO, virt_addr, chunk);
		if (rc <= 0)
		{
			perror("write");
			return EXIT_FAILURE;
		}

		offset += rc;
	}

	return EXIT_SUCCESS;
}
