/*-------------------------------------------------------------------------
 *
 * conversioncmds.h
 *	  prototypes for conversioncmds.c.
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/commands/conversioncmds.h,v 1.18 2009/01/01 17:23:58 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef CONVERSIONCMDS_H
#define CONVERSIONCMDS_H

#include "nodes/parsenodes.h"

extern void CreateConversionCommand(CreateConversionStmt *parsetree);
extern void DropConversionsCommand(DropStmt *drop);
extern void RenameConversion(List *name, const char *newname);
extern void AlterConversionOwner(List *name, Oid newOwnerId);
extern void AlterConversionOwner_oid(Oid conversionOid, Oid newOwnerId);
extern void AlterConversionNamespace(List *name, const char *newschema);
extern Oid	AlterConversionNamespace_oid(Oid convOid, Oid newNspOid);

#endif   /* CONVERSIONCMDS_H */
