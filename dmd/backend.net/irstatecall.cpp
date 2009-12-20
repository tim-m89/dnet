//
// $Id: irstatecall.cpp 24625 2009-07-31 01:05:55Z unknown $
//
// Copyright (c) 2009 Cristian L. Vlasceanu
//
// This file contains code related to generating function calls.

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
#include "declaration.h"
#include "backend.net/irstateutil.h"
#include "backend.net/type.h"
#include "backend.net/msil/instruction.h"
#include "backend.net/msil/member.h"
#include "backend.net/msil/types.h"
#include "backend.net/msil/variable.h"
#include "backend.net/msil/varargs.h"

using namespace std;


namespace {
    ///<summary>
    /// Emits code that copies elements from one array to another.
    ///</summary>
    class ArrayCopy5 : public elem
    {
        int32_t getStackDelta() const { return -5; }

        elem* emitILAsm(wostream& out)
        {
            indent(out) << "call void [mscorlib]System.Array::Copy("
                           "class [mscorlib]System.Array, int32, "
                           "class [mscorlib]System.Array, int32, int32)\n";
            return this;
        }
    };


    ///<summary>
    ///Generate code that creates an AssocArray/Delegate from pointer to func
    ///</summary>
    class NewForeachCallback : public elem
    {
        TYPE* key_;
        TYPE& value_;

        elem* emitILAsm(wostream& out)
        {
            indent(out) << "// construct Foreach delegate\n";
            indent(out);
            out << "newobj instance void class [dnetlib]runtime.AssocArray/Callback`";
            if (key_)
            {
                const char* keyTypeName = key_->name();
                out << 2 << "<" << keyTypeName << ", ";
            }
            else
            {
                out << 1 << "<";
            }
            out << value_.name() << ">::.ctor(object, native int)\n";
            return this;
        }

        int32_t getStackDelta() const
        {
            return -1;
        }

    public:
        NewForeachCallback(TYPE& value, TYPE* key) : key_(key), value_(value)
        {
        }
    };
} //namespace



static void setArrayPropSignature(IRState& irstate,
                                  Method& method,
                                  Loc& loc,
                                  Variable* param,
                                  Type* paramType,
                                  elem* e
                                  )
{
    assert(method.isSynthProp()); //pre-condition
    if (param) paramType = param->getType();

    ArrayType& aType = getArrayType(toILType(loc, paramType));
    //force ret type to be the same as the first param
    method.setRetType(paramType, false);

    wostringstream buf;
    if (aType.isSliceType())
    {
        buf << "$SLICE<!!0>";
    }
    else
    {
        buf << "!!0[]";
    }
    buf << " [dnetlib]runtime.Array::";
    switch (method.isSynthProp())
    {
    case prop_Dup:      buf << "Clone<";    break;
    case prop_Reverse:  buf << "Reverse<";  break;
    case prop_Sort:     buf << "Sort<";     break;
    default:            assert(false);
    }
    buf << aType.elemTYPE().name() << ">(";
    if (aType.isSliceType() || (e && e->isASlice()))
    {
        buf << "$SLICE<!!0>)";
    }
    else
    {
        buf << "!!0[])";
    }
    method.setProto(buf.str(), 1);
}


//Override the signature of _aaApply (or _aaApply2) as synthesized by the
//front-end with the signature of our own AssocArray.Foreach generic method.
static elem* setForeachSignature(Method& method, Loc& loc, Type* type, int args)
{
    assert(args);
    assert(args <= 2);

    AssocArrayType* aaType = toILType(loc, type).isAssociativeArrayType();
    if (!aaType)
    {
        BackEnd::fatalError(loc, "associative array type expected");
    }
    method.setRetType(Type::tvoid, false);
    wostringstream buf;
    buf << "void [dnetlib]runtime.AssocArray::Foreach<" 
        << aaType->keyTYPE().name()
        << ", "
        << aaType->elemTYPE().name()
        << ">(\n\t" << aaType->argName() << ",\n\tint32,\n\t"
        << "class [dnetlib]runtime.AssocArray/Callback`" << args << "<";
    if (args == 2)
    {
        buf << "!!0,";
    }
    buf << "!!1>)";
    method.setProto(buf.str(), args);

    TYPE* key = (args == 1) ? NULL : &aaType->keyTYPE();
    return new NewForeachCallback(aaType->elemTYPE(), key);
}


static void setAArrayPropSignature(IRState& irstate,
                                   Method& method,
                                   Loc& loc,
                                   Type* type
                                   )
{
    assert(method.isSynthProp()); //pre-condition
    wostringstream buf; 
    AssocArrayType& aaType = getAssocArrayType(toILType(loc, type));

    SynthProp prop = method.isSynthProp();

    switch (prop)
    {
        case prop_Len:
            buf << "  instance int32 class [mscorlib]System.Collections.Generic.Dictionary`2<" 
                << aaType.keyTYPE().name() << "," << aaType.elemTYPE().name() << ">::get_Count()";
            break;

        case prop_Keys:
            // set proper return type for the synth prop
            method.setRetType(aaType.keyType().arrayOf(), false);

            buf << "instance class [mscorlib]System.Collections.Generic.Dictionary`2/KeyCollection<!0,!1> "
                    "class [mscorlib]System.Collections.Generic.Dictionary`2<"
                << aaType.keyTYPE().name() << "," << aaType.elemTYPE().name() << ">::get_Keys()\n"
                << "call !!0[] [System.Core]System.Linq.Enumerable::ToArray<"
                << aaType.keyTYPE().name() 
                << ">(class [mscorlib]System.Collections.Generic.IEnumerable`1<!!0>)\n";

                if (aaType.keyType().isString()) 
                {
                    buf << "call unsigned int8[][] [dnetlib]runtime.dnet::strArrayToByteArrayArray(string[])\n";
                }

            break;

        case prop_Values:
            // set proper return type for the synth prop
            method.setRetType(aaType.elemType().arrayOf(), false);

            buf << "instance class [mscorlib]System.Collections.Generic.Dictionary`2/ValueCollection<!0,!1> "
                   "class [mscorlib]System.Collections.Generic.Dictionary`2<"
                << aaType.keyTYPE().name() << "," << aaType.elemTYPE().name() << ">::get_Values()\n"
                << "call !!0[] [System.Core]System.Linq.Enumerable::ToArray<"
                << aaType.elemTYPE().name() 
                << ">(class [mscorlib]System.Collections.Generic.IEnumerable`1<!!0>)\n";
            break;

        default:
            throw logic_error("assoc array property expected");
    }
    method.setProto(buf.str(), 1);
}


//Generate the code that creates a temporary array on the evaluation stack
//and populates it with the elements of the given slice variable
static void sliceToTempArray(IRState& irstate,
                             Loc& loc,
                             SliceType& sliceType,
                             Variable* sliceVar,
                             bool store = false
                             )
{
    assert(!sliceVar || !sliceVar->isPartialResult()); //results handled elsewhere

    VarBlockScope varScope(irstate, loc);
    Method& method = varScope.getMethod();
    block& blk = irstate.getBlock();

    if (store)
    {
        sliceVar = method.addLocal(sliceVar->getDecl());
        sliceVar->store(blk);
        sliceVar->load(blk, NULL, true);
        blk.add(*PropertyGetter::create(sliceType, "Array", "!0[]"));
    }
    Variable* len = method.addLocal(sliceVar->getDecl(), Type::tint32);

    //load the offset
    sliceVar->load(blk, NULL, true);
    blk.add(*PropertyGetter::create(sliceType, "Offset", "int32"));

    //make a new array to fit all elements in the slice
    sliceVar->load(blk, NULL, true);
    blk.add(*PropertyGetter::create(sliceType, "Count", "int32"));
    len->store(blk);
    len->load(blk);
    blk.add(*NewArray::create(sliceType));

    //save array to local variable
    Type* arrayType = sliceType.elemType().arrayOf();
    Variable* arr = method.addLocal(sliceVar->getDecl(), arrayType);
    arr->store(blk);
    arr->load(blk);

    // destination index is zero
    Const<size_t>::create(*TYPE::Uint, 0, blk, loc);

    // destination length
    len->load(blk);

    // copy elements
    blk.add(*new ArrayCopy5);

    // get array on the top of the stack
    arr->load(blk);
}


//Convert arrays to slices before passing them to functions,
//because we modify all functions (except imported from other languages)
//that take and return arrays to take and return slices.
static void convertArrayToSliceIfNeeded(IRState& irstate,
                                        Loc& loc,
                                        TYPE* varType,
                                        elem& e,
                                        bool isRef
                                        )
{
    if (e.isStringLiteral())
    {
        varType = &toILType(loc, Type::tchar->arrayOf());
    }
    if (varType)
    {
        if (ArrayType* aType = varType->isArrayType())
        {
            SliceType* slit = aType->getSliceType(loc);
            if (slit && !aType->isSliceType())
            {
                if (isRef)
                {
                    Variable* v = e.isVariable();
                    assert(v);
                    v->setType(slit->getType(), loc);
                }
                else
                {
                    irstate.getBlock().add(*new ArrayToSlice(*aType, loc));
                }
            }
        }
    }
}


//Helper called from prepCall to load params on the eval stack.
//In this context "synthetic" means that the method call was
//inserted by the front-end; example: _adSort, _adSortChar
static elem* loadParam(IRState& irstate,
                       Expression& exp,
                       Method& method,
                       bool synthetic,
                       Variable** var = NULL
                       )
{
    elem* e = &exprToElem(irstate, exp);
    TYPE* vType = NULL;
    bool isRef = false;

    if (Variable* v = e->isVariable())
    {
        if (var)
        {
            if (*var && (*var)->getDecl().isRef()
                //'this' is passed by reference for structs,
                // I am not interested in this case
                //&& (*var)->getDecl().ident != Id::This
                && v->getDecl().ident != Id::This)
            {
                isRef = true;
            }
            *var = v;
        }
        vType = &toILType(exp.loc, v->getType());
        if (ASlice* slice = e->isASlice())
        {
            ArrayType& aType = getArrayType(*vType);
            irstate.getBlock().add(*NewSlice::create(aType));
            vType = aType.getSliceType(exp.loc);

            if (!synthetic && !method.convertArrays())
            {
                if (SliceType* sliceType = vType->isSliceType())
                {
                    WARNING(exp.loc, "converting slice to temporary array");
                    sliceToTempArray(irstate, exp.loc, *sliceType, v, true);
                }
                else
                {
                    error(exp.loc, "cannot convert slice to array");
                }
            }
        }
        else
        {
            if (synthetic || isRef || method.convertArrays())
            {
                loadExp(irstate, exp, *v, isRef);
            }
            else
            {
                if (SliceType* sliceType = loadArray(irstate, *v))
                {
                    if (!v->isPartialResult()) //loadArray handles this case internally
                    {
                        warning("%s: converting slice to temporary array", exp.loc.toChars());
                        sliceToTempArray(irstate, exp.loc, *sliceType, v);
                    }
                }
                else
                {
                    assert(!method.funcDecl().isImport());
                }
            }
        }
        if (vType->isStringType() && isSysProperty(method))
        {
            method.optimizeOut();
        }
    }
    else if (StringLiteral* lit = e->isStringLiteral())
    {
        if (isSysProperty(method))
        {
            lit->setEncoding(e_SystemString);
            method.optimizeOut();
        }
    }
    else
    {
        vType = &toILType(exp.loc, exp.type);

        if (vType->isDelegateType() && e->isMethod())
        {
            irstate.add(*new Delegate(*vType));
        }
    }
    if (method.convertArrays())
    {
        convertArrayToSliceIfNeeded(irstate, exp.loc, vType, *e, isRef);
    }
    return e;
}


static bool isObjectArray(Type* type)
{
    if (type->ty == Tarray)
    {
        type = static_cast<TypeDArray*>(type)->next;
        return type && type->implicitConvTo(ClassDeclaration::object->type);
    }
    return false;
}


//Check whether this is front-end synthesized call to _aaApply or _aaApply2
static inline int isAssocArrayForeach(Method& method)
{
    Declaration& decl = method.getDecl();
    const char* name = decl.toChars();
    if (strncmp(name, "_aaApply", 8) == 0)
    {
        if (name[8] == 0)
        {
            return 1;
        }
        else if (name[8] == '2' && name[9] == 0)
        {
            return 2;
        }
    }
    return 0;
}


///<summary>
/// Check for array property functions, sort, reverse, dup, etc
/// and mark the method accordingly
///</summary>
static bool isArrayProperty(Method& method)
{
    if (method.isAssocArrayProp())
    {
        return false;
    }
    Declaration& decl = method.getDecl();

    if (decl.ident == Id::adDup)
    {
        method.setSynthProp(prop_Dup);
    }
    else if (decl.ident == Id::adReverse)
    {
        method.setSynthProp(prop_Reverse);
    }
    else 
    {
        const char* name = decl.toChars();

        if (strcmp(name, "_adSort") == 0
         || strcmp(name, "_adSortChar") == 0
         || strcmp(name, "_adSortWchar") == 0)
        {
            method.setSynthProp(prop_Sort);
        }
        else if (strcmp(name, "_adReverseChar") == 0
              || strcmp(name, "_adReversWchar") == 0)
        {
            method.setSynthProp(prop_Reverse);
        }
    }
    return method.isSynthProp();
}


///<summary>
/// Check for associative array property functions, length,
/// and mark the method accordingly
///</summary>
static bool isAArrayProperty(Method& method)
{
    Declaration& decl = method.getDecl();
    const char* name = decl.toChars();
    bool result = false;

    if (strcmp(name, "_aaLen") == 0)
    {
        method.setSynthProp(prop_Len, true);
        result = true;
    }
    else if (strcmp(name, "_aaKeys") == 0)
    {
        method.setSynthProp(prop_Keys);
        result = true;
    }
    else if (strcmp(name, "_aaValues") == 0)
    {
        method.setSynthProp(prop_Values);
        result = true;
    }
    return result;
}


//Get the n-th param of a method
static Variable* getParam(Method& method, unsigned index, unsigned& offs)
{
    Variable* param = NULL;
    block& args = method.getArgs();
    if (!args.empty())
    {
        param = args[index + offs]->isVariable();
        if ((index == 0) && (param->getDecl().ident == Id::This))
        {
            offs = 1;
            param = args[index + offs]->isVariable();
        }
    }
    return param;
}


static void copyClosure(block& blk, Method& method)
{
    if (elem* e = blk.back())
    {
        if (Method* m = e->isMethod())
        {
            if (Closure* ctxt = m->getClosure(false))
            {
                method.setClosure(new Closure(*ctxt));
            }
        }
    }
}


static void addForeachDelegate(Method& method, 
                               block& blk,
                               auto_ptr<elem> foreachDelegate
                               )
{
    copyClosure(blk, method);
    
    if (foreachDelegate.get())
    {
        // is there a delegate on the stack already?
        if (Delegate* d = DEREF(blk.back()).isDelegate())
        {
            // use the Callback delegate type defined externally
            // in runtime.cs, no need to emit a delegate type
            DEREF(d->getType()).setExternal();
            blk.pop_back();
        }
        blk.add(foreachDelegate);
    }
}


/************************************************************************
 * Prepare function call arguments.
 */
Method& IRState::prepCall(Loc& loc, Expressions* arguments, Symbol* sym)
{
    if (!sym)
    {
        BackEnd::fatalError(loc, "expression does not evaluate to Symbol");
    }
    Method* method = sym->isMethod();
    if (!method)
    {
        BackEnd::fatalError(loc, "callee is not a method");
    }
    if (Variable* deleg = sym->isVariable())
    {
        deleg->load(getBlock());
    }
    inCall_ = method;
    if (method->isNested())
    {
        loadClosure(getBlock(), loc, method);
    }

    method->getProto(true); // recompute function signature
    const bool isArrayProp = isArrayProperty(*method);
    const int isForeach = isArrayProp ? 0 : isAssocArrayForeach(*method);
    const bool isAArrayProp = isAArrayProperty(*method);

    auto_ptr<elem> foreachDelegate;
    unsigned offs = 0, argIndex = 0;

    //loop through the call arguments and generate the code for each expression
    ArrayAdapter<Expression*> args(arguments);
    for (ArrayAdapter<Expression*>::iterator i = args.begin(); i != args.end(); ++i)
    {
        assert(argIndex < args.size());

        Variable* param = getParam(*method, argIndex, offs);
        Type* type = DEREF(*i).type;
        if (argIndex++ == method->vaOffs())
        {
            if ((args.size() != argIndex + 1) || !isObjectArray(type))
            {
                VarArgs* va = VarArgs::create(*this, arguments, loc, argIndex);
                getBlock().add(*va);
                break;
            }
        }
        elem* e = loadParam(*this, DEREF(*i), *method, isArrayProp, &param);
        // array properties (such as dup) and variadic functions may take typeinfo args
        // that 1) are not used in the .net back-end 2) don't fit the function prototype
        if (param && param->isTypeInfo() && (isArrayProp || method->vaOffs() >= 0))
        {
            --argIndex;
            getBlock().pop_back(); // remove tuple type info
            continue;
        }
        /**************************************************************
         * Check for front-end synthesized calls to special array funcs
         */
        if (isForeach)
        {
            if (argIndex == 1)
            {
                foreachDelegate.reset(setForeachSignature(*method, loc, type, isForeach));
            }
        }
        else if (isArrayProp)
        {
            if (argIndex == 1) setArrayPropSignature(*this, *method, loc, param, type, e);
            if (argIndex >= method->getArgCount()) break; // discard extra params
        }
        else if (isAArrayProp && (argIndex == 1))
        {
             setAArrayPropSignature(*this, *method, loc, type);
             break;
        }
    }
    if (isForeach)
    {
        addForeachDelegate(*method, getBlock(), foreachDelegate);
    }
    return *method;
}
