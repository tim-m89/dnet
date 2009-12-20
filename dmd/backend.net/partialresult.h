#pragma once
//
// $Id: partialresult.h 368 2009-05-07 15:57:18Z cristiv $
//
#include "backend.net/msil/variable.h"

class AssocArrayElemAccess;

struct IRState;
struct VarDeclaration;
struct block;
struct Type;


class PartialResult : public Variable
{
public:
    PartialResult(VarDeclaration& decl, Type* type);
    virtual void suppress() { }
    PartialResult* isPartialResult() { return this; }
};


///<summary>
/// Wrapping a function result with this utility ensures that
/// we pop it off the stack if it is never referenced.
/// This function also stores the result into a temporary,
/// in case we need to use it later.
///</summary>
Variable* savePartialResult(block&,
                            VarDeclaration&,
                            Type*,
                            bool optimize = true
                            );

Variable* savePartialResult(IRState&,
                            VarDeclaration&,
                            Type* = NULL,
                            bool optimize = true
                            );

Variable* savePartialResult(block&, 
                            VarDeclaration&,
                            Type*,
                            AssocArrayElemAccess&,
                            Variable* keyVar,
                            block*
                            );

