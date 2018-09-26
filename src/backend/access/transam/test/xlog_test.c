#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../xlog.c"

void
test_GetXLogCleanUpToForMaster(void **state)
{
	XLogRecPtr pointer = {0};
	XLogSegNo  actual_logSegNo = 0;

	GpIdentity.segindex = MASTER_CONTENT_ID;

	will_return(WalSndCtlGetXLogCleanUpTo, &pointer);

	/*
	 * Make the KeepLogSeg return immediately
	 */
	wal_keep_segments = 0;

	GetXLogCleanUpTo(pointer, &actual_logSegNo);
}

void
test_GetXLogCleanUpToForSegments(void **state)
{
	XLogRecPtr pointer = {0};
	XLogSegNo  actual_logSegNo = 0;

	GpIdentity.segindex = 0; // not master

	will_return(WalSndCtlGetXLogCleanUpTo, &pointer);

	/*
	 * Make the KeepLogSeg return immediately
	 */
	wal_keep_segments = 0;

	GetXLogCleanUpTo(pointer, &actual_logSegNo);
}

void
test_KeepLogSeg(void **state)
{
	XLogRecPtr recptr;
	XLogSegNo  _logSegNo;

	/*
	 * 64 segments per Xlog logical file.
	 * Configuring (3, 2), 3 log files and 2 segments to keep (3*64 + 2).
	 */
	wal_keep_segments = 194;

	/************************************************
	 * Current Delete greater than what keep wants,
	 * so, delete offset should get updated
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 3 * XLogSegmentsPerXLogId + 10;

	/*
	 * Current xlog location (4, 1)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 4) << 32 | (XLogSegSize * 1);

	KeepLogSeg(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 63);
	/************************************************/


	/************************************************
	 * Current Delete smaller than what keep wants,
	 * so, delete offset should NOT get updated
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 60;

	/*
	 * Current xlog location (4, 1)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 4) << 32 | (XLogSegSize * 1);

	KeepLogSeg(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 60);
	/************************************************/


	/************************************************
	 * Current Delete smaller than what keep wants,
	 * so, delete offset should NOT get updated
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 1 * XLogSegmentsPerXLogId + 60;

	/*
	 * Current xlog location (5, 8)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 5) << 32 | (XLogSegSize * 8);

	KeepLogSeg(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 1 * XLogSegmentsPerXLogId + 60);
	/************************************************/

	/************************************************
	 * UnderFlow case, curent is lower than keep
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 2 * XLogSegmentsPerXLogId + 1;

	/*
	 * Current xlog location (3, 1)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 3) << 32 | (XLogSegSize * 1);

	KeepLogSeg(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 1);
	/************************************************/

	/************************************************
	 * One more simple scenario of updating delete offset
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 2 * XLogSegmentsPerXLogId + 8;

	/*
	 * Current xlog location (5, 8)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 5) << 32 | (XLogSegSize * 8);

	KeepLogSeg(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 2*XLogSegmentsPerXLogId + 6);
	/************************************************/

	/************************************************
	 * Do nothing if wal_keep_segments is not positive
	 ***********************************************/
	/* Current Delete pointer */
	wal_keep_segments = 0;
	_logSegNo = 9 * XLogSegmentsPerXLogId + 45;

	KeepLogSeg(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 9*XLogSegmentsPerXLogId + 45);

	wal_keep_segments = -1;

	KeepLogSeg(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 1);
	/************************************************/
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_KeepLogSeg)
		, unit_test(test_GetXLogCleanUpToForMaster)
		, unit_test(test_GetXLogCleanUpToForSegments)
	};
	return run_tests(tests);
}
