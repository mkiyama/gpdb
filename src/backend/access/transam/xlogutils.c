/*-------------------------------------------------------------------------
 *
 * xlogutils.c
 *
 * PostgreSQL transaction log manager utility routines
 *
 * This file contains support routines that are used by XLOG replay functions.
 * None of this code is used during normal system operation.
 *
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/backend/access/transam/xlogutils.c,v 1.57 2008/07/13 20:45:47 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "access/xlogutils.h"
#include "storage/bufmgr.h"
#include "storage/smgr.h"
#include "utils/hsearch.h"
#include "utils/rel.h"

#include "cdb/cdbpersistentrecovery.h"
#include "cdb/cdbpersistenttablespace.h"


/*
 * During XLOG replay, we may see XLOG records for incremental updates of
 * pages that no longer exist, because their relation was later dropped or
 * truncated.  (Note: this is only possible when full_page_writes = OFF,
 * since when it's ON, the first reference we see to a page should always
 * be a full-page rewrite not an incremental update.)  Rather than simply
 * ignoring such records, we make a note of the referenced page, and then
 * complain if we don't actually see a drop or truncate covering the page
 * later in replay.
 */
typedef struct xl_invalid_page_key
{
	RelFileNode node;			/* the relation */
	BlockNumber blkno;			/* the page */
} xl_invalid_page_key;

typedef struct xl_invalid_page
{
	xl_invalid_page_key key;	/* hash key ... must be first */
	bool		present;		/* page existed but contained zeroes */
} xl_invalid_page;

static HTAB *invalid_page_tab = NULL;


/* Log a reference to an invalid page */
static void
log_invalid_page(RelFileNode node, BlockNumber blkno, bool present)
{
	xl_invalid_page_key key;
	xl_invalid_page *hentry;
	bool		found;

	/*
	 * Log references to invalid pages at DEBUG1 level.  This allows some
	 * tracing of the cause (note the elog context mechanism will tell us
	 * something about the XLOG record that generated the reference).
	 */
	if (present)
	{
		elog(DEBUG1, "page %u of relation %u/%u/%u is uninitialized",
			 blkno, node.spcNode, node.dbNode, node.relNode);
		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
				 "log_invalid_page: page %u of relation %u/%u/%u is uninitialized",
				 blkno,
				 node.spcNode,
				 node.dbNode,
				 node.relNode);
	}
	else
	{
		elog(DEBUG1, "page %u of relation %u/%u/%u does not exist",
			 blkno, node.spcNode, node.dbNode, node.relNode);
		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
				 "log_invalid_page: page %u of relation %u/%u/%u does not exist",
				 blkno,
				 node.spcNode,
				 node.dbNode,
				 node.relNode);
	}


	if (invalid_page_tab == NULL)
	{
		/* create hash table when first needed */
		HASHCTL		ctl;

		memset(&ctl, 0, sizeof(ctl));
		ctl.keysize = sizeof(xl_invalid_page_key);
		ctl.entrysize = sizeof(xl_invalid_page);
		ctl.hash = tag_hash;

		invalid_page_tab = hash_create("XLOG invalid-page table",
									   100,
									   &ctl,
									   HASH_ELEM | HASH_FUNCTION);
	}

	/* we currently assume xl_invalid_page_key contains no padding */
	key.node = node;
	key.blkno = blkno;
	hentry = (xl_invalid_page *)
		hash_search(invalid_page_tab, (void *) &key, HASH_ENTER, &found);

	if (!found)
	{
		/* hash_search already filled in the key */
		hentry->present = present;
	}
	else
	{
		/* repeat reference ... leave "present" as it was */
	}
}

/* Forget any invalid pages >= minblkno, because they've been dropped */
static void
forget_invalid_pages(RelFileNode node, BlockNumber minblkno)
{
	HASH_SEQ_STATUS status;
	xl_invalid_page *hentry;

	if (invalid_page_tab == NULL)
		return;					/* nothing to do */

	hash_seq_init(&status, invalid_page_tab);

	while ((hentry = (xl_invalid_page *) hash_seq_search(&status)) != NULL)
	{
		if (RelFileNodeEquals(hentry->key.node, node) &&
			hentry->key.blkno >= minblkno)
		{
			elog(DEBUG2, "page %u of relation %u/%u/%u has been dropped",
				 hentry->key.blkno, hentry->key.node.spcNode,
				 hentry->key.node.dbNode, hentry->key.node.relNode);
			if (Debug_persistent_recovery_print)
				elog(PersistentRecovery_DebugPrintLevel(), 
					 "forget_invalid_pages: page %u of relation %u/%u/%u has been dropped",
					 hentry->key.blkno,
					 hentry->key.node.spcNode,
					 hentry->key.node.dbNode, 
					 hentry->key.node.relNode);

			if (hash_search(invalid_page_tab,
							(void *) &hentry->key,
							HASH_REMOVE, NULL) == NULL)
				elog(ERROR, "hash table corrupted");
		}
	}
}

#if 0 /* XLOG_DBASE_DROP is not used in GPDB so this function will never get called */
/* Forget any invalid pages in a whole database */
static void
forget_invalid_pages_db(Oid tblspc, Oid dbid)
{
	HASH_SEQ_STATUS status;
	xl_invalid_page *hentry;

	if (invalid_page_tab == NULL)
		return;					/* nothing to do */

	hash_seq_init(&status, invalid_page_tab);

	while ((hentry = (xl_invalid_page *) hash_seq_search(&status)) != NULL)
	{
		if ((!OidIsValid(tblspc) || hentry->key.node.spcNode == tblspc) &&
			hentry->key.node.dbNode == dbid)
		{
			elog(DEBUG2, "page %u of relation %u/%u/%u has been dropped",
				 hentry->key.blkno, hentry->key.node.spcNode,
				 hentry->key.node.dbNode, hentry->key.node.relNode);
			if (Debug_persistent_recovery_print)
				elog(PersistentRecovery_DebugPrintLevel(), 
					 "forget_invalid_pages_db: %u of relation %u/%u/%u has been dropped",
					 hentry->key.blkno,
					 hentry->key.node.spcNode,
					 hentry->key.node.dbNode, 
					 hentry->key.node.relNode);

			if (hash_search(invalid_page_tab,
							(void *) &hentry->key,
							HASH_REMOVE, NULL) == NULL)
				elog(ERROR, "hash table corrupted");
		}
	}
}
#endif

#ifdef USE_SEGWALREP
/* Forget an invalid AO/AOCO segment file */
static void
forget_invalid_segment_file(RelFileNode rnode, uint32 segmentFileNum)
{
	xl_invalid_page_key key;
	bool		found;

	if (invalid_page_tab == NULL)
		return;					/* nothing to do */

	key.node = rnode;
	key.blkno = segmentFileNum;
	hash_search(invalid_page_tab,
				(void *) &key,
				HASH_FIND, &found);
	if (!found)
		return;

	if (hash_search(invalid_page_tab,
					(void *) &key,
					HASH_REMOVE, &found) == NULL)
		elog(ERROR, "hash table corrupted");

	elog(Debug_persistent_recovery_print ? PersistentRecovery_DebugPrintLevel() : DEBUG2,
		 "segmentfile %u of relation %u/%u/%u has been dropped",
		 key.blkno, key.node.spcNode,
		 key.node.dbNode, key.node.relNode);
}
#endif

/* Complain about any remaining invalid-page entries */
void
XLogCheckInvalidPages(void)
{
	HASH_SEQ_STATUS status;
	xl_invalid_page *hentry;
	bool		foundone = false;

	if (invalid_page_tab == NULL)
		return;					/* nothing to do */

	hash_seq_init(&status, invalid_page_tab);

	/*
	 * Our strategy is to emit WARNING messages for all remaining entries and
	 * only PANIC after we've dumped all the available info.
	 */
	while ((hentry = (xl_invalid_page *) hash_seq_search(&status)) != NULL)
	{
		if (hentry->present)
			elog(WARNING, "page %u of relation %u/%u/%u was uninitialized",
				 hentry->key.blkno, hentry->key.node.spcNode,
				 hentry->key.node.dbNode, hentry->key.node.relNode);
		else
			elog(WARNING, "page %u of relation %u/%u/%u did not exist",
				 hentry->key.blkno, hentry->key.node.spcNode,
				 hentry->key.node.dbNode, hentry->key.node.relNode);
		foundone = true;
	}

	if (foundone)
		elog(PANIC, "WAL contains references to invalid pages");

	hash_destroy(invalid_page_tab);
	invalid_page_tab = NULL;
}


/*
 * XLogReadBuffer
 *		Read a page during XLOG replay
 *
 * This is functionally comparable to ReadBuffer followed by
 * LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE): you get back a pinned
 * and locked buffer.  (Getting the lock is not really necessary, since we
 * expect that this is only used during single-process XLOG replay, but
 * some subroutines such as MarkBufferDirty will complain if we don't.)
 *
 * If "init" is true then the caller intends to rewrite the page fully
 * using the info in the XLOG record.  In this case we will extend the
 * relation if needed to make the page exist, and we will not complain about
 * the page being "new" (all zeroes); in fact, we usually will supply a
 * zeroed buffer without reading the page at all, so as to avoid unnecessary
 * failure if the page is present on disk but has corrupt headers.
 *
 * If "init" is false then the caller needs the page to be valid already.
 * If the page doesn't exist or contains zeroes, we return InvalidBuffer.
 * In this case the caller should silently skip the update on this page.
 * (In this situation, we expect that the page was later dropped or truncated.
 * If we don't see evidence of that later in the WAL sequence, we'll complain
 * at the end of WAL replay.)
 */
Buffer
XLogReadBuffer(RelFileNode rnode, BlockNumber blkno, bool init)
{
	BlockNumber lastblock;
	Buffer		buffer;
	SMgrRelation smgr;

	MIRROREDLOCK_BUFMGR_MUST_ALREADY_BE_HELD;

	Assert(blkno != P_NEW);

	/* Open the relation at smgr level */
	smgr = smgropen(rnode);

	/*
	 * Create the target file if it doesn't already exist.  This lets us cope
	 * if the replay sequence contains writes to a relation that is later
	 * deleted.  (The original coding of this routine would instead suppress
	 * the writes, but that seems like it risks losing valuable data if the
	 * filesystem loses an inode during a crash.  Better to write the data
	 * until we are actually told to delete the file.)
	 */
	/* GPDB_84_MERGE_FIXME: this block of code (brought over from
	 * XLogOpenRelation) was marked to be removed.  Can we? Is it related to
	 * filerep? */
	// UNDONE: Can't remove this block of code yet until boot time calls to this routine are analyzed...
	{
		MirrorDataLossTrackingState mirrorDataLossTrackingState;
		int64 mirrorDataLossTrackingSessionNum;
		bool mirrorDataLossOccurred;
		
		// UNDONE: What about the persistent rel files table???
		// UNDONE: This condition should not occur anymore.
		// UNDONE: segmentFileNum and AO?
		mirrorDataLossTrackingState = 
					FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
													&mirrorDataLossTrackingSessionNum);
		smgrcreate(
			smgr, 
			/* relationName */ NULL,		// Ok to be NULL -- we don't know the name here.
			mirrorDataLossTrackingState,
			mirrorDataLossTrackingSessionNum,
			/* ignoreAlreadyExists */ true,
			&mirrorDataLossOccurred);
		
	}

	lastblock = smgrnblocks(smgr);

	if (blkno < lastblock)
	{
		/* page exists in file */
		buffer = ReadBufferWithoutRelcache(rnode, false, false, blkno, init);
	}
	else
	{
		/* hm, page doesn't exist in file */
		if (!init)
		{
			log_invalid_page(rnode, blkno, false);
			return InvalidBuffer;
		}
		/* OK to extend the file */
		/* we do this in recovery only - no rel-extension lock needed */
		Assert(InRecovery);
		buffer = InvalidBuffer;
		while (blkno >= lastblock)
		{
			if (buffer != InvalidBuffer)
				ReleaseBuffer(buffer);
			buffer = ReadBufferWithoutRelcache(rnode, false, false, P_NEW, false);
			lastblock++;
		}
		Assert(BufferGetBlockNumber(buffer) == blkno);
	}

	LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);

	if (!init)
	{
		/* check that page has been initialized */
		Page		page = (Page) BufferGetPage(buffer);

		if (PageIsNew(page))
		{
			UnlockReleaseBuffer(buffer);
			log_invalid_page(rnode, blkno, true);
			return InvalidBuffer;
		}
	}

	return buffer;
}

#ifdef USE_SEGWALREP
/*
 * If the AO segment file does not exist, log the relfilenode into the
 * invalid_page_table hash table using the segment file number as the
 * block number to avoid creating a new hash table.  The entry will be
 * removed if there is a following MMXLOG_REMOVE_FILE record for the
 * relfilenode.
 */
void
XLogAOSegmentFile(RelFileNode rnode, uint32 segmentFileNum)
{
	log_invalid_page(rnode, segmentFileNum, false);
}
#endif

/*
 * Struct actually returned by XLogFakeRelcacheEntry, though the declared
 * return type is Relation.
 */
typedef struct
{
	RelationData		reldata;	/* Note: this must be first */
	FormData_pg_class	pgc;
} FakeRelCacheEntryData;

typedef FakeRelCacheEntryData *FakeRelCacheEntry;

/*
 * Create a fake relation cache entry for a physical relation
 *
 * It's often convenient to use the same functions in XLOG replay as in the
 * main codepath, but those functions typically work with a relcache entry. 
 * We don't have a working relation cache during XLOG replay, but this 
 * function can be used to create a fake relcache entry instead. Only the 
 * fields related to physical storage, like rd_rel, are initialized, so the 
 * fake entry is only usable in low-level operations like ReadBuffer().
 *
 * Caller must free the returned entry with FreeFakeRelcacheEntry().
 */
Relation
CreateFakeRelcacheEntry(RelFileNode rnode)
{
	FakeRelCacheEntry fakeentry;
	Relation rel;

	/* Allocate the Relation struct and all related space in one block. */
	fakeentry = palloc0(sizeof(FakeRelCacheEntryData));
	rel = (Relation) fakeentry;

	rel->rd_rel = &fakeentry->pgc;
	rel->rd_node = rnode;

	/* GPDB_84_MERGE_FIXME: this if block was moved from the removed
	 * XLogOpenRelation(). Is this the correct place for it? What does it do?
	 */
#if 0
	/*
	 * We need to fault in the database directory on the standby.
	 */
	if (rnode.spcNode != GLOBALTABLESPACE_OID && IsStandbyMode())
	{
		char *primaryFilespaceLocation = NULL;

		char *dbPath;
		
		if (IsBuiltinTablespace(rnode.spcNode))
		{
			/*
			 * No filespace to fetch.
			 */
		}
		else
		{		
			char *mirrorFilespaceLocation = NULL;
		
			/*
			 * Investigate whether the containing directories exist to give more detail.
			 */
			PersistentTablespace_GetPrimaryAndMirrorFilespaces(
												rnode.spcNode,
												&primaryFilespaceLocation,
												&mirrorFilespaceLocation);
			if (primaryFilespaceLocation == NULL ||
				strlen(primaryFilespaceLocation) == 0)
			{
				elog(ERROR, "Empty primary filespace directory location");
			}
		
			if (mirrorFilespaceLocation != NULL)
			{
				pfree(mirrorFilespaceLocation);
				mirrorFilespaceLocation = NULL;
			}
		}
		
		dbPath = (char*)palloc(MAXPGPATH + 1);
		
		FormDatabasePath(
					dbPath,
					primaryFilespaceLocation,
					rnode.spcNode,
					rnode.dbNode);

		if (primaryFilespaceLocation != NULL)
		{
			pfree(primaryFilespaceLocation);
			primaryFilespaceLocation = NULL;
		}
		
		if (mkdir(dbPath, 0700) == 0)
		{
			if (Debug_persistent_recovery_print)
			{
				elog(PersistentRecovery_DebugPrintLevel(), 
					 "XLogOpenRelation: Re-created database directory \"%s\"",
					 dbPath);
			}
		}
		else
		{
			/*
			 * Allowed to already exist.
			 */
			if (errno != EEXIST)
			{
				elog(ERROR, "could not create database directory \"%s\": %m",
					 dbPath);
			}
			else
			{
				if (Debug_persistent_recovery_print)
				{
					elog(PersistentRecovery_DebugPrintLevel(), 
						 "XLogOpenRelation: Database directory \"%s\" already exists",
						 dbPath);
				}
			}
		}

		pfree(dbPath);
	}
#endif
		
	/* We don't know the name of the relation; use relfilenode instead */
	sprintf(RelationGetRelationName(rel), "%u", rnode.relNode);

	/*
	 * We set up the lockRelId in case anything tries to lock the dummy
	 * relation.  Note that this is fairly bogus since relNode may be
	 * different from the relation's OID.  It shouldn't really matter
	 * though, since we are presumably running by ourselves and can't have
	 * any lock conflicts ...
	 */
	rel->rd_lockInfo.lockRelId.dbId = rnode.dbNode;
	rel->rd_lockInfo.lockRelId.relId = rnode.relNode;

	rel->rd_targblock = InvalidBlockNumber;
	rel->rd_smgr = NULL;

	return rel;
}

/*
 * Free a fake relation cache entry.
 */
void
FreeFakeRelcacheEntry(Relation fakerel)
{
	pfree(fakerel);
}

/*
 * Drop a relation during XLOG replay
 *
 * This is called when the relation is about to be deleted; we need to remove
 * any open "invalid-page" records for the relation.
 */
void
XLogDropRelation(RelFileNode rnode)
{
	/* Tell smgr to forget about this relation as well */
	smgrclosenode(rnode);

	forget_invalid_pages(rnode, 0);
}

#ifdef USE_SEGWALREP
/* Drop an AO/CO segment file from the invalid_page_tab hash table */
void
XLogAODropSegmentFile(RelFileNode rnode, uint32 segmentFileNum)
{
	forget_invalid_segment_file(rnode, segmentFileNum);
}
#endif

#if 0 /* XLOG_DBASE_DROP is not used in GPDB so this function will never get called */
/*
 * Drop a whole database during XLOG replay
 *
 * As above, but for DROP DATABASE instead of dropping a single rel
 */
void
XLogDropDatabase(Oid tblspc, Oid dbid)
{
	/*
	 * This is unnecessarily heavy-handed, as it will close SMgrRelation
	 * objects for other databases as well. DROP DATABASE occurs seldom
	 * enough that it's not worth introducing a variant of smgrclose for
	 * just this purpose. XXX: Or should we rather leave the smgr entries
	 * dangling?
	 */
	smgrcloseall();

	forget_invalid_pages_db(tblspc, dbid);
}
#endif

/*
 * Truncate a relation during XLOG replay
 *
 * We need to clean up any open "invalid-page" records for the dropped pages.
 */
void
XLogTruncateRelation(RelFileNode rnode, BlockNumber nblocks)
{
	forget_invalid_pages(rnode, nblocks);
}
