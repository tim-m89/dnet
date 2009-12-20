// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: type.cpp 367 2009-05-07 05:20:50Z cristiv $
//
// Copyright (c) 2008, 2009 Cristian L. Vlasceanu
// Copyright (c) 2008 Ionut-Gabriel Burete

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
#include "aggregate.h"              // for ClassDeclaration
#include "declaration.h"
#include "id.h"
#include "init.h"
#include "module.h"
#include "mtype.h"
#include "backend.net/backend.h"    // for BackEnd::fatalError
#include "backend.net/irstate.h"
#include "backend.net/msil/assembly.h"
#include "backend.net/msil/classemit.h"
#include "backend.net/msil/typedef.h"
#include "backend.net/msil/member.h"
#include "backend.net/msil/types.h"
#include "backend.net/msil/variable.h"
#include "frontend.net/pragma.h"

using namespace std;


const char* className(StructType& sType)
{
    return sType.name();
}


bool isImplicit(Initializer& init)
{
    if (ExpInitializer* expInit = init.isExpInitializer())
    {
        if (expInit->exp)
        {
            return expInit->exp->op == TOKblit;
        }
    }
    return false;
}


static inline bool hasImplicitInitializer(VarDeclaration& decl)
{
    return decl.init ? isImplicit(*decl.init) : false;
}


TYPE& TYPE::systemString(Loc& loc)
{
    if (!SystemString)
    {
        SystemString = new StringType(loc);
    }
    return *SystemString;
}

 
elem* TYPE::initVar(IRState& state, VarDeclaration& decl, Expression* exp, elem* e)
{
    if (state.inCall())
    {
        // no need to initialize the left hand-side in expression such as
        // XType x = fun(args);
        return e;
    }

    block* initBlk = NULL; // initializers block
    if (!decl.isStatic())
    {
        if (Aggregate* aggr = isAggregateField(decl))
        {
            initBlk = &aggr->getInitBlock();

            if (e)
            {
                //move initializer to initializers block
                elem* ini = state.getBlock().back();
                state.getBlock().pop_back(false);
                initBlk->add(*ini);
            }
        }
    }
    if (initBlk)
    {
        state.setBlock(*initBlk);
    }
    e = init(state, decl, exp, e);
    if (Symbol* sym = decl.csym)
    {
        if (Variable* v = sym->isVariable())
        {
            if (v->isStatic() || (v->getDecl().storage_class & STCmanifest))
            {
                v->store(state.getBlock());
            }
            else if (initBlk)
            {
                assert(v->isField());
                initBlk->add(*new ThisArg);
                v->store(*initBlk);
            }
        }
    }
    return e;
}



static TYPE* toArrayType(Loc& loc, TypeArray& tar, Expression* dim = 0)
{
    if (!tar.ctype && tar.next)
    {
        tar.ctype = new ArrayType(loc, *tar.next, dim);
    }
    return tar.ctype;
}


static TYPE* toAssociativeArrayType(Loc& loc, TypeAArray& tar)
{
    if (!tar.ctype && tar.next)
    {
        tar.ctype = new AssocArrayType(loc, *tar.next, *tar.index);
    }
    return tar.ctype;
}


static TYPE* toArrayType(Loc& loc, TypeSArray& tar)
{
    return toArrayType(loc, tar, tar.dim);
}


static TYPE* toPointerType(Loc& loc, TypePointer& tp)
{
    assert(tp.ctype == NULL);
    //turn pointers to functions into .NET delegates
    if (DEREF(tp.next).ty == Tfunction)
    {
        return new DelegateType(tp.next);
    }
    TYPE& pointedType = toILType(loc, tp.next);
    return new PointerType(pointedType);
}


static TYPE* toReferenceType(Loc& loc, TypeReference& tp)
{
    assert(tp.ctype == NULL);
    TYPE& pointedType = toILType(loc, tp.next);
    return new ReferenceType(pointedType);
}


TYPE* toILType(Loc& loc, TypeIdentifier* ti)
{
    TYPE* equiv = NULL;
    Identifier*& ident = ti->ident;

    if (ident == Id::Object)
    {
        ident = Id::object;
        equiv = &toILType(loc, ClassDeclaration::object->type);
    }
    else if (ident == Id::Throwable)
    {
        ident = Lexer::idPool("class [dnetlib]core.Throwable");
    }
    else if (ident == Lexer::idPool("string"))
    {
        // D string is just an array of chars;
        // D char is unsigned
        equiv = &toILType(loc, Type::tchar->arrayOf());
        ident = Lexer::idPool("unsigned int8 []");
    }
    else if (ident == Lexer::idPool("wstring"))
    {
        equiv = &toILType(loc, Type::twchar->arrayOf());
        ident = Lexer::idPool("char []");
    }
    else
    {
        if (ti->idents.dim)
        {
            ident = static_cast<Identifier*>(ti->idents.data[ti->idents.dim - 1]);
        }
        if (ident == Lexer::idPool("String") /* || strcmp(ti->toChars(), "System.String") == 0 */)
        {
            return &TYPE::systemString(loc);
        }
        //try resolving the identifier to a type
        if (Dsymbol* s = Module::rootModule->search(loc, ident, 0))
        {
            if (AggregateDeclaration* ad = s->isAggregateDeclaration())
            {
                if (ad->type)
                {
                    return &toILType(loc, ad->type);
                }
            }
        }
    }
    return new IdentType(*ti, equiv);
}


///<summary>
/// Map D types onto MSIL types
///</summary>
TYPE& toILType(Loc& loc, Type* type)
{
    if (!type)
    {
        BackEnd::fatalError(loc, "null type");
        assert(false);
    }
    TYPE*& ilType = type->ctype;
    if (!ilType)
    {
        switch (type->ty)
        {
        case Tarray:            // dynamic array
            ilType = toArrayType(loc, static_cast<TypeDArray&>(*type));
            break;
        case Tsarray:           // static array
            ilType = toArrayType(loc, static_cast<TypeSArray&>(*type));
            break;
        case Taarray:           // associative array
            ilType = toAssociativeArrayType(loc, static_cast<TypeAArray&>(*type));
            break;
        case Tclass:
            if (DEREF(static_cast<TypeClass*>(type)->sym).ident == Lexer::idPool("String"))
            {
                ilType = &TYPE::systemString(loc);
            }
            //otherwise fallthru to toCType
            break;

        case Tenum:
            ilType = new EnumType(static_cast<TypeEnum*>(type));
            break;
        case Tpointer:
            ilType = toPointerType(loc, static_cast<TypePointer&>(*type));
            break;
        case Treference:
            ilType = toReferenceType(loc, static_cast<TypeReference&>(*type));
            break;
        case Tfunction:
            ilType = new IdentType(*type);
            break;
        case Ttypedef: // fall through to toCtype()
            break;

        case Tident:
            ilType = toILType(loc, static_cast<TypeIdentifier*>(type));
            break;

        case Tvoid: ilType = TYPE::Void; break;
        case Tuns8: ilType = TYPE::Uint8; break;
        case Tint8: ilType = TYPE::Int8; break;
        case Tuns16: ilType = TYPE::Uint16; break;
        case Tint16: ilType = TYPE::Int16; break;
        case Tuns32: ilType = TYPE::Uint32; break;
        case Tint32: ilType = TYPE::Int32; break;
        case Tuns64: ilType = TYPE::Uint64; break;
        case Tint64: ilType = TYPE::Uint64; break;

        case Tfloat32: ilType = TYPE::FloatingPoint32; break;
        case Tfloat64: ilType = TYPE::FloatingPoint64; break;
        case Tfloat80: /* todo ?? */ break;

        case Tbool: ilType = TYPE::Boolean; break;
        case Tchar: ilType = TYPE::Char; break;
        case Twchar:ilType = TYPE::Wchar; break;
        case Tdchar:ilType = TYPE::Dchar; break;

        //any better ideas?
        case Ttuple: ilType = new IdentType(*type);
            break;

#pragma region TODO
        case Timaginary32:
        case Timaginary64:
        case Timaginary80:

        case Tcomplex32:
        case Tcomplex64:
        case Tcomplex80:
            break;
        case Tbit:
            break;

        case Tnone:
            break;

        case Terror:
        case Tinstance:
        case Ttypeof:
        case Tslice:
        case Treturn:
            break;

#pragma endregion
        }
        if (!ilType)
        {
            type->toCtype();
        }
        if (!ilType)
        {
            BackEnd::fatalError(loc, type->toChars());
        }
    }
    return *ilType;
}


TYPE& toILType(Declaration& decl)
{
    return toILType(decl.loc, decl.type);
}


TYPE& toILType(Variable& v)
{
    return toILType(v.getDecl().loc, v.getType());
}


TYPE& toILType(Loc& loc, Variable* v)
{
    return toILType(loc, DEREF(v).getType());
}


type* TypeClass::toCtype()
{
    if (!ctype)
    {
        ctype = new ObjectType(this);
    }
    return ctype;
}


type* TypeDelegate::toCtype()
{
    if (!ctype)
    {
        ctype = new DelegateType(next);
    }
    return ctype;
}


type* TypeTypedef::toCtype()
{
    if (!ctype)
    {
        assert(sym);
        ctype = &toILType(sym->loc, sym->basetype);

        if (DelegateType* dt = ctype->isDelegateType())
        {
            dt->setSuffix(sym->toPrettyChars());
            if (inPragmaAssembly(sym->toParent()))
            {
                dt->setExternal(); // defined in another assembly, do not emit it

                //temporary turn the modifier bits off, I am not
                //interested in "shared", "const", whatever here
                Temporary<unsigned char> setMod(this->mod, 0);
                const char* alias = this->toChars();
                DefineDirective* def = new DefineDirective(dt->name(), alias);
                if (!BackEnd::getModuleScope(sym->loc).add(*def))
                {
                    delete def;
                }
            }
        }
    }
    return ctype;
}


type* TypeStruct::toCtype()
{
    if (!ctype)
    {
        ctype = new StructType(this);
    }
    return ctype;
}


type* TypePointer::toCtype()        { NOT_IMPLEMENTED(0); }
type* TypeAArray::toCtype()         { NOT_IMPLEMENTED(0); }
type* TypeDArray::toCtype()         { NOT_IMPLEMENTED(0); }
type* TypeSArray::toCtype()         { NOT_IMPLEMENTED(0); }
type* TypeEnum::toCtype()           { NOT_IMPLEMENTED(0); }
type* TypeFunction::toCtype()       { NOT_IMPLEMENTED(0); }
type* Type::toCtype()               { return ctype; }

type* TypeSArray::toCParamtype()    { NOT_IMPLEMENTED(0); }
type* TypeTypedef::toCParamtype()   { NOT_IMPLEMENTED(0); }

type* Type::toCParamtype()          { NOT_IMPLEMENTED(0); }
unsigned Type::totym()              { NOT_IMPLEMENTED(0); } 
RET TypeFunction::retStyle()        { return RET(0); } 
unsigned TypeFunction::totym()      { NOT_IMPLEMENTED(0); }



namespace {
    ///<summary>
    ///
    ///</summary>
    class LoadType : public Variable
    {
        elem* load(block&, Loc*, bool)
        {
            return this;
        }
        elem* store(block&, Loc*)       // not used
        {
            assert(false); return NULL; // should not be ever called
        }
        bool isTypeInfo() const { return true; }

    public:
        explicit LoadType(TypeInfoDeclaration& decl) : Variable(decl, decl.tinfo)
        {
        }

        elem* emitILAsm(wostream& out)
        {
            if (getType()->ty == Ttuple)
            {
                indent(out) << "ldtoken object[]\n";
            }
            else
            {
                TYPE& type = toILType(getDecl().loc, getType());
                indent(out) << "ldtoken " << type.name() << '\n';
            }
            indent(out) << "call class [mscorlib]System.Type "
                "[mscorlib]System.Type::GetTypeFromHandle(valuetype [mscorlib]System.RuntimeTypeHandle)\n";
            return this;
        }
    };
}


Variable* loadTypeInfo(TypeInfoDeclaration& decl)
{
    return new LoadType(decl);
}
