// Compiler implementation of the D programming language
// Copyright (c) 1999-2009 by Digital Mars
// All Rights Reserved
// written by Walter Bright
// http://www.digitalmars.com
// License for redistribution is by either the Artistic License
// in artistic.txt, or the GNU General Public License in gnu.txt.
// See the included readme.txt for details.

#include <stdio.h>
#include <assert.h>

#include "mars.h"
#include "init.h"
#include "declaration.h"
#include "attrib.h"
#include "expression.h"
#include "scope.h"
#include "mtype.h"
#include "aggregate.h"
#include "identifier.h"
#include "id.h"
#include "module.h"
#include "statement.h"
#include "template.h"
#include "hdrgen.h"

#ifdef IN_GCC
#include "d-dmd-gcc.h"
#endif

/********************************* FuncDeclaration ****************************/

FuncDeclaration::FuncDeclaration(Loc loc, Loc endloc, Identifier *id, enum STC storage_class, Type *type)
    : Declaration(id)
{
    //printf("FuncDeclaration(id = '%s', type = %p)\n", id->toChars(), type);
    //printf("storage_class = x%x\n", storage_class);
    this->storage_class = storage_class;
    this->type = type;
    this->loc = loc;
    this->endloc = endloc;
    fthrows = NULL;
    frequire = NULL;
    fdrequire = NULL;
    fdensure = NULL;
    outId = NULL;
    vresult = NULL;
    returnLabel = NULL;
    fensure = NULL;
    fbody = NULL;
    localsymtab = NULL;
    vthis = NULL;
    v_arguments = NULL;
#if IN_GCC
    v_argptr = NULL;
#endif
    parameters = NULL;
    labtab = NULL;
    overnext = NULL;
    vtblIndex = -1;
    hasReturnExp = 0;
    naked = 0;
    inlineStatus = ILSuninitialized;
    inlineNest = 0;
    inlineAsm = 0;
    cantInterpret = 0;
    semanticRun = 0;
#if DMDV1
    nestedFrameRef = 0;
#endif
    fes = NULL;
    introducing = 0;
    tintro = NULL;
    /* The type given for "infer the return type" is a TypeFunction with
     * NULL for the return type.
     */
    inferRetType = (type && type->nextOf() == NULL);
    hasReturnExp = 0;
    nrvo_can = 1;
    nrvo_var = NULL;
    shidden = NULL;
#if DMDV2
    builtin = BUILTINunknown;
    tookAddressOf = 0;
#endif
}

Dsymbol *FuncDeclaration::syntaxCopy(Dsymbol *s)
{
    FuncDeclaration *f;

    //printf("FuncDeclaration::syntaxCopy('%s')\n", toChars());
    if (s)
	f = (FuncDeclaration *)s;
    else
	f = new FuncDeclaration(loc, endloc, ident, (enum STC) storage_class, type->syntaxCopy());
    f->outId = outId;
    f->frequire = frequire ? frequire->syntaxCopy() : NULL;
    f->fensure  = fensure  ? fensure->syntaxCopy()  : NULL;
    f->fbody    = fbody    ? fbody->syntaxCopy()    : NULL;
    assert(!fthrows); // deprecated
    return f;
}


// Do the semantic analysis on the external interface to the function.

void FuncDeclaration::semantic(Scope *sc)
{   TypeFunction *f;
    StructDeclaration *sd;
    ClassDeclaration *cd;
    InterfaceDeclaration *id;
    Dsymbol *pd;

#if 0
    printf("FuncDeclaration::semantic(sc = %p, this = %p, '%s', linkage = %d)\n", sc, this, toPrettyChars(), sc->linkage);
    if (isFuncLiteralDeclaration())
	printf("\tFuncLiteralDeclaration()\n");
    printf("sc->parent = %s, parent = %s\n", sc->parent->toChars(), parent ? parent->toChars() : "");
    printf("type: %p, %s\n", type, type->toChars());
#endif

    if (semanticRun && isFuncLiteralDeclaration())
    {
	/* Member functions that have return types that are
	 * forward references can have semantic() run more than
	 * once on them.
	 * See test\interface2.d, test20
	 */
	return;
    }
    assert(semanticRun <= 1);
    semanticRun = 1;

    storage_class |= sc->stc & ~STCref;
    //printf("function storage_class = x%x\n", storage_class);

    if (!originalType)
	originalType = type;
    if (!type->deco)
    {
	/* Apply const and invariant storage class
	 * to the function type
	 */
	type = type->semantic(loc, sc);
	unsigned stc = storage_class;
	if (type->isInvariant())
	    stc |= STCimmutable;
	if (type->isConst())
	    stc |= STCconst;
	if (type->isShared() || storage_class & STCsynchronized)
	    stc |= STCshared;
	switch (stc & STC_TYPECTOR)
	{
	    case STCimmutable:
	    case STCimmutable | STCconst:
	    case STCimmutable | STCconst | STCshared:
	    case STCimmutable | STCshared:
		// Don't use toInvariant(), as that will do a merge()
		type = type->makeInvariant();
		goto Lmerge;

	    case STCconst:
		type = type->makeConst();
		goto Lmerge;

	    case STCshared | STCconst:
		type = type->makeSharedConst();
		goto Lmerge;

	    case STCshared:
		type = type->makeShared();
	    Lmerge:
		if (!(type->ty == Tfunction && !type->nextOf()))
		    /* Can't do merge if return type is not known yet
		     */
		    type->deco = type->merge()->deco;
		break;

	    case 0:
		break;

	    default:
		assert(0);
	}
    }
    //type->print();
    if (type->ty != Tfunction)
    {
	error("%s must be a function", toChars());
	return;
    }
    f = (TypeFunction *)(type);
    size_t nparams = Argument::dim(f->parameters);

    linkage = sc->linkage;
//    if (!parent)
    {
	//parent = sc->scopesym;
	parent = sc->parent;
    }
    protection = sc->protection;
    Dsymbol *parent = toParent();

    if (storage_class & STCscope)
	error("functions cannot be scope");

    if (isAbstract() && !isVirtual())
	error("non-virtual functions cannot be abstract");

    if ((f->isConst() || f->isInvariant()) && !isThis())
	error("without 'this' cannot be const/immutable");

    if (isAbstract() && isFinal())
	error("cannot be both final and abstract");
#if 0
    if (isAbstract() && fbody)
	error("abstract functions cannot have bodies");
#endif

#if 0
    if (isStaticConstructor() || isStaticDestructor())
    {
	if (!isStatic() || type->nextOf()->ty != Tvoid)
	    error("static constructors / destructors must be static void");
	if (f->arguments && f->arguments->dim)
	    error("static constructors / destructors must have empty parameter list");
	// BUG: check for invalid storage classes
    }
#endif

#ifdef IN_GCC
    AggregateDeclaration *ad;

    ad = parent->isAggregateDeclaration();
    if (ad)
	ad->methods.push(this);
#endif
    sd = parent->isStructDeclaration();
    if (sd)
    {
	if (isCtorDeclaration())
	{
	    return;
	}
#if 0
	// Verify no constructors, destructors, etc.
	if (isCtorDeclaration()
	    //||isDtorDeclaration()
	    //|| isInvariantDeclaration()
	    //|| isUnitTestDeclaration()
	   )
	{
	    error("special member functions not allowed for %ss", sd->kind());
	}

	if (!sd->inv)
	    sd->inv = isInvariantDeclaration();

	if (!sd->aggNew)
	    sd->aggNew = isNewDeclaration();

	if (isDelete())
	{
	    if (sd->aggDelete)
		error("multiple delete's for struct %s", sd->toChars());
	    sd->aggDelete = (DeleteDeclaration *)(this);
	}
#endif
    }

    id = parent->isInterfaceDeclaration();
    if (id)
    {
	storage_class |= STCabstract;

	if (isCtorDeclaration() ||
#if DMDV2
	    isPostBlitDeclaration() ||
#endif
	    isDtorDeclaration() ||
	    isInvariantDeclaration() ||
	    isUnitTestDeclaration() || isNewDeclaration() || isDelete())
	    error("special function not allowed in interface %s", id->toChars());
	if (fbody)
	    error("function body is not abstract in interface %s", id->toChars());
    }

    /* Template member functions aren't virtual:
     *   interface TestInterface { void tpl(T)(); }
     * and so won't work in interfaces
     */
    if ((pd = toParent()) != NULL &&
	pd->isTemplateInstance() &&
	(pd = toParent2()) != NULL &&
	(id = pd->isInterfaceDeclaration()) != NULL)
    {
	error("template member function not allowed in interface %s", id->toChars());
    }

    cd = parent->isClassDeclaration();
    if (cd)
    {	int vi;
	CtorDeclaration *ctor;
	DtorDeclaration *dtor;
	InvariantDeclaration *inv;

	if (isCtorDeclaration())
	{
//	    ctor = (CtorDeclaration *)this;
//	    if (!cd->ctor)
//		cd->ctor = ctor;
	    return;
	}

#if 0
	dtor = isDtorDeclaration();
	if (dtor)
	{
	    if (cd->dtor)
		error("multiple destructors for class %s", cd->toChars());
	    cd->dtor = dtor;
	}

	inv = isInvariantDeclaration();
	if (inv)
	{
	    cd->inv = inv;
	}

	if (isNewDeclaration())
	{
	    if (!cd->aggNew)
		cd->aggNew = (NewDeclaration *)(this);
	}

	if (isDelete())
	{
	    if (cd->aggDelete)
		error("multiple delete's for class %s", cd->toChars());
	    cd->aggDelete = (DeleteDeclaration *)(this);
	}
#endif

	if (storage_class & STCabstract)
	    cd->isabstract = 1;

	// if static function, do not put in vtbl[]
	if (!isVirtual())
	{
	    //printf("\tnot virtual\n");
	    goto Ldone;
	}

	// Find index of existing function in vtbl[] to override
	vi = findVtblIndex(&cd->vtbl, cd->baseClass ? cd->baseClass->vtbl.dim : 0);
	switch (vi)
	{
	    case -1:
		/* Didn't find one, so
		 * This is an 'introducing' function which gets a new
		 * slot in the vtbl[].
		 */

		// Verify this doesn't override previous final function
		if (cd->baseClass)
		{   Dsymbol *s = cd->baseClass->search(loc, ident, 0);
		    if (s)
		    {
			FuncDeclaration *f = s->isFuncDeclaration();
			f = f->overloadExactMatch(type);
			if (f && f->isFinal() && f->prot() != PROTprivate)
			    error("cannot override final function %s", f->toPrettyChars());
		    }
		}

		if (isFinal())
		{
		    if (isOverride())
			error("does not override any function");
		    cd->vtblFinal.push(this);
		}
		else
		{
		    // Append to end of vtbl[]
		    //printf("\tintroducing function\n");
		    introducing = 1;
		    vi = cd->vtbl.dim;
		    cd->vtbl.push(this);
		    vtblIndex = vi;
		}
		break;

	    case -2:	// can't determine because of fwd refs
		cd->sizeok = 2;	// can't finish due to forward reference
		return;

	    default:
	    {   FuncDeclaration *fdv = (FuncDeclaration *)cd->vtbl.data[vi];
		// This function is covariant with fdv
		if (fdv->isFinal())
		    error("cannot override final function %s", fdv->toPrettyChars());

#if DMDV2
		if (!isOverride())
		    warning(loc, "overrides base class function %s, but is not marked with 'override'", fdv->toPrettyChars());
#endif

		if (fdv->toParent() == parent)
		{
		    // If both are mixins, then error.
		    // If either is not, the one that is not overrides
		    // the other.
		    if (fdv->parent->isClassDeclaration())
			break;
		    if (!this->parent->isClassDeclaration()
#if !BREAKABI
			&& !isDtorDeclaration()
#endif
#if DMDV2
			&& !isPostBlitDeclaration()
#endif
			)
			error("multiple overrides of same function");
		}
		cd->vtbl.data[vi] = (void *)this;
		vtblIndex = vi;

		/* Remember which functions this overrides
		 */
		foverrides.push(fdv);

		/* This works by whenever this function is called,
		 * it actually returns tintro, which gets dynamically
		 * cast to type. But we know that tintro is a base
		 * of type, so we could optimize it by not doing a
		 * dynamic cast, but just subtracting the isBaseOf()
		 * offset if the value is != null.
		 */

		if (fdv->tintro)
		    tintro = fdv->tintro;
		else if (!type->equals(fdv->type))
		{
		    /* Only need to have a tintro if the vptr
		     * offsets differ
		     */
		    int offset;
		    if (fdv->type->nextOf()->isBaseOf(type->nextOf(), &offset))
		    {
			tintro = fdv->type;
		    }
		}
		break;
	    }
	}

	/* Go through all the interface bases.
	 * If this function is covariant with any members of those interface
	 * functions, set the tintro.
	 */
	for (int i = 0; i < cd->interfaces_dim; i++)
	{
	    BaseClass *b = cd->interfaces[i];
	    vi = findVtblIndex(&b->base->vtbl, b->base->vtbl.dim);
	    switch (vi)
	    {
		case -1:
		    break;

		case -2:
		    cd->sizeok = 2;	// can't finish due to forward reference
		    return;

		default:
		{   FuncDeclaration *fdv = (FuncDeclaration *)b->base->vtbl.data[vi];
		    Type *ti = NULL;

		    /* Remember which functions this overrides
		     */
		    foverrides.push(fdv);

		    if (fdv->tintro)
			ti = fdv->tintro;
		    else if (!type->equals(fdv->type))
		    {
			/* Only need to have a tintro if the vptr
			 * offsets differ
			 */
			int offset;
			if (fdv->type->nextOf()->isBaseOf(type->nextOf(), &offset))
			{
			    ti = fdv->type;
#if 0
			    if (offset)
				ti = fdv->type;
			    else if (type->nextOf()->ty == Tclass)
			    {   ClassDeclaration *cdn = ((TypeClass *)type->nextOf())->sym;
				if (cdn && cdn->sizeok != 1)
				    ti = fdv->type;
			    }
#endif
			}
		    }
		    if (ti)
		    {
			if (tintro && !tintro->equals(ti))
			{
			    error("incompatible covariant types %s and %s", tintro->toChars(), ti->toChars());
			}
			tintro = ti;
		    }
		    goto L2;
		}
	    }
	}

	if (introducing && isOverride())
	{
	    error("does not override any function");
	}

    L2: ;
    }
    else if (isOverride() && !parent->isTemplateInstance())
	error("override only applies to class member functions");

    /* Do not allow template instances to add virtual functions
     * to a class.
     */
    if (isVirtual())
    {
	TemplateInstance *ti = parent->isTemplateInstance();
	if (ti)
	{
	    // Take care of nested templates
	    while (1)
	    {
		TemplateInstance *ti2 = ti->tempdecl->parent->isTemplateInstance();
		if (!ti2)
		    break;
		ti = ti2;
	    }

	    // If it's a member template
	    ClassDeclaration *cd = ti->tempdecl->isClassMember();
	    if (cd)
	    {
		error("cannot use template to add virtual function to class '%s'", cd->toChars());
	    }
	}
    }

    if (isMain())
    {
	// Check parameters to see if they are either () or (char[][] args)
	switch (nparams)
	{
	    case 0:
		break;

	    case 1:
	    {
		Argument *arg0 = Argument::getNth(f->parameters, 0);
		if (arg0->type->ty != Tarray ||
		    arg0->type->nextOf()->ty != Tarray ||
		    arg0->type->nextOf()->nextOf()->ty != Tchar ||
		    arg0->storageClass & (STCout | STCref | STClazy))
		    goto Lmainerr;
		break;
	    }

	    default:
		goto Lmainerr;
	}

	if (!f->nextOf())
	    error("must return int or void");
	else if (f->nextOf()->ty != Tint32 && f->nextOf()->ty != Tvoid)
	    error("must return int or void, not %s", f->nextOf()->toChars());
	if (f->varargs)
	{
	Lmainerr:
	    error("parameters must be main() or main(char[][] args)");
	}
    }

    if (ident == Id::assign && (sd || cd))
    {	// Disallow identity assignment operator.

	// opAssign(...)
	if (nparams == 0)
	{   if (f->varargs == 1)
		goto Lassignerr;
	}
	else
	{
	    Argument *arg0 = Argument::getNth(f->parameters, 0);
	    Type *t0 = arg0->type->toBasetype();
	    Type *tb = sd ? sd->type : cd->type;
	    if (arg0->type->implicitConvTo(tb) ||
		(sd && t0->ty == Tpointer && t0->nextOf()->implicitConvTo(tb))
	       )
	    {
		if (nparams == 1)
		    goto Lassignerr;
		Argument *arg1 = Argument::getNth(f->parameters, 1);
		if (arg1->defaultArg)
		    goto Lassignerr;
	    }
	}
    }

    if (isVirtual())
    {
	/* Rewrite contracts as nested functions, then call them.
	 * Doing it as nested functions means that overriding functions
	 * can call them.
	 */
	if (frequire)
	{   /*   in { ... }
	     * becomes:
	     *   void __require() { ... }
	     *   __require();
	     */
	    Loc loc = frequire->loc;
	    TypeFunction *tf = new TypeFunction(NULL, Type::tvoid, 0, LINKd);
	    FuncDeclaration *fd = new FuncDeclaration(loc, loc,
		Id::require, STCundefined, tf);
	    fd->fbody = frequire;
	    Statement *s1 = new DeclarationStatement(loc, fd);
	    Expression *e = new CallExp(loc, new VarExp(loc, fd, 0), (Expressions *)NULL);
	    Statement *s2 = new ExpStatement(loc, e);
	    frequire = new CompoundStatement(loc, s1, s2);
	    fdrequire = fd;
	}

	if (fensure)
	{   /*   out (result) { ... }
	     * becomes:
	     *   tret __ensure(ref tret result) { ... }
	     *   __ensure(result);
	     */
	    if (!outId && f->nextOf()->toBasetype()->ty != Tvoid)
		outId = Id::result;	// provide a default

	    Loc loc = fensure->loc;
	    Arguments *arguments = new Arguments();
	    Argument *a = NULL;
	    if (outId)
	    {	a = new Argument(STCref, f->nextOf(), outId, NULL);
		arguments->push(a);
	    }
	    TypeFunction *tf = new TypeFunction(arguments, Type::tvoid, 0, LINKd);
	    FuncDeclaration *fd = new FuncDeclaration(loc, loc,
		Id::ensure, STCundefined, tf);
	    fd->fbody = fensure;
	    Statement *s1 = new DeclarationStatement(loc, fd);
	    Expression *eresult = NULL;
	    if (outId)
		eresult = new IdentifierExp(loc, outId);
	    Expression *e = new CallExp(loc, new VarExp(loc, fd, 0), eresult);
	    Statement *s2 = new ExpStatement(loc, e);
	    fensure = new CompoundStatement(loc, s1, s2);
	    fdensure = fd;
	}
    }

Ldone:
    /* Save scope for possible later use (if we need the
     * function internals)
     */
    scope = new Scope(*sc);
    scope->setNoFree();
    return;

Lassignerr:
    if (sd)
    {
	sd->hasIdentityAssign = 1;	// don't need to generate it
	goto Ldone;
    }
    error("identity assignment operator overload is illegal");
}

void FuncDeclaration::semantic2(Scope *sc)
{
}

// Do the semantic analysis on the internals of the function.

void FuncDeclaration::semantic3(Scope *sc)
{   TypeFunction *f;
    VarDeclaration *argptr = NULL;
    VarDeclaration *_arguments = NULL;

    if (!parent)
    {
	if (global.errors)
	    return;
	//printf("FuncDeclaration::semantic3(%s '%s', sc = %p)\n", kind(), toChars(), sc);
	assert(0);
    }
    //printf("FuncDeclaration::semantic3('%s.%s', sc = %p, loc = %s)\n", parent->toChars(), toChars(), sc, loc.toChars());
    //fflush(stdout);
    //printf("storage class = x%x %x\n", sc->stc, storage_class);
    //{ static int x; if (++x == 2) *(char*)0=0; }
    //printf("\tlinkage = %d\n", sc->linkage);

    //printf(" sc->incontract = %d\n", sc->incontract);
    if (semanticRun >= 3)
	return;
    semanticRun = 3;

    if (!type || type->ty != Tfunction)
	return;
    f = (TypeFunction *)(type);

    // Check the 'throws' clause
    if (fthrows)
    {
	for (int i = 0; i < fthrows->dim; i++)
	{
	    Type *t = (Type *)fthrows->data[i];

	    t = t->semantic(loc, sc);
	    if (!t->isClassHandle())
		error("can only throw classes, not %s", t->toChars());
	}
    }

    frequire = mergeFrequire(frequire);
    fensure = mergeFensure(fensure);

    if (fbody || frequire)
    {
	/* Symbol table into which we place parameters and nested functions,
	 * solely to diagnose name collisions.
	 */
	localsymtab = new DsymbolTable();

	// Establish function scope
	ScopeDsymbol *ss = new ScopeDsymbol();
	ss->parent = sc->scopesym;
	Scope *sc2 = sc->push(ss);
	sc2->func = this;
	sc2->parent = this;
	sc2->callSuper = 0;
	sc2->sbreak = NULL;
	sc2->scontinue = NULL;
	sc2->sw = NULL;
	sc2->fes = fes;
	sc2->linkage = LINKd;
	sc2->stc &= ~(STCauto | STCscope | STCstatic | STCabstract | STCdeprecated | STC_TYPECTOR | STCfinal | STCtls | STCgshared | STCref);
	sc2->protection = PROTpublic;
	sc2->explicitProtection = 0;
	sc2->structalign = 8;
	sc2->incontract = 0;
	sc2->tf = NULL;
	sc2->noctor = 0;

	// Declare 'this'
	AggregateDeclaration *ad = isThis();
	if (ad)
	{   VarDeclaration *v;

	    if (isFuncLiteralDeclaration() && isNested())
	    {
		error("literals cannot be class members");
		return;
	    }
	    else
	    {
		assert(!isNested());	// can't be both member and nested
		assert(ad->handle);
		Type *thandle = ad->handle;
#if STRUCTTHISREF
		thandle = thandle->addMod(type->mod);
		thandle = thandle->addStorageClass(storage_class);
		if (isPure())
		    thandle = thandle->addMod(MODconst);
#else
		if (storage_class & STCconst || type->isConst())
		{
		    assert(0); // BUG: shared not handled
		    if (thandle->ty == Tclass)
			thandle = thandle->constOf();
		    else
		    {	assert(thandle->ty == Tpointer);
			thandle = thandle->nextOf()->constOf()->pointerTo();
		    }
		}
		else if (storage_class & STCimmutable || type->isInvariant())
		{
		    if (thandle->ty == Tclass)
			thandle = thandle->invariantOf();
		    else
		    {	assert(thandle->ty == Tpointer);
			thandle = thandle->nextOf()->invariantOf()->pointerTo();
		    }
		}
		else if (storage_class & STCshared || type->isShared())
		{
		    assert(0);  // not implemented
		}
#endif
		v = new ThisDeclaration(loc, thandle);
		v->storage_class |= STCparameter;
#if STRUCTTHISREF
		if (thandle->ty == Tstruct)
		    v->storage_class |= STCref;
#endif
		v->semantic(sc2);
		if (!sc2->insert(v))
		    assert(0);
		v->parent = this;
		vthis = v;
	    }
	}
	else if (isNested())
	{
	    /* The 'this' for a nested function is the link to the
	     * enclosing function's stack frame.
	     * Note that nested functions and member functions are disjoint.
	     */
	    VarDeclaration *v = new ThisDeclaration(loc, Type::tvoid->pointerTo());
	    v->storage_class |= STCparameter;
	    v->semantic(sc2);
	    if (!sc2->insert(v))
		assert(0);
	    v->parent = this;
	    vthis = v;
	}

	// Declare hidden variable _arguments[] and _argptr
	if (f->varargs == 1)
	{
#if TARGET_NET
        varArgs(sc2, f, argptr, _arguments);
#else
        Type *t;

	    if (f->linkage == LINKd)
	    {	// Declare _arguments[]
#if BREAKABI
		v_arguments = new VarDeclaration(0, Type::typeinfotypelist->type, Id::_arguments_typeinfo, NULL);
		v_arguments->storage_class = STCparameter;
		v_arguments->semantic(sc2);
		sc2->insert(v_arguments);
		v_arguments->parent = this;

		//t = Type::typeinfo->type->constOf()->arrayOf();
		t = Type::typeinfo->type->arrayOf();
		_arguments = new VarDeclaration(0, t, Id::_arguments, NULL);
		_arguments->semantic(sc2);
		sc2->insert(_arguments);
		_arguments->parent = this;
#else
		t = Type::typeinfo->type->arrayOf();
		v_arguments = new VarDeclaration(0, t, Id::_arguments, NULL);
		v_arguments->storage_class = STCparameter | STCin;
		v_arguments->semantic(sc2);
		sc2->insert(v_arguments);
		v_arguments->parent = this;
#endif
	    }
	    if (f->linkage == LINKd || (parameters && parameters->dim))
	    {	// Declare _argptr
#if IN_GCC
		t = d_gcc_builtin_va_list_d_type;
#else
		t = Type::tvoid->pointerTo();
#endif
		argptr = new VarDeclaration(0, t, Id::_argptr, NULL);
		argptr->semantic(sc2);
		sc2->insert(argptr);
		argptr->parent = this;
	    }
#endif
	}

	// Propagate storage class from tuple parameters to their element-parameters.
	if (f->parameters)
	{
	    for (size_t i = 0; i < f->parameters->dim; i++)
	    {	Argument *arg = (Argument *)f->parameters->data[i];

		//printf("[%d] arg->type->ty = %d %s\n", i, arg->type->ty, arg->type->toChars());
		if (arg->type->ty == Ttuple)
		{   TypeTuple *t = (TypeTuple *)arg->type;
		    size_t dim = Argument::dim(t->arguments);
		    for (size_t j = 0; j < dim; j++)
		    {	Argument *narg = Argument::getNth(t->arguments, j);
			narg->storageClass = arg->storageClass;
		    }
		}
	    }
	}

	/* Declare all the function parameters as variables
	 * and install them in parameters[]
	 */
	size_t nparams = Argument::dim(f->parameters);
	if (nparams)
	{   /* parameters[] has all the tuples removed, as the back end
	     * doesn't know about tuples
	     */
	    parameters = new Dsymbols();
	    parameters->reserve(nparams);
	    for (size_t i = 0; i < nparams; i++)
	    {
		Argument *arg = Argument::getNth(f->parameters, i);
		Identifier *id = arg->ident;
		if (!id)
		{
		    /* Generate identifier for un-named parameter,
		     * because we need it later on.
		     */
		    arg->ident = id = Identifier::generateId("_param_", i);
		}
		Type *vtype = arg->type;
		if (isPure())
		    vtype = vtype->addMod(MODconst);
		VarDeclaration *v = new VarDeclaration(loc, vtype, id, NULL);
		//printf("declaring parameter %s of type %s\n", v->toChars(), v->type->toChars());
		v->storage_class |= STCparameter;
		if (f->varargs == 2 && i + 1 == nparams)
		    v->storage_class |= STCvariadic;
		v->storage_class |= arg->storageClass & (STCin | STCout | STCref | STClazy | STCfinal | STC_TYPECTOR | STCnodtor);
		v->semantic(sc2);
		if (!sc2->insert(v))
		    error("parameter %s.%s is already defined", toChars(), v->toChars());
		else
		    parameters->push(v);
		localsymtab->insert(v);
		v->parent = this;
	    }
	}

	// Declare the tuple symbols and put them in the symbol table,
	// but not in parameters[].
	if (f->parameters)
	{
	    for (size_t i = 0; i < f->parameters->dim; i++)
	    {	Argument *arg = (Argument *)f->parameters->data[i];

		if (!arg->ident)
		    continue;			// never used, so ignore
		if (arg->type->ty == Ttuple)
		{   TypeTuple *t = (TypeTuple *)arg->type;
		    size_t dim = Argument::dim(t->arguments);
		    Objects *exps = new Objects();
		    exps->setDim(dim);
		    for (size_t j = 0; j < dim; j++)
		    {	Argument *narg = Argument::getNth(t->arguments, j);
			assert(narg->ident);
			VarDeclaration *v = sc2->search(0, narg->ident, NULL)->isVarDeclaration();
			assert(v);
			Expression *e = new VarExp(v->loc, v);
			exps->data[j] = (void *)e;
		    }
		    assert(arg->ident);
		    TupleDeclaration *v = new TupleDeclaration(loc, arg->ident, exps);
		    //printf("declaring tuple %s\n", v->toChars());
		    v->isexp = 1;
		    if (!sc2->insert(v))
			error("parameter %s.%s is already defined", toChars(), v->toChars());
		    localsymtab->insert(v);
		    v->parent = this;
		}
	    }
	}

	/* Do the semantic analysis on the [in] preconditions and
	 * [out] postconditions.
	 */
	sc2->incontract++;

	if (frequire)
	{   /* frequire is composed of the [in] contracts
	     */
	    // BUG: need to error if accessing out parameters
	    // BUG: need to treat parameters as const
	    // BUG: need to disallow returns and throws
	    // BUG: verify that all in and ref parameters are read
	    frequire = frequire->semantic(sc2);
	    labtab = NULL;		// so body can't refer to labels
	}

	if (fensure || addPostInvariant())
	{   /* fensure is composed of the [out] contracts
	     */
	    if (!type->nextOf())		// if return type is inferred
	    {	/* This case:
		 *   auto fp = function() out { } body { };
		 * Can fix by doing semantic() onf fbody first.
		 */
		error("post conditions are not supported if the return type is inferred");
		return;
	    }

	    ScopeDsymbol *sym = new ScopeDsymbol();
	    sym->parent = sc2->scopesym;
	    sc2 = sc2->push(sym);

	    assert(type->nextOf());
	    if (type->nextOf()->ty == Tvoid)
	    {
		if (outId)
		    error("void functions have no result");
	    }
	    else
	    {
		if (!outId)
		    outId = Id::result;		// provide a default
	    }

	    if (outId)
	    {	// Declare result variable
		VarDeclaration *v;
		Loc loc = this->loc;

		if (fensure)
		    loc = fensure->loc;

		v = new VarDeclaration(loc, type->nextOf(), outId, NULL);
		v->noauto = 1;
#if DMDV2
		if (f->isref)
		{
		    v->storage_class |= STCref | STCforeach;
		}
#endif
		sc2->incontract--;
		v->semantic(sc2);
		sc2->incontract++;
		if (!sc2->insert(v))
		    error("out result %s is already defined", v->toChars());
		v->parent = this;
		vresult = v;

		// vresult gets initialized with the function return value
		// in ReturnStatement::semantic()
	    }

	    // BUG: need to treat parameters as const
	    // BUG: need to disallow returns and throws
	    if (fensure)
	    {	fensure = fensure->semantic(sc2);
		labtab = NULL;		// so body can't refer to labels
	    }

	    if (!global.params.useOut)
	    {	fensure = NULL;		// discard
		vresult = NULL;
	    }

	    // Postcondition invariant
	    if (addPostInvariant())
	    {
		Expression *e = NULL;
		if (isCtorDeclaration())
		{
		    // Call invariant directly only if it exists
		    InvariantDeclaration *inv = ad->inv;
		    ClassDeclaration *cd = ad->isClassDeclaration();

		    while (!inv && cd)
		    {
			cd = cd->baseClass;
			if (!cd)
			    break;
			inv = cd->inv;
		    }
		    if (inv)
		    {
			e = new DsymbolExp(0, inv);
			e = new CallExp(0, e);
			e = e->semantic(sc2);
		    }
		}
		else
		{   // Call invariant virtually
		    Expression *v = new ThisExp(0);
		    v->type = vthis->type;
#if STRUCTTHISREF
		    if (ad->isStructDeclaration())
			v = v->addressOf(sc);
#endif
		    e = new AssertExp(0, v);
		}
		if (e)
		{
		    ExpStatement *s = new ExpStatement(0, e);
		    if (fensure)
			fensure = new CompoundStatement(0, s, fensure);
		    else
			fensure = s;
		}
	    }

	    if (fensure)
	    {	returnLabel = new LabelDsymbol(Id::returnLabel);
		LabelStatement *ls = new LabelStatement(0, Id::returnLabel, fensure);
		ls->isReturnLabel = 1;
		returnLabel->statement = ls;
	    }
	    sc2 = sc2->pop();
	}

	sc2->incontract--;

	if (fbody)
	{   ClassDeclaration *cd = isClassMember();

	    /* If this is a class constructor
	     */
	    if (isCtorDeclaration() && cd)
	    {
		for (int i = 0; i < cd->fields.dim; i++)
		{   VarDeclaration *v = (VarDeclaration *)cd->fields.data[i];

		    v->ctorinit = 0;
		}
	    }

	    if (inferRetType || f->retStyle() != RETstack)
		nrvo_can = 0;

	    fbody = fbody->semantic(sc2);
	    if (!fbody)
		fbody = new CompoundStatement(0, new Statements());

	    if (inferRetType)
	    {	// If no return type inferred yet, then infer a void
		if (!type->nextOf())
		{
		    ((TypeFunction *)type)->next = Type::tvoid;
		    type = type->semantic(loc, sc);
		}
		f = (TypeFunction *)type;
	    }

	    if (isStaticCtorDeclaration())
	    {	/* It's a static constructor. Ensure that all
		 * ctor consts were initialized.
		 */

		Dsymbol *p = toParent();
		ScopeDsymbol *ad = p->isScopeDsymbol();
		if (!ad)
		{
		    error("static constructor can only be member of struct/class/module, not %s %s", p->kind(), p->toChars());
		}
		else
		{
		    for (int i = 0; i < ad->members->dim; i++)
		    {   Dsymbol *s = (Dsymbol *)ad->members->data[i];

			s->checkCtorConstInit();
		    }
		}
	    }

	    if (isCtorDeclaration() && cd)
	    {
		//printf("callSuper = x%x\n", sc2->callSuper);

		// Verify that all the ctorinit fields got initialized
		if (!(sc2->callSuper & CSXthis_ctor))
		{
		    for (int i = 0; i < cd->fields.dim; i++)
		    {   VarDeclaration *v = (VarDeclaration *)cd->fields.data[i];

			if (v->ctorinit == 0 && v->isCtorinit())
			    error("missing initializer for final field %s", v->toChars());
		    }
		}

		if (!(sc2->callSuper & CSXany_ctor) &&
		    cd->baseClass && cd->baseClass->ctor)
		{
		    sc2->callSuper = 0;

		    // Insert implicit super() at start of fbody
		    Expression *e1 = new SuperExp(0);
		    Expression *e = new CallExp(0, e1);

		    e = e->trySemantic(sc2);
		    if (!e)
			error("no match for implicit super() call in constructor");
		    else
		    {
			Statement *s = new ExpStatement(0, e);
			fbody = new CompoundStatement(0, s, fbody);
		    }
		}
	    }
	    else if (fes)
	    {	// For foreach(){} body, append a return 0;
		Expression *e = new IntegerExp(0);
		Statement *s = new ReturnStatement(0, e);
		fbody = new CompoundStatement(0, fbody, s);
		assert(!returnLabel);
	    }
	    else if (!hasReturnExp && type->nextOf()->ty != Tvoid)
		error("expected to return a value of type %s", type->nextOf()->toChars());
	    else if (!inlineAsm)
	    {
#if DMDV2
		int blockexit = fbody ? fbody->blockExit() : BEfallthru;
		if (f->isnothrow && blockexit & BEthrow)
		    error("'%s' is nothrow yet may throw", toChars());

		int offend = blockexit & BEfallthru;
#endif
		if (type->nextOf()->ty == Tvoid)
		{
		    if (offend && isMain())
		    {	// Add a return 0; statement
			Statement *s = new ReturnStatement(0, new IntegerExp(0));
			fbody = new CompoundStatement(0, fbody, s);
		    }
		}
		else
		{
		    if (offend)
		    {   Expression *e;
#if DMDV1
			warning(loc, "no return exp; or assert(0); at end of function");
#else
			error("no return exp; or assert(0); at end of function");
#endif
			if (global.params.useAssert &&
			    !global.params.useInline)
			{   /* Add an assert(0, msg); where the missing return
			     * should be.
			     */
			    e = new AssertExp(
				  endloc,
				  new IntegerExp(0),
				  new StringExp(loc, (char *)"missing return expression")
				);
			}
			else
			    e = new HaltExp(endloc);
			e = new CommaExp(0, e, type->nextOf()->defaultInit());
			e = e->semantic(sc2);
			Statement *s = new ExpStatement(0, e);
			fbody = new CompoundStatement(0, fbody, s);
		    }
		}
	    }
	}

	{
	    Statements *a = new Statements();

	    // Merge in initialization of 'out' parameters
	    if (parameters)
	    {	for (size_t i = 0; i < parameters->dim; i++)
		{
		    VarDeclaration *v = (VarDeclaration *)parameters->data[i];
		    if (v->storage_class & STCout)
		    {
			assert(v->init);
			ExpInitializer *ie = v->init->isExpInitializer();
			assert(ie);
			a->push(new ExpStatement(0, ie->exp));
		    }
		}
	    }

	    if (argptr)
	    {	// Initialize _argptr to point past non-variadic arg
#if IN_GCC
		// Handled in FuncDeclaration::toObjFile
		v_argptr = argptr;
		v_argptr->init = new VoidInitializer(loc);
#else
		Type *t = argptr->type;
		VarDeclaration *p;
		unsigned offset;

		Expression *e1 = new VarExp(0, argptr);
		if (parameters && parameters->dim)
		    p = (VarDeclaration *)parameters->data[parameters->dim - 1];
		else
		    p = v_arguments;		// last parameter is _arguments[]
		if (p->storage_class & STClazy)
		    // If the last parameter is lazy, it's the size of a delegate
		    offset = PTRSIZE * 2;
		else
		    offset = p->type->size();
		offset = (offset + 3) & ~3;	// assume stack aligns on 4
		Expression *e = new SymOffExp(0, p, offset);
		e = new AssignExp(0, e1, e);
		e->type = t;
		a->push(new ExpStatement(0, e));
		p->isargptr = TRUE;
#endif
	    }

	    if (_arguments)
	    {
		/* Advance to elements[] member of TypeInfo_Tuple with:
		 *  _arguments = v_arguments.elements;
		 */
		Expression *e = new VarExp(0, v_arguments);
		e = new DotIdExp(0, e, Id::elements);
		Expression *e1 = new VarExp(0, _arguments);
		e = new AssignExp(0, e1, e);
		e->op = TOKconstruct;
		e = e->semantic(sc2);
		a->push(new ExpStatement(0, e));
	    }

	    // Merge contracts together with body into one compound statement

#ifdef _DH
	    if (frequire && global.params.useIn)
	    {	frequire->incontract = 1;
		a->push(frequire);
	    }
#else
	    if (frequire && global.params.useIn)
		a->push(frequire);
#endif

	    // Precondition invariant
	    if (addPreInvariant())
	    {
		Expression *e = NULL;
		if (isDtorDeclaration())
		{
		    // Call invariant directly only if it exists
		    InvariantDeclaration *inv = ad->inv;
		    ClassDeclaration *cd = ad->isClassDeclaration();

		    while (!inv && cd)
		    {
			cd = cd->baseClass;
			if (!cd)
			    break;
			inv = cd->inv;
		    }
		    if (inv)
		    {
			e = new DsymbolExp(0, inv);
			e = new CallExp(0, e);
			e = e->semantic(sc2);
		    }
		}
		else
		{   // Call invariant virtually
		    Expression *v = new ThisExp(0);
		    v->type = vthis->type;
#if STRUCTTHISREF
		    if (ad->isStructDeclaration())
			v = v->addressOf(sc);
#endif
		    Expression *se = new StringExp(0, (char *)"null this");
		    se = se->semantic(sc);
		    se->type = Type::tchar->arrayOf();
		    e = new AssertExp(loc, v, se);
		}
		if (e)
		{
		    ExpStatement *s = new ExpStatement(0, e);
		    a->push(s);
		}
	    }

	    if (fbody)
		a->push(fbody);

	    if (fensure)
	    {
		a->push(returnLabel->statement);

		if (type->nextOf()->ty != Tvoid)
		{
		    // Create: return vresult;
		    assert(vresult);
		    Expression *e = new VarExp(0, vresult);
		    if (tintro)
		    {	e = e->implicitCastTo(sc, tintro->nextOf());
			e = e->semantic(sc);
		    }
		    ReturnStatement *s = new ReturnStatement(0, e);
		    a->push(s);
		}
	    }

	    fbody = new CompoundStatement(0, a);
#if DMDV2
	    /* Append destructor calls for parameters as finally blocks.
	     */
	    if (parameters)
	    {	for (size_t i = 0; i < parameters->dim; i++)
		{
		    VarDeclaration *v = (VarDeclaration *)parameters->data[i];

		    if (v->storage_class & (STCref | STCout))
			continue;

		    /* Don't do this for static arrays, since static
		     * arrays are called by reference. Remove this
		     * when we change them to call by value.
		     */
		    if (v->type->toBasetype()->ty == Tsarray)
			continue;

		    Expression *e = v->callAutoDtor(sc);
		    if (e)
		    {	Statement *s = new ExpStatement(0, e);
			s = s->semantic(sc);
			if (fbody->blockExit() == BEfallthru)
			    fbody = new CompoundStatement(0, fbody, s);
			else
			    fbody = new TryFinallyStatement(0, fbody, s);
		    }
		}
	    }
#endif

#if 1
	    if (isSynchronized())
	    {	/* Wrap the entire function body in a synchronized statement
		 */
		ClassDeclaration *cd = parent->isClassDeclaration();
		if (cd)
		{
#if TARGET_WINDOS
		    if (/*config.flags2 & CFG2seh &&*/	// always on for WINDOS
			!isStatic() && !fbody->usesEH())
		    {
			/* The back end uses the "jmonitor" hack for syncing;
			 * no need to do the sync at this level.
			 */
		    }
		    else
#endif
		    {
			Expression *vsync;
			if (isStatic())
			{   // The monitor is in the ClassInfo
			    vsync = new DotIdExp(loc, new DsymbolExp(loc, cd), Id::classinfo);
			}
			else
			{   // 'this' is the monitor
			    vsync = new VarExp(loc, vthis);
			}
			fbody = new PeelStatement(fbody);	// don't redo semantic()
			fbody = new SynchronizedStatement(loc, vsync, fbody);
			fbody = fbody->semantic(sc2);
		    }
		}
		else
		{
		    error("synchronized function %s must be a member of a class", toChars());
		}
	    }
#endif
	}

	sc2->callSuper = 0;
	sc2->pop();
    }
    semanticRun = 4;
}

void FuncDeclaration::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    //printf("FuncDeclaration::toCBuffer() '%s'\n", toChars());

    StorageClassDeclaration::stcToCBuffer(buf, storage_class);
    type->toCBuffer(buf, ident, hgs);
    bodyToCBuffer(buf, hgs);
}


void FuncDeclaration::bodyToCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    if (fbody &&
	(!hgs->hdrgen || hgs->tpltMember || canInline(1,1))
       )
    {	buf->writenl();

	// in{}
	if (frequire)
	{   buf->writestring("in");
	    buf->writenl();
	    frequire->toCBuffer(buf, hgs);
	}

	// out{}
	if (fensure)
	{   buf->writestring("out");
	    if (outId)
	    {   buf->writebyte('(');
		buf->writestring(outId->toChars());
		buf->writebyte(')');
	    }
	    buf->writenl();
	    fensure->toCBuffer(buf, hgs);
	}

        if (frequire || fensure)
	{   buf->writestring("body");
	    buf->writenl();
	}

	buf->writebyte('{');
	buf->writenl();
	fbody->toCBuffer(buf, hgs);
	buf->writebyte('}');
	buf->writenl();
    }
    else
    {	buf->writeByte(';');
	buf->writenl();
    }
}

/****************************************************
 * Merge into this function the 'in' contracts of all it overrides.
 * 'in's are OR'd together, i.e. only one of them needs to pass.
 */

Statement *FuncDeclaration::mergeFrequire(Statement *sf)
{
    /* Implementing this is done by having the overriding function call
     * nested functions (the fdrequire functions) nested inside the overridden
     * function. This requires that the stack layout of the calling function's
     * parameters and 'this' pointer be in the same place (as the nested
     * function refers to them).
     * This is easy for the parameters, as they are all on the stack in the same
     * place by definition, since it's an overriding function. The problem is
     * getting the 'this' pointer in the same place, since it is a local variable.
     * We did some hacks in the code generator to make this happen:
     *	1. always generate exception handler frame, or at least leave space for it
     *     in the frame (Windows 32 SEH only)
     *	2. always generate an EBP style frame
     *  3. since 'this' is passed in a register that is subsequently copied into
     *     a stack local, allocate that local immediately following the exception
     *     handler block, so it is always at the same offset from EBP.
     */
    for (int i = 0; i < foverrides.dim; i++)
    {
	FuncDeclaration *fdv = (FuncDeclaration *)foverrides.data[i];
	sf = fdv->mergeFrequire(sf);
	if (fdv->fdrequire)
	{
	    //printf("fdv->frequire: %s\n", fdv->frequire->toChars());
	    /* Make the call:
	     *   try { __require(); }
	     *   catch { frequire; }
	     */
	    Expression *eresult = NULL;
	    Expression *e = new CallExp(loc, new VarExp(loc, fdv->fdrequire, 0), eresult);
	    Statement *s2 = new ExpStatement(loc, e);

	    if (sf)
	    {	Catch *c = new Catch(loc, NULL, NULL, sf);
		Array *catches = new Array();
		catches->push(c);
		sf = new TryCatchStatement(loc, s2, catches);
	    }
	    else
		sf = s2;
	}
    }
    return sf;
}

/****************************************************
 * Merge into this function the 'out' contracts of all it overrides.
 * 'out's are AND'd together, i.e. all of them need to pass.
 */

Statement *FuncDeclaration::mergeFensure(Statement *sf)
{
    /* Same comments as for mergeFrequire(), except that we take care
     * of generating a consistent reference to the 'result' local by
     * explicitly passing 'result' to the nested function as a reference
     * argument.
     * This won't work for the 'this' parameter as it would require changing
     * the semantic code for the nested function so that it looks on the parameter
     * list for the 'this' pointer, something that would need an unknown amount
     * of tweaking of various parts of the compiler that I'd rather leave alone.
     */
    for (int i = 0; i < foverrides.dim; i++)
    {
	FuncDeclaration *fdv = (FuncDeclaration *)foverrides.data[i];
	sf = fdv->mergeFensure(sf);
	if (fdv->fdensure)
	{
	    //printf("fdv->fensure: %s\n", fdv->fensure->toChars());
	    // Make the call: __ensure(result)
	    Expression *eresult = NULL;
	    if (outId)
		eresult = new IdentifierExp(loc, outId);
	    Expression *e = new CallExp(loc, new VarExp(loc, fdv->fdensure, 0), eresult);
	    Statement *s2 = new ExpStatement(loc, e);

	    if (sf)
	    {
		sf = new CompoundStatement(fensure->loc, s2, sf);
	    }
	    else
		sf = s2;
	}
    }
    return sf;
}

/****************************************************
 * Determine if 'this' overrides fd.
 * Return !=0 if it does.
 */

int FuncDeclaration::overrides(FuncDeclaration *fd)
{   int result = 0;

    if (fd->ident == ident)
    {
	int cov = type->covariant(fd->type);
	if (cov)
	{   ClassDeclaration *cd1 = toParent()->isClassDeclaration();
	    ClassDeclaration *cd2 = fd->toParent()->isClassDeclaration();

	    if (cd1 && cd2 && cd2->isBaseOf(cd1, NULL))
		result = 1;
	}
    }
    return result;
}

/*************************************************
 * Find index of function in vtbl[0..dim] that
 * this function overrides.
 * Returns:
 *	-1	didn't find one
 *	-2	can't determine because of forward references
 */

int FuncDeclaration::findVtblIndex(Array *vtbl, int dim)
{
    for (int vi = 0; vi < dim; vi++)
    {
	FuncDeclaration *fdv = ((Dsymbol *)vtbl->data[vi])->isFuncDeclaration();
	if (fdv && fdv->ident == ident)
	{
	    int cov = type->covariant(fdv->type);
	    //printf("\tbaseclass cov = %d\n", cov);
	    switch (cov)
	    {
		case 0:		// types are distinct
		    break;

		case 1:
		    return vi;

		case 2:
		    //type->print();
		    //fdv->type->print();
		    //printf("%s %s\n", type->deco, fdv->type->deco);
#if TARGET_NET
			if (!isOverride())
				break;
#endif
		    error("of type %s overrides but is not covariant with %s of type %s",
			type->toChars(), fdv->toPrettyChars(), fdv->type->toChars());
		    break;

		case 3:
		    return -2;	// forward references

		default:
		    assert(0);
	    }
	}
    }
    return -1;
}

/****************************************************
 * Overload this FuncDeclaration with the new one f.
 * Return !=0 if successful; i.e. no conflict.
 */

int FuncDeclaration::overloadInsert(Dsymbol *s)
{
    FuncDeclaration *f;
    AliasDeclaration *a;

    //printf("FuncDeclaration::overloadInsert(%s)\n", s->toChars());
    a = s->isAliasDeclaration();
    if (a)
    {
	if (overnext)
	    return overnext->overloadInsert(a);
	if (!a->aliassym && a->type->ty != Tident && a->type->ty != Tinstance)
	{
	    //printf("\ta = '%s'\n", a->type->toChars());
	    return FALSE;
	}
	overnext = a;
	//printf("\ttrue: no conflict\n");
	return TRUE;
    }
    f = s->isFuncDeclaration();
    if (!f)
	return FALSE;

#if 0
    /* Disable this check because:
     *	const void foo();
     * semantic() isn't run yet on foo(), so the const hasn't been
     * applied yet.
     */
    if (type)
    {   printf("type = %s\n", type->toChars());
	printf("f->type = %s\n", f->type->toChars());
    }
    if (type && f->type &&	// can be NULL for overloaded constructors
	f->type->covariant(type) &&
	f->type->mod == type->mod &&
	!isFuncAliasDeclaration())
    {
	//printf("\tfalse: conflict %s\n", kind());
	return FALSE;
    }
#endif

    if (overnext)
	return overnext->overloadInsert(f);
    overnext = f;
    //printf("\ttrue: no conflict\n");
    return TRUE;
}

/********************************************
 * Find function in overload list that exactly matches t.
 */

/***************************************************
 * Visit each overloaded function in turn, and call
 * (*fp)(param, f) on it.
 * Exit when no more, or (*fp)(param, f) returns 1.
 * Returns:
 *	0	continue
 *	1	done
 */

int overloadApply(FuncDeclaration *fstart,
	int (*fp)(void *, FuncDeclaration *),
	void *param)
{
    FuncDeclaration *f;
    Declaration *d;
    Declaration *next;

    for (d = fstart; d; d = next)
    {	FuncAliasDeclaration *fa = d->isFuncAliasDeclaration();

	if (fa)
	{
	    if (overloadApply(fa->funcalias, fp, param))
		return 1;
	    next = fa->overnext;
	}
	else
	{
	    AliasDeclaration *a = d->isAliasDeclaration();

	    if (a)
	    {
		Dsymbol *s = a->toAlias();
		next = s->isDeclaration();
		if (next == a)
		    break;
		if (next == fstart)
		    break;
	    }
	    else
	    {
		f = d->isFuncDeclaration();
		if (!f)
		{   d->error("is aliased to a function");
		    break;		// BUG: should print error message?
		}
		if ((*fp)(param, f))
		    return 1;

		next = f->overnext;
	    }
	}
    }
    return 0;
}

/********************************************
 * If there are no overloads of function f, return that function,
 * otherwise return NULL.
 */

static int fpunique(void *param, FuncDeclaration *f)
{   FuncDeclaration **pf = (FuncDeclaration **)param;

    if (*pf)
    {	*pf = NULL;
	return 1;		// ambiguous, done
    }
    else
    {	*pf = f;
	return 0;
    }
}

FuncDeclaration *FuncDeclaration::isUnique()
{   FuncDeclaration *result = NULL;

    overloadApply(this, &fpunique, &result);
    return result;
}

/********************************************
 * Find function in overload list that exactly matches t.
 */

struct Param1
{
    Type *t;		// type to match
    FuncDeclaration *f;	// return value
};

int fp1(void *param, FuncDeclaration *f)
{   Param1 *p = (Param1 *)param;
    Type *t = p->t;

    if (t->equals(f->type))
    {	p->f = f;
	return 1;
    }

#if DMDV2
    /* Allow covariant matches, if it's just a const conversion
     * of the return type
     */
    if (t->ty == Tfunction)
    {   TypeFunction *tf = (TypeFunction *)f->type;
	if (tf->covariant(t) == 1 &&
	    tf->nextOf()->implicitConvTo(t->nextOf()) >= MATCHconst)
	{
	    p->f = f;
	    return 1;
	}
    }
#endif
    return 0;
}

FuncDeclaration *FuncDeclaration::overloadExactMatch(Type *t)
{
    Param1 p;
    p.t = t;
    p.f = NULL;
    overloadApply(this, &fp1, &p);
    return p.f;
}


/********************************************
 * Decide which function matches the arguments best.
 */

struct Param2
{
    Match *m;
    Expression *ethis;
    Expressions *arguments;
};

int fp2(void *param, FuncDeclaration *f)
{   Param2 *p = (Param2 *)param;
    Match *m = p->m;
    Expressions *arguments = p->arguments;
    MATCH match;

    if (f != m->lastf)		// skip duplicates
    {
	m->anyf = f;
	TypeFunction *tf = (TypeFunction *)f->type;
	match = (MATCH) tf->callMatch(f->needThis() ? p->ethis : NULL, arguments);
	//printf("test: match = %d\n", match);
	if (match != MATCHnomatch)
	{
	    if (match > m->last)
		goto LfIsBetter;

	    if (match < m->last)
		goto LlastIsBetter;

	    /* See if one of the matches overrides the other.
	     */
	    if (m->lastf->overrides(f))
		goto LlastIsBetter;
	    else if (f->overrides(m->lastf))
		goto LfIsBetter;

	    /* Try to disambiguate using template-style partial ordering rules.
	     * In essence, if f() and g() are ambiguous, if f() can call g(),
	     * but g() cannot call f(), then pick f().
	     * This is because f() is "more specialized."
	     */
	    {
	    MATCH c1 = f->leastAsSpecialized(m->lastf);
	    MATCH c2 = m->lastf->leastAsSpecialized(f);
	    //printf("c1 = %d, c2 = %d\n", c1, c2);
	    if (c1 > c2)
		goto LfIsBetter;
	    if (c1 < c2)
		goto LlastIsBetter;
	    }

	Lambiguous:
	    m->nextf = f;
	    m->count++;
	    return 0;

	LfIsBetter:
	    m->last = match;
	    m->lastf = f;
	    m->count = 1;
	    return 0;

	LlastIsBetter:
	    return 0;
	}
    }
    return 0;
}

void overloadResolveX(Match *m, FuncDeclaration *fstart,
	Expression *ethis, Expressions *arguments)
{
    Param2 p;
    p.m = m;
    p.ethis = ethis;
    p.arguments = arguments;
    overloadApply(fstart, &fp2, &p);
}


FuncDeclaration *FuncDeclaration::overloadResolve(Loc loc, Expression *ethis, Expressions *arguments, int flags)
{
    TypeFunction *tf;
    Match m;

#if 0
printf("FuncDeclaration::overloadResolve('%s')\n", toChars());
if (arguments)
{   int i;

    for (i = 0; i < arguments->dim; i++)
    {   Expression *arg;

	arg = (Expression *)arguments->data[i];
	assert(arg->type);
	printf("\t%s: ", arg->toChars());
	arg->type->print();
    }
}
#endif

    memset(&m, 0, sizeof(m));
    m.last = MATCHnomatch;
    overloadResolveX(&m, this, ethis, arguments);

    if (m.count == 1)		// exactly one match
    {
	return m.lastf;
    }
    else
    {
	OutBuffer buf;

	buf.writeByte('(');
	if (arguments)
	{
	    HdrGenState hgs;

	    argExpTypesToCBuffer(&buf, arguments, &hgs);
	    buf.writeByte(')');
	    if (ethis)
		ethis->type->modToBuffer(&buf);
	}
	else
	    buf.writeByte(')');

	if (m.last == MATCHnomatch)
	{
	    if (flags & 1)		// if do not print error messages
		return NULL;		// no match

	    tf = (TypeFunction *)type;

	    OutBuffer buf2;
	    tf->modToBuffer(&buf2);

	    //printf("tf = %s, args = %s\n", tf->deco, ((Expression *)arguments->data[0])->type->deco);
	    error(loc, "%s%s is not callable using argument types %s",
		Argument::argsTypesToChars(tf->parameters, tf->varargs),
		buf2.toChars(),
		buf.toChars());
	    return m.anyf;		// as long as it's not a FuncAliasDeclaration
	}
	else
	{
#if 1
	    TypeFunction *t1 = (TypeFunction *)m.lastf->type;
	    TypeFunction *t2 = (TypeFunction *)m.nextf->type;

	    error(loc, "called with argument types:\n\t(%s)\nmatches both:\n\t%s%s\nand:\n\t%s%s",
		    buf.toChars(),
		    m.lastf->toPrettyChars(), Argument::argsTypesToChars(t1->parameters, t1->varargs),
		    m.nextf->toPrettyChars(), Argument::argsTypesToChars(t2->parameters, t2->varargs));
#else
	    error(loc, "overloads %s and %s both match argument list for %s",
		    m.lastf->type->toChars(),
		    m.nextf->type->toChars(),
		    m.lastf->toChars());
#endif
	    return m.lastf;
	}
    }
}

/*************************************
 * Determine partial specialization order of 'this' vs g.
 * This is very similar to TemplateDeclaration::leastAsSpecialized().
 * Returns:
 *	match	'this' is at least as specialized as g
 *	0	g is more specialized than 'this'
 */

MATCH FuncDeclaration::leastAsSpecialized(FuncDeclaration *g)
{
#define LOG_LEASTAS     0

#if LOG_LEASTAS
    printf("%s.leastAsSpecialized(%s)\n", toChars(), g->toChars());
#endif

    /* This works by calling g() with f()'s parameters, and
     * if that is possible, then f() is at least as specialized
     * as g() is.
     */

    TypeFunction *tf = (TypeFunction *)type;
    TypeFunction *tg = (TypeFunction *)g->type;
    size_t nfparams = Argument::dim(tf->parameters);
    size_t ngparams = Argument::dim(tg->parameters);
    MATCH match = MATCHexact;

    /* If both functions have a 'this' pointer, and the mods are not
     * the same and g's is not const, then this is less specialized.
     */
    if (needThis() && g->needThis())
    {
	if (tf->mod != tg->mod)
	{
	    if (tg->mod == MODconst)
		match = MATCHconst;
	    else
		return MATCHnomatch;
	}
    }

    /* Create a dummy array of arguments out of the parameters to f()
     */
    Expressions args;
    args.setDim(nfparams);
    for (int u = 0; u < nfparams; u++)
    {
	Argument *p = Argument::getNth(tf->parameters, u);
	Expression *e;
	if (p->storageClass & (STCref | STCout))
	{
	    e = new IdentifierExp(0, p->ident);
	    e->type = p->type;
	}
	else
	    e = p->type->defaultInit();
	args.data[u] = e;
    }

    MATCH m = (MATCH) tg->callMatch(NULL, &args);
    if (m)
    {
        /* A variadic parameter list is less specialized than a
         * non-variadic one.
         */
        if (tf->varargs && !tg->varargs)
            goto L1;	// less specialized

#if LOG_LEASTAS
        printf("  matches %d, so is least as specialized\n", m);
#endif
        return m;
    }
  L1:
#if LOG_LEASTAS
    printf("  doesn't match, so is not as specialized\n");
#endif
    return MATCHnomatch;
}

/*******************************************
 * Given a symbol that could be either a FuncDeclaration or
 * a function template, resolve it to a function symbol.
 *	sc		instantiation scope
 *	loc		instantiation location
 *	targsi		initial list of template arguments
 *	ethis		if !NULL, the 'this' pointer argument
 *	fargs		arguments to function
 *	flags		1: do not issue error message on no match, just return NULL
 */

FuncDeclaration *resolveFuncCall(Scope *sc, Loc loc, Dsymbol *s,
	Objects *tiargs,
	Expression *ethis,
	Expressions *arguments,
	int flags)
{
    if (!s)
	return NULL;			// no match
    FuncDeclaration *f = s->isFuncDeclaration();
    if (f)
	f = f->overloadResolve(loc, ethis, arguments);
    else
    {	TemplateDeclaration *td = s->isTemplateDeclaration();
	assert(td);
	f = td->deduceFunctionTemplate(sc, loc, tiargs, NULL, arguments, flags);
    }
    return f;
}

/********************************
 * Labels are in a separate scope, one per function.
 */

LabelDsymbol *FuncDeclaration::searchLabel(Identifier *ident)
{   Dsymbol *s;

    if (!labtab)
	labtab = new DsymbolTable();	// guess we need one

    s = labtab->lookup(ident);
    if (!s)
    {
	s = new LabelDsymbol(ident);
	labtab->insert(s);
    }
    return (LabelDsymbol *)s;
}

/****************************************
 * If non-static member function that has a 'this' pointer,
 * return the aggregate it is a member of.
 * Otherwise, return NULL.
 */

AggregateDeclaration *FuncDeclaration::isThis()
{   AggregateDeclaration *ad;

    //printf("+FuncDeclaration::isThis() '%s'\n", toChars());
    ad = NULL;
    if ((storage_class & STCstatic) == 0)
    {
	ad = isMember2();
    }
    //printf("-FuncDeclaration::isThis() %p\n", ad);
    return ad;
}

AggregateDeclaration *FuncDeclaration::isMember2()
{   AggregateDeclaration *ad;

    //printf("+FuncDeclaration::isMember2() '%s'\n", toChars());
    ad = NULL;
    for (Dsymbol *s = this; s; s = s->parent)
    {
//printf("\ts = '%s', parent = '%s', kind = %s\n", s->toChars(), s->parent->toChars(), s->parent->kind());
	ad = s->isMember();
	if (ad)
{   //printf("test4\n");
	    break;
}
	if (!s->parent ||
	    (!s->parent->isTemplateInstance()))
{   //printf("test5\n");
	    break;
}
    }
    //printf("-FuncDeclaration::isMember2() %p\n", ad);
    return ad;
}

/*****************************************
 * Determine lexical level difference from 'this' to nested function 'fd'.
 * Error if this cannot call fd.
 * Returns:
 *	0	same level
 *	-1	increase nesting by 1 (fd is nested within 'this')
 *	>0	decrease nesting by number
 */

int FuncDeclaration::getLevel(Loc loc, FuncDeclaration *fd)
{   int level;
    Dsymbol *s;
    Dsymbol *fdparent;

    //printf("FuncDeclaration::getLevel(fd = '%s')\n", fd->toChars());
    fdparent = fd->toParent2();
    if (fdparent == this)
	return -1;
    s = this;
    level = 0;
    while (fd != s && fdparent != s->toParent2())
    {
	//printf("\ts = '%s'\n", s->toChars());
	FuncDeclaration *thisfd = s->isFuncDeclaration();
	if (thisfd)
	{   if (!thisfd->isNested() && !thisfd->vthis)
		goto Lerr;
	}
	else
	{
	    AggregateDeclaration *thiscd = s->isAggregateDeclaration();
	    if (thiscd)
	    {	if (!thiscd->isNested())
		    goto Lerr;
	    }
	    else
		goto Lerr;
	}

	s = s->toParent2();
	assert(s);
	level++;
    }
    return level;

Lerr:
    error(loc, "cannot access frame of function %s", fd->toChars());
    return 1;
}

void FuncDeclaration::appendExp(Expression *e)
{   Statement *s;

    s = new ExpStatement(0, e);
    appendState(s);
}

void FuncDeclaration::appendState(Statement *s)
{
    if (!fbody)
	fbody = s;
    else
    {
	CompoundStatement *cs = fbody->isCompoundStatement();
	if (cs)
	{
	    if (!cs->statements)
		fbody = s;
	    else
		cs->statements->push(s);
	}
	else
	    fbody = new CompoundStatement(0, fbody, s);
    }
}

const char *FuncDeclaration::toPrettyChars()
{
    if (isMain())
	return "D main";
    else
	return Dsymbol::toPrettyChars();
}

int FuncDeclaration::isMain()
{
    return ident == Id::main &&
	linkage != LINKc && !isMember() && !isNested();
}

int FuncDeclaration::isWinMain()
{
    //printf("FuncDeclaration::isWinMain() %s\n", toChars());
#if 0
    int x = ident == Id::WinMain &&
	linkage != LINKc && !isMember();
    printf("%s\n", x ? "yes" : "no");
    return x;
#else
    return ident == Id::WinMain &&
	linkage != LINKc && !isMember();
#endif
}

int FuncDeclaration::isDllMain()
{
    return ident == Id::DllMain &&
	linkage != LINKc && !isMember();
}

int FuncDeclaration::isExport()
{
    return protection == PROTexport;
}

int FuncDeclaration::isImportedSymbol()
{
    //printf("isImportedSymbol()\n");
    //printf("protection = %d\n", protection);
    return (protection == PROTexport) && !fbody;
}

// Determine if function goes into virtual function pointer table

int FuncDeclaration::isVirtual()
{
#if 0
    printf("FuncDeclaration::isVirtual(%s)\n", toChars());
    printf("isMember:%p isStatic:%d private:%d ctor:%d !Dlinkage:%d\n", isMember(), isStatic(), protection == PROTprivate, isCtorDeclaration(), linkage != LINKd);
    printf("result is %d\n",
	isMember() &&
	!(isStatic() || protection == PROTprivate || protection == PROTpackage) &&
	toParent()->isClassDeclaration());
#endif
    return isMember() &&
	!(isStatic() || protection == PROTprivate || protection == PROTpackage) &&
	toParent()->isClassDeclaration();
}

int FuncDeclaration::isFinal()
{
    ClassDeclaration *cd;
#if 0
    printf("FuncDeclaration::isFinal(%s)\n", toChars());
    printf("%p %d %d %d %d\n", isMember(), isStatic(), protection == PROTprivate, isCtorDeclaration(), linkage != LINKd);
    printf("result is %d\n",
	isMember() &&
	!(isStatic() || protection == PROTprivate || protection == PROTpackage) &&
	(cd = toParent()->isClassDeclaration()) != NULL &&
	cd->storage_class & STCfinal);
#endif
    return isMember() &&
	(Declaration::isFinal() ||
	 ((cd = toParent()->isClassDeclaration()) != NULL && cd->storage_class & STCfinal));
}

int FuncDeclaration::isAbstract()
{
    return storage_class & STCabstract;
}

int FuncDeclaration::isCodeseg()
{
    return TRUE;		// functions are always in the code segment
}

int FuncDeclaration::isOverloadable()
{
    return 1;			// functions can be overloaded
}

int FuncDeclaration::isPure()
{
    //printf("FuncDeclaration::isPure() '%s'\n", toChars());
    assert(type->ty == Tfunction);
    return ((TypeFunction *)this->type)->ispure;
}

// Determine if function needs
// a static frame pointer to its lexically enclosing function

int FuncDeclaration::isNested()
{
    //if (!toParent())
	//printf("FuncDeclaration::isNested('%s') parent=%p\n", toChars(), parent);
    //printf("\ttoParent2() = '%s'\n", toParent2()->toChars());
    return ((storage_class & STCstatic) == 0) &&
	   (toParent2()->isFuncDeclaration() != NULL);
}

int FuncDeclaration::needThis()
{
    //printf("FuncDeclaration::needThis() '%s'\n", toChars());
    int i = isThis() != NULL;
    //printf("\t%d\n", i);
    if (!i && isFuncAliasDeclaration())
	i = ((FuncAliasDeclaration *)this)->funcalias->needThis();
    return i;
}

int FuncDeclaration::addPreInvariant()
{
    AggregateDeclaration *ad = isThis();
    return (ad &&
	    //ad->isClassDeclaration() &&
	    global.params.useInvariants &&
	    (protection == PROTpublic || protection == PROTexport) &&
	    !naked &&
	    ident != Id::cpctor);
}

int FuncDeclaration::addPostInvariant()
{
    AggregateDeclaration *ad = isThis();
    return (ad &&
	    ad->inv &&
	    //ad->isClassDeclaration() &&
	    global.params.useInvariants &&
	    (protection == PROTpublic || protection == PROTexport) &&
	    !naked &&
	    ident != Id::cpctor);
}

/**********************************
 * Generate a FuncDeclaration for a runtime library function.
 */

FuncDeclaration *FuncDeclaration::genCfunc(Type *treturn, const char *name)
{
    return genCfunc(treturn, Lexer::idPool(name));
}

FuncDeclaration *FuncDeclaration::genCfunc(Type *treturn, Identifier *id)
{
    FuncDeclaration *fd;
    TypeFunction *tf;
    Dsymbol *s;
    static DsymbolTable *st = NULL;

    //printf("genCfunc(name = '%s')\n", id->toChars());
    //printf("treturn\n\t"); treturn->print();

    // See if already in table
    if (!st)
	st = new DsymbolTable();
    s = st->lookup(id);
    if (s)
    {
	fd = s->isFuncDeclaration();
	assert(fd);
	assert(fd->type->nextOf()->equals(treturn));
    }
    else
    {
	tf = new TypeFunction(NULL, treturn, 0, LINKc);
	fd = new FuncDeclaration(0, 0, id, STCstatic, tf);
	fd->protection = PROTpublic;
	fd->linkage = LINKc;

	st->insert(fd);
    }
    return fd;
}

const char *FuncDeclaration::kind()
{
    return "function";
}

/*******************************
 * Look at all the variables in this function that are referenced
 * by nested functions, and determine if a closure needs to be
 * created for them.
 */

#if DMDV2
int FuncDeclaration::needsClosure()
{
    /* Need a closure for all the closureVars[] if any of the
     * closureVars[] are accessed by a
     * function that escapes the scope of this function.
     * We take the conservative approach and decide that any function that:
     * 1) is a virtual function
     * 2) has its address taken
     * 3) has a parent that escapes
     *
     * Note that since a non-virtual function can be called by
     * a virtual one, if that non-virtual function accesses a closure
     * var, the closure still has to be taken. Hence, we check for isThis()
     * instead of isVirtual(). (thanks to David Friedman)
     */

    //printf("FuncDeclaration::needsClosure() %s\n", toChars());
    for (int i = 0; i < closureVars.dim; i++)
    {	VarDeclaration *v = (VarDeclaration *)closureVars.data[i];
	assert(v->isVarDeclaration());
	//printf("\tv = %s\n", v->toChars());

	for (int j = 0; j < v->nestedrefs.dim; j++)
	{   FuncDeclaration *f = (FuncDeclaration *)v->nestedrefs.data[j];
	    assert(f != this);

	    //printf("\t\tf = %s, %d, %p, %d\n", f->toChars(), f->isVirtual(), f->isThis(), f->tookAddressOf);
	    if (f->isThis() || f->tookAddressOf)
		goto Lyes;	// assume f escapes this function's scope

	    // Look to see if any parents of f that are below this escape
	    for (Dsymbol *s = f->parent; s && s != this; s = s->parent)
	    {
		f = s->isFuncDeclaration();
		if (f && (f->isThis() || f->tookAddressOf))
		    goto Lyes;
	    }
	}
    }
    return 0;

Lyes:
    //printf("\tneeds closure\n");
    return 1;
}
#endif

/****************************** FuncAliasDeclaration ************************/

// Used as a way to import a set of functions from another scope into this one.

FuncAliasDeclaration::FuncAliasDeclaration(FuncDeclaration *funcalias)
    : FuncDeclaration(funcalias->loc, funcalias->endloc, funcalias->ident,
	(enum STC)funcalias->storage_class, funcalias->type)
{
    assert(funcalias != this);
    this->funcalias = funcalias;
}

const char *FuncAliasDeclaration::kind()
{
    return "function alias";
}


/****************************** FuncLiteralDeclaration ************************/

FuncLiteralDeclaration::FuncLiteralDeclaration(Loc loc, Loc endloc, Type *type,
	enum TOK tok, ForeachStatement *fes)
    : FuncDeclaration(loc, endloc, NULL, STCundefined, type)
{
    const char *id;

    if (fes)
	id = "__foreachbody";
    else if (tok == TOKdelegate)
	id = "__dgliteral";
    else
	id = "__funcliteral";
    this->ident = Lexer::uniqueId(id);
    this->tok = tok;
    this->fes = fes;
    //printf("FuncLiteralDeclaration() id = '%s', type = '%s'\n", this->ident->toChars(), type->toChars());
}

Dsymbol *FuncLiteralDeclaration::syntaxCopy(Dsymbol *s)
{
    FuncLiteralDeclaration *f;

    //printf("FuncLiteralDeclaration::syntaxCopy('%s')\n", toChars());
    if (s)
	f = (FuncLiteralDeclaration *)s;
    else
    {	f = new FuncLiteralDeclaration(loc, endloc, type->syntaxCopy(), tok, fes);
	f->ident = ident;		// keep old identifier
    }
    FuncDeclaration::syntaxCopy(f);
    return f;
}

int FuncLiteralDeclaration::isNested()
{
    //printf("FuncLiteralDeclaration::isNested() '%s'\n", toChars());
    return (tok == TOKdelegate);
}

int FuncLiteralDeclaration::isVirtual()
{
    return FALSE;
}

const char *FuncLiteralDeclaration::kind()
{
    // GCC requires the (char*) casts
    return (tok == TOKdelegate) ? (char*)"delegate" : (char*)"function";
}

void FuncLiteralDeclaration::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    static Identifier *idfunc;
    static Identifier *iddel;

    if (!idfunc)
	idfunc = new Identifier("function", 0);
    if (!iddel)
	iddel = new Identifier("delegate", 0);

    type->toCBuffer(buf, ((tok == TOKdelegate) ? iddel : idfunc), hgs);
    bodyToCBuffer(buf, hgs);
}


/********************************* CtorDeclaration ****************************/

CtorDeclaration::CtorDeclaration(Loc loc, Loc endloc, Arguments *arguments, int varargs)
    : FuncDeclaration(loc, endloc, Id::ctor, STCundefined, NULL)
{
    this->arguments = arguments;
    this->varargs = varargs;
    //printf("CtorDeclaration(loc = %s) %s\n", loc.toChars(), toChars());
}

Dsymbol *CtorDeclaration::syntaxCopy(Dsymbol *s)
{
    CtorDeclaration *f;

    f = new CtorDeclaration(loc, endloc, NULL, varargs);

    f->outId = outId;
    f->frequire = frequire ? frequire->syntaxCopy() : NULL;
    f->fensure  = fensure  ? fensure->syntaxCopy()  : NULL;
    f->fbody    = fbody    ? fbody->syntaxCopy()    : NULL;
    assert(!fthrows); // deprecated

    f->arguments = Argument::arraySyntaxCopy(arguments);
    return f;
}


void CtorDeclaration::semantic(Scope *sc)
{
    AggregateDeclaration *ad;
    Type *tret;

    //printf("CtorDeclaration::semantic() %s\n", toChars());
    if (type)
	return;

    sc = sc->push();
    sc->stc &= ~STCstatic;		// not a static constructor

    parent = sc->parent;
    Dsymbol *parent = toParent2();
    ad = parent->isAggregateDeclaration();
    if (!ad || parent->isUnionDeclaration())
    {
	error("constructors are only for class or struct definitions");
	tret = Type::tvoid;
    }
    else
    {	tret = ad->handle;
	assert(tret);
    }
    type = new TypeFunction(arguments, tret, varargs, LINKd);
#if STRUCTTHISREF
    if (ad && ad->isStructDeclaration())
	((TypeFunction *)type)->isref = 1;
#endif
    if (!originalType)
	originalType = type;

    sc->flags |= SCOPEctor;
    type = type->semantic(loc, sc);
    sc->flags &= ~SCOPEctor;

    // Append:
    //	return this;
    // to the function body
    if (fbody)
    {
	Expression *e = new ThisExp(loc);
	Statement *s = new ReturnStatement(loc, e);
	fbody = new CompoundStatement(loc, fbody, s);
    }

    FuncDeclaration::semantic(sc);

    sc->pop();

    // See if it's the default constructor
    if (ad && varargs == 0 && Argument::dim(arguments) == 0)
    {	if (ad->isStructDeclaration())
	    error("default constructor not allowed for structs");
	else
	    ad->defaultCtor = this;
    }
}

const char *CtorDeclaration::kind()
{
    return "constructor";
}

char *CtorDeclaration::toChars()
{
    return (char *)"this";
}

int CtorDeclaration::isVirtual()
{
    return FALSE;
}

int CtorDeclaration::addPreInvariant()
{
    return FALSE;
}

int CtorDeclaration::addPostInvariant()
{
    return (isThis() && vthis && global.params.useInvariants);
}


void CtorDeclaration::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("this");
    Argument::argsToCBuffer(buf, hgs, arguments, varargs);
    bodyToCBuffer(buf, hgs);
}

/********************************* PostBlitDeclaration ****************************/

#if DMDV2
PostBlitDeclaration::PostBlitDeclaration(Loc loc, Loc endloc)
    : FuncDeclaration(loc, endloc, Id::_postblit, STCundefined, NULL)
{
}

PostBlitDeclaration::PostBlitDeclaration(Loc loc, Loc endloc, Identifier *id)
    : FuncDeclaration(loc, endloc, id, STCundefined, NULL)
{
}

Dsymbol *PostBlitDeclaration::syntaxCopy(Dsymbol *s)
{
    assert(!s);
    PostBlitDeclaration *dd = new PostBlitDeclaration(loc, endloc, ident);
    return FuncDeclaration::syntaxCopy(dd);
}


void PostBlitDeclaration::semantic(Scope *sc)
{
    //printf("PostBlitDeclaration::semantic() %s\n", toChars());
    //printf("ident: %s, %s, %p, %p\n", ident->toChars(), Id::dtor->toChars(), ident, Id::dtor);
    parent = sc->parent;
    Dsymbol *parent = toParent();
    StructDeclaration *ad = parent->isStructDeclaration();
    if (!ad)
    {
	error("post blits are only for struct/union definitions, not %s %s", parent->kind(), parent->toChars());
    }
    else if (ident == Id::_postblit)
	ad->postblits.push(this);
    type = new TypeFunction(NULL, Type::tvoid, FALSE, LINKd);

    sc = sc->push();
    sc->stc &= ~STCstatic;		// not static
    sc->linkage = LINKd;

    FuncDeclaration::semantic(sc);

    sc->pop();
}

int PostBlitDeclaration::overloadInsert(Dsymbol *s)
{
    return FALSE;	// cannot overload postblits
}

int PostBlitDeclaration::addPreInvariant()
{
    return FALSE;
}

int PostBlitDeclaration::addPostInvariant()
{
    return (isThis() && vthis && global.params.useInvariants);
}

int PostBlitDeclaration::isVirtual()
{
    return FALSE;
}

void PostBlitDeclaration::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("this(this)");
    bodyToCBuffer(buf, hgs);
}
#endif

/********************************* DtorDeclaration ****************************/

DtorDeclaration::DtorDeclaration(Loc loc, Loc endloc)
    : FuncDeclaration(loc, endloc, Id::dtor, STCundefined, NULL)
{
}

DtorDeclaration::DtorDeclaration(Loc loc, Loc endloc, Identifier *id)
    : FuncDeclaration(loc, endloc, id, STCundefined, NULL)
{
}

Dsymbol *DtorDeclaration::syntaxCopy(Dsymbol *s)
{
    assert(!s);
    DtorDeclaration *dd = new DtorDeclaration(loc, endloc, ident);
    return FuncDeclaration::syntaxCopy(dd);
}


void DtorDeclaration::semantic(Scope *sc)
{
    //printf("DtorDeclaration::semantic() %s\n", toChars());
    //printf("ident: %s, %s, %p, %p\n", ident->toChars(), Id::dtor->toChars(), ident, Id::dtor);
    parent = sc->parent;
    Dsymbol *parent = toParent();
    AggregateDeclaration *ad = parent->isAggregateDeclaration();
    if (!ad)
    {
	error("destructors are only for class/struct/union definitions, not %s %s", parent->kind(), parent->toChars());
    }
    else if (ident == Id::dtor)
	ad->dtors.push(this);
    type = new TypeFunction(NULL, Type::tvoid, FALSE, LINKd);

    sc = sc->push();
    sc->stc &= ~STCstatic;		// not a static destructor
    sc->linkage = LINKd;

    FuncDeclaration::semantic(sc);

    sc->pop();
}

int DtorDeclaration::overloadInsert(Dsymbol *s)
{
    return FALSE;	// cannot overload destructors
}

int DtorDeclaration::addPreInvariant()
{
    return (isThis() && vthis && global.params.useInvariants);
}

int DtorDeclaration::addPostInvariant()
{
    return FALSE;
}

const char *DtorDeclaration::kind()
{	
    return "destructor";
}

char *DtorDeclaration::toChars()
{
    return (char *)"~this";
}

int DtorDeclaration::isVirtual()
{
    /* This should be FALSE so that dtor's don't get put into the vtbl[],
     * but doing so will require recompiling everything.
     */
#if BREAKABI
    return FALSE;
#else
    return FuncDeclaration::isVirtual();
#endif
}

void DtorDeclaration::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("~this()");
    bodyToCBuffer(buf, hgs);
}

/********************************* StaticCtorDeclaration ****************************/

StaticCtorDeclaration::StaticCtorDeclaration(Loc loc, Loc endloc)
    : FuncDeclaration(loc, endloc,
      Identifier::generateId("_staticCtor"), STCstatic, NULL)
{
}

Dsymbol *StaticCtorDeclaration::syntaxCopy(Dsymbol *s)
{
    StaticCtorDeclaration *scd;

    assert(!s);
    scd = new StaticCtorDeclaration(loc, endloc);
    return FuncDeclaration::syntaxCopy(scd);
}


void StaticCtorDeclaration::semantic(Scope *sc)
{
    //printf("StaticCtorDeclaration::semantic()\n");

    type = new TypeFunction(NULL, Type::tvoid, FALSE, LINKd);

    /* If the static ctor appears within a template instantiation,
     * it could get called multiple times by the module constructors
     * for different modules. Thus, protect it with a gate.
     */
    if (inTemplateInstance())
    {
	/* Add this prefix to the function:
	 *	static int gate;
	 *	if (++gate != 1) return;
	 * Note that this is not thread safe; should not have threads
	 * during static construction.
	 */
	Identifier *id = Lexer::idPool("__gate");
	VarDeclaration *v = new VarDeclaration(0, Type::tint32, id, NULL);
	v->storage_class = STCstatic;
	Statements *sa = new Statements();
	Statement *s = new DeclarationStatement(0, v);
	sa->push(s);
	Expression *e = new IdentifierExp(0, id);
	e = new AddAssignExp(0, e, new IntegerExp(1));
	e = new EqualExp(TOKnotequal, 0, e, new IntegerExp(1));
	s = new IfStatement(0, NULL, e, new ReturnStatement(0, NULL), NULL);
	sa->push(s);
	if (fbody)
	    sa->push(fbody);
	fbody = new CompoundStatement(0, sa);
    }

    FuncDeclaration::semantic(sc);

    // We're going to need ModuleInfo
    Module *m = getModule();
    if (!m)
	m = sc->module;
    if (m)
    {	m->needmoduleinfo = 1;
	//printf("module1 %s needs moduleinfo\n", m->toChars());
#ifdef IN_GCC
	m->strictlyneedmoduleinfo = 1;
#endif
    }
}

AggregateDeclaration *StaticCtorDeclaration::isThis()
{
    return NULL;
}

int StaticCtorDeclaration::isStaticConstructor()
{
    return TRUE;
}

int StaticCtorDeclaration::isVirtual()
{
    return FALSE;
}

int StaticCtorDeclaration::addPreInvariant()
{
    return FALSE;
}

int StaticCtorDeclaration::addPostInvariant()
{
    return FALSE;
}

void StaticCtorDeclaration::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    if (hgs->hdrgen)
    {	buf->writestring("static this();\n");
	return;
    }
    buf->writestring("static this()");
    bodyToCBuffer(buf, hgs);
}

/********************************* StaticDtorDeclaration ****************************/

StaticDtorDeclaration::StaticDtorDeclaration(Loc loc, Loc endloc)
    : FuncDeclaration(loc, endloc,
      Identifier::generateId("_staticDtor"), STCstatic, NULL)
{
    vgate = NULL;
}

Dsymbol *StaticDtorDeclaration::syntaxCopy(Dsymbol *s)
{
    StaticDtorDeclaration *sdd;

    assert(!s);
    sdd = new StaticDtorDeclaration(loc, endloc);
    return FuncDeclaration::syntaxCopy(sdd);
}


void StaticDtorDeclaration::semantic(Scope *sc)
{
    ClassDeclaration *cd;
    Type *tret;

    cd = sc->scopesym->isClassDeclaration();
    if (!cd)
    {
    }
    type = new TypeFunction(NULL, Type::tvoid, FALSE, LINKd);

    /* If the static ctor appears within a template instantiation,
     * it could get called multiple times by the module constructors
     * for different modules. Thus, protect it with a gate.
     */
    if (inTemplateInstance())
    {
	/* Add this prefix to the function:
	 *	static int gate;
	 *	if (--gate != 0) return;
	 * Increment gate during constructor execution.
	 * Note that this is not thread safe; should not have threads
	 * during static destruction.
	 */
	Identifier *id = Lexer::idPool("__gate");
	VarDeclaration *v = new VarDeclaration(0, Type::tint32, id, NULL);
	v->storage_class = STCstatic;
	Statements *sa = new Statements();
	Statement *s = new DeclarationStatement(0, v);
	sa->push(s);
	Expression *e = new IdentifierExp(0, id);
	e = new AddAssignExp(0, e, new IntegerExp(-1));
	e = new EqualExp(TOKnotequal, 0, e, new IntegerExp(0));
	s = new IfStatement(0, NULL, e, new ReturnStatement(0, NULL), NULL);
	sa->push(s);
	if (fbody)
	    sa->push(fbody);
	fbody = new CompoundStatement(0, sa);
	vgate = v;
    }

    FuncDeclaration::semantic(sc);

    // We're going to need ModuleInfo
    Module *m = getModule();
    if (!m)
	m = sc->module;
    if (m)
    {	m->needmoduleinfo = 1;
	//printf("module2 %s needs moduleinfo\n", m->toChars());
#ifdef IN_GCC
	m->strictlyneedmoduleinfo = 1;
#endif
    }
}

AggregateDeclaration *StaticDtorDeclaration::isThis()
{
    return NULL;
}

int StaticDtorDeclaration::isStaticDestructor()
{
    return TRUE;
}

int StaticDtorDeclaration::isVirtual()
{
    return FALSE;
}

int StaticDtorDeclaration::addPreInvariant()
{
    return FALSE;
}

int StaticDtorDeclaration::addPostInvariant()
{
    return FALSE;
}

void StaticDtorDeclaration::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    if (hgs->hdrgen)
	return;
    buf->writestring("static ~this()");
    bodyToCBuffer(buf, hgs);
}

/********************************* InvariantDeclaration ****************************/

InvariantDeclaration::InvariantDeclaration(Loc loc, Loc endloc)
    : FuncDeclaration(loc, endloc, Id::classInvariant, STCundefined, NULL)
{
}

Dsymbol *InvariantDeclaration::syntaxCopy(Dsymbol *s)
{
    InvariantDeclaration *id;

    assert(!s);
    id = new InvariantDeclaration(loc, endloc);
    FuncDeclaration::syntaxCopy(id);
    return id;
}


void InvariantDeclaration::semantic(Scope *sc)
{
    AggregateDeclaration *ad;
    Type *tret;

    parent = sc->parent;
    Dsymbol *parent = toParent();
    ad = parent->isAggregateDeclaration();
    if (!ad)
    {
	error("invariants are only for struct/union/class definitions");
	return;
    }
    else if (ad->inv && ad->inv != this)
    {
	error("more than one invariant for %s", ad->toChars());
    }
    ad->inv = this;
    type = new TypeFunction(NULL, Type::tvoid, FALSE, LINKd);

    sc = sc->push();
    sc->stc &= ~STCstatic;		// not a static invariant
    sc->incontract++;
    sc->linkage = LINKd;

    FuncDeclaration::semantic(sc);

    sc->pop();
}

int InvariantDeclaration::isVirtual()
{
    return FALSE;
}

int InvariantDeclaration::addPreInvariant()
{
    return FALSE;
}

int InvariantDeclaration::addPostInvariant()
{
    return FALSE;
}

void InvariantDeclaration::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    if (hgs->hdrgen)
	return;
    buf->writestring("invariant");
    bodyToCBuffer(buf, hgs);
}


/********************************* UnitTestDeclaration ****************************/

/*******************************
 * Generate unique unittest function Id so we can have multiple
 * instances per module.
 */

static Identifier *unitTestId()
{
    return Lexer::uniqueId("__unittest");
}

UnitTestDeclaration::UnitTestDeclaration(Loc loc, Loc endloc)
    : FuncDeclaration(loc, endloc, unitTestId(), STCundefined, NULL)
{
}

Dsymbol *UnitTestDeclaration::syntaxCopy(Dsymbol *s)
{
    UnitTestDeclaration *utd;

    assert(!s);
    utd = new UnitTestDeclaration(loc, endloc);
    return FuncDeclaration::syntaxCopy(utd);
}


void UnitTestDeclaration::semantic(Scope *sc)
{
    if (global.params.useUnitTests)
    {
	type = new TypeFunction(NULL, Type::tvoid, FALSE, LINKd);
	Scope *sc2 = sc->push();
	sc2->linkage = LINKd;
	FuncDeclaration::semantic(sc2);
	sc2->pop();
    }

#if 0
    // We're going to need ModuleInfo even if the unit tests are not
    // compiled in, because other modules may import this module and refer
    // to this ModuleInfo.
    // (This doesn't make sense to me?)
    Module *m = getModule();
    if (!m)
	m = sc->module;
    if (m)
    {
	//printf("module3 %s needs moduleinfo\n", m->toChars());
	m->needmoduleinfo = 1;
    }
#endif
}

AggregateDeclaration *UnitTestDeclaration::isThis()
{
    return NULL;
}

int UnitTestDeclaration::isVirtual()
{
    return FALSE;
}

int UnitTestDeclaration::addPreInvariant()
{
    return FALSE;
}

int UnitTestDeclaration::addPostInvariant()
{
    return FALSE;
}

void UnitTestDeclaration::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    if (hgs->hdrgen)
	return;
    buf->writestring("unittest");
    bodyToCBuffer(buf, hgs);
}

/********************************* NewDeclaration ****************************/

NewDeclaration::NewDeclaration(Loc loc, Loc endloc, Arguments *arguments, int varargs)
    : FuncDeclaration(loc, endloc, Id::classNew, STCstatic, NULL)
{
    this->arguments = arguments;
    this->varargs = varargs;
}

Dsymbol *NewDeclaration::syntaxCopy(Dsymbol *s)
{
    NewDeclaration *f;

    f = new NewDeclaration(loc, endloc, NULL, varargs);

    FuncDeclaration::syntaxCopy(f);

    f->arguments = Argument::arraySyntaxCopy(arguments);

    return f;
}


void NewDeclaration::semantic(Scope *sc)
{
    ClassDeclaration *cd;
    Type *tret;

    //printf("NewDeclaration::semantic()\n");

    parent = sc->parent;
    Dsymbol *parent = toParent();
    cd = parent->isClassDeclaration();
    if (!cd && !parent->isStructDeclaration())
    {
	error("new allocators only are for class or struct definitions");
    }
    tret = Type::tvoid->pointerTo();
    type = new TypeFunction(arguments, tret, varargs, LINKd);

    type = type->semantic(loc, sc);
    assert(type->ty == Tfunction);

    // Check that there is at least one argument of type size_t
    TypeFunction *tf = (TypeFunction *)type;
    if (Argument::dim(tf->parameters) < 1)
    {
	error("at least one argument of type size_t expected");
    }
    else
    {
	Argument *a = Argument::getNth(tf->parameters, 0);
	if (!a->type->equals(Type::tsize_t))
	    error("first argument must be type size_t, not %s", a->type->toChars());
    }

    FuncDeclaration::semantic(sc);
}

const char *NewDeclaration::kind()
{
    return "allocator";
}

int NewDeclaration::isVirtual()
{
    return FALSE;
}

int NewDeclaration::addPreInvariant()
{
    return FALSE;
}

int NewDeclaration::addPostInvariant()
{
    return FALSE;
}

void NewDeclaration::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("new");
    Argument::argsToCBuffer(buf, hgs, arguments, varargs);
    bodyToCBuffer(buf, hgs);
}


/********************************* DeleteDeclaration ****************************/

DeleteDeclaration::DeleteDeclaration(Loc loc, Loc endloc, Arguments *arguments)
    : FuncDeclaration(loc, endloc, Id::classDelete, STCstatic, NULL)
{
    this->arguments = arguments;
}

Dsymbol *DeleteDeclaration::syntaxCopy(Dsymbol *s)
{
    DeleteDeclaration *f;

    f = new DeleteDeclaration(loc, endloc, NULL);

    FuncDeclaration::syntaxCopy(f);

    f->arguments = Argument::arraySyntaxCopy(arguments);

    return f;
}


void DeleteDeclaration::semantic(Scope *sc)
{
    ClassDeclaration *cd;

    //printf("DeleteDeclaration::semantic()\n");

    parent = sc->parent;
    Dsymbol *parent = toParent();
    cd = parent->isClassDeclaration();
    if (!cd && !parent->isStructDeclaration())
    {
	error("new allocators only are for class or struct definitions");
    }
    type = new TypeFunction(arguments, Type::tvoid, 0, LINKd);

    type = type->semantic(loc, sc);
    assert(type->ty == Tfunction);

    // Check that there is only one argument of type void*
    TypeFunction *tf = (TypeFunction *)type;
    if (Argument::dim(tf->parameters) != 1)
    {
	error("one argument of type void* expected");
    }
    else
    {
	Argument *a = Argument::getNth(tf->parameters, 0);
	if (!a->type->equals(Type::tvoid->pointerTo()))
	    error("one argument of type void* expected, not %s", a->type->toChars());
    }

    FuncDeclaration::semantic(sc);
}

const char *DeleteDeclaration::kind()
{
    return "deallocator";
}

int DeleteDeclaration::isDelete()
{
    return TRUE;
}

int DeleteDeclaration::isVirtual()
{
    return FALSE;
}

int DeleteDeclaration::addPreInvariant()
{
    return FALSE;
}

int DeleteDeclaration::addPostInvariant()
{
    return FALSE;
}

void DeleteDeclaration::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("delete");
    Argument::argsToCBuffer(buf, hgs, arguments, 0);
    bodyToCBuffer(buf, hgs);
}




