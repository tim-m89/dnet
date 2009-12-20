
// Compiler implementation of the D programming language
// Copyright (c) 1999-2009 by Digital Mars
// All Rights Reserved
// written by Walter Bright
// http://www.digitalmars.com

#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include <assert.h>

#if __sun&&__SVR4
#include <alloca.h>
#endif

#include "mars.h"
#include "module.h"
#include "mtype.h"
#include "declaration.h"
#include "statement.h"
#include "enum.h"
#include "aggregate.h"
#include "init.h"
#include "attrib.h"
#include "id.h"
#include "import.h"
#include "template.h"
#include "lib.h"

#include "rmem.h"
#include "cc.h"
#include "global.h"
#include "oper.h"
#include "code.h"
#include "type.h"
#include "dt.h"
#include "cgcv.h"
#include "outbuf.h"
#include "irstate.h"

struct Environment;

Environment *benv;

void out_config_init();
void slist_add(Symbol *s);
void slist_reset();
void clearStringTab();

#define STATICCTOR	0

elem *eictor;
symbol *ictorlocalgot;
elem *ector;
Array ectorgates;
elem *edtor;
elem *etest;

int dtorcount;

char *lastmname;

/**************************************
 * Append s to list of object files to generate later.
 */

Array obj_symbols_towrite;

void obj_append(Dsymbol *s)
{
    obj_symbols_towrite.push(s);
}

void obj_write_deferred(Library *library)
{
    for (int i = 0; i < obj_symbols_towrite.dim; i++)
    {	Dsymbol *s = (Dsymbol *)obj_symbols_towrite.data[i];
	Module *m = s->getModule();

	char *mname;
	if (m)
	{   mname = m->srcfile->toChars();
	    lastmname = mname;
	}
	else
	{
	    //mname = s->ident->toChars();
	    mname = lastmname;
	    assert(mname);
	}

	obj_start(mname);

	static int count;
	count++;		// sequence for generating names

	/* Create a module that's a doppelganger of m, with just
	 * enough to be able to create the moduleinfo.
	 */
	OutBuffer idbuf;
	idbuf.printf("%s.%d", m ? m->ident->toChars() : mname, count);
	char *idstr = idbuf.toChars();
	idbuf.data = NULL;
	Identifier *id = new Identifier(idstr, TOKidentifier);

	Module *md = new Module(mname, id, 0, 0);
	md->members = new Array();
	md->members->push(s);	// its only 'member' is s
	if (m)
	{
	    md->doppelganger = 1;	// identify this module as doppelganger
	    md->md = m->md;
	    md->aimports.push(m);	// it only 'imports' m
	    md->massert = m->massert;
	    md->marray = m->marray;
	}

	md->genobjfile(0);

	/* Set object file name to be source name with sequence number,
	 * as mangled symbol names get way too long.
	 */
	char *fname = FileName::removeExt(mname);
	OutBuffer namebuf;
	unsigned hash = 0;
	for (char *p = s->toChars(); *p; p++)
	    hash += *p;
	namebuf.printf("%s_%x_%x.%s", fname, count, hash, global.obj_ext);
	namebuf.writeByte(0);
	mem.free(fname);
	fname = (char *)namebuf.extractData();

	//printf("writing '%s'\n", fname);
	File *objfile = new File(fname);
	obj_end(library, objfile);
    }
    obj_symbols_towrite.dim = 0;
}

/**************************************
 * Prepare for generating obj file.
 */

Outbuffer objbuf;

void obj_start(char *srcfile)
{
    //printf("obj_start()\n");

    out_config_init();

    rtlsym_reset();
    slist_reset();
    clearStringTab();

    obj_init(&objbuf, srcfile, NULL);

    el_reset();
    cg87_reset();
    out_reset();
}

void obj_end(Library *library, File *objfile)
{
    obj_term();

    if (library)
    {
	// Transfer image to library
	library->addObject(objfile->name->toChars(), objbuf.buf, objbuf.p - objbuf.buf);
	objbuf.buf = NULL;
    }
    else
    {
	// Transfer image to file
	objfile->setbuffer(objbuf.buf, objbuf.p - objbuf.buf);
	objbuf.buf = NULL;

	char *p = FileName::path(objfile->name->toChars());
	FileName::ensurePathExists(p);
	//mem.free(p);

	//printf("write obj %s\n", objfile->name->toChars());
	objfile->writev();
    }
    objbuf.pend = NULL;
    objbuf.p = NULL;
    objbuf.len = 0;
    objbuf.inc = 0;
}

/**************************************
 * Generate .obj file for Module.
 */

void Module::genobjfile(int multiobj)
{
    //EEcontext *ee = env->getEEcontext();

    //printf("Module::genobjfile(multiobj = %d) %s\n", multiobj, toChars());

    lastmname = srcfile->toChars();

    obj_initfile(lastmname, NULL, toPrettyChars());

    eictor = NULL;
    ictorlocalgot = NULL;
    ector = NULL;
    ectorgates.setDim(0);
    edtor = NULL;
    etest = NULL;
    dtorcount = 0;

    if (doppelganger)
    {
	/* Generate a reference to the moduleinfo, so the module constructors
	 * and destructors get linked in.
	 */
	Module *m = (Module *)aimports.data[0];
	assert(m);
	if (m->sictor || m->sctor || m->sdtor)
	{
	    Symbol *s = m->toSymbol();
	    //objextern(s);
	    //if (!s->Sxtrnnum) objextdef(s->Sident);
	    if (!s->Sxtrnnum)
	    {
		//printf("%s\n", s->Sident);
#if 0 /* This should work, but causes optlink to fail in common/newlib.asm */
		objextdef(s->Sident);
#else
#if ELFOBJ || MACHOBJ
		int nbytes = reftoident(DATA, Offset(DATA), s, 0, CFoff);
		Offset(DATA) += nbytes;
#else
		int nbytes = reftoident(DATA, Doffset, s, 0, CFoff);
		Doffset += nbytes;
#endif
#endif
	    }
	}
    }

    if (global.params.cov)
    {
	/* Create coverage identifier:
	 *  private uint[numlines] __coverage;
	 */
	cov = symbol_calloc("__coverage");
	cov->Stype = type_fake(TYint);
	cov->Stype->Tmangle = mTYman_c;
	cov->Stype->Tcount++;
	cov->Sclass = SCstatic;
	cov->Sfl = FLdata;
#if ELFOBJ || MACHOBJ
	cov->Sseg = UDATA;
#endif
	dtnzeros(&cov->Sdt, 4 * numlines);
	outdata(cov);
	slist_add(cov);

	covb = (unsigned *)calloc((numlines + 32) / 32, sizeof(*covb));
    }

    for (int i = 0; i < members->dim; i++)
    {
	Dsymbol *member = (Dsymbol *)members->data[i];
	member->toObjFile(multiobj);
    }

    if (global.params.cov)
    {
	/* Generate
	 *	bit[numlines] __bcoverage;
	 */
	Symbol *bcov = symbol_calloc("__bcoverage");
	bcov->Stype = type_fake(TYuint);
	bcov->Stype->Tcount++;
	bcov->Sclass = SCstatic;
	bcov->Sfl = FLdata;
#if ELFOBJ || MACHOBJ
	bcov->Sseg = DATA;
#endif
	dtnbytes(&bcov->Sdt, (numlines + 32) / 32 * sizeof(*covb), (char *)covb);
	outdata(bcov);

	free(covb);
	covb = NULL;

	/* Generate:
	 *  _d_cover_register(uint[] __coverage, BitArray __bcoverage, string filename);
	 * and prepend it to the static constructor.
	 */

	/* t will be the type of the functions generated:
	 *	extern (C) void func();
	 */
	type *t = type_alloc(TYnfunc);
	t->Tflags |= TFprototype | TFfixed;
	t->Tmangle = mTYman_c;
	t->Tnext = tsvoid;
	tsvoid->Tcount++;

	sictor = toSymbolX("__modictor", SCglobal, t, "FZv");
	cstate.CSpsymtab = &sictor->Sfunc->Flocsym;
	localgot = ictorlocalgot;
	elem *e;

	e = el_params(el_ptr(cov), el_long(TYuint, numlines),
		      el_ptr(bcov), el_long(TYuint, numlines),
		      toEfilename(),
		      NULL);
	e = el_bin(OPcall, TYvoid, el_var(rtlsym[RTLSYM_DCOVER]), e);
	eictor = el_combine(e, eictor);
	ictorlocalgot = localgot;
    }

    // If coverage / static constructor / destructor / unittest calls
    if (eictor || ector || ectorgates.dim || edtor || etest)
    {
	/* t will be the type of the functions generated:
	 *	extern (C) void func();
	 */
	type *t = type_alloc(TYnfunc);
	t->Tflags |= TFprototype | TFfixed;
	t->Tmangle = mTYman_c;
	t->Tnext = tsvoid;
	tsvoid->Tcount++;

	static char moddeco[] = "FZv";

	if (eictor)
	{
	    localgot = ictorlocalgot;

	    block *b = block_calloc();
	    b->BC = BCret;
	    b->Belem = eictor;
	    sictor->Sfunc->Fstartblock = b;
	    writefunc(sictor);
	}

	if (ector || ectorgates.dim)
	{
	    localgot = NULL;
	    sctor = toSymbolX("__modctor", SCglobal, t, moddeco);
	    cstate.CSpsymtab = &sctor->Sfunc->Flocsym;

	    for (int i = 0; i < ectorgates.dim; i++)
	    {	StaticDtorDeclaration *f = (StaticDtorDeclaration *)ectorgates.data[i];

		Symbol *s = f->vgate->toSymbol();
		elem *e = el_var(s);
		e = el_bin(OPaddass, TYint, e, el_long(TYint, 1));
		ector = el_combine(ector, e);
	    }

	    block *b = block_calloc();
	    b->BC = BCret;
	    b->Belem = ector;
	    sctor->Sfunc->Fstartblock = b;
	    writefunc(sctor);
#if STATICCTOR
	    obj_staticctor(sctor, dtorcount, 1);
#endif
	}

	if (edtor)
	{
	    localgot = NULL;
	    sdtor = toSymbolX("__moddtor", SCglobal, t, moddeco);

	    block *b = block_calloc();
	    b->BC = BCret;
	    b->Belem = edtor;
	    sdtor->Sfunc->Fstartblock = b;
	    writefunc(sdtor);
	}

	if (etest)
	{
	    localgot = NULL;
	    stest = toSymbolX("__modtest", SCglobal, t, moddeco);

	    block *b = block_calloc();
	    b->BC = BCret;
	    b->Belem = etest;
	    stest->Sfunc->Fstartblock = b;
	    writefunc(stest);
	}

	if (doppelganger)
	    genmoduleinfo();
    }

    if (doppelganger)
    {
	obj_termfile();
	return;
    }

    if (global.params.multiobj)
    {	/* This is necessary because the main .obj for this module is written
	 * first, but determining whether marray or massert are needed is done
	 * possibly later in the doppelganger modules.
	 * Another way to fix it is do the main one last.
	 */
	toModuleAssert();
	toModuleArray();
    }

    // If module assert
    for (int i = 0; i < 2; i++)
    {
	Symbol *ma = i ? marray : massert;

	if (ma)
	{
	    elem *elinnum;
	    elem *efilename;

	    localgot = NULL;

	    // Call dassert(filename, line)
	    // Get sole parameter, linnum
	    {
		Symbol *sp;

		sp = symbol_calloc("linnum");
		sp->Stype = type_fake(TYint);
		sp->Stype->Tcount++;
		sp->Sclass = SCfastpar;
		sp->Spreg = AX;
		sp->Sflags &= ~SFLspill;
		sp->Sfl = FLpara;	// FLauto?
		cstate.CSpsymtab = &ma->Sfunc->Flocsym;
		symbol_add(sp);

		elinnum = el_var(sp);
	    }

	    efilename = toEmodulename();

	    elem *e = el_var(rtlsym[i ? RTLSYM_DARRAY : RTLSYM_DASSERT]);
	    e = el_bin(OPcall, TYvoid, e, el_param(elinnum, efilename));

	    block *b = block_calloc();
	    b->BC = BCret;
	    b->Belem = e;
	    ma->Sfunc->Fstartblock = b;
	    ma->Sclass = SCglobal;
	    ma->Sfl = 0;
	    writefunc(ma);
	}
    }

#if 1
    // Always generate module info, because of templates and -cov
    if (1 || needModuleInfo())
	genmoduleinfo();
#endif
    
    obj_termfile();
}


/* ================================================================== */

void FuncDeclaration::toObjFile(int multiobj)
{
    Symbol *s;
    func_t *f;
    Symbol *senter;
    Symbol *sexit;
    FuncDeclaration *func = this;
    ClassDeclaration *cd = func->parent->isClassDeclaration();
    int reverse;
    int i;
    int has_arguments;

    //printf("FuncDeclaration::toObjFile(%p, %s.%s)\n", func, parent->toChars(), func->toChars());
#if 0
    //printf("line = %d\n",func->getWhere() / LINEINC);
    EEcontext *ee = env->getEEcontext();
    if (ee->EEcompile == 2)
    {
	if (ee->EElinnum < (func->getWhere() / LINEINC) ||
	    ee->EElinnum > (func->endwhere / LINEINC)
	   )
	    return;		// don't compile this function
	ee->EEfunc = func->toSymbol();
    }
#endif

    if (multiobj && !isStaticDtorDeclaration() && !isStaticCtorDeclaration())
    {	obj_append(this);
	return;
    }

    if (semanticRun >= 5)	// if toObjFile() already run
	return;
    semanticRun = 5;

    if (!func->fbody)
    {
	return;
    }
    if (func->isUnitTestDeclaration() && !global.params.useUnitTests)
	return;

    if (global.params.verbose)
	printf("function  %s\n",func->toChars());

    s = func->toSymbol();
    f = s->Sfunc;

#if TARGET_WINDOS
    /* This is done so that the 'this' pointer on the stack is the same
     * distance away from the function parameters, so that an overriding
     * function can call the nested fdensure or fdrequire of its overridden function
     * and the stack offsets are the same.
     */
    if (isVirtual() && (fensure || frequire))
	f->Fflags3 |= Ffakeeh;
#endif

#if TARGET_OSX
    s->Sclass = SCcomdat;
#else
    s->Sclass = SCglobal;
#endif
    for (Dsymbol *p = parent; p; p = p->parent)
    {
	if (p->isTemplateInstance())
	{
	    s->Sclass = SCcomdat;
	    break;
	}
    }

    if (isNested())
    {
//	if (!(config.flags3 & CFG3pic))
//	    s->Sclass = SCstatic;
	f->Fflags3 |= Fnested;
    }
    else
    {
	const char *libname = (global.params.symdebug)
				? global.params.debuglibname
				: global.params.defaultlibname;

	// Pull in RTL startup code
	if (func->isMain())
	{   objextdef("_main");
#if TARGET_LINUX || TARGET_OSX || TARGET_FREEBSD || TARGET_SOLARIS
	    obj_ehsections();	// initialize exception handling sections
#else
	    objextdef("__acrtused_con");
#endif
	    obj_includelib(libname);
	    s->Sclass = SCglobal;
	}
	else if (strcmp(s->Sident, "main") == 0 && linkage == LINKc)
	    s->Sclass = SCglobal;

	else if (func->isWinMain())
	{
	    objextdef("__acrtused");
	    obj_includelib(libname);
	    s->Sclass = SCglobal;
	}

	// Pull in RTL startup code
	else if (func->isDllMain())
	{
	    objextdef("__acrtused_dll");
	    obj_includelib(libname);
	    s->Sclass = SCglobal;
	}
    }

    cstate.CSpsymtab = &f->Flocsym;

    // Find module m for this function
    Module *m = NULL;
    for (Dsymbol *p = parent; p; p = p->parent)
    {
	m = p->isModule();
	if (m)
	    break;
    }

    IRState irs(m, func);
    Array deferToObj;			// write these to OBJ file later
    irs.deferToObj = &deferToObj;

    TypeFunction *tf;
    enum RET retmethod;
    symbol *shidden = NULL;
    Symbol *sthis = NULL;
    tym_t tyf;

    tyf = tybasic(s->Stype->Tty);
    //printf("linkage = %d, tyf = x%x\n", linkage, tyf);
    reverse = tyrevfunc(s->Stype->Tty);

    assert(func->type->ty == Tfunction);
    tf = (TypeFunction *)(func->type);
    has_arguments = (tf->linkage == LINKd) && (tf->varargs == 1);
    retmethod = tf->retStyle();
    if (retmethod == RETstack)
    {
	// If function returns a struct, put a pointer to that
	// as the first argument
	::type *thidden = tf->next->pointerTo()->toCtype();
	char hiddenparam[5+4+1];
	static int hiddenparami;    // how many we've generated so far

	sprintf(hiddenparam,"__HID%d",++hiddenparami);
	shidden = symbol_name(hiddenparam,SCparameter,thidden);
	shidden->Sflags |= SFLtrue | SFLfree;
#if DMDV1
	if (func->nrvo_can && func->nrvo_var && func->nrvo_var->nestedref)
#else
	if (func->nrvo_can && func->nrvo_var && func->nrvo_var->nestedrefs.dim)
#endif
	    type_setcv(&shidden->Stype, shidden->Stype->Tty | mTYvolatile);
	irs.shidden = shidden;
	this->shidden = shidden;
    }

    if (vthis)
    {
	assert(!vthis->csym);
	sthis = vthis->toSymbol();
	irs.sthis = sthis;
	if (!(f->Fflags3 & Fnested))
	    f->Fflags3 |= Fmember;
    }

    Symbol **params;
    unsigned pi;

    // Estimate number of parameters, pi
    pi = (v_arguments != NULL);
    if (parameters)
	pi += parameters->dim;
    // Allow extra 2 for sthis and shidden
    params = (Symbol **)alloca((pi + 2) * sizeof(Symbol *));

    // Get the actual number of parameters, pi, and fill in the params[]
    pi = 0;
    if (v_arguments)
    {
	params[pi] = v_arguments->toSymbol();
	pi += 1;
    }
    if (parameters)
    {
	for (i = 0; i < parameters->dim; i++)
	{   VarDeclaration *v = (VarDeclaration *)parameters->data[i];
	    if (v->csym)
	    {
		error("compiler error, parameter '%s', bugzilla 2962?", v->toChars());
		assert(0);
	    }
	    params[pi + i] = v->toSymbol();
	}
	pi += i;
    }

    if (reverse)
    {	// Reverse params[] entries
	for (i = 0; i < pi/2; i++)
	{   Symbol *sptmp;

	    sptmp = params[i];
	    params[i] = params[pi - 1 - i];
	    params[pi - 1 - i] = sptmp;
	}
    }

    if (shidden)
    {
#if 0
	// shidden becomes last parameter
	params[pi] = shidden;
#else
	// shidden becomes first parameter
	memmove(params + 1, params, pi * sizeof(params[0]));
	params[0] = shidden;
#endif
	pi++;
    }


    if (sthis)
    {
#if 0
	// sthis becomes last parameter
	params[pi] = sthis;
#else
	// sthis becomes first parameter
	memmove(params + 1, params, pi * sizeof(params[0]));
	params[0] = sthis;
#endif
	pi++;
    }

    if ((global.params.isLinux || global.params.isOSX || global.params.isFreeBSD || global.params.isSolaris) &&
	 linkage != LINKd && shidden && sthis)
    {
	/* swap shidden and sthis
	 */
	Symbol *sp = params[0];
	params[0] = params[1];
	params[1] = sp;
    }

    for (i = 0; i < pi; i++)
    {	Symbol *sp = params[i];
	sp->Sclass = SCparameter;
	sp->Sflags &= ~SFLspill;
	sp->Sfl = FLpara;
	symbol_add(sp);
    }

    // First parameter goes in register
    if (pi)
    {
	Symbol *sp = params[0];
	if ((tyf == TYjfunc || tyf == TYmfunc) &&
	    type_jparam(sp->Stype))
	{   sp->Sclass = SCfastpar;
	    sp->Spreg = (tyf == TYjfunc) ? AX : CX;
	    sp->Sfl = FLauto;
	    //printf("'%s' is SCfastpar\n",sp->Sident);
	}
    }

    if (func->fbody)
    {   block *b;
	Blockx bx;
	Statement *sbody;

	localgot = NULL;

	sbody = func->fbody;
	memset(&bx,0,sizeof(bx));
	bx.startblock = block_calloc();
	bx.curblock = bx.startblock;
	bx.funcsym = s;
	bx.scope_index = -1;
	bx.classdec = cd;
	bx.member = func;
	bx.module = getModule();
	irs.blx = &bx;

	buildClosure(&irs);

#if 0
	if (func->isSynchronized())
	{
	    if (cd)
	    {	elem *esync;
		if (func->isStatic())
		{   // monitor is in ClassInfo
		    esync = el_ptr(cd->toSymbol());
		}
		else
		{   // 'this' is the monitor
		    esync = el_var(sthis);
		}

		if (func->isStatic() || sbody->usesEH() ||
		    !(config.flags2 & CFG2seh))
		{   // BUG: what if frequire or fensure uses EH?

		    sbody = new SynchronizedStatement(func->loc, esync, sbody);
		}
		else
		{
#if TARGET_WINDOS
		    if (config.flags2 & CFG2seh)
		    {
			/* The "jmonitor" uses an optimized exception handling frame
			 * which is a little shorter than the more general EH frame.
			 * It isn't strictly necessary.
			 */
			s->Sfunc->Fflags3 |= Fjmonitor;
		    }
#endif
		    el_free(esync);
		}
	    }
	    else
	    {
		error("synchronized function %s must be a member of a class", func->toChars());
	    }
	}
#elif TARGET_WINDOS
	if (func->isSynchronized() && cd && config.flags2 & CFG2seh &&
	    !func->isStatic() && !sbody->usesEH())
	{
	    /* The "jmonitor" hack uses an optimized exception handling frame
	     * which is a little shorter than the more general EH frame.
	     */
	    s->Sfunc->Fflags3 |= Fjmonitor;
	}
#endif

	sbody->toIR(&irs);
	bx.curblock->BC = BCret;

	f->Fstartblock = bx.startblock;
//	einit = el_combine(einit,bx.init);

	if (isCtorDeclaration())
	{
	    assert(sthis);
	    for (b = f->Fstartblock; b; b = b->Bnext)
	    {
		if (b->BC == BCret)
		{
		    b->BC = BCretexp;
		    b->Belem = el_combine(b->Belem, el_var(sthis));
		}
	    }
	} 
    }

    // If static constructor
    if (isStaticConstructor())
    {
	elem *e = el_una(OPucall, TYvoid, el_var(s));
	ector = el_combine(ector, e);
    }

    // If static destructor
    if (isStaticDestructor())
    {
	elem *e;

#if STATICCTOR
	e = el_bin(OPcall, TYvoid, el_var(rtlsym[RTLSYM_FATEXIT]), el_ptr(s));
	ector = el_combine(ector, e);
	dtorcount++;
#else
	StaticDtorDeclaration *f = isStaticDtorDeclaration();
	assert(f);
	if (f->vgate)
	{   /* Increment destructor's vgate at construction time
	     */
	    ectorgates.push(f);
	}

	e = el_una(OPucall, TYvoid, el_var(s));
	edtor = el_combine(e, edtor);
#endif
    }

    // If unit test
    if (isUnitTestDeclaration())
    {
	elem *e = el_una(OPucall, TYvoid, el_var(s));
	etest = el_combine(etest, e);
    }

    if (global.errors)
	return;

    writefunc(s);
    if (isExport())
	obj_export(s, Poffset);

    for (i = 0; i < irs.deferToObj->dim; i++)
    {
	Dsymbol *s = (Dsymbol *)irs.deferToObj->data[i];
	s->toObjFile(0);
    }

#if TARGET_LINUX || TARGET_OSX || TARGET_FREEBSD || TARGET_SOLARIS
    // A hack to get a pointer to this function put in the .dtors segment
    if (ident && memcmp(ident->toChars(), "_STD", 4) == 0)
	obj_staticdtor(s);
#endif
#if DMDV2
    if (irs.startaddress)
    {
	printf("Setting start address\n");
	obj_startaddress(irs.startaddress);
    }
#endif
}

/* ================================================================== */

/*****************************
 * Return back end type corresponding to D front end type.
 */

unsigned Type::totym()
{   unsigned t;

    switch (ty)
    {
	case Tvoid:	t = TYvoid;	break;
	case Tint8:	t = TYschar;	break;
	case Tuns8:	t = TYuchar;	break;
	case Tint16:	t = TYshort;	break;
	case Tuns16:	t = TYushort;	break;
	case Tint32:	t = TYint;	break;
	case Tuns32:	t = TYuint;	break;
	case Tint64:	t = TYllong;	break;
	case Tuns64:	t = TYullong;	break;
	case Tfloat32:	t = TYfloat;	break;
	case Tfloat64:	t = TYdouble;	break;
	case Tfloat80:	t = TYldouble;	break;
	case Timaginary32: t = TYifloat; break;
	case Timaginary64: t = TYidouble; break;
	case Timaginary80: t = TYildouble; break;
	case Tcomplex32: t = TYcfloat;	break;
	case Tcomplex64: t = TYcdouble;	break;
	case Tcomplex80: t = TYcldouble; break;
	//case Tbit:	t = TYuchar;	break;
	case Tbool:	t = TYbool;	break;
	case Tchar:	t = TYchar;	break;
#if TARGET_LINUX || TARGET_OSX || TARGET_FREEBSD || TARGET_SOLARIS
	case Twchar:	t = TYwchar_t;	break;
	case Tdchar:	t = TYdchar;	break;
#else
	case Twchar:	t = TYwchar_t;	break;
	case Tdchar:
		t = (global.params.symdebug == 1) ? TYdchar : TYulong;
		break;
#endif

	case Taarray:	t = TYaarray;	break;
	case Tclass:
	case Treference:
	case Tpointer:	t = TYnptr;	break;
	case Tdelegate:	t = TYdelegate;	break;
	case Tarray:	t = TYdarray;	break;
#if SARRAYVALUE
	case Tsarray:	t = TYstruct;	break;
#else
	case Tsarray:	t = TYarray;	break;
#endif
	case Tstruct:	t = TYstruct;	break;

	case Tenum:
	case Ttypedef:
	     t = toBasetype()->totym();
	     break;

	case Tident:
	case Ttypeof:
#ifdef DEBUG
	    printf("ty = %d, '%s'\n", ty, toChars());
#endif
	    error(0, "forward reference of %s", toChars());
	    t = TYint;
	    break;

	default:
#ifdef DEBUG
	    printf("ty = %d, '%s'\n", ty, toChars());
	    halt();
#endif
	    assert(0);
    }

#if DMDV2
    // Add modifiers
    switch (mod)
    {
	case 0:
	    break;
	case MODconst:
	    t |= mTYconst;
	    break;
	case MODinvariant:
	    t |= mTYimmutable;
	    break;
	case MODshared:
	    t |= mTYshared;
	    break;
	case MODshared | MODconst:
	    t |= mTYshared | mTYconst;
	    break;
	default:
	    assert(0);
    }
#endif

    return t;
}

unsigned TypeFunction::totym()
{
    tym_t tyf;

    //printf("TypeFunction::totym(), linkage = %d\n", linkage);
    switch (linkage)
    {
	case LINKwindows:
	    tyf = (varargs == 1) ? TYnfunc : TYnsfunc;
	    break;

	case LINKpascal:
	    tyf = (varargs == 1) ? TYnfunc : TYnpfunc;
	    break;

	case LINKc:
	    tyf = TYnfunc;
#if TARGET_LINUX || TARGET_OSX || TARGET_FREEBSD || TARGET_SOLARIS
	    if (retStyle() == RETstack)
		tyf = TYhfunc;
#endif
	    break;

	case LINKd:
	    tyf = (varargs == 1) ? TYnfunc : TYjfunc;
	    break;

	case LINKcpp:
	    tyf = TYnfunc;
	    break;

	default:
	    printf("linkage = %d\n", linkage);
	    assert(0);
    }
#if DMDV2
    if (isnothrow)
	tyf |= mTYnothrow;
#endif
    return tyf;
}

/**************************************
 */

Symbol *Type::toSymbol()
{
    assert(0);
    return NULL;
}

Symbol *TypeClass::toSymbol()
{
    return sym->toSymbol();
}

/*************************************
 * Generate symbol in data segment for critical section.
 */

Symbol *Module::gencritsec()
{
    Symbol *s;
    type *t;

    t = Type::tint32->toCtype();
    s = symbol_name("critsec", SCstatic, t);
    s->Sfl = FLdata;
    /* Must match D_CRITICAL_SECTION in phobos/internal/critical.c
     */
    dtnzeros(&s->Sdt, PTRSIZE + os_critsecsize());
#if ELFOBJ || MACHOBJ // Burton
    s->Sseg = DATA;
#endif
    outdata(s);
    return s;
}

/**************************************
 * Generate elem that is a pointer to the module file name.
 */

elem *Module::toEmodulename()
{
   elem *efilename;

    // Get filename
    if (needModuleInfo())
    {   Symbol *si;

	/* Class ModuleInfo is defined in std.moduleinfo.
	 * The first member is the name of it, char name[],
	 * which will be at offset 8.
	 */

	si = toSymbol();
#if 1
	// Use this instead so -fPIC will work
	efilename = el_ptr(si);
	efilename = el_bin(OPadd, TYnptr, efilename, el_long(TYuint, 8));
	efilename = el_una(OPind, TYdarray, efilename);
#else
	efilename = el_var(si);
	efilename->Ety = TYdarray;
	efilename->EV.sp.Voffset += 8;
#endif
    }
    else // generate our own filename
    {
	efilename = toEfilename();
    }
    return efilename;
}

elem *Module::toEfilename()
{   elem *efilename;

    if (!sfilename)
    {
	dt_t *dt = NULL;
	char *id;
	int len;

	id = srcfile->toChars();
	len = strlen(id);
	dtdword(&dt, len);
	dtabytes(&dt,TYnptr, 0, len + 1, id);

	sfilename = symbol_generate(SCstatic,type_fake(TYdarray));
	sfilename->Sdt = dt;
	sfilename->Sfl = FLdata;
#if ELFOBJ
	sfilename->Sseg = CDATA;
#endif
#if MACHOBJ
	// Because of PIC and CDATA being in the _TEXT segment, cannot
	// have pointers in CDATA
	sfilename->Sseg = DATA;
#endif
	outdata(sfilename);
    }

    efilename = el_var(sfilename);
    return efilename;
}


