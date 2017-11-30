/*-------------------------------------------------------------------------
 *
 * pg_dump.h
 *	  Common header file for the pg_dump utility
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/bin/pg_dump/pg_dump.h,v 1.145 2009/01/01 17:23:54 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_DUMP_H
#define PG_DUMP_H

#include "postgres_fe.h"
#include "pqexpbuffer.h"
#include "libpq-fe.h"

/*
 * WIN32 does not provide 64-bit off_t, but does provide the functions operating
 * with 64-bit offsets.
 */
#ifdef WIN32
#define pgoff_t __int64
#undef fseeko
#undef ftello
#ifdef WIN32_ONLY_COMPILER
#define fseeko(stream, offset, origin) _fseeki64(stream, offset, origin)
#define ftello(stream) _ftelli64(stream)
#else
#define fseeko(stream, offset, origin) fseeko64(stream, offset, origin)
#define ftello(stream) ftello64(stream)
#endif
#else
#define pgoff_t off_t
#endif

/*
 * pg_dump uses two different mechanisms for identifying database objects:
 *
 * CatalogId represents an object by the tableoid and oid of its defining
 * entry in the system catalogs.  We need this to interpret pg_depend entries,
 * for instance.
 *
 * DumpId is a simple sequential integer counter assigned as dumpable objects
 * are identified during a pg_dump run.  We use DumpId internally in preference
 * to CatalogId for two reasons: it's more compact, and we can assign DumpIds
 * to "objects" that don't have a separate CatalogId.  For example, it is
 * convenient to consider a table, its data, and its ACL as three separate
 * dumpable "objects" with distinct DumpIds --- this lets us reason about the
 * order in which to dump these things.
 */

typedef struct
{
	Oid			tableoid;
	Oid			oid;
} CatalogId;

typedef int DumpId;

/*
 * Data structures for simple lists of OIDs and strings.  The support for
 * these is very primitive compared to the backend's List facilities, but
 * it's all we need in pg_dump.
 */

typedef struct SimpleOidListCell
{
	struct SimpleOidListCell *next;
	Oid			val;
} SimpleOidListCell;

typedef struct SimpleOidList
{
	SimpleOidListCell *head;
	SimpleOidListCell *tail;
} SimpleOidList;

typedef struct SimpleStringListCell
{
	struct SimpleStringListCell *next;
	char		val[1];			/* VARIABLE LENGTH FIELD */
} SimpleStringListCell;

typedef struct SimpleStringList
{
	SimpleStringListCell *head;
	SimpleStringListCell *tail;
} SimpleStringList;

/*
 * The data structures used to store system catalog information.  Every
 * dumpable object is a subclass of DumpableObject.
 *
 * NOTE: the structures described here live for the entire pg_dump run;
 * and in most cases we make a struct for every object we can find in the
 * catalogs, not only those we are actually going to dump.	Hence, it's
 * best to store a minimal amount of per-object info in these structs,
 * and retrieve additional per-object info when and if we dump a specific
 * object.	In particular, try to avoid retrieving expensive-to-compute
 * information until it's known to be needed.  We do, however, have to
 * store enough info to determine whether an object should be dumped and
 * what order to dump in.
 */

typedef enum
{
	/* When modifying this enum, update priority tables in pg_dump_sort.c! */
	DO_NAMESPACE,
	DO_EXTENSION,
	DO_TYPE,
	DO_SHELL_TYPE,
	DO_FUNC,
	DO_AGG,
	DO_OPERATOR,
	DO_OPCLASS,
	DO_OPFAMILY,
	DO_CONVERSION,
	DO_TABLE,
	DO_ATTRDEF,
	DO_INDEX,
	DO_RULE,
	DO_TRIGGER,
	DO_CONSTRAINT,
	DO_FK_CONSTRAINT,			/* see note for ConstraintInfo */
	DO_PROCLANG,
	DO_CAST,
	DO_TABLE_DATA,
	DO_DUMMY_TYPE,
	DO_TSPARSER,
	DO_TSDICT,
	DO_TSTEMPLATE,
	DO_TSCONFIG,
	DO_FDW,
	DO_FOREIGN_SERVER,
	DO_BLOBS,
	DO_BLOB_COMMENTS,
	DO_EXTPROTOCOL,
	DO_TYPE_STORAGE_OPTIONS
} DumpableObjectType;

typedef struct _dumpableObject
{
	DumpableObjectType objType;
	CatalogId	catId;			/* zero if not a cataloged object */
	DumpId		dumpId;			/* assigned by AssignDumpId() */
	char	   *name;			/* object name (should never be NULL) */
	struct _namespaceInfo *namespace;	/* containing namespace, or NULL */
	bool		dump;			/* true if we want to dump this object */
	bool		ext_member;		/* true if object is member of extension */
	DumpId	   *dependencies;	/* dumpIds of objects this one depends on */
	int			nDeps;			/* number of valid dependencies */
	int			allocDeps;		/* allocated size of dependencies[] */
} DumpableObject;

typedef struct _namespaceInfo
{
	DumpableObject dobj;
	char	   *rolname;		/* name of owner, or empty string */
	char	   *nspacl;
} NamespaceInfo;

typedef struct _extensionInfo
{
	DumpableObject dobj;
	char 	   *namespace;		/* schema containing extension's objects */
	bool		relocatable;
	char	   *extversion;
	char	   *extconfig;		/* info about configuration tables */
	char	   *extcondition;
} ExtensionInfo;

typedef struct _typeInfo
{
	DumpableObject dobj;

	/*
	 * Note: dobj.name is the pg_type.typname entry.  format_type() might
	 * produce something different than typname
	 */
	char	   *rolname;		/* name of owner, or empty string */
	Oid			typelem;
	Oid			typrelid;
	char		typrelkind;		/* 'r', 'v', 'c', etc */
	char		typtype;		/* 'b', 'c', etc */
	bool		isArray;		/* true if auto-generated array type */
	bool		isDefined;		/* true if typisdefined */
	/* If it's a dumpable base type, we create a "shell type" entry for it */
	struct _shellTypeInfo *shellType;	/* shell-type entry, or NULL */
	/* If it's a domain, we store links to its constraints here: */
	int			nDomChecks;
	struct _constraintInfo *domChecks;
} TypeInfo;

typedef struct _typeCache
{
	DumpableObject dobj;

	Oid			typnsp;

	Oid			arraytypoid;
	char	   *arraytypname;
	Oid			arraytypnsp;
} TypeCache;

typedef struct _typeStorageOptions
{
	DumpableObject dobj;

	/*
	 * Note: dobj.name is the pg_type.typname entry.  format_type() might
	 * produce something different than typname
	 */
	char     *typnamespace;
	char     *typoptions; /* storage options */
	char     *rolname;		/* name of owner, or empty string */
} TypeStorageOptions;



typedef struct _shellTypeInfo
{
	DumpableObject dobj;

	TypeInfo   *baseType;		/* back link to associated base type */
} ShellTypeInfo;

typedef struct _funcInfo
{
	DumpableObject dobj;
	char	   *rolname;		/* name of owner, or empty string */
	Oid			lang;
	int			nargs;
	Oid		   *argtypes;
	Oid			prorettype;
	char	   *proacl;
} FuncInfo;

/* AggInfo is a superset of FuncInfo */
typedef struct _aggInfo
{
	FuncInfo	aggfn;
	/* we don't require any other fields at the moment */
} AggInfo;

typedef struct _ptcInfo
{
	DumpableObject dobj;
	char	   *ptcreadfn;
	char	   *ptcwritefn;
	char	   *ptcowner;
	char	   *ptcacl;
	bool	   ptctrusted;
	Oid		   ptcreadid;
	Oid		   ptcwriteid;
	Oid		   ptcvalidid;
} ExtProtInfo;

typedef struct _oprInfo
{
	DumpableObject dobj;
	char	   *rolname;
	Oid			oprcode;
} OprInfo;

typedef struct _opclassInfo
{
	DumpableObject dobj;
	char	   *rolname;
} OpclassInfo;

typedef struct _opfamilyInfo
{
	DumpableObject dobj;
	char	   *rolname;
} OpfamilyInfo;

typedef struct _convInfo
{
	DumpableObject dobj;
	char	   *rolname;
} ConvInfo;

typedef struct _tableInfo
{
	/*
	 * These fields are collected for every table in the database.
	 */
	DumpableObject dobj;
	char	   *rolname;		/* name of owner, or empty string */
	char	   *relacl;
	char		relkind;
	char		relstorage;
	char	   *reltablespace;	/* relation tablespace */
	char	   *reloptions;		/* options specified by WITH (...) */
	bool		hasindex;		/* does it have any indexes? */
	bool		hasrules;		/* does it have any rules? */
	bool		hastriggers;	/* does it have any triggers? */
	bool		hasoids;		/* does it have OIDs? */
	int			ncheck;			/* # of CHECK expressions */
	/* these two are set only if table is a sequence owned by a column: */
	Oid			owning_tab;		/* OID of table owning sequence */
	int			owning_col;		/* attr # of column owning sequence */

	bool		interesting;	/* true if need to collect more data */

	/*
	 * These fields are computed only if we decide the table is interesting
	 * (it's either a table to dump, or a direct parent of a dumpable table).
	 */
	int			numatts;		/* number of attributes */
	char	  **attnames;		/* the attribute names */
	char	  **atttypnames;	/* attribute type names */
	int		   *atttypmod;		/* type-specific type modifiers */
	int		   *attstattarget;	/* attribute statistics targets */
	char	   *attstorage;		/* attribute storage scheme */
	char	   *typstorage;		/* type storage scheme */
	bool	   *attisdropped;	/* true if attr is dropped; don't dump it */
	int		   *attlen;			/* attribute length, used by binary_upgrade */
	char	   *attalign;		/* attribute align, used by binary_upgrade */
	bool	   *attislocal;		/* true if attr has local definition */
	bool	   *notnull;		/* NOT NULL constraints on attributes */
	bool	   *inhNotNull;		/* true if NOT NULL is inherited */
	char	  **attencoding;	/* the attribute encoding values */
	struct _attrDefInfo **attrdefs;		/* DEFAULT expressions */
	struct _constraintInfo *checkexprs; /* CHECK constraints */

	/*
	 * Stuff computed only for dumpable tables.
	 */
	int			numParents;		/* number of (immediate) parent tables */
	struct _tableInfo **parents;	/* TableInfos of immediate parents */
	struct _tableDataInfo *dataObj;	/* TableDataInfo, if dumping its data */
	Oid			parrelid;			/* external partition's parent oid */
} TableInfo;

typedef struct _attrDefInfo
{
	DumpableObject dobj;		/* note: dobj.name is name of table */
	TableInfo  *adtable;		/* link to table of attribute */
	int			adnum;
	char	   *adef_expr;		/* decompiled DEFAULT expression */
	bool		separate;		/* TRUE if must dump as separate item */
} AttrDefInfo;

typedef struct _tableDataInfo
{
	DumpableObject dobj;
	TableInfo  *tdtable;		/* link to table to dump */
	bool		oids;			/* include OIDs in data? */
	char	   *filtercond;		/* WHERE condition to limit rows dumped */
} TableDataInfo;

typedef struct _indxInfo
{
	DumpableObject dobj;
	TableInfo  *indextable;		/* link to table the index is for */
	char	   *indexdef;
	char	   *tablespace;		/* tablespace in which index is stored */
	char	   *options;		/* options specified by WITH (...) */
	int			indnkeys;
	Oid		   *indkeys;
	bool		indisclustered;
	/* if there is an associated constraint object, its dumpId: */
	DumpId		indexconstraint;
} IndxInfo;

typedef struct _ruleInfo
{
	DumpableObject dobj;
	TableInfo  *ruletable;		/* link to table the rule is for */
	char		ev_type;
	bool		is_instead;
	char		ev_enabled;
	bool		separate;		/* TRUE if must dump as separate item */
	/* separate is always true for non-ON SELECT rules */
} RuleInfo;

typedef struct _triggerInfo
{
	DumpableObject dobj;
	TableInfo  *tgtable;		/* link to table the trigger is for */
	char	   *tgfname;
	int			tgtype;
	int			tgnargs;
	char	   *tgargs;
	bool		tgisconstraint;
	char	   *tgconstrname;
	Oid			tgconstrrelid;
	char	   *tgconstrrelname;
	char		tgenabled;
	bool		tgdeferrable;
	bool		tginitdeferred;
} TriggerInfo;

/*
 * struct ConstraintInfo is used for all constraint types.	However we
 * use a different objType for foreign key constraints, to make it easier
 * to sort them the way we want.
 */
typedef struct _constraintInfo
{
	DumpableObject dobj;
	TableInfo  *contable;		/* NULL if domain constraint */
	TypeInfo   *condomain;		/* NULL if table constraint */
	char		contype;
	char	   *condef;			/* definition, if CHECK or FOREIGN KEY */
	Oid			confrelid;		/* referenced table, if FOREIGN KEY */
	DumpId		conindex;		/* identifies associated index if any */
	bool		conislocal;		/* TRUE if constraint has local definition */
	bool		separate;		/* TRUE if must dump as separate item */
} ConstraintInfo;

typedef struct _procLangInfo
{
	DumpableObject dobj;
	bool		lanpltrusted;
	Oid			lanplcallfoid;
	Oid			laninline;
	Oid			lanvalidator;
	char	   *lanacl;
	char	   *lanowner;		/* name of owner, or empty string */
} ProcLangInfo;

typedef struct _castInfo
{
	DumpableObject dobj;
	Oid			castsource;
	Oid			casttarget;
	Oid			castfunc;
	char		castcontext;
	char		castmethod;
} CastInfo;

/* InhInfo isn't a DumpableObject, just temporary state */
typedef struct _inhInfo
{
	Oid			inhrelid;		/* OID of a child table */
	Oid			inhparent;		/* OID of its parent */
} InhInfo;

typedef struct _prsInfo
{
	DumpableObject dobj;
	Oid			prsstart;
	Oid			prstoken;
	Oid			prsend;
	Oid			prsheadline;
	Oid			prslextype;
} TSParserInfo;

typedef struct _dictInfo
{
	DumpableObject dobj;
	char	   *rolname;
	Oid			dicttemplate;
	char	   *dictinitoption;
} TSDictInfo;

typedef struct _tmplInfo
{
	DumpableObject dobj;
	Oid			tmplinit;
	Oid			tmpllexize;
} TSTemplateInfo;

typedef struct _cfgInfo
{
	DumpableObject dobj;
	char	   *rolname;
	Oid			cfgparser;
} TSConfigInfo;

typedef struct _fdwInfo
{
	DumpableObject dobj;
	char	   *rolname;
	char	   *fdwlibrary;
	char	   *fdwoptions;
	char	   *fdwacl;
} FdwInfo;

typedef struct _foreignServerInfo
{
	DumpableObject dobj;
	char	   *rolname;
	Oid			srvfdw;
	char	   *srvtype;
	char	   *srvversion;
	char	   *srvacl;
	char	   *srvoptions;
} ForeignServerInfo;

/*
 * We build an array of these with an entry for each object that is an
 * extension member according to pg_depend.
 */
typedef struct _extensionMemberId
{
	CatalogId	catId;			/* tableoid+oid of some member object */
	ExtensionInfo *ext;			/* owning extension */
} ExtensionMemberId;

/* global decls */
extern bool force_quotes;		/* double-quotes for identifiers flag */
extern bool g_verbose;			/* verbose flag */

/* placeholders for comment starting and ending delimiters */
extern char g_comment_start[10];
extern char g_comment_end[10];

extern char g_opaque_type[10];	/* name for the opaque type */

extern const char *EXT_PARTITION_NAME_POSTFIX;
/*
 *	common utility functions
 */

extern TableInfo *getSchemaData(int *numTablesPtr, int g_role);

typedef enum _OidOptions
{
	zeroAsOpaque = 1,
	zeroAsAny = 2,
	zeroAsStar = 4,
	zeroAsNone = 8
} OidOptions;

extern void DetectChildConstraintDropped(TableInfo *tbinfo, PQExpBuffer q); /* GPDB only */
extern void AssignDumpId(DumpableObject *dobj);
extern DumpId createDumpId(void);
extern DumpId getMaxDumpId(void);
extern DumpableObject *findObjectByDumpId(DumpId dumpId);
extern DumpableObject *findObjectByCatalogId(CatalogId catalogId);
extern void getDumpableObjects(DumpableObject ***objs, int *numObjs);

extern void addObjectDependency(DumpableObject *dobj, DumpId refId);
extern void removeObjectDependency(DumpableObject *dobj, DumpId refId);

extern DumpableObject *findObjectByOid(Oid oid, DumpableObject **indexArray, int numObjs);

extern TableInfo *findTableByOid(Oid oid);
extern TypeInfo *findTypeByOid(Oid oid);
extern FuncInfo *findFuncByOid(Oid oid);
extern OprInfo *findOprByOid(Oid oid);
extern NamespaceInfo *findNamespaceByOid(Oid oid);
extern ExtensionInfo *findExtensionByOid(Oid oid);

extern void setExtensionMembership(ExtensionMemberId *extmems, int nextmems);
extern ExtensionInfo *findOwningExtension(CatalogId catalogId);

extern void simple_oid_list_append(SimpleOidList *list, Oid val);
extern void simple_string_list_append(SimpleStringList *list, const char *val);
extern bool simple_oid_list_member(SimpleOidList *list, Oid val);
extern bool simple_string_list_member(SimpleStringList *list, const char *val);
extern bool open_file_and_append_to_list(const char *fileName, SimpleStringList *list, const char *reason);

extern char *pg_strdup(const char *string);
extern void *pg_malloc(size_t size);
extern void *pg_calloc(size_t nmemb, size_t size);
extern void *pg_realloc(void *ptr, size_t size);

extern void check_sql_result(PGresult *res, PGconn *conn, const char *query,
				 ExecStatusType expected);
extern void check_conn_and_db(void);
extern void exit_nicely(void) pg_attribute_noreturn();

extern void parseOidArray(const char *str, Oid *array, int arraysize);

extern void sortDumpableObjects(DumpableObject **objs, int numObjs);
extern void sortDumpableObjectsByTypeName(DumpableObject **objs, int numObjs);
extern void sortDumpableObjectsByTypeOid(DumpableObject **objs, int numObjs);


/*
 * version specific routines
 */
extern NamespaceInfo *getNamespaces(int *numNamespaces);
extern ExtensionInfo *getExtensions(int *numExtensions);
extern TypeInfo *getTypes(int *numTypes);
extern TypeStorageOptions *getTypeStorageOptions(int *numTypes);
extern FuncInfo *getFuncs(int *numFuncs);
extern AggInfo *getAggregates(int *numAggregates);
extern ExtProtInfo *getExtProtocols(int *numExtProtocols);
extern OprInfo *getOperators(int *numOperators);
extern OpclassInfo *getOpclasses(int *numOpclasses);
extern OpfamilyInfo *getOpfamilies(int *numOpfamilies);
extern ConvInfo *getConversions(int *numConversions);
extern TableInfo *getTables(int *numTables);
extern void getOwnedSeqs(TableInfo tblinfo[], int numTables);
extern InhInfo *getInherits(int *numInherits);
extern void getIndexes(TableInfo tblinfo[], int numTables);
extern void getConstraints(TableInfo tblinfo[], int numTables);
extern RuleInfo *getRules(int *numRules);
extern void getTriggers(TableInfo tblinfo[], int numTables);
extern ProcLangInfo *getProcLangs(int *numProcLangs);
extern CastInfo *getCasts(int *numCasts);
extern void getTableAttrs(TableInfo *tbinfo, int numTables);
extern bool shouldPrintColumn(TableInfo *tbinfo, int colno);
extern TSParserInfo *getTSParsers(int *numTSParsers);
extern TSDictInfo *getTSDictionaries(int *numTSDicts);
extern TSTemplateInfo *getTSTemplates(int *numTSTemplates);
extern TSConfigInfo *getTSConfigurations(int *numTSConfigs);
extern FdwInfo *getForeignDataWrappers(int *numForeignDataWrappers);
extern ForeignServerInfo *getForeignServers(int *numForeignServers);
extern void getExtensionMembership(ExtensionInfo extinfo[], int numExtensions);
extern void processExtensionTables(ExtensionInfo extinfo[], int numExtensions);

extern bool	testExtProtocolSupport(void);


#endif   /* PG_DUMP_H */
