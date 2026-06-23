#include "coredb_internal.h"

#include <stdlib.h>
#include <time.h>

#define COREDB_CRC32_POLY 0xEDB88320u

/* Standard reflected CRC-32 (zlib polynomial), bit-at-a-time. */
uint32_t coredb_crc32(const void *data, size_t len)
{
	const uint8_t	*bytes;
	uint32_t		crc;
	size_t			i;
	int				bit;

	bytes = data;
	crc = 0xFFFFFFFFu;
	i = 0;
	while (i < len)
	{
		crc ^= bytes[i];
		bit = 0;
		while (bit < 8)
		{
			if (crc & 1u)
				crc = (crc >> 1) ^ COREDB_CRC32_POLY;
			else
				crc >>= 1;
			bit++;
		}
		i++;
	}
	return (~crc);
}

// returns the current time in milliseconds since the epoch (1970-01-01 00:00:00 UTC)
uint64_t coredb_now_ms(void)
{
	struct timespec	ts;

	if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
		return (0);
	return ((uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000);
}

// takes a empty header and fills in all the fields
// after this, hdr is a complete and ready to write header
void coredb_record_header_build(t_coredb_record_header *hdr,
		uint16_t event_type, uint64_t seq_no,
		uint32_t payload_len, const void *payload)
{
	hdr->magic = COREDB_RECORD_MAGIC;
	hdr->version = COREDB_RECORD_VERSION;
	hdr->event_type = event_type;
	hdr->seq_no = seq_no;
	hdr->timestamp_ms = coredb_now_ms();
	hdr->payload_len = payload_len;
	hdr->payload_crc = coredb_crc32(payload, payload_len);
}

// it takes a header and checks that it is valid (magic, version, etc)
// returns EXIT_SUCCESS if the header is valid, EXIT_FAILURE otherwise
int	coredb_record_header_check(const t_coredb_record_header *hdr)
{
	if (hdr->magic != COREDB_RECORD_MAGIC)
		return (EXIT_FAILURE);
	if (hdr->version != COREDB_RECORD_VERSION)
		return (EXIT_FAILURE);
	return (EXIT_SUCCESS);
}

// it takes a header and payload and checks that the payload CRC is valid
// returns EXIT_SUCCESS if the payload is valid, EXIT_FAILURE otherwise
int	coredb_record_payload_check(const t_coredb_record_header *hdr,
		const void *payload)
{
	if (coredb_crc32(payload, hdr->payload_len) != hdr->payload_crc)
		return (EXIT_FAILURE);
	return (EXIT_SUCCESS);
}
