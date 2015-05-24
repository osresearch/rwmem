#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
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

	const uint8_t * const buf = map_physical(addr, map_len);
	if (buf == NULL)
	{
		perror("mmap");
		return EXIT_FAILURE;
	}


	size_t offset = 0;
	while (offset < len)
	{
		ssize_t rc = write(STDOUT_FILENO, buf + page_offset + offset, len - offset);
		if (rc <= 0)
		{
			perror("write");
			return EXIT_FAILURE;
		}

		offset += rc;
	}

	return EXIT_SUCCESS;
}
