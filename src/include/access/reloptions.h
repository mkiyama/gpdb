/*-------------------------------------------------------------------------
 *
 * reloptions.h
 *	  Core support for relation options (pg_class.reloptions)
 *
 * Note: the functions dealing with text-array reloptions values declare
 * them as Datum, not ArrayType *, to avoid needing to include array.h
 * into a lot of low-level code.
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/access/reloptions.h,v 1.7 2009/01/05 17:14:28 alvherre Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef RELOPTIONS_H
#define RELOPTIONS_H

#include "access/htup.h"
#include "nodes/pg_list.h"
#include "utils/rel.h"

#define AO_DEFAULT_APPENDONLY     false
#define AO_DEFAULT_BLOCKSIZE      DEFAULT_APPENDONLY_BLOCK_SIZE
/* Compression is turned off by default. */
#define AO_DEFAULT_COMPRESSLEVEL  0
#define AO_MIN_COMPRESSLEVEL  0
#define AO_MAX_COMPRESSLEVEL  19
/*
 * If compression is turned on without specifying compresstype, this
 * is the default.
 */
#define AO_DEFAULT_COMPRESSTYPE   "zlib"
#define AO_DEFAULT_CHECKSUM       true
#define AO_DEFAULT_COLUMNSTORE    false

/* types supported by reloptions */
typedef enum relopt_type
{
	RELOPT_TYPE_BOOL,
	RELOPT_TYPE_INT,
	RELOPT_TYPE_REAL,
	RELOPT_TYPE_STRING
} relopt_type;

/* kinds supported by reloptions */
typedef enum relopt_kind
{
	RELOPT_KIND_HEAP,
	/* XXX do we need a separate kind for TOAST tables? */
	RELOPT_KIND_BTREE,
	RELOPT_KIND_HASH,
	RELOPT_KIND_GIN,
	RELOPT_KIND_GIST,
	RELOPT_KIND_BITMAP,
	RELOPT_KIND_INTERNAL,
	/* if you add a new kind, make sure you update "last_default" too */
	RELOPT_KIND_LAST_DEFAULT = RELOPT_KIND_INTERNAL,
	RELOPT_KIND_MAX = 255
} relopt_kind;

/* generic struct to hold shared data */
typedef struct relopt_gen
{
	const char *name;	/* must be first (used as list termination marker) */
	const char *desc;
	relopt_kind	kind;
	int			namelen;
	relopt_type	type;
} relopt_gen;

/* holds a parsed value */
typedef struct relopt_value
{
	relopt_gen *gen;
	bool		isset;
	union
	{
		bool	bool_val;
		int		int_val;
		double	real_val;
		char   *string_val;	/* allocated separately */
	} values;
} relopt_value;

/* reloptions records for specific variable types */
typedef struct relopt_bool
{
	relopt_gen	gen;
	bool		default_val;
} relopt_bool;
	
typedef struct relopt_int
{
	relopt_gen	gen;
	int			default_val;
	int			min;
	int			max;
} relopt_int;

typedef struct relopt_real
{
	relopt_gen	gen;
	double		default_val;
	double		min;
	double		max;
} relopt_real;

typedef struct relopt_string
{
	relopt_gen	gen;
	int			default_len;
	bool		default_isnull;
	char		default_val[1];	/* variable length */
} relopt_string;

/* This is the table datatype for fillRelOptions */
typedef struct
{
	const char *optname;		/* option's name */
	relopt_type opttype;		/* option's datatype */
	int			offset;			/* offset of field in result struct */
} relopt_parse_elt;


/*
 * These macros exist for the convenience of amoptions writers.  See
 * default_reloptions for an example of the intended usage.  Beware of
 * multiple evaluation of arguments!
 *
 * Most of the time there's no need to call HAVE_RELOPTION manually, but it's
 * possible that an amoptions routine needs to walk the array with a different
 * purpose (say, to compute the size of a struct to allocate beforehand.)
 */
#define HAVE_RELOPTION(optname, option) \
	(pg_strncasecmp(option.gen->name, optname, option.gen->namelen) == 0)

#define HANDLE_INT_RELOPTION(optname, var, option) 					\
	do {															\
		if (HAVE_RELOPTION(optname, option))						\
		{															\
			if (option.isset)										\
				var = option.values.int_val; 						\
			else													\
				var = ((relopt_int *) option.gen)->default_val; 	\
			continue;												\
		}															\
	} while (0)

#define HANDLE_BOOL_RELOPTION(optname, var, option) 				\
	do {															\
		if (HAVE_RELOPTION(optname, option))						\
		{															\
			if (option.isset)										\
				var = option.values.bool_val; 						\
			else													\
				var = ((relopt_bool *) option.gen)->default_val;	\
			continue;												\
		}															\
	} while (0)

#define HANDLE_REAL_RELOPTION(optname, var, option) 				\
	do {															\
		if (HAVE_RELOPTION(optname, option))						\
		{															\
			if (option.isset)										\
				var = option.values.real_val; 						\
			else													\
				var = ((relopt_real *) option.gen)->default_val;	\
			continue;												\
		}															\
	} while (0)

/* Note that this assumes that the variable is already allocated! */
#define HANDLE_STRING_RELOPTION(optname, var, option) 				\
	do {															\
		if (HAVE_RELOPTION(optname, option))						\
		{															\
			relopt_string *optstring = (relopt_string *) option.gen;\
			if (optstring->default_isnull)							\
				var[0] = '\0';										\
			else													\
				strcpy(var,											\
					   option.isset ? option.values.string_val : 	\
					   optstring->default_val);						\
			continue;												\
		}															\
	} while (0)

/*
 * For use during amoptions: get the strlen of a string option
 * (either default or the user defined value)
 */
#define GET_STRING_RELOPTION_LEN(option) \
	((option).isset ? strlen((option).values.string_val) : \
	 ((relopt_string *) (option).gen)->default_len)

/*
 * For use by code reading options already parsed: get a pointer to the string
 * value itself.  "optstruct" is the StdRdOption struct or equivalent, "member"
 * is the struct member corresponding to the string option
 */
#define GET_STRING_RELOPTION(optstruct, member) \
	((optstruct)->member == 0 ? NULL : \
	 (char *)(optstruct) + (optstruct)->member)

extern int add_reloption_kind(void);
extern void add_bool_reloption(int kind, char *name, char *desc,
				   bool default_val);
extern void add_int_reloption(int kind, char *name, char *desc,
				  int default_val, int min_val, int max_val);
extern void add_real_reloption(int kind, char *name, char *desc,
				   double default_val, double min_val, double max_val);
extern void add_string_reloption(int kind, char *name, char *desc,
					 char *default_val);
			
extern Datum transformRelOptions(Datum oldOptions, List *defList,
					bool ignoreOids, bool isReset);
extern List *untransformRelOptions(Datum options);
extern relopt_value *parseRelOptions(Datum options, bool validate,
				relopt_kind kind, int *numrelopts);
extern void *allocateReloptStruct(Size base, relopt_value *options,
					 int numoptions);
extern void fillRelOptions(void *rdopts, Size basesize,
			   relopt_value *options, int numoptions,
			   bool validate,
			   const relopt_parse_elt *elems, int nelems);

extern bytea *default_reloptions(Datum reloptions, bool validate,
				   relopt_kind kind);
extern bytea *heap_reloptions(char relkind, Datum reloptions, bool validate);
extern bytea *index_reloptions(RegProcedure amoptions, Datum reloptions,
				bool validate);

extern void validateAppendOnlyRelOptions(bool ao, int blocksize, int writesize,
										 int complevel, char* comptype, 
										 bool checksum, char relkind, bool co);

extern Datum transformAOStdRdOptions(StdRdOptions *opts, Datum withOpts);

extern void resetDefaultAOStorageOpts(void);
extern void resetAOStorageOpts(StdRdOptions *ao_opts);
extern bool isDefaultAOCS(void);
extern bool isDefaultAO(void);
extern void setDefaultAOStorageOpts(StdRdOptions *copy);
extern const StdRdOptions *currentAOStorageOptions(void);
extern Datum parseAOStorageOpts(const char *opts_str, bool *aovalue);
extern void parse_validate_reloptions(StdRdOptions *result, Datum reloptions,
									  bool validate, relopt_kind relkind);

/* in reloptions_gp.c */
extern void initialize_reloptions_gp(void);
extern void validate_and_refill_options(StdRdOptions *result, relopt_value *options,
							int numoptions, relopt_kind kind, bool validate);
extern void validate_and_adjust_options(StdRdOptions *result, relopt_value *options,
										int num_options, relopt_kind kind, bool validate);

#endif   /* RELOPTIONS_H */
