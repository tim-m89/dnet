#pragma once
//
// $Id: externassembly.h 24923 2009-08-06 04:26:28Z unknown $
//
// Copyright (c) 2009 Cristian L. Vlasceanu
//
#include <cassert>
#include <string>

class ExternAssembly
{
    std::string name_;
    std::string version_;
	std::string token_;

public:
	explicit ExternAssembly(const std::string& name, const std::string& version, const std::string& token) : name_(name), version_(version), token_(token)
    {
    }
    explicit ExternAssembly(const char* name)
    {
        assert(name);
        name_.assign(name);
    }
    const std::string& name() const
    {
        return name_;
    }
    const std::string& version() const
    {
        return version_;
    }
	std::string toString() const;
};


bool operator<(const ExternAssembly&, const ExternAssembly&);

