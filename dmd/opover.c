
// Compiler implementation of the D programming language
// Copyright (c) 1999-2009 by Digital Mars
// All Rights Reserved
// written by Walter Bright
// http://www.digitalmars.com
// License for redistribution is by either the Artistic License
// in artistic.txt, or the GNU General Public License in gnu.txt.
// See the included readme.txt for details.

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#if _MSC_VER
#include <complex>
#else
#include <complex.h>
#endif

#ifdef __APPLE__
#define integer_t dmd_integer_t
#endif

#include "rmem.h"

//#include "port.h"
#include "mtype.h"
#include "init.h"
#include "expression.h"
#include "id.h"
#include "declaration.h"
#include "aggregate.h"
#include "template.h"

static void inferApplyArgTypesX(FuncDeclaration *fstart, Arguments *arguments);
static void inferApplyArgTypesZ(TemplateDeclaration *tstart, Arguments *arguments);
static int inferApplyArgTypesY(TypeFunction *tf, Arguments *arguments);
static void templateResolve(Match *m, TemplateDeclaration *td, Scope *sc, Loc loc, Objects *targsi, Expression *ethis, Expressions *arguments);

/******************************** Expression **************************/


/***********************************
 * Determine if operands of binary op can be reversed
 * to fit operator overload.
 */

int Expression::isCommutative()
{
    return FALSE;	// default is no reverse
}

/***********************************
 * Get Identifier for operator overload.
 */

Identifier *Expression::opId()
{
    assert(0);
    return NULL;
}

/***********************************
 * Get Identifier for reverse operator overload,
 * NULL if not supported for this operator.
 */

Identifier *Expression::opId_r()
{
    return NULL;
}

/************************* Operators *****************************/

Identifier *UAddExp::opId()   { return Id::uadd; }

Identifier *NegExp::opId()   { return Id::neg; }

Identifier *ComExp::opId()   { return Id::com; }

Identifier *CastExp::opId()   { return Id::cast; }

Identifier *InExp::opId()     { return Id::opIn; }
Identifier *InExp::opId_r()     { return Id::opIn_r; }

Identifier *PostExp::opId() { return (op == TOKplusplus)
				? Id::postinc
				: Id::postdec; }

int AddExp::isCommutative()  { return TRUE; }
Identifier *AddExp::opId()   { return Id::add; }
Identifier *AddExp::opId_r() { return Id::add_r; }

Identifier *MinExp::opId()   { return Id::sub; }
Identifier *MinExp::opId_r() { return Id::sub_r; }

int MulExp::isCommutative()  { return TRUE; }
Identifier *MulExp::opId()   { return Id::mul; }
Identifier *MulExp::opId_r() { return Id::mul_r; }

Identifier *DivExp::opId()   { return Id::div; }
Identifier *DivExp::opId_r() { return Id::div_r; }

Identifier *ModExp::opId()   { return Id::mod; }
Identifier *ModExp::opId_r() { return Id::mod_r; }

Identifier *ShlExp::opId()   { return Id::shl; }
Identifier *ShlExp::opId_r() { return Id::shl_r; }

Identifier *ShrExp::opId()   { return Id::shr; }
Identifier *ShrExp::opId_r() { return Id::shr_r; }

Identifier *UshrExp::opId()   { return Id::ushr; }
Identifier *UshrExp::opId_r() { return Id::ushr_r; }

int AndExp::isCommutative()  { return TRUE; }
Identifier *AndExp::opId()   { return Id::iand; }
Identifier *AndExp::opId_r() { return Id::iand_r; }

int OrExp::isCommutative()  { return TRUE; }
Identifier *OrExp::opId()   { return Id::ior; }
Identifier *OrExp::opId_r() { return Id::ior_r; }

int XorExp::isCommutative()  { return TRUE; }
Identifier *XorExp::opId()   { return Id::ixor; }
Identifier *XorExp::opId_r() { return Id::ixor_r; }

Identifier *CatExp::opId()   { return Id::cat; }
Identifier *CatExp::opId_r() { return Id::cat_r; }

Identifier *    AssignExp::opId()  { return Id::assign;  }
Identifier * AddAssignExp::opId()  { return Id::addass;  }
Identifier * MinAssignExp::opId()  { return Id::subass;  }
Identifier * MulAssignExp::opId()  { return Id::mulass;  }
Identifier * DivAssignExp::opId()  { return Id::divass;  }
Identifier * ModAssignExp::opId()  { return Id::modass;  }
Identifier * AndAssignExp::opId()  { return Id::andass;  }
Identifier *  OrAssignExp::opId()  { return Id::orass;   }
Identifier * XorAssignExp::opId()  { return Id::xorass;  }
Identifier * ShlAssignExp::opId()  { return Id::shlass;  }
Identifier * ShrAssignExp::opId()  { return Id::shrass;  }
Identifier *UshrAssignExp::opId()  { return Id::ushrass; }
Identifier * CatAssignExp::opId()  { return Id::catass;  }

int EqualExp::isCommutative()  { return TRUE; }
Identifier *EqualExp::opId()   { return Id::eq; }

int CmpExp::isCommutative()  { return TRUE; }
Identifier *CmpExp::opId()   { return Id::cmp; }

Identifier *ArrayExp::opId()	{ return Id::index; }
Identifier *PtrExp::opId()	{ return Id::opStar; }

/************************************
 * Operator overload.
 * Check for operator overload, if so, replace
 * with function call.
 * Return NULL if not an operator overload.
 */

Expression *UnaExp::op_overload(Scope *sc)
{
    //printf("UnaExp::op_overload() (%s)\n", toChars());
    AggregateDeclaration *ad;
    Dsymbol *fd;
    Type *t1 = e1->type->toBasetype();

    if (t1->ty == Tclass)
    {
	ad = ((TypeClass *)t1)->sym;
	goto L1;
    }
    else if (t1->ty == Tstruct)
    {
	ad = ((TypeStruct *)t1)->sym;

    L1:
	fd = search_function(ad, opId());
	if (fd)
	{
	    if (op == TOKarray)
	    {
		/* Rewrite op e1[arguments] as:
		 *    e1.fd(arguments)
		 */
		Expression *e = new DotIdExp(loc, e1, fd->ident);
		ArrayExp *ae = (ArrayExp *)this;
		e = new CallExp(loc, e, ae->arguments);
		e = e->semantic(sc);
		return e;
	    }
	    else
	    {
		// Rewrite +e1 as e1.add()
		return build_overload(loc, sc, e1, NULL, fd->ident);
	    }
	}

#if DMDV2
	// Didn't find it. Forward to aliasthis
	if (ad->aliasthis)
	{
	    /* Rewrite op(e1) as:
	     *	op(e1.aliasthis)
	     */
	    Expression *e1 = new DotIdExp(loc, this->e1, ad->aliasthis->ident);
	    Expression *e = copy();
	    ((UnaExp *)e)->e1 = e1;
	    e = e->semantic(sc);
	    return e;
	}
#endif
    }
    return NULL;
}


Expression *BinExp::op_overload(Scope *sc)
{
    //printf("BinExp::op_overload() (%s)\n", toChars());

    AggregateDeclaration *ad;
    Type *t1 = e1->type->toBasetype();
    Type *t2 = e2->type->toBasetype();
    Identifier *id = opId();
    Identifier *id_r = opId_r();

    Match m;
    Expressions args1;
    Expressions args2;
    int argsset = 0;

    AggregateDeclaration *ad1;
    if (t1->ty == Tclass)
	ad1 = ((TypeClass *)t1)->sym;
    else if (t1->ty == Tstruct)
	ad1 = ((TypeStruct *)t1)->sym;
    else
	ad1 = NULL;

    AggregateDeclaration *ad2;
    if (t2->ty == Tclass)
	ad2 = ((TypeClass *)t2)->sym;
    else if (t2->ty == Tstruct)
	ad2 = ((TypeStruct *)t2)->sym;
    else
	ad2 = NULL;

    Dsymbol *s = NULL;
    Dsymbol *s_r = NULL;
    FuncDeclaration *fd = NULL;
    TemplateDeclaration *td = NULL;
    if (ad1 && id)
    {
	s = search_function(ad1, id);
    }
    if (ad2 && id_r)
    {
	s_r = search_function(ad2, id_r);
    }

    if (s || s_r)
    {
	/* Try:
	 *	a.opfunc(b)
	 *	b.opfunc_r(a)
	 * and see which is better.
	 */
	Expression *e;
	FuncDeclaration *lastf;

	args1.setDim(1);
	args1.data[0] = (void*) e1;
	args2.setDim(1);
	args2.data[0] = (void*) e2;
	argsset = 1;

	memset(&m, 0, sizeof(m));
	m.last = MATCHnomatch;

	if (s)
	{
	    fd = s->isFuncDeclaration();
	    if (fd)
	    {
		overloadResolveX(&m, fd, NULL, &args2);
	    }
	    else
	    {   td = s->isTemplateDeclaration();
		templateResolve(&m, td, sc, loc, NULL, NULL, &args2);
	    }
	}
	
	lastf = m.lastf;

	if (s_r)
	{
	    fd = s_r->isFuncDeclaration();
	    if (fd)
	    {
		overloadResolveX(&m, fd, NULL, &args1);
	    }
	    else
	    {   td = s_r->isTemplateDeclaration();
		templateResolve(&m, td, sc, loc, NULL, NULL, &args1);
	    }
	}

	if (m.count > 1)
	{
	    // Error, ambiguous
	    error("overloads %s and %s both match argument list for %s",
		    m.lastf->type->toChars(),
		    m.nextf->type->toChars(),
		    m.lastf->toChars());
	}
	else if (m.last == MATCHnomatch)
	{
	    m.lastf = m.anyf;
	}

	if (op == TOKplusplus || op == TOKminusminus)
	    // Kludge because operator overloading regards e++ and e--
	    // as unary, but it's implemented as a binary.
	    // Rewrite (e1 ++ e2) as e1.postinc()
	    // Rewrite (e1 -- e2) as e1.postdec()
	    e = build_overload(loc, sc, e1, NULL, id);
	else if (lastf && m.lastf == lastf || m.last == MATCHnomatch)
	    // Rewrite (e1 op e2) as e1.opfunc(e2)
	    e = build_overload(loc, sc, e1, e2, id);
	else
	    // Rewrite (e1 op e2) as e2.opfunc_r(e1)
	    e = build_overload(loc, sc, e2, e1, id_r);
	return e;
    }

    if (isCommutative())
    {
	s = NULL;
	s_r = NULL;
	if (ad1 && id_r)
	{
	    s_r = search_function(ad1, id_r);
	}
	if (ad2 && id)
	{
	    s = search_function(ad2, id);
	}

	if (s || s_r)
	{
	    /* Try:
	     *	a.opfunc_r(b)
	     *	b.opfunc(a)
	     * and see which is better.
	     */

	    if (!argsset)
	    {	args1.setDim(1);
		args1.data[0] = (void*) e1;
		args2.setDim(1);
		args2.data[0] = (void*) e2;
	    }

	    memset(&m, 0, sizeof(m));
	    m.last = MATCHnomatch;

	    if (s_r)
	    {
		fd = s_r->isFuncDeclaration();
		if (fd)
		{
		    overloadResolveX(&m, fd, NULL, &args2);
		}
		else
		{   td = s_r->isTemplateDeclaration();
		    templateResolve(&m, td, sc, loc, NULL, NULL, &args2);
		}
	    }
	    FuncDeclaration *lastf = m.lastf;

	    if (s)
	    {
		fd = s->isFuncDeclaration();
		if (fd)
		{
		    overloadResolveX(&m, fd, NULL, &args1);
		}
		else
		{   td = s->isTemplateDeclaration();
		    templateResolve(&m, td, sc, loc, NULL, NULL, &args1);
		}
	    }

	    if (m.count > 1)
	    {
		// Error, ambiguous
		error("overloads %s and %s both match argument list for %s",
			m.lastf->type->toChars(),
			m.nextf->type->toChars(),
			m.lastf->toChars());
	    }
	    else if (m.last == MATCHnomatch)
	    {
		m.lastf = m.anyf;
	    }

	    Expression *e;
	    if (lastf && m.lastf == lastf ||
		id_r && m.last == MATCHnomatch)
		// Rewrite (e1 op e2) as e1.opfunc_r(e2)
		e = build_overload(loc, sc, e1, e2, id_r);
	    else
		// Rewrite (e1 op e2) as e2.opfunc(e1)
		e = build_overload(loc, sc, e2, e1, id);

	    // When reversing operands of comparison operators,
	    // need to reverse the sense of the op
	    switch (op)
	    {
		case TOKlt:	op = TOKgt;	break;
		case TOKgt:	op = TOKlt;	break;
		case TOKle:	op = TOKge;	break;
		case TOKge:	op = TOKle;	break;

		// Floating point compares
		case TOKule:	op = TOKuge;	 break;
		case TOKul:	op = TOKug;	 break;
		case TOKuge:	op = TOKule;	 break;
		case TOKug:	op = TOKul;	 break;

		// These are symmetric
		case TOKunord:
		case TOKlg:
		case TOKleg:
		case TOKue:
		    break;
	    }

	    return e;
	}
    }

#if DMDV2
    // Try alias this on first operand
    if (ad1 && ad1->aliasthis)
    {
	/* Rewrite (e1 op e2) as:
	 *	(e1.aliasthis op e2)
	 */
	Expression *e1 = new DotIdExp(loc, this->e1, ad1->aliasthis->ident);
	Expression *e = copy();
	((BinExp *)e)->e1 = e1;
	e = e->semantic(sc);
	return e;
    }

    // Try alias this on second operand
    if (ad2 && ad2->aliasthis)
    {
	/* Rewrite (e1 op e2) as:
	 *	(e1 op e2.aliasthis)
	 */
	Expression *e2 = new DotIdExp(loc, this->e2, ad2->aliasthis->ident);
	Expression *e = copy();
	((BinExp *)e)->e2 = e2;
	e = e->semantic(sc);
	return e;
    }
#endif
    return NULL;
}

/***********************************
 * Utility to build a function call out of this reference and argument.
 */

Expression *build_overload(Loc loc, Scope *sc, Expression *ethis, Expression *earg, Identifier *id)
{
    Expression *e;

    //printf("build_overload(id = '%s')\n", id->toChars());
    //earg->print();
    //earg->type->print();
    e = new DotIdExp(loc, ethis, id);

    if (earg)
	e = new CallExp(loc, e, earg);
    else
	e = new CallExp(loc, e);

    e = e->semantic(sc);
    return e;
}

/***************************************
 * Search for function funcid in aggregate ad.
 */

Dsymbol *search_function(ScopeDsymbol *ad, Identifier *funcid)
{
    Dsymbol *s;
    FuncDeclaration *fd;
    TemplateDeclaration *td;

    s = ad->search(0, funcid, 0);
    if (s)
    {	Dsymbol *s2;

	//printf("search_function: s = '%s'\n", s->kind());
	s2 = s->toAlias();
	//printf("search_function: s2 = '%s'\n", s2->kind());
	fd = s2->isFuncDeclaration();
	if (fd && fd->type->ty == Tfunction)
	    return fd;

	td = s2->isTemplateDeclaration();
	if (td)
	    return td;
    }
    return NULL;
}


/*****************************************
 * Given array of arguments and an aggregate type,
 * if any of the argument types are missing, attempt to infer
 * them from the aggregate type.
 */

void inferApplyArgTypes(enum TOK op, Arguments *arguments, Expression *aggr)
{
    if (!arguments || !arguments->dim)
	return;

    /* Return if no arguments need types.
     */
    for (size_t u = 0; 1; u++)
    {	if (u == arguments->dim)
	    return;
	Argument *arg = (Argument *)arguments->data[u];
	if (!arg->type)
	    break;
    }

    AggregateDeclaration *ad;

    Argument *arg = (Argument *)arguments->data[0];
    Type *taggr = aggr->type;
    if (!taggr)
	return;
    Type *tab = taggr->toBasetype();
    switch (tab->ty)
    {
	case Tarray:
	case Tsarray:
	case Ttuple:
	    if (arguments->dim == 2)
	    {
		if (!arg->type)
		    arg->type = Type::tsize_t;	// key type
		arg = (Argument *)arguments->data[1];
	    }
	    if (!arg->type && tab->ty != Ttuple)
		arg->type = tab->nextOf();	// value type
	    break;

	case Taarray:
	{   TypeAArray *taa = (TypeAArray *)tab;

	    if (arguments->dim == 2)
	    {
		if (!arg->type)
		    arg->type = taa->index;	// key type
		arg = (Argument *)arguments->data[1];
	    }
	    if (!arg->type)
		arg->type = taa->next;		// value type
	    break;
	}

	case Tclass:
	    ad = ((TypeClass *)tab)->sym;
	    goto Laggr;

	case Tstruct:
	    ad = ((TypeStruct *)tab)->sym;
	    goto Laggr;

	Laggr:
	    if (arguments->dim == 1)
	    {
		if (!arg->type)
		{
		    /* Look for a head() or rear() overload
		     */
		    Identifier *id = (op == TOKforeach) ? Id::Fhead : Id::Ftoe;
		    Dsymbol *s = search_function(ad, id);
		    FuncDeclaration *fd = s ? s->isFuncDeclaration() : NULL;
		    if (!fd)
		    {	if (s && s->isTemplateDeclaration())
			    break;
			goto Lapply;
		    }
		    arg->type = fd->type->nextOf();
		}
		break;
	    }

	Lapply:
	{   /* Look for an
	     *	int opApply(int delegate(ref Type [, ...]) dg);
	     * overload
	     */
	    Dsymbol *s = search_function(ad,
			(op == TOKforeach_reverse) ? Id::applyReverse
						   : Id::apply);
	    if (s)
	    {
		FuncDeclaration *fd = s->isFuncDeclaration();
		if (fd) 
		{   inferApplyArgTypesX(fd, arguments);
		    break;
		}
#if 0
		TemplateDeclaration *td = s->isTemplateDeclaration();
		if (td)
		{   inferApplyArgTypesZ(td, arguments);
		    break;
		}
#endif
	    }
	    break;
	}

	case Tdelegate:
	{
	    if (0 && aggr->op == TOKdelegate)
	    {	DelegateExp *de = (DelegateExp *)aggr;

		FuncDeclaration *fd = de->func->isFuncDeclaration();
		if (fd)
		    inferApplyArgTypesX(fd, arguments);
	    }
	    else
	    {
		inferApplyArgTypesY((TypeFunction *)tab->nextOf(), arguments);
	    }
	    break;
	}

	default:
	    break;		// ignore error, caught later
    }
}

/********************************
 * Recursive helper function,
 * analogous to func.overloadResolveX().
 */

int fp3(void *param, FuncDeclaration *f)
{
    Arguments *arguments = (Arguments *)param;
    TypeFunction *tf = (TypeFunction *)f->type;
    if (inferApplyArgTypesY(tf, arguments) == 1)
	return 0;
    if (arguments->dim == 0)
	return 1;
    return 0;
}

static void inferApplyArgTypesX(FuncDeclaration *fstart, Arguments *arguments)
{
    overloadApply(fstart, &fp3, arguments);
}

/******************************
 * Infer arguments from type of function.
 * Returns:
 *	0 match for this function
 *	1 no match for this function
 */

static int inferApplyArgTypesY(TypeFunction *tf, Arguments *arguments)
{   size_t nparams;
    Argument *p;

    if (Argument::dim(tf->parameters) != 1)
	goto Lnomatch;
    p = Argument::getNth(tf->parameters, 0);
    if (p->type->ty != Tdelegate)
	goto Lnomatch;
    tf = (TypeFunction *)p->type->nextOf();
    assert(tf->ty == Tfunction);

    /* We now have tf, the type of the delegate. Match it against
     * the arguments, filling in missing argument types.
     */
    nparams = Argument::dim(tf->parameters);
    if (nparams == 0 || tf->varargs)
	goto Lnomatch;		// not enough parameters
    if (arguments->dim != nparams)
	goto Lnomatch;		// not enough parameters

    for (size_t u = 0; u < nparams; u++)
    {
	Argument *arg = (Argument *)arguments->data[u];
	Argument *param = Argument::getNth(tf->parameters, u);
	if (arg->type)
	{   if (!arg->type->equals(param->type))
	    {
		/* Cannot resolve argument types. Indicate an
		 * error by setting the number of arguments to 0.
		 */
		arguments->dim = 0;
		goto Lmatch;
	    }
	    continue;
	}
	arg->type = param->type;
    }
  Lmatch:
    return 0;

  Lnomatch:
    return 1;
}

/*******************************************
 * Infer foreach arg types from a template function opApply which looks like:
 *    int opApply(alias int func(ref uint))() { ... }
 */

#if 0
void inferApplyArgTypesZ(TemplateDeclaration *tstart, Arguments *arguments)
{
    for (TemplateDeclaration *td = tstart; td; td = td->overnext)
    {
        if (!td->scope)
        {
            error("forward reference to template %s", td->toChars());
            return;
        }
        if (!td->onemember || !td->onemember->toAlias()->isFuncDeclaration())
        {
            error("is not a function template");
            return;
        }
	if (!td->parameters || td->parameters->dim != 1)
	    continue;
	TemplateParameter *tp = (TemplateParameter *)td->parameters->data[0];
	TemplateAliasParameter *tap = tp->isTemplateAliasParameter();
	if (!tap || !tap->specType || tap->specType->ty != Tfunction)
	    continue;
	TypeFunction *tf = (TypeFunction *)tap->specType;
	if (inferApplyArgTypesY(tf, arguments) == 0)	// found it
	    return;
    }
}
#endif

/**************************************
 */

static void templateResolve(Match *m, TemplateDeclaration *td, Scope *sc, Loc loc, Objects *targsi, Expression *ethis, Expressions *arguments)
{
    FuncDeclaration *fd;

    assert(td);
    fd = td->deduceFunctionTemplate(sc, loc, targsi, ethis, arguments);
    if (!fd)
	return;
    m->anyf = fd;
    if (m->last >= MATCHexact)
    {
	m->nextf = fd;
	m->count++;
    }
    else
    {
	m->last = MATCHexact;
	m->lastf = fd;
	m->count = 1;
    }
}

