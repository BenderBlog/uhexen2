/*
 * hcc.c
 *
 * $Id: hcc.c,v 1.22 2009-06-15 09:12:21 sezero Exp $
 *
 * Hash table modifications based on fastqcc by Jonathan Roy
 * (roy@atlantic.net).
 *
 * Copyright (C) 1996-1997  Id Software, Inc.
 * Copyright (C) 1997-1998  Raven Software Corp.
 * Copyright (C) 2005-2012  O.Sezer <sezero@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


// HEADER FILES ------------------------------------------------------------

#include "q_stdinc.h"
#include "compiler.h"
#include "arch_def.h"
#include "cmdlib.h"
#include "util_io.h"
#include "crc.h"
#include "hcc.h"
#include "q_endian.h"
#include "byteordr.h"
#include "filenames.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void BeginCompilation(void);
static qboolean FinishCompilation(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int		hcc_OptimizeImmediates;
int		hcc_OptimizeNameTable;
qboolean	old_hcc_behavior;
qboolean	hcc_WarningsActive;
qboolean	hcc_ShowUnrefFuncs;

byte		_pr_globals[MAX_REGS * sizeof(float)];
float		*pr_globals;
int			numpr_globals;

char		strings[MAX_STRINGS];
int			strofs;

dstatement_t	statements[MAX_STATEMENTS];
int			numstatements;
int			statement_linenums[MAX_STATEMENTS];

dfunction_t	functions[MAX_FUNCTIONS];
int			numfunctions;

ddef_t		globals[MAX_GLOBALS];
int			numglobaldefs;

ddef_t		fields[MAX_FIELDS];
int			numfielddefs;

char		precache_sounds[MAX_SOUNDS][MAX_DATA_PATH];
int			precache_sounds_block[MAX_SOUNDS];
int			numsounds;

char		precache_models[MAX_MODELS][MAX_DATA_PATH];
int			precache_models_block[MAX_SOUNDS];
int			nummodels;

char		precache_files[MAX_FILES][MAX_DATA_PATH];
int			precache_files_block[MAX_SOUNDS];
int			numfiles;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char	sourcedir[1024];
static char	destfile[1024];

// CODE --------------------------------------------------------------------

/*
============
WriteFiles

  Generates files.dat, which contains all of the
  data files actually used by the game, to be
  processed by qfiles.exe
============
*/
static void WriteFiles (void)
{
	FILE	*f;
	int		i;
	char	filename[1024];

	sprintf (filename, "%sfiles.dat", sourcedir);
	f = fopen (filename, "w");
	if (!f)
		COM_Error ("Couldn't open %s", filename);

	fprintf (f, "%i\n", numsounds);
	for (i = 0; i < numsounds; i++)
		fprintf (f, "%i %s\n", precache_sounds_block[i], precache_sounds[i]);

	fprintf (f, "%i\n", nummodels);
	for (i = 0; i < nummodels; i++)
		fprintf (f, "%i %s\n", precache_models_block[i], precache_models[i]);

	fprintf (f, "%i\n", numfiles);
	for (i = 0; i < numfiles; i++)
		fprintf (f, "%i %s\n", precache_files_block[i], precache_files[i]);

	fclose (f);
}


// CopyString returns an offset from the string heap
int	CopyString (const char *str)
{
	int		old;

	old = strofs;
	strcpy (strings+strofs, str);
	strofs += strlen(str)+1;
	return old;
}


#if 0	/* all uses are commented out */
static void PrintStrings (void)
{
	int		i, l, j;

	for (i = 0; i < strofs; i += l)
	{
		l = strlen(strings+i) + 1;
		printf ("%5i : ", i);
		for (j = 0; j < l; j++)
		{
			if (strings[i+j] == '\n')
			{
				putchar ('\\');
				putchar ('n');
			}
			else
				putchar (strings[i+j]);
		}
		printf ("\n");
	}
}

static void PrintFunctions (void)
{
	int		i, j;
	dfunction_t	*d;

	for (i = 0; i < numfunctions; i++)
	{
		d = &functions[i];
		printf ("%s : %s : %i %i (", strings + d->s_file, strings + d->s_name, d->first_statement, d->parm_start);
		for (j = 0; j < d->numparms; j++)
			printf ("%i ", d->parm_size[j]);
		printf (")\n");
	}
}

static void PrintFields (void)
{
	int		i;
	ddef_t	*d;

	for (i = 0; i < numfielddefs; i++)
	{
		d = &fields[i];
		printf ("%5i : (%i) %s\n", d->ofs, d->type, strings + d->s_name);
	}
}

static void PrintGlobals (void)
{
	int		i;
	ddef_t	*d;

	for (i = 0; i < numglobaldefs; i++)
	{
		d = &globals[i];
		printf ("%5i : (%i) %s\n", d->ofs, d->type, strings + d->s_name);
	}
}
#endif	/* end of unused stuff */


static void InitData (void)
{
	int		i;

	numstatements = 1;
	strofs = 1;
	numfunctions = 1;
	numglobaldefs = 1;
	numfielddefs = 1;

	pr_globals = (float *)_pr_globals;
	def_ret.ofs = OFS_RETURN;
	for (i = 0; i < MAX_PARMS; i++)
		def_parms[i].ofs = OFS_PARM0 + 3*i;
}


static void WriteData (int crc)
{
	def_t		*def;
	ddef_t		*dd;
	dprograms_t	progs;
	FILE	*h;
	int	i, localName;

	localName = (hcc_OptimizeNameTable) ? CopyString("LCL+") : 0;
						/* was "LOCAL+" in old HCC from the "H2_UTILS" package */

	for (def = pr.def_head.next; def; def = def->next)
	{
		if (def->type->type == ev_field)
		{
			dd = &fields[numfielddefs];
			numfielddefs++;
			dd->type = def->type->aux_type->type;
			dd->s_name = strofs; /* The name gets copied below */
			dd->ofs = G_INT(def->ofs);
		}
		dd = &globals[numglobaldefs];
		numglobaldefs++;
		dd->type = def->type->type;
		if (!def->initialized &&
		    def->type->type != ev_function &&
		    def->type->type != ev_field &&
		    def->scope == NULL)
		{
			/* STR_ is a special case string constant */
			if (strncmp(def->name,"STR_", 4) != 0 || old_hcc_behavior)
				dd->type |= DEF_SAVEGLOBAL;
		}
		if (hcc_OptimizeNameTable && ((def->scope != NULL) ||
			(!(dd->type&DEF_SAVEGLOBAL)&&(def->type->type < ev_field))))
		{
			dd->s_name = localName;
		}
		else
		{
			dd->s_name = CopyString(def->name);
		}
		dd->ofs = def->ofs;
	}

//	PrintStrings ();
//	PrintFunctions ();
//	PrintFields ();
//	PrintGlobals ();

	strofs = (strofs + 3) & ~3;

	printf("object file %s\n", destfile);
	printf("      registers: %10d / %10d (%10d bytes)\n", numpr_globals, MAX_REGS, numpr_globals*(int)sizeof(float));
	printf("     statements: %10d / %10d (%10d bytes)\n", numstatements, MAX_STATEMENTS, numstatements*(int)sizeof(dstatement_t));
	printf("      functions: %10d / %10d (%10d bytes)\n", numfunctions, MAX_FUNCTIONS, numfunctions*(int)sizeof(dfunction_t));
	printf("    global defs: %10d / %10d (%10d bytes)\n", numglobaldefs, MAX_GLOBALS, numglobaldefs*(int)sizeof(ddef_t));
	printf("     field defs: %10d / %10d (%10d bytes)\n", numfielddefs, MAX_FIELDS, numfielddefs*(int)sizeof(ddef_t));
	printf("    string heap: %10d / %10d\n", strofs, MAX_STRINGS);
	printf("  entity fields: %10d\n", pr.size_fields);

	h = SafeOpenWrite (destfile);
	SafeWrite (h, &progs, sizeof(progs));

	progs.ofs_strings = ftell (h);
	progs.numstrings = strofs;
	SafeWrite (h, strings, strofs);

	progs.ofs_statements = ftell (h);
	progs.numstatements = numstatements;
	for (i = 0; i < numstatements; i++)
	{
		statements[i].op = LittleShort(statements[i].op);
		statements[i].a = LittleShort(statements[i].a);
		statements[i].b = LittleShort(statements[i].b);
		statements[i].c = LittleShort(statements[i].c);
	}
	SafeWrite (h, statements, numstatements*sizeof(dstatement_t));

	progs.ofs_functions = ftell (h);
	progs.numfunctions = numfunctions;
	for (i = 0; i < numfunctions; i++)
	{
		functions[i].first_statement = LittleLong (functions[i].first_statement);
		functions[i].parm_start = LittleLong (functions[i].parm_start);
		functions[i].s_name = LittleLong (functions[i].s_name);
		functions[i].s_file = LittleLong (functions[i].s_file);
		functions[i].numparms = LittleLong (functions[i].numparms);
		functions[i].locals = LittleLong (functions[i].locals);
	}
	SafeWrite (h, functions, numfunctions*sizeof(dfunction_t));

	progs.ofs_globaldefs = ftell (h);
	progs.numglobaldefs = numglobaldefs;
	for (i = 0; i < numglobaldefs; i++)
	{
		globals[i].type = LittleShort (globals[i].type);
		globals[i].ofs = LittleShort (globals[i].ofs);
		globals[i].s_name = LittleLong (globals[i].s_name);
	}
	SafeWrite (h, globals, numglobaldefs*sizeof(ddef_t));

	progs.ofs_fielddefs = ftell (h);
	progs.numfielddefs = numfielddefs;
	for (i = 0; i < numfielddefs; i++)
	{
		fields[i].type = LittleShort (fields[i].type);
		fields[i].ofs = LittleShort (fields[i].ofs);
		fields[i].s_name = LittleLong (fields[i].s_name);
	}
	SafeWrite (h, fields, numfielddefs*sizeof(ddef_t));

	progs.ofs_globals = ftell (h);
	progs.numglobals = numpr_globals;
	for (i = 0; i < numpr_globals; i++)
		((int *)pr_globals)[i] = LittleLong (((int *)pr_globals)[i]);
	SafeWrite (h, pr_globals, numpr_globals*4);

	printf("     total size: %10d bytes\n", (int)ftell(h));

	progs.entityfields = pr.size_fields;

	progs.version = PROG_VERSION;
	progs.crc = crc;

// byte swap the header and write it out
	for (i = 0; i < (int)sizeof(progs)/4; i++)
		((int *)&progs)[i] = LittleLong ( ((int *)&progs)[i] );
	fseek (h, 0, SEEK_SET);
	SafeWrite (h, &progs, sizeof(progs));
	fclose (h);
}


/*
===============
PR_String

Returns a string suitable for printing (no newlines, max 60 chars length)
===============
*/
static const char *PR_String (const char *string)
{
	static char	buf[80];
	char	*s;

	s = buf;
	*s++ = '"';
	while (string && *string)
	{
		if (s == buf + sizeof(buf) - 2)
			break;
		if (*string == '\n')
		{
			*s++ = '\\';
			*s++ = 'n';
		}
		else if (*string == '"')
		{
			*s++ = '\\';
			*s++ = '"';
		}
		else
			*s++ = *string;
		string++;
		if (s - buf > 60)
		{
			*s++ = '.';
			*s++ = '.';
			*s++ = '.';
			break;
		}
	}
	*s++ = '"';
	*s++ = 0;
	return buf;
}

static def_t *PR_DefForFieldOfs (gofs_t ofs)
{
	def_t	*d;

	for (d = pr.def_head.next; d; d = d->next)
	{
		if (d->type->type != ev_field)
			continue;
		if (((int *)pr_globals)[d->ofs] == ofs)
			return d;
	}
	COM_Error ("%s: couldn't find %i", __thisfunc__, ofs);
	return NULL;
}

/*
============
PR_ValueString

Returns a string describing *data in a type specific manner
=============
*/
static const char *PR_ValueString (etype_t type, void *val)
{
	static char	line[256];
	def_t		*def;
	dfunction_t	*f;

	switch (type)
	{
	case ev_string:
		sprintf (line, "%s", PR_String(strings + *(int *)val));
		break;
	case ev_entity:
		sprintf (line, "entity %i", *(int *)val);
		break;
	case ev_function:
		f = functions + *(int *)val;
		if (!f)
			sprintf (line, "undefined function");
		else
			sprintf (line, "%s()", strings + f->s_name);
		break;
	case ev_field:
		def = PR_DefForFieldOfs ( *(int *)val );
		sprintf (line, ".%s", def->name);
		break;
	case ev_void:
		sprintf (line, "void");
		break;
	case ev_float:
		sprintf (line, "%5.1f", *(float *)val);
		break;
	case ev_vector:
		sprintf (line, "'%5.1f %5.1f %5.1f'", ((float *)val)[0], ((float *)val)[1], ((float *)val)[2]);
		break;
	case ev_pointer:
		sprintf (line, "pointer");
		break;
	default:
		sprintf (line, "bad type %i", type);
		break;
	}

	return line;
}

/*
============
PR_GlobalString

Returns a string with a description and the contents of a global,
padded to 20 field width
============
*/
static const char *PR_GlobalStringNoContents (gofs_t ofs)
{
	static char	line[128];
	def_t		*def;
	int	i;

	def = pr_global_defs[ofs];
	if (!def)
	//	COM_Error ("%s: no def for %i", __thisfunc__, ofs);
		sprintf (line, "%i(?)", ofs);
	else
		sprintf (line, "%i(%s)", ofs, def->name);

	i = strlen(line);
	for ( ; i < 16; i++)
		strcat (line, " ");
	strcat (line, " ");

	return line;
}

static const char *PR_GlobalString (gofs_t ofs)
{
	static char	line[128];
	const char	*s;
	def_t		*def;
	int	i;

	def = pr_global_defs[ofs];
	if (!def)
		return PR_GlobalStringNoContents(ofs);
	if (def->initialized && def->type->type != ev_function)
	{
		s = PR_ValueString (def->type->type, &pr_globals[ofs]);
		sprintf (line, "%i(%s)", ofs, s);
	}
	else
		sprintf (line, "%i(%s)", ofs, def->name);

	i = strlen(line);
	for ( ; i < 16; i++)
		strcat (line, " ");
	strcat (line, " ");

	return line;
}

/*
=================
PR_PrintStatement
=================
*/
static void PR_PrintStatement (dstatement_t *s)
{
	int		i;

	printf ("%4i : %4i : %s ", (int)(s - statements), statement_linenums[s-statements], pr_opcodes[s->op].opname);
	i = strlen(pr_opcodes[s->op].opname);
	for ( ; i < 10; i++)
		printf (" ");

	if (s->op == OP_IF || s->op == OP_IFNOT)
		printf ("%sbranch %i", PR_GlobalString(s->a), s->b);
	else if (s->op == OP_GOTO)
	{
		printf ("branch %i", s->a);
	}
	else if ((unsigned int)(s->op - OP_STORE_F) < 6)
	{
		printf ("%s", PR_GlobalString(s->a));
		printf ("%s", PR_GlobalStringNoContents(s->b));
	}
	else if ((unsigned int)(s->op - OP_SWITCH_F) < 5)
	{
		printf ("%sbranch %i", PR_GlobalString(s->a),s->b);
	}
	else if (s->op == OP_CASE)
	{
		printf ("of %i branch %i",s->a, s->b);
	}
	else
	{
		if (s->a)
			printf ("%s", PR_GlobalString(s->a));
		if (s->b)
			printf ("%s", PR_GlobalString(s->b));
		if (s->c)
			printf ("%s", PR_GlobalStringNoContents(s->c));
	}
	printf ("\n");
}

/*
==============
BeginCompilation

called before compiling a batch of files, clears the pr struct
==============
*/
static void BeginCompilation (void)
{
	int		i;

	numpr_globals = RESERVED_OFS;
	pr.def_tail = &pr.def_head;
	for (i = 0; i < RESERVED_OFS; i++)
	{
		pr_global_defs[i] = &def_void;
	}

	// Link the function type in so state forward declarations match
	// proper type.
	pr.types = &type_function;
	type_function.next = NULL;
	pr_error_count = 0;
}

/*
==============
FinishCompilation

called after all files are compiled to check for errors
Returns false if errors were detected.
==============
*/
static qboolean FinishCompilation (void)
{
	def_t		*d;
	qboolean	errors, globals_done;

	globals_done = false;
	errors = false;
	for (d = pr.def_head.next; d; d = d->next)
	{
		if (!strcmp (d->name, "end_sys_globals"))
			globals_done = true;
		if (d->type->type == ev_function && !d->scope)
		{
			if (!d->initialized)
			{
				printf("function '%s' was not defined\n", d->name);
				errors = true;
			}
			if (hcc_ShowUnrefFuncs && d->referenceCount == 0 && globals_done)
			{
				printf("unreferenced function '%s'\n", d->name);
			}
		}
	}

	return !errors;
}

/*
============
PR_WriteProgdefs

Writes the global and entity structures out
Returns a crc of the header, to be stored in the progs file for comparison
at load time.
============
*/
static int PR_WriteProgdefs (const char *filename)
{
	def_t	*d;
	FILE	*f;
	unsigned short	crc;
	int	c;

	printf ("writing %s\n", filename);
	f = fopen (filename, "w");

	// print global vars until the first field is defined
	fprintf (f,"\n/* generated by hcc, do not modify */\n\ntypedef struct\n{\tint\tpad[%i];\n", RESERVED_OFS);
	for (d = pr.def_head.next; d; d = d->next)
	{
		if (!strcmp (d->name, "end_sys_globals"))
			break;

		switch (d->type->type)
		{
		case ev_float:
			fprintf (f, "\tfloat\t%s;\n", d->name);
			break;
		case ev_vector:
			fprintf (f, "\tvec3_t\t%s;\n", d->name);
			d = d->next->next->next;	// skip the elements
			break;
		case ev_string:
			fprintf (f, "\tstring_t\t%s;\n", d->name);
			break;
		case ev_function:
			fprintf (f, "\tfunc_t\t%s;\n", d->name);
			break;
		case ev_entity:
			fprintf (f, "\tint\t%s;\n", d->name);
			break;
		default:
			fprintf (f, "\tint\t%s;\n", d->name);
			break;
		}
	}
	fprintf (f, "} globalvars_t;\n\n");

	// print all fields
	fprintf (f, "typedef struct\n{\n");
	for (d = pr.def_head.next; d; d = d->next)
	{
		if (!strcmp (d->name, "end_sys_fields"))
			break;

		if (d->type->type != ev_field)
			continue;

		switch (d->type->aux_type->type)
		{
		case ev_float:
			fprintf (f, "\tfloat\t%s;\n", d->name);
			break;
		case ev_vector:
			fprintf (f, "\tvec3_t\t%s;\n", d->name);
			d = d->next->next->next;	// skip the elements
			break;
		case ev_string:
			fprintf (f, "\tstring_t\t%s;\n", d->name);
			break;
		case ev_function:
			fprintf (f, "\tfunc_t\t%s;\n", d->name);
			break;
		case ev_entity:
			fprintf (f, "\tint\t%s;\n", d->name);
			break;
		default:
			fprintf (f, "\tint\t%s;\n", d->name);
			break;
		}
	}
	fprintf (f, "} entvars_t;\n\n");

	fclose (f);

	// do a crc of the file
	CRC_Init (&crc);
	f = fopen (filename, "r+");
	while ((c = fgetc(f)) != EOF)
		CRC_ProcessByte (&crc, (byte)c);

	fprintf (f, "#define PROGHEADER_CRC %i\n", crc);
	fclose (f);

	return crc;
}

static void PrintFunction (const char *name)
{
	int		i;
	dstatement_t	*ds;
	dfunction_t	*df;

	for (i = 0; i < numfunctions; i++)
		if (!strcmp (name, strings + functions[i].s_name))
			break;
	if (i == numfunctions)
		COM_Error ("No function names \"%s\"", name);
	df = functions + i;

	printf ("Statements for %s:\n", name);
	ds = statements + df->first_statement;
	while (1)
	{
		PR_PrintStatement (ds);
		if (!ds->op)
			break;
		ds++;
	}
}

/*
==============
PR_ParseError
==============
*/
void PR_ParseError (const char *error, ...)
{
	va_list		argptr;
	char	string[1024];

	va_start (argptr, error);
	q_vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);

	printf ("%s(%d) : %s\n", strings+s_file, lx_SourceLine, string);

	longjmp (pr_parse_abort, 1);
}

/*
==============
PR_ParseWarning
==============
*/
void PR_ParseWarning (const char *error, ...)
{
	va_list		argptr;
	char	string[1024];

	if (!hcc_WarningsActive)
		return;

	va_start (argptr, error);
	q_vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);

	printf ("%s(%d) : warning : %s\n", strings+s_file, lx_SourceLine, string);
}

/*
============
main
============
*/
int main (int argc, char **argv)
{
	const char	*psrc;
	void		*src, *src2;
	char	filename[1024];
	char	*nameptr; /* filename[] without the parent sourcedir */
	int		p, crc;
	double	start, stop;
	int		registerCount, registerSize;
	int		statementCount, statementSize;
	int		functionCount, functionSize;
	int		fileInfo;
	int		quiet;

	myargc = argc;
	myargv = argv;

	if (CheckParm("-?") || CheckParm("-h") || CheckParm("-help") || CheckParm("--help"))
	{
		printf(" -oi              : Optimize Immediates\n");
		printf(" -on              : Optimize Name Table\n");
		printf(" -quiet           : Quiet mode\n");
		printf(" -fileinfo        : Show object sizes per file\n");
		printf(" -old :           : Old HCC behavior: STR_ constants will be saved\n");
		printf("                    globals and precache_file() will go into progs\n");
		printf(" -src <directory> : Specify source directory\n");
		printf(" -name <source>   : Specify the name of the .src file\n");
		exit(0);
	}

	ValidateByteorder ();

	start = COM_GetTime ();

	p = CheckParm("-src");
	if (p != 0)
	{
	/* everything will now be relative to sourcedir: */
		if (p >= argc - 1)
			COM_Error ("No source dirname specified with -src");
		strcpy(sourcedir, argv[p+1]);
		p = strlen (sourcedir);
		if (p && !IS_DIR_SEPARATOR(sourcedir[p - 1]))
		{
			sourcedir[p] = DIR_SEPARATOR_CHAR;
			sourcedir[p + 1] = '\0';
		}
		printf("Source directory: %s\n", sourcedir);
		strcpy(filename, sourcedir);
		nameptr = strchr(filename, '\0');
	}
	else
	{
		sourcedir[0] = '\0';
		nameptr = filename;
	}

	p = CheckParm("-name");
	if (p != 0)
	{
		if (p >= argc - 1)
			COM_Error ("No input filename specified with -name");
		strcpy(nameptr, argv[p+1]);
		printf("Input file: %s\n", nameptr);
	}
	else
	{
		strcpy(nameptr, "progs.src");
	}
	LoadFile(filename, &src);
	psrc = (char *) src;

	psrc = COM_Parse(psrc);
	if (!psrc)
		COM_Error("No destination filename.  HCC -help for info.\n");
	sprintf(destfile, "%s%s", sourcedir, com_token);

	InitData ();
	LX_Init ();
	CO_Init ();
	EX_Init ();

	BeginCompilation();

	hcc_OptimizeImmediates = CheckParm("-oi");
	hcc_OptimizeNameTable = CheckParm("-on");
	old_hcc_behavior = CheckParm("-old") ? true : false;
	hcc_WarningsActive = CheckParm("-nowarnings") ? false : true;
	hcc_ShowUnrefFuncs = CheckParm("-urfunc") ? true : false;

	fileInfo = CheckParm("-fileinfo");
	quiet = CheckParm("-quiet");

	// compile all the files
	do
	{
		psrc = COM_Parse(psrc);
		if (!psrc)
			break;

		registerCount = numpr_globals;
		statementCount = numstatements;
		functionCount = numfunctions;
		strcpy (nameptr, com_token);
		if (!quiet)
			printf("compiling %s\n", nameptr);
		LoadFile (filename, &src2);

		if (!CO_CompileFile((char *)src2, nameptr))
			exit (1);
		if (!quiet && fileInfo)
		{
			registerCount = numpr_globals-registerCount;
			registerSize = registerCount*sizeof(float);
			statementCount = numstatements-statementCount;
			statementSize = statementCount*sizeof(dstatement_t);
			functionCount = numfunctions-functionCount;
			functionSize = functionCount*sizeof(dfunction_t);
			printf("      registers: %10d (%10d bytes)\n", registerCount, registerSize);
			printf("     statements: %10d (%10d bytes)\n", statementCount, statementSize);
			printf("      functions: %10d (%10d bytes)\n", functionCount, functionSize);
			printf("     total size: %10d bytes\n", registerSize+statementSize+functionSize);
		}
	} while (1);

	if (!FinishCompilation())
		COM_Error ("compilation errors");

	p = CheckParm("-asm");
	if (p != 0)
	{
		for (p++; p < argc; p++)
		{
			if (argv[p][0] == '-')
				break;
			PrintFunction(argv[p]);
		}
	}

	// write progdefs.h
	strcpy(nameptr, "progdefs.h");
	crc = PR_WriteProgdefs(filename);

	// write data file
	WriteData(crc);

	// write files.dat
	WriteFiles();
	printf(" precache_sound: %10d / %10d\n", numsounds, MAX_SOUNDS);
	printf(" precache_model: %10d / %10d\n", nummodels, MAX_MODELS);
	printf("  precache_file: %10d / %10d\n", numfiles, MAX_FILES);

	stop = COM_GetTime ();
	printf("\n%d seconds elapsed.\n", (int)(stop - start));

	exit (0);
}

