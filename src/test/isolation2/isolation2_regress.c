#include "postgres.h"
#include "funcapi.h"
#include "tablefuncapi.h"
#include "miscadmin.h"

#include "access/aocssegfiles.h"
#include "access/heapam.h"
#include "storage/bufmgr.h"
#include "utils/numeric.h"

PG_MODULE_MAGIC;

extern void flush_relation_buffers(PG_FUNCTION_ARGS);

/* numeric upgrade tests */
extern Datum convertNumericToGPDB4(PG_FUNCTION_ARGS);
extern Datum setAOFormatVersion(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(flush_relation_buffers);
void
flush_relation_buffers(PG_FUNCTION_ARGS)
{
	Oid relid = PG_GETARG_OID(0);
	Relation r = heap_open(relid, AccessShareLock);
	FlushRelationBuffers(r);
	heap_close(r, AccessShareLock);
}

/* Mangle a numeric Datum to match the GPDB4 (Postgres 8.2) format. */
PG_FUNCTION_INFO_V1(convertNumericToGPDB4);
Datum
convertNumericToGPDB4(PG_FUNCTION_ARGS)
{
	Datum	numeric = PG_GETARG_DATUM(0);
	void   *varlena = DatumGetPointer(numeric);
	void   *newvarlena;
	char  *newdata;
	uint16	tmp;

	/*
	 * Postgres 9.1 added the short format to numeric types. To convert to 8.2,
	 * we must force the use of the long format. This has the useful side effect
	 * of making a copy for us that we can scratch over.
	 */
	newvarlena = numeric_force_long_format(DatumGetNumeric(numeric));
	if (newvarlena == varlena)
	{
		/* Already in long format; we have to manually copy ourselves. */
		size_t datalen = VARSIZE_ANY(varlena);

		newvarlena = palloc(datalen);
		memcpy(newvarlena, varlena, datalen);
	}

	newdata = VARDATA_ANY(newvarlena);

	memcpy(&tmp, &newdata[0], 2);
	memcpy(&newdata[0], &newdata[2], 2);
	memcpy(&newdata[2], &tmp, 2);

	PG_RETURN_POINTER(newvarlena);
}

/* Override the format version for an AO/CO table. */
PG_FUNCTION_INFO_V1(setAOFormatVersion);
Datum
setAOFormatVersion(PG_FUNCTION_ARGS)
{
	Oid				aosegrelid = PG_GETARG_OID(0);
	int16			formatversion = PG_GETARG_INT16(1);
	bool			columnoriented = PG_GETARG_BOOL(2);
	Relation		aosegrel;
	HeapScanDesc	scan;
	HeapTuple		oldtuple;
	HeapTuple		newtuple;
	TupleDesc		tupdesc;
	Datum		   *values;
	bool		   *isnull;
	bool		   *replace;
	int				natts;
	int				formatversion_attnum;

	/*
	 * The segment descriptor's rowtype is different for row- and
	 * column-oriented tables.
	 */
	natts = columnoriented ? Natts_pg_aocsseg : Natts_pg_aoseg;
	formatversion_attnum = columnoriented ? Anum_pg_aocs_formatversion :
											Anum_pg_aoseg_formatversion;

	/* Create our replacement attribute. */
	values = palloc(sizeof(Datum) * natts);
	isnull = palloc0(sizeof(bool) * natts);
	replace = palloc0(sizeof(bool) * natts);

	values[formatversion_attnum - 1] = Int16GetDatum(formatversion);
	replace[formatversion_attnum - 1] = true;

	/* Open the segment descriptor table. */
	aosegrel = heap_open(aosegrelid, RowExclusiveLock);

	if (!RelationIsValid(aosegrel))
		elog(ERROR, "could not open aoseg table with OID %d", (int) aosegrelid);

	tupdesc = RelationGetDescr(aosegrel);

	/* Try to sanity-check a little bit... */
	if (tupdesc->natts != natts)
		elog(ERROR, "table with OID %d does not appear to be an aoseg table",
			 (int) aosegrelid);

	/* Scan over the rows, overriding the formatversion for each entry. */
	scan = heap_beginscan(aosegrel, SnapshotNow, 0, NULL);
	while ((oldtuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
	{
		newtuple = heap_modify_tuple(oldtuple, tupdesc, values, isnull, replace);
		simple_heap_update(aosegrel, &oldtuple->t_self, newtuple);
		pfree(newtuple);
	}
	heap_endscan(scan);

	/* Done. Clean up. */
	heap_close(aosegrel, RowExclusiveLock);

	pfree(replace);
	pfree(isnull);
	pfree(values);

	PG_RETURN_BOOL(true);
}
