// $Id: assocarray.cpp 24625 2009-07-31 01:05:55Z unknown $
// Copyright (c) 2008 Ionut-Gabriel Burete
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
#include "declaration.h"
#include "init.h"
#include "mtype.h"
#include "backend.net/block.h"
#include "backend.net/irstate.h"
#include "backend.net/msil/instruction.h"
#include "backend.net/msil/types.h"
#include "backend.net/msil/variable.h" // for AssocArrayElem

using namespace std;


/****************************************************/
#pragma region NewAssociativeArray

NewAssociativeArray::NewAssociativeArray(const AssocArrayType& type) : type_(type)
{
}


elem* NewAssociativeArray::emitILAsm(wostream& out)
{
    indent(out) << "newobj instance void " << type_.name() << "::.ctor()\n";
    return this; 
}


int32_t NewAssociativeArray::getStackDelta() const 
{
    return 1;
}
#pragma endregion



AssocArrayType::AssocArrayType(Loc& loc, Type& elemType, Type& keyType)
    : loc_(loc)
    , elemType_(elemType)
    , keyType_(keyType)
{
    name_ = "class [mscorlib]System.Collections.Generic.Dictionary`2<";
    name_ += keyTYPE().name();
    name_ += ",";
    name_ += elemTYPE().name();
    name_ += ">";
  
    IF_WIN_DEBUG(OutputDebugStringA((name_ + "\n").c_str()));
}


TYPE& AssocArrayType::elemTYPE() const
{ 
    return toILType(loc_, &elemType_);
}


const char* AssocArrayType::argName() const
{
    return "class [mscorlib]System.Collections.Generic.Dictionary`2<!!0,!!1>";
}


///<summary>
/// When the key in an associative array is a D string we need to change to
/// a System.String, because Equals for arrays is not a lexicographical
/// comparison as the user would expect (D strings do not map automatically
/// to System.String, but to uint8[], because they are UTF8, not UTF16).
///</summary>
static TYPE& getKeyType(Loc& loc, Type& keyType)
{
    if (keyType.isString())
    {
        return TYPE::systemString(loc);
    }
    return toILType(loc, &keyType);
}


TYPE& AssocArrayType::keyTYPE() const
{
    return getKeyType(loc_, keyType_);
}


const char* AssocArrayType::name() const
{
    return name_.c_str();
}


const char* AssocArrayType::getSuffix() const
{
    return name();
}


elem* AssocArrayType::init(IRState& state, VarDeclaration& decl, Expression* exp, elem* e)
{
    assert((decl.init == NULL) == (exp == NULL));
    assert(decl.type);
    assert(decl.type->ctype == this);

    if (e && e->isNewAssociativeArray()) return e;

    // create new array on the top of the stack
    block& newBlock = *state.getBlock().addNewBlock(&decl.loc);
    elem* result = newBlock.add(*NewAssociativeArray::create(*this));
    block& block = state.getBlock();

    if (decl.init)
    {
        //code for expression has already been generated,
        //move the newarr stuff in front on the initializer
        block.swapTail();

        //default array initializer?
        if (isImplicit(*decl.init))
        {
            //do not generate it, the VES does it for us anyway
            block.pop_back();
        }
    }
    return result;
}


AssocArrayType& getAssocArrayType(Loc& loc, Type* type)
{
    AssocArrayType* aaType = toILType(loc, type).isAssociativeArrayType();
    if (!aaType)
    {
        BackEnd::fatalError(loc, "associative array expected");
    }
    return *aaType;
}


static AssocArrayType& getAssocArrayType(Declaration& decl, Type* type, Loc* loc)
{
    if (!loc)
    {
        loc = &decl.loc;
    }
    return getAssocArrayType(*loc, type);
}


/****************************************************/
#pragma region AssocArrayElem

AssocArrayElem::AssocArrayElem(block& blk,
                               Variable& arrayVar,
                               Variable* keyVar,
                               Type& type
                               )
    : BaseArrayElem(blk, arrayVar.getDecl())
    , keyVar_(keyVar)
{
    type_ = &type;
}


/*****************************************************
 * Helpers for generating UTF8 encoding / decoding
 */
namespace {
    class UTF8Encoding : public elem
    {
        elem* emitILAsm(std::wostream& out)
        {
            indent(out) << "call class [mscorlib]System.Text.Encoding "
                           "[mscorlib]System.Text.Encoding::get_UTF8()\n";
            return this;
        }
        int32_t getStackDelta() const { return 1; }
    };


    class EncodingGetString : public elem
    {
        elem* emitILAsm(std::wostream& out)
        {
            indent(out) << "callvirt instance string "
                           "[mscorlib]System.Text.Encoding::GetString(unsigned int8[])\n";
            return this;
        }
        int32_t getStackDelta() const { return -1; }
    };
}


bool AssocArrayElem::convertKey(block& blk, AssocArrayType& aaType, Variable* keyVar, bool needSwap)
{
    bool result = false; //conversion generated?

    if (keyVar && aaType.keyTYPE().isStringType())
    {
        block* keyBlock = needSwap ? blk.addNewBlock() : &blk;
        //do not convert if the key is explicitly of the System.String type
        if (aaType.keyType().isString())
        {
#if 0
            keyVar->store(*keyBlock);
            loadArray(*keyBlock, *keyVar);
#else
            Variable* v = savePartialResult(*keyBlock, keyVar->getDecl(), keyVar->getType(), false);
            keyBlock->add(*new UTF8Encoding);
            loadArray(*keyBlock, *v);
#endif
            keyBlock->add(*new EncodingGetString);
            result = true;
        }
        if (needSwap)
        {
            blk.swapTail();
        }
    }
    return result;
}


AssocArrayType& AssocArrayElem::getAssocArrayType(Loc* loc)
{
    return ::getAssocArrayType(decl_, decl_.type, loc);
}


elem* AssocArrayElem::load(block& b, Loc* loc, bool addr)
{    
    block* blk = b.addNewBlock(loc); // group as one element
    loadRefParent(*this, *blk, loc);
    AssocArrayType& aaType = getAssocArrayType(loc);
    TYPE& type = aaType.elemTYPE();
    TYPE& keyType = aaType.keyTYPE(); 
    convertKey(*blk, aaType, keyVar_);
    AssocArrayElemAccess* inst = AssocArrayElemAccess::load(*blk, type, keyType, loc);
    VarDeclaration* vd = new VarDeclaration(0, &aaType.elemType(), Lexer::idPool("$aaElem"), NULL);
    Variable* tmp = savePartialResult(*blk, *vd, vd->type, *inst, keyVar_, getOwner());
    tmp->bind(getParent());
    tmp->load(b, loc, addr);
    return tmp;
}


elem* AssocArrayElem::store(block& blk, Loc* loc)
{
    loadRefParent(*this, blk, loc);
    blk.swapTail();
    AssocArrayType& aaType = getAssocArrayType(loc);
    TYPE& type = aaType.elemTYPE();
    TYPE& keyType = aaType.keyTYPE();
    convertKey(blk, aaType, keyVar_, true);
    return AssocArrayElemAccess::store(blk, type, keyType, loc);
}


elem* AssocArrayElem::emitILAsm(std::wostream& out)
{
    return NULL;
}

#pragma endregion


AssocArrayElemAccess::AssocArrayElemAccess(block& blk,
                                           Op op,
                                           TYPE& elemType,
                                           TYPE& keyType,
                                           Loc* loc
                                           )
    // the opcode passed to the base class is just for the 
    // getStackDelta() computation, see comments in emitILAsm
    : Instruction(blk, (op == STORE ? IL_stelem_ref : IL_ldelem_ref), loc)
    , op_(op)
    , elemType_(elemType)
    , keyType_(keyType)
{
}


AssocArrayElemAccess* AssocArrayElemAccess::load(block& blk,
                                                 TYPE& elemType,
                                                 TYPE& keyType,
                                                 Loc* loc
                                                 )
{
    return new AssocArrayElemAccess(blk, LOAD, elemType, keyType, loc);
}


AssocArrayElemAccess* AssocArrayElemAccess::store(block& blk,
                                                  TYPE& elemType,
                                                  TYPE& keyType,
                                                  Loc* loc
                                                  )
{
    return new AssocArrayElemAccess(blk, STORE, elemType, keyType, loc);
}


elem* AssocArrayElemAccess::emitILAsm(wostream& out)
{
    if (elemType_.getElemSuffix()[0] == 0)
    {
        throw logic_error("cannot access array elements of type " + string(elemType_.name()));
    }
    indent(out);
    switch(op_)
    {
    case LOAD:
        // the get_Item call pops the hidden `this' and the key, and returns the item,
        // so it has a stack delta of -1, same as ldelem.ref
        out << "call instance !1 class [mscorlib]System.Collections.Generic.Dictionary`2<"
            << keyType_.name() << "," << elemType_.name()
            << ">::get_Item(!0)" << endl;
        break;

    case STORE:
        // set_Item pops `this', the key, and the value; returns void;
        // it has a stack delta of -3, same as stelem.ref
        out << "call instance void class [mscorlib]System.Collections.Generic.Dictionary`2<"
            << keyType_.name() << "," << elemType_.name()
            << ">::set_Item(!0,!1)" << endl;
        break;
    }
    return this;
}


elem* ArrayElemAccess::load(block& block, 
                            Variable& arrayElem,
                            Loc* loc,
                            bool loadAddr
                            )
{
    if (!loc)
    {
        loc = &block.loc();
    }
    const TYPE& elemTYPE = toILType(*loc, arrayElem.getType());
    return new ArrayElemAccess(block, LOAD, elemTYPE, loc, loadAddr);
}
