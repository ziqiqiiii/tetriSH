#include "coredb_internal.h"

#include <stdlib.h>
#include <unistd.h>

/* Builds the header for seq_no = db->next_seq_no, writes header + payload,
 * flushes to the OS and fsyncs to disk, then advances next_seq_no. Caller
 * already holds db->mutex (see plan's write-path pseudocode). */
int	coredb_storage_append_record(t_coredb *db, uint16_t event_type,
		uint32_t payload_len, const void *payload, uint64_t *out_seq_no)
{
	t_coredb_record_header	hdr;
	uint64_t				seq_no;

	if (db == NULL || db->file == NULL)
		return (EXIT_FAILURE);
	seq_no = db->next_seq_no;
	coredb_record_header_build(&hdr, event_type, seq_no, payload_len,
		payload);
	if (fwrite(&hdr, sizeof(hdr), 1, db->file) != 1)
		return (EXIT_FAILURE);
	if (payload_len > 0
		&& fwrite(payload, 1, payload_len, db->file) != payload_len)
		return (EXIT_FAILURE);
	if (fflush(db->file) != 0)
		return (EXIT_FAILURE);
	if (fsync(fileno(db->file)) != 0)
		return (EXIT_FAILURE);
	db->next_seq_no = seq_no + 1;
	if (out_seq_no != NULL)
		*out_seq_no = seq_no;
	return (EXIT_SUCCESS);
}

/* Reads exactly one record at the current file position. Distinguishes a
 * clean end of file (nothing left to read) from a truncated trailing
 * record (partial header or payload — crash mid-write) so replay can stop
 * and warn instead of misreading garbage as data. Does not check the
 * payload CRC; that is the caller's job via coredb_record_payload_check. */
t_coredb_io_status	coredb_storage_read_record(FILE *file,
		t_coredb_record_header *out_hdr, void *payload_buf,
		size_t payload_buf_cap)
{
	size_t	got;

	if (file == NULL || out_hdr == NULL)
		return (COREDB_IO_BAD_HEADER);
	got = fread(out_hdr, 1, sizeof(*out_hdr), file);
	if (got == 0)
		return (COREDB_IO_EOF);
	if (got != sizeof(*out_hdr))
		return (COREDB_IO_TRUNCATED);
	if (coredb_record_header_check(out_hdr) != EXIT_SUCCESS)
		return (COREDB_IO_BAD_HEADER);
	if (out_hdr->payload_len > payload_buf_cap)
		return (COREDB_IO_BAD_HEADER);
	if (out_hdr->payload_len == 0)
		return (COREDB_IO_OK);
	got = fread(payload_buf, 1, out_hdr->payload_len, file);
	if (got != out_hdr->payload_len)
		return (COREDB_IO_TRUNCATED);
	return (COREDB_IO_OK);
}
