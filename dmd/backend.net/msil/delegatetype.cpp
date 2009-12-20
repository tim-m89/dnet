// $Id: delegatetype.cpp 24625 2009-07-31 01:05:55Z unknown $
// Copyright (c) 2009 Cristian L. Vlasceanu

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
#include "declaration.h"
#include "scope.h"
#include "statement.h"
#include "backend.net/msil/assembly.h"
#include "backend.net/msil/member.h"
#include "backend.net/msil/types.h"

using namespace std;


DelegateType::DelegateType(Type* type) 
    : ObjectType(type)
    , ident_(Identifier::generateId("$Delegate_"))
    , method_(NULL)
    , external_(false)
    , cd_(NULL)
{
    BackEnd::instance().getAssembly().addDelegateType(this);
}


const char* DelegateType::getSuffix() const
{
    if (suffix_.empty())
    {
        suffix_ = ident_->toChars();
    }
    return suffix_.c_str();
}


unsigned DelegateType::depth() const
{
    return 0;
}


void DelegateType::emit(wostream& out)
{
    if (external_)
    {
        return;
    }
    unsigned depth = this->depth();

    indent(out, depth) << ".class sealed " << getSuffix()
                       << " extends [mscorlib]System.MulticastDelegate\n";
    indent(out, depth) << "{\n";
    ++depth;
    indent(out, depth) << ".method public instance void .ctor(object, native int) runtime managed {}\n";

    if (TypeFunction* tfun = static_cast<TypeFunction*>(getType()))
    {
        indent(out, depth) << ".method public virtual ";
        getMethod()->emitRetType(out);
        out << " Invoke(";
        emitFormalArgs(NULL, *tfun, out);
        out << ") runtime managed {}\n";
    }

    --depth;
    indent(out, depth) << "}\n";
}


ClassDeclaration* DelegateType::getClassDecl()
{
    if (!cd_)
    {
        cd_ = new ClassDeclaration(0, ident_, NULL);
    }
    return cd_;
}


Method* DelegateType::getMethod()
{
    if (!method_)
    {
        Identifier* invoke = Lexer::idPool("Invoke");
        Assembly& assembly = BackEnd::instance().getAssembly();
        FuncDeclaration* fd = new FuncDeclaration(0, 0, invoke, STCextern, getType());

        Expression* exp = new IntegerExp(0, 0, Type::tint32);
        Statement* ret = new ReturnStatement(0, exp);
        fd->parent = getClassDecl();
        fd->fbody = new ScopeStatement(0, ret);
        fd->hasReturnExp = true;
        fd->semantic3(&scope_);
        fd->linkage = LINKwindows; // to prevent mangling
        method_ = new Method(*fd, 1);
        assembly.add(*method_);
    }
    return method_;
}


void DelegateType::setSuffix(const char* suffix)
{
    assert(suffix);
    ident_ = Lexer::idPool(suffix);
    suffix_ = suffix;
    method_ = NULL;
}


void Assembly::emitDelegateTypes(wostream& out)
{
    bool haveDelegates = false;
    for (set<DelegateType*>::const_iterator i = delegates_.begin(); i != delegates_.end(); ++i)
    {
        if (!(*i)->isExternal())
        {
            haveDelegates = true;
            break;
        }
    }
    if (haveDelegates)
    {
        out << "//--------------------------------------------------------------\n";
        out << "// Delegate types\n";
        out << "//--------------------------------------------------------------\n";
    }
    for (set<DelegateType*>::iterator i = delegates_.begin(); i != delegates_.end(); ++i)
    {
        (*i)->emit(out);
    }
}
