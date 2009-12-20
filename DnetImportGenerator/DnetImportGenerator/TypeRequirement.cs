/********************************************************************
 *       This file is distributed under the Microsoft Public        *
 *       License (Ms-PL). See license.txt for details.              *
 *       Copyright (c) 2009 Tim Matthews & Cristian L. Vlasceanu    *
 *       All rights reserved                                        *
 * *****************************************************************/

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Reflection;

namespace DnetImportGenerator
{
    class TypeRequirement
    {
        Type type = null;
        List<Type> requirements = new List<Type>();

        public TypeRequirement(Type t, NS ns)
        {
            type = t; //set the type
            findReq(); //find types that it depends on
            foreach (Type req in requirements) //add the import declarations for each requirement to my namespace
            {
                if ((req.Namespace != null) && (req.Namespace != type.Namespace) && (!Generator.isPrimitive(req)) && req.IsVisible)
                    ns.getCreateDImport(req.Namespace);
            }
        }

        void findReq()
        {
            if (type.IsSubclassOf(typeof(MulticastDelegate)))
            {
                MethodInfo mi = type.GetMethod("Invoke");
                AddReq(mi.ReturnType);
                ParameterInfo[] pars = mi.GetParameters();
                foreach (ParameterInfo pi in pars)
                {
                    AddReq(pi.ParameterType);
                }
            }
            else if (type != typeof(Object))
            {
                if ((type.BaseType != null) && (type.BaseType != typeof(Object)))
                    AddReq(type.BaseType);

                Type[] interfaces = type.GetInterfaces();
                foreach (Type iface in interfaces)
                {
                    if (!iface.IsVisible)
                        continue;
                    AddReq(iface);
                }

                MemberInfo[] members = type.GetMembers(Generator.memberBindingFlags());
                foreach (MemberInfo member in members)
                {
                    if (member is MethodBase)
                    {
                        ParameterInfo[] pars = ((MethodBase)member).GetParameters();
                        foreach (ParameterInfo pi in pars)
                        {
                            AddReq(Generator.elementType(pi.ParameterType));
                        }
                        if (member.MemberType != MemberTypes.Constructor)
                            AddReq(((MethodInfo)member).ReturnType);
                    }
                }
            }

        }

        void AddReq(Type t)
        {
            if (!requirements.Contains(t))
            {
                if (t.IsArray)
                {
                }
                else if (t.IsPointer)
                {
                }
                else
                    requirements.Add(t);
            }
        }
    }
}
