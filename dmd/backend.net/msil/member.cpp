// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: member.cpp 24625 2009-07-31 01:05:55Z unknown $
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
#include <sstream>
#include <stdexcept>
#include "aggregate.h"
#include "declaration.h"
#include "id.h"
#include "init.h"
#include "module.h"
#include "scope.h"
#include "statement.h"
#include "backend.net/irstate.h"
#include "backend.net/type.h"
#include "backend.net/utils.h"
#include "backend.net/msil/assembly.h"
#include "backend.net/msil/classemit.h"
#include "backend.net/msil/deref.h"
#include "backend.net/msil/except.h"
#include "backend.net/msil/instruction.h"
#include "backend.net/msil/label.h"
#include "backend.net/msil/member.h"
#include "backend.net/msil/types.h" // for ArrayType

using namespace std;
static const char lazyArgType[] = "class [mscorlib]System.Delegate";


///<summary>
/// Base class for locals and arguments.
/// Can be referenced positionaly by load and store instrs.
///</summary>
class Positional : public Variable
{
    unsigned pos_;

public:
    Positional(VarDeclaration& decl, unsigned pos, Type* type)
        : Variable(decl, type), pos_(pos)
    { }
    virtual ~Positional()
    {
        pos_ = -1;
    }
    virtual const unsigned* position() const
    {
        assert(pos_ >= 0);
        return &pos_;
    }
    virtual void setPosition(unsigned pos)
    {
        assert(pos_ >= 0);
        pos_ = pos;
    }
    Positional* isPositional()
    {
        assert(pos_ >= 0);
        return this;
    }
};


/*************************************/
namespace {
    ///<summary>
    /// Local variable
    ///</summary>
    class Local : public Positional
    {
        elem* emitILAsm(wostream&);

    public:
        Local(VarDeclaration& decl, unsigned pos, Type* = NULL);
        virtual elem* load(block&, Loc* = NULL, bool = false);
        virtual elem* store(block&, Loc* = NULL);
        virtual long refCount() const
        {
            if (hooks_ && !hooks_->isReferenced(*this))
            {
                return 0;
            }
            return Positional::refCount();
        }
    };

    ///<summary>
    /// Method argument
    ///</summary>
    class Arg : public Local
    {
        elem* emitILAsm(wostream&) sealed;

    public:
        Arg(VarDeclaration& decl, unsigned pos, Type* = NULL);
        virtual elem* load(block&, Loc* = NULL, bool = false);
        virtual elem* store(block&, Loc* = NULL);
    };

    ///<summary>
    /// Local variable in inner scope (for example: a foreach argument)
    ///</summary>
    class NestedLocal : public Local
    {
        Positional& slot_;
        string name_;

    public:
        NestedLocal(Positional& slot, VarDeclaration& decl)
            : Local(decl, 0, slot.getType())
            , slot_(slot)
        {
            name_ = "$";
            name_ += slot.getName();
            slot.incRefCount();
        }
        virtual bool isNested() const { return true; }

        virtual long incRefCount()
        {
            slot_.incRefCount();
            return Local::incRefCount();
        }
        virtual long decRefCount()
        {
            slot_.decRefCount();
            return Local::decRefCount();
        }
        virtual const unsigned* position() const
        {
            return slot_.position();
        }
        virtual void setPosition(unsigned pos) 
        {
        }
        virtual void releaseSlot()
        {
            if (slot_.refCount())
            {
                slot_.decRefCount();
            }
        }
        virtual const char* getName()
        {
            return name_.c_str();
        }
    };
} // namespace


///<summary>
/// Objects of this class are used to reserve a slot in 
/// a methods's locals_ block. The slot is then going to
/// be used by nested locals in non-overlapping blocks
///</summary>
class Slot : public Local
{
    elem* emitILAsm(wostream& out) { return NULL; }

public:
    Slot(VarDeclaration& decl, unsigned pos, Type* type)
        : Local(decl, pos, type)
    { }
    Slot* isSlot() { return this; }
};
/*************************************/


wostream& operator<<(wostream& out, PROT prot)
{
    switch (prot)
    {
    case PROTundefined: break;
    case PROTnone: out << "privatescope "; break;

    //case PROTprivate: out << "private "; break;
    // Walter says: "all methods in a module are all automatically 
    //"friends" with classes within that same module, and so 
    // have access to any private members. Private only kicks
    // in for access from another module." 
    // So I guess it should map to "assembly" -- which is more relaxed,
    // but the frontend will make sure it works properly inter-modules.
    case PROTprivate: // fallthru

    case PROTpackage: out << "assembly "; break;
    case PROTprotected: out << "family "; break;
    case PROTexport: // fallthru
    case PROTpublic: out << "public "; break;
    }
    return out;
}


Member::Member(Declaration& decl) : decl_(decl)
{
}


//return the class that owns this member
Class* Member::getClass()
{
    if (AggregateDeclaration* decl = getAggregateDecl())
    {
        if (decl->csym)
        {
            return decl->csym->isClass();
        }
    }
    return NULL;
}


Aggregate* Member::getAggregate()
{
    if (AggregateDeclaration* decl = getAggregateDecl())
    {
        if (decl->csym)
        {
            return decl->csym->isAggregate();
        }
    }
    return NULL;
}


Field::Field(VarDeclaration& decl, bool isStatic)
    : BaseField<Field>(decl)
    , parent_(NULL)
    , dt_(NULL)
    , static_(isStatic || decl.isStatic())
    , silent_(false)
    , bound_(false)
    , typeinfo_(false)
{
    if (decl.storage_class & STClazy)
    {
        static Type* type = new TypeIdentifier(0, Lexer::idPool(lazyArgType));
        decl_.type = type;
    }

    if (hasTLS(decl))
    {
#if TLS_DATA_SLOT
        if (isValueType(toILType(decl)))
        {
            error(decl.loc, "%s does not support TLS for value types", BackEnd::name());
        }
#endif
        if (!decl.isStatic() && !isStatic)
        {
            static_ = true;
            decl.storage_class |= STCstatic;
        }
#if TLS_DATA_SLOT
        dt_ = BackEnd::getModuleScope(decl.loc).createTls(*this);
#endif
        // set the TLS flag in the parent, so that we know to emit
        // a "beforefieldinit" flag for the class or struct
        if (AggregateDeclaration* ad = decl.isMember())
        {
            ad->storage_class |= STCtls;
        }
    }
}


const TYPE& Field::getTYPE() const
{
    Loc& loc = decl_.loc;
    TYPE& type = toILType(loc, decl_.type);

    if (static_)
    {
        if (PointerType* pt = type.isPointerType())
        {
            BackEnd::getModuleScope(loc).setUnverifiable(loc);
            return DEREF(pt->getUnmanagedType());
        }
    }
    return type;
}


elem* Field::emitILAsm(wostream& out)
{
    if (!silent_)
    {
        PROT prot = decl_.prot();

    /* The idea here is that if a datum is declared static then it
       should not be visible in other modules within the same
       assembly -- at least *I think* this is the rule;
       but then again, the front-end may be enforcing it anyway.
       TODO: revisit!

        if (isStatic())
        {
            prot = PROTprivate;
        }
      */
        indent(out) << ".field " << (isStatic() ? "static " : "") << prot;
        //
        // TODO: emit other field attributes that may be needed here
        //
        out << getTYPE().name() << " '" << decl_.toChars() << '\'';
        if (!getParent() && dt_)
        {
            assert(dt_->isDt());
            if (const char* label = dt_->getDataLabel())
            {
                out << " at " << label;
            }
        }
        out << '\n';

#if! TLS_DATA_SLOT
        //use ThreadStaticAttribute instead of .tls mapping
        if (decl_.storage_class & STCtls)
        {
            indent(out) << ".custom instance void "
                "[mscorlib]System.ThreadStaticAttribute::.ctor() = ( 01 00 00 00 )\n";
        }
#endif
    }
    return this;
}


namespace {
    ///<summary>
    /// Generate code that invokes the static destructor as a ProcessExit event handler.
    /// http://the-free-meme.blogspot.com/2009/04/static-ctors-in-dnet-part-2.html
    ///</summary>
    class UnloadEvent : public elem
    {
        StaticDtorDeclaration& dtor_;

    public:
        explicit UnloadEvent(StaticDtorDeclaration& dtor) : dtor_(dtor) { }
        
        int32_t getMaxStackDelta() const
        {
            return 3;
        }

        elem* emitILAsm(wostream& out)
        {
            indent(out) << "// register static dtor as ProcessExit event handler\n";
            //generate call to retrieve current domain
            indent(out) << "call class [mscorlib]System.AppDomain [mscorlib]System.AppDomain::get_CurrentDomain()\n";
            //generate code that wraps static dtor into an EventHandler
            indent(out) << "ldnull\n";
            if (ClassDeclaration* cd = dtor_.isClassMember())
            {
                indent(out) << "ldftn void " << className(*cd) << "::'" << dtor_.toChars();
            }
            else
            {
                indent(out) << "ldftn void '" << dtor_.mangle();
            }
            out << "'(object, class [mscorlib]System.EventArgs)\n";
            indent(out) << "newobj instance void [mscorlib]System.EventHandler::.ctor(object, native int)\n";
            indent(out) << "callvirt instance void [mscorlib]System.AppDomain::add_ProcessExit(class [mscorlib]System.EventHandler)\n";
            return this;
        }
    };
}


Field* Closure::isMapped(Variable& v)
{
    VarMap::iterator i = varMap_.find(&v);
    if (i != varMap_.end())
    {
        return i->second;
    }
    return NULL;
}


void Closure::copyIn(block& blk, Variable* contextObj)
{
    contextObj_ = contextObj;
    for (VarMap::iterator i = varMap_.begin(), end = varMap_.end(); i != end; ++i)
    {
        Variable* v = i->first;
        v->load(blk, NULL, v->hasClosureReference());
        Field* f = i->second;
        Variable* parent = f->getParent();
        f->bind(contextObj, false);
        //temporarily turn off nested references
        Temporary<unsigned> temp(f->getDecl().nestedrefs.dim, 0);
        f->store(blk);
        f->bind(parent);
    }
}


Method::Method(FuncDeclaration& decl, unsigned depth, bool nested, Dsymbol* parent)
    : Member(decl)
    , retType_(NULL)
    , body_(NULL, decl.loc, depth)
    , args_(NULL, decl.loc, depth)
    , locals_(NULL, decl.loc, depth + 1)
    , ret_(NULL)
    , obj_(NULL)
    , argCount_(0)
    , vaOffs_(-1)
    , varScope_(NULL)
    , closure_(NULL)
    , setProto_(false)
    , needsConversion_(false)
    , template_(false)
    , nested_(nested)
    , nestedMember_(false)
    , delegatedTo_(false)
    , optOut_(false)
    , aaProp_(false)
    , synthProp_(prop_None)
    , parent_(parent)
{
    checkVisibility(decl);

    TypeFunction* tfun = static_cast<TypeFunction*>(decl.type);
    retType_ = DEREF(tfun).next;

#if !defined( VALUETYPE_STRUCT )
    AggregateDeclaration* adecl = decl.isMember();
    if (adecl && adecl->isStructDeclaration() && retType_->ty == Tpointer)
    {
        retType_ = adecl->type;
    }
#endif
    // Create a local variable to temporarily store the 
    // function's return value -- used when exception handling 
    // is needed; it is automatically optimized out otherwise 
    // (because it will have a zero ref count).

    // Another use case for ret_ is when the front-end 
    // references the  "__result" symbol.

    if (DEREF(retType_).ty != Tvoid)
    {
        VarDeclaration* rdecl = new VarDeclaration(decl.loc, retType_, Id::result, NULL);
        if (returnsRef(decl))
        {
            rdecl->storage_class |= STCref;
        }
        ret_ = new Local(*rdecl, 0, retType_);
        locals_.add(*ret_);
    }

    if (StaticDtorDeclaration* dtor = decl.isStaticDtorDeclaration())
    {
        Assembly& assembly = BackEnd::instance().getAssembly();
        assembly.addStaticDtor(*new UnloadEvent(*dtor));
    }
}


Method::~Method()
{
    delete closure_;
}


void Method::checkVisibility(FuncDeclaration& decl)
{
    if (decl.isOverride())
    {
        ClassDeclaration* cd = decl.isClassMember();
        if (!cd)
        {
            // can't be an override unless it's a class member
            BackEnd::fatalError(decl.loc, "null parent class");
        }
        ArrayAdapter<Dsymbol*> funcs(cd->vtbl);
        for (ArrayAdapter<Dsymbol*>::iterator i = funcs.begin(), end = funcs.end(); i != end; ++i)
        {
            if (FuncDeclaration* fd = (*i)->isFuncDeclaration())
            {
                if (decl.overrides(fd) && decl.prot() < fd->prot())
                {
                    error(decl.loc, "visibility cannot be lowered");
                }
            }
        }
    }
}


static void emitClassDecl(wostream& out, Dsymbol& sym)
{
    if (AggregateDeclaration* parent = sym.isMember())
    {
        out << className(*parent) << "::";
    }
}


static void emitMethodName(wostream& out, Declaration& decl)
{
    if (decl.isCtorDeclaration())
    {
        out << ".ctor";
    }
    else if (decl.isDtorDeclaration())
    {
        out << "__dtor";
    }
    // do not mangle class member for readability
    else if (!decl.isMember() && !decl.isFuncLiteralDeclaration())
    {
        if (decl.parent && decl.parent->ident == Lexer::idPool("[dnetlib]core"))
        {
            out << "[dnetlib]runtime.dnet::'" << decl.toChars() << '\'';
        }
        else
        {
            // standalone functions may be nested, so we 
            // mangle them in order to avoid name conflicts
            out << '\'' << decl.mangle() << '\'';
        }
    }
    else
    {
        // quote identifier to avoid conflict with IL keywords
        out << '\'' << decl.toChars() << '\'';
    }
}


//Programmatically set the prototype of a method.
//This is needed for overriding front-end-synthesized stuff.
void Method::setProto(const wstring& proto, int32_t argCount)
{
    proto_ = proto;
    argCount_ = argCount;
    setProto_ = true;
}


static ClassDeclaration* isInterfaceImpl(FuncDeclaration& decl)
{
    assert(decl.type);
    assert(decl.type->ty == Tfunction);

    if (ClassDeclaration* cd = decl.isClassMember())
    {
        if (cd->interfaces_dim && cd->interfaces)
        {
            BaseClass* iface = *cd->interfaces;
            for (int i = 0; i != cd->interfaces_dim; ++i, iface = cd->interfaces[i])
            {
                assert(iface);
                assert(iface->base);
                assert(decl.type->ty == Tfunction);

                if (iface->base->findFunc(decl.ident, static_cast<TypeFunction*>(decl.type)))
                {
                    return iface->base;
                }
            }
        }
    }
    return NULL;
}


static bool convertArrays(FuncDeclaration& decl)
{
    return !decl.fes && decl.fbody && !decl.isMain() && !isInterfaceImpl(decl);
}


bool Method::convertArrays() const
{
    FuncDeclaration& decl = funcDecl();
    return !isOptimizedOut() && ::convertArrays(decl);
}


//Get the name, transforming arrays into slices;
//used in generating function prototypes.
//
//In D arrays and slices can be used interchangeably,
//a variable of array type can be assigned
//a slice at runtime. To make this work with IL, all
//params and return values of array type are converted
//under the hood to slices.
static const char* getParamTypeName(FuncDeclaration& decl,
                                    unsigned& storage,
                                    Type* type,
                                    Type** pType = NULL,
                                    bool isRet = false
                                    )
{
    if (storage & STClazy)
    {
        return lazyArgType;
    }
    if (!isRet && (DEREF(type).ty == Tpointer || DEREF(type).ty == Tclass))
    {
        storage &= ~STCref;
    }
    const bool isRef = (storage & STCref);
    if (isRef && DEREF(type).ty != Treference)
    {
        type = DEREF(type).referenceTo();
        if (pType)
        {
            *pType = type;
        }
    }
    
    TYPE* t = &toILType(decl.loc, type);
    if (convertArrays(decl))
    {
        if (ArrayType* aType = t->isArrayType())
        {
            if (SliceType* sliceType = aType->getSliceType(decl.loc))
            {
                t = sliceType;
                if (pType)
                {
                    *pType = sliceType->getType();
                }

                if (isRef)
                {
                    string sliceRef = t->name() + string("&");
                    return Lexer::idPool(sliceRef.c_str())->toChars();
                }
            }
        }
    }
    return t->name();
}


bool Method::returnsRef() const
{
    if (retType_ && retType_->ty == Treference)
    {
        return true;
    }
    return returnsRef(funcDecl());
}


bool Method::returnsRef(FuncDeclaration& fdecl) const
{
    bool result = false;

    if (fdecl.type && (fdecl.type->ty == Tfunction))
    {
        if (static_cast<TypeFunction*>(fdecl.type)->isref)
        {
            result = true;
        }
    }
    return result;
}


void Method::emitRetType(wostream& out)
{
    FuncDeclaration& fdecl = funcDecl();
    unsigned storage = 0;
    if (returnsRef(fdecl))
    {
        storage = STCref;
    }
    out << getParamTypeName(fdecl, storage, retType_, &retType_, true);
}


void Method::emitILProto(wostream& out, bool full)
{
    //function prototype (aka signature) has been programmatically set?
    if (setProto_)
    {
        out << proto_;
    }
    else
    {
        //IF_WIN_DEBUG(OutputDebugStringA((string(getDecl().toChars()) + "\n").c_str()));
        if (getDecl().isCtorDeclaration())
        {
            out << "void";  // IL tors return void
        }
        else if (retType_)
        {
            emitRetType(out);
            if (ret_)
            {
                ret_->setType(retType_, 0);
            }
        }
        out << ' ';
        if (full)
        {
            emitClassDecl(out, getDecl());
        }
        emitMethodName(out, getDecl());
        emitILArgs(out << " (");
        out << ')';
    }
}


Mnemonic Method::getCallInstr() const
{
    Declaration& decl = getDecl();
    if ( decl.isClassMember()  &&
        !decl.isStatic()       &&
        !decl.isFinal()        &&
        !decl.isCtorDeclaration())
    {
        return IL_callvirt;
    }
    return IL_call;
}


const wstring& Method::getProto(bool recompute)
{
    if (proto_.empty() || recompute)
    {
        wostringstream proto;
        emitILProto(proto);
        proto_ = proto.str();
    }
    return proto_;
}


// Create a new Arg for the VarDeclaration and add it to the given block
static void makeArg(block& argsBlock, VarDeclaration& varDecl, Type* type = NULL)
{
    assert(varDecl.csym == NULL); // pre-condition;
    Symbol* sym = new Arg(varDecl, argsBlock.size(), type);
    varDecl.csym = sym;
    argsBlock.add(*sym);
}



// Emit the tail arguments of a variadic template function
static size_t emitVariadicArgs( FuncDeclaration& decl,
                                Arguments* arguments, 
                                unsigned pos,
                                block& argsBlock,
                                const string& name,
                                wostream& out
                                )
{
    ArrayAdapter<Argument*> args(arguments);
    ArrayAdapter<Argument*>::const_iterator i = args.begin();
    for (unsigned n = 0; i != args.end(); ++i, ++pos, ++n)
    {
        Argument* arg = *i;
        if (pos)
        {
            out << ", ";
        }
        out << getParamTypeName(decl, DEREF(arg).storageClass, DEREF(arg).type);
        if (!arg->ident)
        {
            arg->ident = Identifier::generateId(("_" + name + "_field_").c_str(), n);
        }
        if (decl.localsymtab && !decl.localsymtab->lookup(arg->ident))
        {
            VarDeclaration* varDecl = new VarDeclaration(decl.loc, arg->type, arg->ident, NULL);
            makeArg(argsBlock, *varDecl);
            decl.localsymtab->insert(arg->ident, varDecl);
        }
    }
    return args.size();
}


int32_t emitFormalArgs(FuncDeclaration* decl, TypeFunction& tfun, wostream& out)
{
    //returns the offset where var args start, or -1 if no var args
    int32_t vaOffs = -1;

    ArrayAdapter<Argument*> args(tfun.parameters);
    ArrayAdapter<Argument*>::const_iterator i = args.begin(); 
    for (unsigned npos = 0; i != args.end(); ++i, ++npos)
    {
        Argument* arg = *i;
        if (npos)
        {
            out << ", ";
        }
        unsigned storageClass = DEREF(arg).storageClass;
        if (decl)
        {
            out << getParamTypeName(*decl, storageClass, DEREF(arg).type);
        }
        else if (storageClass & STClazy)
        {
            out << lazyArgType;
        }
        else
        {
            Type* type = DEREF(arg).type;
            if ((storageClass & STCref) 
                && (DEREF(type).ty != Tpointer)
                && (DEREF(type).ty != Tclass)
                )
            {
                type = DEREF(type).referenceTo();
            }
            static Loc loc;
            out << toILType(loc, type).name();
        }
    }
    if (tfun.varargs)
    {
        vaOffs = args.size();
        if (args.size())
        {
            out << ", ";
        }
        out << "object[]";
    }
    return vaOffs;
}


static Arg* makeThisArg(FuncDeclaration& fdecl, block& argsBlock)
{
    Arg* This = NULL;

    VarDeclaration* vthis = fdecl.vthis;
    // have a 'this' hidden arg?
    if (vthis && !vthis->csym && fdecl.isMember() && !fdecl.isStatic())
    {
        This = new Arg(*vthis, 0);
        vthis->csym = This;
        assert(argsBlock.empty());
        argsBlock.add(*This);
    }
    return This;
}


static int32_t emitParams(FuncDeclaration& fdecl,
                          block& argsBlock,
                          int32_t& argCount,
                          wostream& out
                          )
{
    int vaOffs = -1;
    makeThisArg(fdecl, argsBlock);
    
    ArrayAdapter<VarDeclaration*> args(fdecl.parameters);
    ArrayAdapter<VarDeclaration*>::const_iterator i = args.begin();
    for (unsigned pos = 0; i != args.end(); ++i, ++pos)
    {
        VarDeclaration* decl = *i;
        Type* type = DEREF(decl).type;
        if (type && type->ty == Ttuple) // dealing with a variadic template?
        {
            assert(i + 1 == args.end());// tail arg
            Arguments* tailArgs = static_cast<TypeTuple*>(type)->arguments;
            Dsymbols params;
            const char* name = decl->toChars();
            assert(name);
            assert(argCount);
            --argCount; // remove the tuple
            argCount += emitVariadicArgs(fdecl, tailArgs, pos, argsBlock, name, out);
        }
        else
        {
            unsigned& storage = DEREF(decl).storage_class;

            if (pos)
            {
                out << ", ";
            }
            else if (type && fdecl.fes)
            {
                ArrayAdapter<Argument*> args(fdecl.fes->arguments);
                if (args.size() > 1)
                {
                    storage &= ~STCref; // never pass first param by reference
                    // This is a similar hack to getKeyType():
                    // When the key in an associative array is a D string we need to change to
                    // a System.String, because Equals for arrays is not a lexicographical
                    // comparison as the user would expect (D strings do not map automatically
                    // to System.String, but to uint8[], because they are UTF8, not UTF16).

                    if (type->isString())
                    {
                        type = getStringType();
                        args[0]->type = type;
                        decl->type = type;
                    }
                }
            }
            if (fdecl.isMain())
            {
                // the entrypoint must either have no arguments, or take an array
                // of System.String, otherwise the program will not run
                type = getStringType()->arrayOf();
            }

            out << getParamTypeName(fdecl, storage, type, &type);
            out << " '" << DEREF(decl).toChars() << "'";
        }
        if (DEREF(decl).csym)
        {
            continue; // have back-end symbol already
        }
        if (!decl->csym)
        {
            makeArg(argsBlock, DEREF(decl), type);
        }
    }

    if (VarDeclaration* vdecl = fdecl.v_arguments)
    {
        vaOffs = args.size();
        if (args.size())
        {
            out << ", ";
        }
        out << "object[] " << Id::_arguments->toChars();
        if (!vdecl->csym)
        {
            makeArg(argsBlock, *vdecl);
        }
    }
    return vaOffs;
}


FuncDeclaration& Method::funcDecl() const
{
    FuncDeclaration* decl = getDecl().isFuncDeclaration();
    if (!decl)
    {
        BackEnd::fatalError(getDecl().loc, "function declaration expected");
    }
    return *decl;
}


void Method::emitILArgs(wostream& out)
{
    FuncDeclaration& decl = funcDecl();
    if (decl.isStaticDtorDeclaration())
    {
        //synthesize args to match expected EventHandler signature:
        out << "object, class [mscorlib]System.EventArgs";
    }
    else if (decl.parameters)
    {
        argCount_ = decl.parameters->dim;
        vaOffs_ = emitParams(decl, args_, argCount_, out);
    }
    else if (decl.type && (decl.type->ty == Tfunction))
    {
        TypeFunction* tfun = NULL;
        if (Type* origType = decl.originalType)
        {
            tfun = static_cast<TypeFunction*>(origType);
        }
        else if (decl.type)
        {
            tfun = static_cast<TypeFunction*>(decl.type);
        }
        vaOffs_ = emitFormalArgs(&decl, DEREF(tfun), out);
        if (tfun->parameters)
        {
            argCount_ = tfun->parameters->dim + tfun->varargs;
        }
    }
    else
    {
        BackEnd::fatalError(decl.loc, "cannot emit args, NULL type or not a function");
    }
}



static void checkOverride(FuncDeclaration& decl)
{
    if (ClassDeclaration* cd = decl.isClassMember())
    {
        if (cd->baseClass)
        {
            assert(DEREF(decl.type).ty == Tfunction);
            TypeFunction* tfun = static_cast<TypeFunction*>(decl.type);

            // the front-end checks this too, but only gives an error if
            // -w is given in the command line; a non-critical warning may also
            // be helpful
            if (FuncDeclaration* fd = cd->baseClass->findFunc(decl.ident, tfun))
            {
                WARNING(decl.loc,
                    "overrides base class function %s, but is not marked with 'override'", 
                    fd->toPrettyChars());
            }
        }
    }
}


elem* Method::emitILAsm(wostream& out)
{
    if (body_.empty())
    {
        return NULL;
    }
    FuncDeclaration& decl = funcDecl();
    if (!decl.fbody && !decl.isMember())
    {
        return NULL;
    }
    //out << '\n';

    if (decl.isMain())
    {
        out << "//--------------------------------------------------------------\n";
        out << "// main program\n";
        out << "//--------------------------------------------------------------\n";
    }
    indent(out) << ".method " << decl.prot();

    if (decl.isVirtual() || decl.isDtorDeclaration())
    {
        out << "virtual ";
    }
    if (decl.isAbstract())
    {
        out << "abstract ";
    }
    if (decl.isFinal())
    {
        out << "final ";
    }
    if (!decl.isOverride())
    {
        checkOverride(decl);
        if (decl.isVirtual())
        {
            out << "newslot ";
        }
        //"The method hides all methods of the parent classes that have a matching 
        //signature and name (as opposed to having a matching name only). This flag
        //is ignored by the common language runtime and is provided for the use of
        //compilers only. The IL assembler recognizes this flag but does not use it"
        //Serge Lidin -- Expert .NET 2.0 IL Assembler, APress
        out << "hidebysig ";
    }
    if (decl.isStatic() || !decl.isMember())
    {
        out << "static";
    }
    else
    {
        out << "instance";
    }
    out << ' ' << getProto();
    if (decl.storage_class & (STCsynchronized | STCshared))
    {
        out << " synchronized";
    }
    out << '\n';
    emitILBody(out);
    return this;
}


///<summary>
/// Remove local variables that are not referenced
///</summary>
static void removeUnused(block& blk)
{
    unsigned pos = 0;
    for (block::iterator i = blk.begin(); i != blk.end(); )
    {
        if (Positional* v = (*i)->isPositional())
        {
            //if EmitHooks prevent the block from being emitted,
            //remove the variable that it contains
            while (!blk.isEmitable() && v->decRefCount())
            { }
            if (v->refCount() == 0)
            {
                //IF_DEBUG(clog << "unused var: " << v->getName() << endl);
                //erase does not free the object
                i = blk.erase(i);

                //release "slot", if any is associated with this variable
                v->releaseSlot();
                //it is not safe to delete v, just handle it
                //to the assembly block (for memory management only,
                //it never gets emitted)
                BackEnd::instance().getAssembly().add(*v);
            }
            else
            {
                v->setPosition(pos++);
                ++i;
            }
        }
        else
        {
            if (block* b = (*i)->isBlock())
            {
                removeUnused(*b);
            }
            ++i;
        }
    }
}


namespace {
    ///<summary>
    /// A block that is emitted only if the ctor is delegated to.
    ///</summary>
    class CtorGuardBlock : public AutoManaged<block>
    {
        Method& ctor_;
        Label&  label_;

        elem* emitILAsm(wostream& out)
        {
            if (ctor_.isDelegatedTo())
            {
                return block::emitILAsm(out);
            }
            else
            {
                label_.decRefCount();
            }
            IF_DEBUG(indent(out) << "// no .ctor guard\n");
            return this;
        }

    public:
        CtorGuardBlock(block& blk, Method& ctor, Label& label) 
            : AutoManaged<block>(&blk, blk.loc(), blk.depth())
            , ctor_(ctor)
            , label_(label)
        {
        }
    };
}


// Generate code to call the ctor of the base class.
// Add initializers block.
static void chainCtorAndIni(block& blk, Method& m, CtorDeclaration& ctor)
{
    if (Aggregate* aggr = m.getAggregate())
    {
        block* skip = new block(&blk, ctor.loc, blk.depth());
        block* guardBlock(new CtorGuardBlock(blk, m, skip->getLabel(false)));
        //generate code that checks $in_ctor
        guardBlock->add(*new ThisArg);
        Variable& guard = aggr->getCtorDelegationGuard();
        guard.load(*guardBlock);
        elem* brinst = BranchInstruction::create(blk, IL_brtrue);
        Branch::create(brinst, *guardBlock, skip);
        //classes are slightly different from structs
        if (Class* c = aggr->isClass())
        {
            if (ClassDeclaration* base = c->getDecl().baseClass)
            {
                if (!base->defaultCtor)
                {
                    //the front-end handles the case when there is a base default ctor
                    blk.add(*new ThisArg);
                    new DefaultCtorCall<ClassDeclaration>(blk, IL_call, *base);
                }
            }
        }
        else
        {
            assert(aggr->isStruct());
#if !VALUETYPE_STRUCT
            static StructType object(NULL);
            blk.add(*new ThisArg);
            new DefaultCtorCall<StructType>(blk, IL_call, object);
#endif
        }
        //add initializers
        blk.add(*new SharedElem(aggr->getInitBlock()));
        blk.add(*skip);
    }
}


static void ensureRet(IRState& irstate,
                      block& body, 
                      FuncDeclaration& decl, 
                      Type& retType
                      )
{
    assert(irstate.hasFuncDecl());
    if (body.hasRetInstruction())
    {
        return;
    }
    if (block* ret = body.hasRetBlock())
    {
        if (ret->size() <= 1) // the only elem is a label?
        {
            Variable* v = irstate.getRetVar();
            if (v)
            {
                v->load(*ret, &decl.endloc);
            }
            ret->addRet(&decl.endloc, v);
        }
    }
    else if (!irstate.getBlock().hasRetInstruction())
    {
        if (retType.ty == Tvoid)
        {
            irstate.getBlock().addRet(&decl.endloc);
        }
        else if (!decl.csym)
        {
            WARNING(decl.loc, "unresolved function %s", decl.toPrettyChars());
        }
        else if (Method* method = DEREF(decl.csym).isMethod())
        {
            if (Variable* v = method->getResult())
            {
                v->load(body, &decl.loc);
                irstate.getBlock().addRet(&decl.endloc);
            }
        }
    }
}


namespace {
    /// <summary>
    /// Wrap __dtor code with .try { } finalize { }
    /// </summary>
    class DtorEH : public ExceptionHandler
    {
        DtorDeclaration& dtor_;

        DtorEH(block* parent, Loc& loc, DtorDeclaration& dtor, unsigned depth)
            : ExceptionHandler(parent, loc, depth)
            , dtor_(dtor)
        { }

        void emitILAsmImpl(wostream& out, const wstring&)
        {
            unsigned depth = this->depth();
            if (depth) --depth;

            indent(out, depth) << "finally\n";
            indent(out, depth) << "{\n";
            if (AggregateDeclaration* decl = dtor_.isMember())
            {
                if (ClassDeclaration* cd = decl->isClassDeclaration())
                {
                    indent(out, depth + 1) << "ldarg.0\n";
                    // emit call to base class dtor
                    indent(out, depth + 1) << "call instance void ";
                    if (ClassDeclaration* base = cd->baseClass)
                    {
                        out << className(*base);
                    }
                    else
                    {
                        out << className(DEREF(cd->object));
                    }
                    out << "::__dtor()\n";
                }
                else
                {
#if !VALUETYPE_STRUCT
                    indent(out, depth + 1) << "ldarg.0\n";
                    // emit call to base class dtor
                    indent(out, depth + 1) << "call instance void ";
                    out << "[mscorlib]System.Object::Finalize()\n";
#endif // VALUETYPE_STRUCT
                }
            }
            indent(out, depth + 1) << "endfinally\n";
            indent(out, depth) << "}\n";
        }

    public:
        static ExceptionHandler* create(block& blk, DtorDeclaration& decl)
        {
            return new DtorEH(&blk, blk.loc(), decl, blk.depth());
        }
    };
}


static void handleSpecialFuncs(IRState& irstate,
                               Method& method,
                               FuncDeclaration& decl,
                               block& body
                               )
{
    if (decl.isMain())
    {
        if (global.params.debugc)
        {
            block* blk = body.addNewBlock();
            blk->addDefaultEH();
            irstate.setBlock(*blk);
        }
    }
    else if (DtorDeclaration* dtorDecl = decl.isDtorDeclaration())
    {
        block* blk = body.addNewBlock();
        blk->incDepth();
        blk->setEH(DtorEH::create(*blk, *dtorDecl));
        irstate.setBlock(*blk);
    }
    else if (decl.isStaticConstructor())
    {
        if (ClassDeclaration* classDecl = decl.isClassMember())
        {
            if (!classDecl->csym)
            {
                throw logic_error("static ctor in class decl with NULL symbol");
            }
            if (Class* c = classDecl->csym->isClass())
            {
                // Generating one .cctor for the static ctor does not work
                // in the case of multiple static constructors (which D allows).
                // Synthesize a call to this static ctor from the static initializers'
                // block, which is used to synthesize the body of the .cctor
                CallInstruction::create(c->getStaticInitBlock(), IL_call, method);
            }
        }
    }
}


void Method::generateBody()
{
    assert(body_.empty());

    getProto(); // ensure that arguments are generated
    body_.incDepth();
    FuncDeclaration& decl = funcDecl();
    if (decl.fbody)
    {
        IRState irstate(body_);
        irstate.setDecl(&decl);
        irstate.setRetVar(ret_);
        if (decl.fes)
        {
            irstate.setForeachAggr(decl.fes->aggr);
        }
        handleSpecialFuncs(irstate, *this, decl, body_);

        // make 'this' arg if needed
        makeThisArg(decl, args_);

        if (CtorDeclaration* ctor = decl.isCtorDeclaration())
        {
            chainCtorAndIni(irstate.getBlock(), *this, *ctor);
        }
        if (decl.frequire)
        {
            decl.frequire->toIR(&irstate);
        }
        //generate function body
        decl.fbody->toIR(&irstate);

        // make sure there is a "ret" instruction at the end of this
        // function (even if there is no explicit return in the source)
        ensureRet(irstate, body_, decl, DEREF(retType_));
    }
}


void Method::emitILBody(wostream& out)
{
    body_.unindent(out) << "{\n";
    FuncDeclaration& decl = funcDecl();
    if (decl.fbody)
    {
        BackEnd::instance().emitLineInfo(body_, decl.loc, out);
        if (decl.isMain())
        {
            g_haveEntryPoint = true;
            body_.indent(out) << ".entrypoint\n";
        }
        const unsigned maxStack = body_.getMaxStackDelta();
        body_.indent(out) << ".maxstack " << maxStack << "\n";
        removeUnused(body_);
        removeUnused(locals_);
        locals_.emitIL(out);
        body_.emitIL(out);
    }
    body_.unindent(out) << "}\n";
}


static Slot* getSlot(block& locals, VarDeclaration& decl, Type* type)
{
    Slot* slot = NULL;
    //look for a suitable slot
    for (block::iterator i = locals.begin(); i != locals.end(); ++i)
    {
        if (Slot* s = (*i)->isSlot())
        {
            if ((s->refCount() == 1) && (s->getType() == type))
            {
                slot = s;
                break;
            }
        }
    }
    if (!slot)
    {
        slot = new Slot(decl, locals.size(), type);
        slot->incRefCount();
        assert(slot->refCount() == 1);
        locals.add(*slot);
    }
    return slot;
}


// Add a local variable to the body of this method / function
Variable* Method::addLocal(VarDeclaration& decl, Type* type)
{
    Local* loc = NULL;
    if (varScope_) // at a nested scope?
    {
        if (!type)
        {
            type = decl.type;
        }
        Slot* slot = getSlot(locals_, decl, type);
        loc = new NestedLocal(*slot, decl);
        varScope_->add(*loc);
    }
    else
    {
        //search a local variable with the same name and type
        for (LocalsBlock::iterator i = locals_.begin(); i != locals_.end(); ++i)
        {
            if (Variable* v = (*i)->isVariable())
            {
                if (!type && (v->getType() == decl.type) 
                          && (v->getDecl().ident->compare(decl.ident) == 0))
                {
                    return v;
                }
            }
        }
        loc = new Local(decl, locals_.size(), type);
        locals_.add(*loc);
    }
    return loc;
}


int32_t Method::getCallStackDelta() const
{
    int32_t delta = -argCount_; // pops the args off the stack
    if (getDecl().isCtorDeclaration())
    {
        assert(getDecl().isMember());
        assert(!getDecl().isStatic());

        ++delta;
    }
    else
    {
        if (!getDecl().isStatic() && getDecl().isClassMember())
        {
            --delta;            // it also pops the object ref
        }
        if (DEREF(retType_).ty != Tvoid)
        {
            ++delta;
        }
    }
    // IF_DEBUG(clog << decl_.toChars() << ": " << delta << endl);
    return delta;
}


Closure* Method::getClosure(bool create) 
{
    if (!closure_ && create)
    {
        closure_ = new Closure;
    }
    return closure_;
}


void Method::setClosure(Closure* context) 
{
    delete closure_;
    closure_ = context;
}



/********************************/
#pragma region Arg, Local, Field

Arg::Arg(VarDeclaration& decl, unsigned pos, Type* type)
    : Local(decl, pos, type)
{
}


elem* Arg::emitILAsm(wostream&)
{
    throw logic_error("emitIL called on arg without LOAD or STORE context");
    return NULL;
}


Local::Local(VarDeclaration& decl, unsigned pos, Type* type)
    : Positional(decl, pos, type)
{
}


elem* Local::emitILAsm(wostream& out)
{
#if _MSC_VER
    if (out.tellp() > -1)
#else
    if (out.tellp() > -0)
#endif
    {
        out << ",\n";
    }
    //front-end type
    Type* varType = getType();

    //get the back-end type
    const TYPE& type = toILType(decl_.loc, varType);

    const unsigned pos = DEREF(position());
    indent(out, depth() + 1) << '[' << pos << "] ";
    out << type.name();
    if ((decl_.storage_class & STCpinned) == STCpinned)
    {
        out << " pinned";
    }
    out << " '" << getName() << "'";
    IF_DEBUG(out << " /* refcount=" << refCount()  << " */ ");
    return this;
}


namespace {
    enum LoadStore { LOAD, LOADA, STORE };
    enum WhatIs { ARG, LOCAL, FIELD };

    template<typename T> struct VarTraits;
    template<> struct VarTraits<Arg> { static const WhatIs whatIs = ARG; };
    template<> struct VarTraits<Local> { static const WhatIs whatIs = LOCAL; };
    template<> struct VarTraits<Field> { static const WhatIs whatIs = FIELD; };

    ///<summary>
    /// Generates code for loading and storing fields, args and local vars
    ///</summary>
    class LoadStoreVar : public AutoManaged<elem>
    {
        static const wstring name[];

        const WhatIs whatIs_;
        LoadStore op_; 
        Declaration& decl_;
        const unsigned* pos_;
        Variable& var_;
        Loc* loc_;

    public:
        template<typename V>
        LoadStoreVar(block& blk, LoadStore op, V& var, Loc* loc) 
            : AutoManaged<elem>(blk)
            , whatIs_(VarTraits<V>::whatIs)
            , op_(op)
            , pos_(var.position())
            , decl_(var.getDecl())
            , var_(var)
            , loc_(loc)
        {
            assert(!var.isLoad());
            var.incRefCount();
            assert(var.getOwner());
        }

        virtual Variable* isLoad() 
        { 
            return (op_ == LOAD || op_ == LOADA) ? &var_ : NULL;
        }
        
        virtual Variable* isStore()
        {
            return op_ == STORE ? &var_ : NULL;
        }

        int32_t getStackDelta() const
        {
            int32_t delta = (op_ == STORE ? -1 : 1);
            if ((whatIs_ == FIELD) && !var_.isStatic())
            {
                --delta;
            }
            return delta;
        }

        void emitOpCode(wostream& out)
        {
            out << (op_ == STORE ? "st" : "ld");
            if ((whatIs_ == FIELD) && var_.isStatic())
            {
                out << "s";
            }
            out << name[whatIs_];
        }

        elem* emitILAsm(wostream& out)
        {
            if (loc_)
            {
                BackEnd::instance().emitLineInfo(*this, *loc_, out);
            }
            emitOpCode(indent(out));
            bool needSpace = true, addr = false;

            switch (op_)
            {
            case LOAD:
                if (pos_ != NULL && *pos_ <= 255) // fits in one unsigned byte?
                {
                    out << ".";
                    // Use the specialized version for ordinals < 4
                    if (*pos_ < 4)
                    {
                        needSpace = false;
                    }
                    else
                    {
                        out << "s";
                    }
                }
                break;

            case LOADA:
                addr = true;
                break;

            case STORE:
                if (pos_ != NULL && *pos_ <= 255)
                {
                    out << ".s";
                }
                break;
            }
            if (addr)
            {
                out << 'a';
            }
            if (needSpace)
            {
                out << ' ';
            }
            switch (whatIs_)
            {
            case FIELD:
                out << DEREF(var_.isField()).getTYPE().name() << ' ';
                if (AggregateDeclaration* aggr = decl_.isMember())
                {
                    out << className(*aggr) << "::";
                }
                if (decl_.csym              &&
                    decl_.csym->isField()   &&
                    decl_.csym->isField()->isStatic()
                    )
                {
                    //do not quote static fields because toChars()
                    //may yield a fully qualified name
                    out << var_.getName() << '\n';
                }
                else
                {
                    out << "'" << decl_.toChars() << "'\n";
                }
                break;

            default:
                if (pos_)
                {
                    out << *pos_ << "\t// '" << var_.getName() << "'";
                    IF_DEBUG(out << " (" << toILType(var_).name() << ')');
                }
                out << '\n';
                break;
            }
            return this;
        }
    };

    const wstring LoadStoreVar::name[] = { L"arg", L"loc", L"fld" };
}


elem* Arg::load(block& blk, Loc* loc, bool addr)
{
    bool ref = getDecl().isRef() && (getDecl().ident != Id::This);
    if (ref && addr)
    {
        addr = false;
        ref = false;
    }
    block* group = &blk;
    if (ref)
    {
        if (!loc) loc = &blk.loc();
        group = blk.addNewBlock(*loc);
    }
    elem* e = new LoadStoreVar(*group, addr ? LOADA : LOAD, *this, loc);
    if (ref)
    {
        TYPE& t = toILType(*this);
        e = group->add(*new LoadIndirect(t));
    }
    return e;
}


elem* Arg::store(block& blk, Loc* loc)
{
    if (getDecl().isRef())  // assignment via reference?
    {
        bool blit = (getDecl().ident == Id::This);
        TYPE& t = toILType(*this);
        return blk.add(*new StoreIndirect(t, blit));
    }
    return new LoadStoreVar(blk, STORE, *this, loc);
}


elem* Local::load(block& blk, Loc* loc, bool addr)
{
    elem* res = new LoadStoreVar(blk, addr? LOADA : LOAD, *this, loc);
    if (isRefType())
    {
        res = blk.add(*new LoadIndirect(toILType(*this)));
    }
    return res;
}


elem* Local::store(block& blk, Loc* loc)
{
    return new LoadStoreVar(blk, STORE, *this, loc);
}


elem* Field::load(block& blk, Loc* loc, bool addr)
{
    block* varBlock = &blk;
    if (!loc) loc = &blk.loc();

    if (!isStatic())
    {   // group the loading of the parent object
        // together with loading the field -- so
        // that they are treated as a unit when
        // swapTail() is called on block
        varBlock = blk.addNewBlock(*loc);
        loadRefParent(*this, *varBlock, loc);
    }
    const bool isPtr = hasClosureReference();
    if (isPtr)
    {
        varBlock = blk.addNewBlock(*loc);
        //closure pointers are unmanaged pointers
        BackEnd::getModuleScope(*loc).setUnverifiable(*loc);
    }
    elem* e = new LoadStoreVar(*varBlock, addr ? LOADA : LOAD, *this, loc);
    if (hasClosureReference())
    {
        TYPE& t = toILType(*this);
        e = varBlock->add(*new LoadIndirect(t));
    }
    return e;
}


// If "v" is the child of a temporary aggregate, then the temporary needs 
// to be copied back into its "master" to reflect the change;
// for example, consider an assoc. array of structs, MyStruct s[int];
// for an expression like this s[42].f = 13, a temporary result for
// s[42] is created; after f is assigned to, s[42] needs to be updated.

inline static void storePartial(block& blk, Loc* loc, Variable* v)
{
    assert(v);
    bool partial = false;

    Variable* p = v->getParent();
    for (v = p; v; v = v->getParent())
    {
        p = v;
        partial |= (v->isPartialResult() != NULL);
    }
    if (partial && p && isValueType(toILType(*p)))
    {
        p->load(blk, loc);
        p->store(blk);
    }
}


elem* Field::store(block& blk, Loc* loc)
{
    if (!isStatic())
    {
        loadRefParent(*this, blk, loc);
        // LDFLD expects the value on top of the stack, then the object reference
        blk.swapTail();
    }
    elem* result = NULL;
    if (hasClosureReference())
    {
        if (!loc) loc = &blk.loc();
        BackEnd::getModuleScope(*loc).setUnverifiable(*loc);
        TYPE& t = toILType(*this);
        new LoadStoreVar(blk, LOAD, *this, loc);
        if (!isStatic())
        {
            blk.swapTail();
        }
        result = blk.add(*new StoreIndirect(t));
    }
    else
    {
        result = new LoadStoreVar(blk, STORE, *this, loc);
    }
    storePartial(blk, loc, this);
    return result;
}


Symbol* Field::isSymbol()
{
    if (getParent())
    {
        assert(getOwner());
        Field* clone = new Field(*this);
        clone->silent_ = true;
        getOwner()->add(*clone);
        return clone;
    }
    return this;
}
#pragma endregion


bool hasTLS(Declaration& decl)
{
    //todo: what about structs?
    if (ClassDeclaration* cd = decl.type->isClassHandle())
    {
        if (cd->storage_class & STCtls)
        {
            return true;
        }
    }
    return (decl.storage_class & STCtls);
}


Method* Method::parent() const
{
    if (parent_)
    {
        if (FuncDeclaration* fd = parent_->isFuncDeclaration())
        {
            assert(nested_);
            if (fd->csym) return fd->csym->isMethod();
        }
    }
    return NULL;
}
