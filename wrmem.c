/** \file
 * Write arbitrary physical memory locations.
 *
 * WARNING: This is a dangerous tool.
 * It can/will crash or corrupt your system if you write
 * to the wrong locations.
 *
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
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

	uintptr_t addr = strtoul(argv[1], NULL, 0);
	size_t len = strtoul(argv[2], NULL, 0);

	// align to a page boundary
	const uintptr_t page_mask = 0xFFF;
	uintptr_t page_offset = addr & page_mask;
	addr &= ~page_mask;

	size_t map_len = (len + page_offset + page_mask) & ~page_mask;

	if (iopl(0) < 0)
	{
		perror("iopl");
		return EXIT_FAILURE;
	}

	volatile uint8_t * const buf = map_physical(addr, map_len);
	if (buf == NULL)
	{
		perror("mmap");
		return EXIT_FAILURE;
	}

	uint8_t * const inbuf = calloc(1, len);
	if (!inbuf)
	{
		perror("malloc");
		return EXIT_FAILURE;
	}

	size_t offset = 0;
	while (offset < len)
	{
		ssize_t rc = read(STDIN_FILENO, inbuf + offset, len - offset);
		if (rc <= 0)
		{
			perror("read");
			return EXIT_FAILURE;
		}

		offset += rc;
	}

	for (size_t i = 0 ; i < len ; i++)
		buf[i+page_offset] = inbuf[i];

	return EXIT_SUCCESS;
}
