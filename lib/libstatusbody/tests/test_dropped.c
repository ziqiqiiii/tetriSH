#include "statusbody.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>

void	test_dropped_encode_matches_the_use_case(void)
{
	t_body_dropped	d;
	char			out[64];
	int				n;

	d.dropped = 152;
	n = body_dropped_encode(&d, out, sizeof(out));
	assert(n > 0);
	assert(strcmp(out, "dropped 152\n") == 0);
	assert((size_t)n == strlen(out));
	printf("PASS test_dropped_encode_matches_the_use_case\n");
}

void	test_dropped_round_trip_including_zero(void)
{
	t_body_dropped	d;
	t_body_dropped	back;
	char			out[64];
	int				n;

	d.dropped = 0;
	n = body_dropped_encode(&d, out, sizeof(out));
	assert(n > 0);
	assert(body_dropped_decode(out, (size_t)n, &back) == 0);
	assert(back.dropped == 0); // a healthy server is not an empty body
	d.dropped = UINT64_MAX;
	n = body_dropped_encode(&d, out, sizeof(out));
	assert(n > 0);
	assert(body_dropped_decode(out, (size_t)n, &back) == 0);
	assert(back.dropped == UINT64_MAX);
	printf("PASS test_dropped_round_trip_including_zero\n");
}

void	test_dropped_rejects_bad_input(void)
{
	t_body_dropped	d;
	t_body_dropped	back;
	char			out[64];
	const char		*wrong_key;
	const char		*not_a_number;
	const char		*trailing;

	d.dropped = 1;
	assert(body_dropped_encode(NULL, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(body_dropped_encode(&d, NULL, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(body_dropped_encode(&d, out, 4) == -1);
	assert(errno == ERANGE);
	assert(body_dropped_decode(NULL, 0, &back) == -1);
	assert(errno == EINVAL);
	assert(body_dropped_decode("", 0, &back) == -1);
	assert(errno == EBADMSG); // an empty body is not zero dropped
	wrong_key = "rejected 12\n";
	assert(body_dropped_decode(wrong_key, strlen(wrong_key), &back) == -1);
	assert(errno == EBADMSG);
	not_a_number = "dropped lots\n";
	assert(body_dropped_decode(not_a_number, strlen(not_a_number),
			&back) == -1);
	assert(errno == EBADMSG);
	trailing = "dropped 12\ndropped 13\n";
	assert(body_dropped_decode(trailing, strlen(trailing), &back) == -1);
	assert(errno == EBADMSG);
	printf("PASS test_dropped_rejects_bad_input\n");
}

int	main(void)
{
	test_dropped_encode_matches_the_use_case();
	test_dropped_round_trip_including_zero();
	test_dropped_rejects_bad_input();
	return (0);
}
