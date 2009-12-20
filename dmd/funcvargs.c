#include <stdlib.h>
#include "mars.h"
#include "aggregate.h"
#include "declaration.h"
#include "id.h"
#include "scope.h"


void FuncDeclaration::varArgs(Scope* sc2,
                              TypeFunction* f,
                              VarDeclaration*& argptr,
                              VarDeclaration*& _arguments
                              )
{
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
}
