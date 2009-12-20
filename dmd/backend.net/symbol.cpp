// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: symbol.cpp 370 2009-05-09 19:45:40Z cristiv $
//
// Copyright (c) 2008, 2009 Cristian L. Vlasceanu

/*****
This license governs use of the accompanying software. If you use the software, you
accept this license. If you do not accept the license, do not use the software.

1. Definitions
The terms "reproduce," "reproduction," "derivative works," and "distribution" have the
same meaning here as under U.S. copyright law.
A "contribution" is the original software, or any additions or changes to the software.
A "contributor" is any person that distributes its contribution under this license.
"Licensed patents" are a contributor's patent claims that read directly on its contribution.

2. Grant of Rights
(A) Copyright Grant- Subject to the terms of this license, including the license conditions and limitations in section 3,
each contributor grants you a non-exclusive, worldwide, royalty-free copyright license to reproduce its contribution, 
prepare derivative works of its contribution, and distribute its contribution or any derivative works that you create.
(B) Patent Grant- Subject to the terms of this license, including the license conditions and limitations in section 3, 
each contributor grants you a non-exclusive, worldwide, royalty-free license under its licensed patents to make, have made,
use, sell, offer for sale, import, and/or otherwise dispose of its contribution in the software or derivative works of the 
contribution in the software.

3. Conditions and Limitations
(A) No Trademark License- This license does not grant you rights to use any contributors' name, logo, or trademarks.
(B) If you bring a patent claim against any contributor over patents that you claim are infringed by the software,
your patent license from such contributor to the software ends automatically.
(C) If you distribute any portion of the software, you must retain all copyright, patent, trademark,
and attribution notices that are present in the software.
(D) If you distribute any portion of the software in source code form, you may do so only under this license by 
including a complete copy of this license with your distribution. If you distribute any portion of the software 
in compiled or object code form, you may only do so under a license that complies with this license.
(E) The software is licensed "as-is." You bear the risk of using it. The contributors give no express warranties,
guarantees or conditions. You may have additional consumer rights under your local laws which this license cannot change.
To the extent permitted under your local laws, the contributors exclude the implied warranties of merchantability,
fitness for a particular purpose and non-infringement.
*****/
#include "aggregate.h"
#include "declaration.h"
#include "id.h"
#include "module.h"
#include "statement.h"
#include "backend.net/backend.h"
#include "backend.net/irstate.h"
#include "backend.net/msil/member.h"

using namespace std;


static inline void checkCSym(Dsymbol& dsym)
{
    if (!dsym.csym)
    {
        string err = "symbol is null: '";
        err += dsym.toChars();
        err += "'";
        BackEnd::fatalError(dsym.loc, err.c_str());
    }
}


Symbol* AggregateDeclaration::toInitializer()
{
    return csym;
}


Symbol* ClassDeclaration::toSymbol() 
{
    if ((csym == NULL) /* && (ident == Id::TypeInfo || ident == Id::Object) */)
    {
        //Generate the TypeInfo class into a hidden module;
        //the class is implemented in the runtime, so we do not
        //need to declare it explicitly in the generated code
        Module& mod = BackEnd::getModuleScope(loc).getModule();
        auto_ptr<ModuleEmitter> temp(BackEnd::instance().getModule(mod));

        toObjFile(0);
    }
    checkCSym(*this);
    return csym;
}


Symbol* ClassInfoDeclaration::toSymbol() { NOT_IMPLEMENTED(0); }
Symbol* Dsymbol::toSymbol()              { return csym; }
Symbol* FuncAliasDeclaration::toSymbol() { NOT_IMPLEMENTED(0); }
Symbol* InterfaceDeclaration::toSymbol() { NOT_IMPLEMENTED(0); }
Symbol* ModuleInfoDeclaration::toSymbol(){ NOT_IMPLEMENTED(0); }
Symbol* Module::toSymbol()               { NOT_IMPLEMENTED(0); }


Symbol* SymbolDeclaration::toSymbol()
{
    if (!sym && dsym)
    {
        return dsym->toSymbol();
    }
    assert(sym);
    return sym;
}


Symbol* TypeClass::toSymbol()
{
    if (sym)
    {
        return sym->toSymbol();
    }
    return NULL;
}


Symbol* TypeInfoDeclaration::toSymbol()
{
    toObjFile(0);
    checkCSym(*this);
    return csym;
}


Symbol* Type::toSymbol()
{
    NOT_IMPLEMENTED(0);
}


Symbol* FuncDeclaration::toSymbol()
{
    if (!csym)
    {
        BackEnd::instance().resolve(*this);
    }
    checkCSym(*this);
    return csym;
}


Symbol* VarDeclaration::toSymbol()
{
    assert(!aliassym);
    if (!csym)
    {
        if (ident == Id::result)
        {
            Method& method = BackEnd::getCurrentMethod(loc);
            csym = method.getResult();
        }
        else if (parent)
        {
            if (isStatic())
            {
                ModuleEmitter& mod = BackEnd::getModuleScope(loc);
                mod.createGlobalVar(*this);
            }
            //lookup symbol in the parent function's local symtab
            else if (FuncDeclaration* fdecl = parent->isFuncDeclaration())
            {
                if (fdecl->localsymtab)
                {
                    if (Dsymbol* sym = fdecl->localsymtab->lookup(ident))
                    {
                        csym = sym->csym;
                    }
                }
            }
            else if (Module* m = parent->isModule())
            {
        #if _DEBUG
                //assert that symbol is imported from another module
                ModuleEmitter& mod = BackEnd::getModuleScope(loc);
                assert(&mod.getModule() != m);
        #endif

        #if 0
                mod.createGlobalVar(*this);
        #else                
                ModuleEmitter* modEmit = BackEnd::instance().getModule(*m, false);
                modEmit->createGlobalVar(*this);
                modEmit->pop(); //return to the current module context
        #endif
            }
        }
    }
    checkCSym(*this);

    //call the virtual isSymbol() here to give symbol
    //a chance to clone itself, if needed
    return DEREF(csym).isSymbol();
}

