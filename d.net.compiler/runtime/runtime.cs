// $Id: runtime.cs 358 2009-05-01 01:01:37Z cristiv $
// Copyright(c) 2008, 2009 Cristian L. Vlasceanu
// Copyright (c) 2008, 2009 Ionut-Gabriel Burete

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

// Runtime support for the D.NET compiler.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace core
{
    /* This is D's root Object definition in druntime/import/object.di
    class Object
    {
        string   toString();
        hash_t   toHash();
        int      opCmp(Object o);
        equals_t opEquals(Object o);

        interface Monitor
        {
            void lock();
            void unlock();
        }

        static Object factory(string classname);
    }
    */

    public class Object : IDisposable
    {
        private bool isDeleted = false;

        //overriden by dtors defined (D language-written) in derived classes
        public virtual void __dtor()
        {
            if (isDeleted) throw new AssertError("double deletion");
            isDeleted = true;
        }
        ~Object()
        {
            __dtor();
        }
        public void Dispose()
        {
            __dtor();
            GC.SuppressFinalize(this);
        }
        public override int GetHashCode()
        {
            return toHash();
        }
        public override bool Equals(object o)
        {
            return opEquals(o as core.Object);
        }
        public virtual byte[] toString()
        {
            return System.Text.Encoding.UTF8.GetBytes(ToString());
        }
        public virtual int toHash()
        {
            return base.GetHashCode();
        }
        public virtual int opCmp(object o)
        {
            return GetHashCode() - o.GetHashCode();
        }
        public virtual bool opEquals(core.Object o)
        {
            return base.Equals(o);
        }
        public virtual bool opEquals(object o)
        {
            return base.Equals(o);
        }
        public static Object factory(string classname)
        {
            System.Console.WriteLine("[dnetlib]core.Object.factory({0}): not implemented", classname);
            return null;
        }
    }


    /********************************************
    /* D's exception classes

    class Throwable : Object
    {
        interface TraceInfo
        {
            int opApply(int delegate(inout char[]));
            string toString();
        }

        string      msg;
        string      file;
        size_t      line;
        TraceInfo   info;
        Throwable   next;

        this(string msg, Throwable next = null);
        this(string msg, string file, size_t line, Throwable next = null);
        override string toString();
    }


    class Exception : Throwable
    {
        this(string msg, Throwable next = null);
        this(string msg, string file, size_t line, Throwable next = null);
    }


    class Error : Throwable
    {
        this(string msg, Throwable next = null);
        this(string msg, string file, size_t line, Throwable next = null);
    }
    ********************************************/

    public class Throwable : Object
    {
        public delegate int Callback(byte[] msg);

        public interface TraceInfo
        {
            int opApply(Callback cb);
            byte[] toString();
        }

        byte[] msg;
        byte[] file;
        uint line;
        //TraceInfo info;
        Throwable next;

        public Throwable(byte[] what, Throwable nextThrowable)
        {
            msg = what;
            next = nextThrowable;
        }
        public Throwable(byte[] what, byte[] fileName, uint lineNum, Throwable nextThrowable)
            : this(what, nextThrowable)
        {
            file = fileName;
            line = lineNum;
        }
        public override byte[] toString()
        {
            if (file != null)
            {
                System.Text.Encoding utf8 = System.Text.Encoding.UTF8;
                string str = String.Format("{0}:{1} {2}", utf8.GetString(file), line, utf8.GetString(msg));
                return utf8.GetBytes(str);
            }
            return msg;
        }
        public override String ToString()
        {
            System.Text.Encoding utf8 = System.Text.Encoding.UTF8;
            if (file != null)
            {
                string str = String.Format("{0}:{1} {2}", utf8.GetString(file), line, utf8.GetString(msg));
                return str;
            }
            return utf8.GetString(msg);
        }
    }


    public class Exception : Throwable
    {
        public Exception(byte[] msg, Throwable next)
            : base(msg, next)
        {
        }
        public Exception(byte[] msg, byte[] file, uint line, Throwable next)
            : base(msg, file, line, next)
        {
        }
        //for compiler use
        public Exception(string msg)
            : this(System.Text.Encoding.UTF8.GetBytes(msg), null)
        {
        }
    }


    public class Error : Exception
    {
        public Error(byte[] msg, Throwable next)
            : base(msg, next)
        {
        }
        public Error(byte[] msg, byte[] file, uint line, Throwable next)
            : base(msg, file, line, next)
        {
        }
        // for compiler use
        public Error(string msg)
            : base(System.Text.Encoding.UTF8.GetBytes(msg), null)
        {
        }
    }


    ///<summary>
    /// Specific exception class, not catchable by D code.
    ///</summary>
    public class AssertError : System.Exception
    {
        public AssertError(string msg)
            : base(msg)
        {
        }
    }


    public class HiddenFuncError : Error
    {
        public HiddenFuncError(string msg)
            : base("hidden method call: " + msg)
        {
        }
    }


    ///<summary>
    ///Wrap a System.Exception-based object into a D error.
    ///</summary>
    public class ForeignError : Error
    {
        private System.Exception ex;

        public ForeignError(string msg, System.Exception ex)
            : base(msg)
        {
            this.ex = ex;
        }
        public System.Exception Exception
        {
            get { return this.ex; }
        }
        ///<summary>
        /// Wrap a System.Exception into a ForeignError object;
        /// it cannot be thrown here because C# forbids throwing
        /// things not derived from System.Exception. 
        /// The object is returned on the top of the IL stack.
        /// The compiler generates a throw statement which works
        /// because IL does not have C#'s restriction.
        ///</summary>
        static public object Wrap(System.Exception ex)
        {
            if (ex is AssertError)
            {
                return ex; // assertion are not catchable
            }
            return new ForeignError(ex.Message, ex);
        }
    }

    public class SwitchError : Error
    {
        public SwitchError()
            : base("Switch Error")
        {
        }
    }

    /* D Type Info
    struct OffsetTypeInfo
    {
        size_t   offset;
        TypeInfo ti;
    }

    class TypeInfo
    {
        hash_t   getHash(in void* p);
        equals_t equals(in void* p1, in void* p2);
        int      compare(in void* p1, in void* p2);
        size_t   tsize();
        void     swap(void* p1, void* p2);
        TypeInfo next();
        void[]   init();
        uint     flags();
        // 1:    // has possible pointers into GC memory
        OffsetTypeInfo[] offTi();
        void destroy(void* p);
        void postblit(void* p);
    }
    */
    public class TypeInfo : System.Reflection.TypeDelegator
    {
        public TypeInfo(Type type) : base(type)
        {
        }
    }
} // namespace core


namespace runtime
{
    /// <summary>
    /// Implement stuff declared in dnet.d
    /// </summary>
    public class dnet
    {
        static public string sys(byte[] bytes)
        {
            return System.Text.Encoding.UTF8.GetString(bytes);
        }
        
        static public byte[] toBytes(object obj)
        {
            return System.Text.Encoding.UTF8.GetBytes(obj.ToString());
        }

        static public Byte[][] strArrayToByteArrayArray(string[] str)
        {
            System.Text.ASCIIEncoding encoding = new System.Text.ASCIIEncoding();
            int size = str.Length;

            byte[][] result = new byte[size][];
            for (int i = 0; i < size; ++i)
            {
                result[i] = encoding.GetBytes(str[i]);
            }
            return result;
        }

        //wrap type information into a class that D understands
        static public core.TypeInfo _argtype(object o)
        {
            return new core.TypeInfo(o.GetType());
        }

        /// <summary>
        /// Helper for copying structs, in the non VALUETYPE_STRUCT implementation.
        /// </summary>
        /// <param name="dest">destination struct</param>
        /// <param name="src">source struct; we assume both objects are of the same type.</param>
        /// <returns></returns>
        static public void blit(Object dest, Object src)
        {
            int size = Marshal.SizeOf(src.GetType());
            IntPtr p = Marshal.AllocHGlobal(size);
            try
            {
                Marshal.StructureToPtr(src, p, false);
                Marshal.PtrToStructure(p, dest);
            }
            finally
            {
                Marshal.FreeHGlobal(p);
            }
        }
    }

    /// <summary>
    /// Runtime helper for operations on arrays and slices.
    /// </summary>
    /// NOTE: the code is a bit schizophrenic here, in the sense that
    /// it is not clear what type should be used for array indexing and
    /// traversal, 32 or 64-bit integers? In a conversation that I had
    /// with Walter at Kahili in Kirkland, he recommended
    /// to do int32 and damn users silly enough to expect contiguous 
    /// memory areas larger than 4G! Which I tend to agree with, but I 
    /// am still hoping for a way to automagically select the best type.
    /// One valid use case for 64-bit indexing is with memory mapped files,
    /// but I am not sure that can even be done in a verifiable way in .NET
    /// More research needed here.
    ///
    /// Also: should we fold Slice and Array (see below) into one class?
    ///
    public class Slice
    {
        static public object Fill(System.Array a, System.Array slice)
        {
            long srcLen = slice.GetLongLength(0);
            long destLen = a.GetLongLength(0);
            for (long i = 0; (i < srcLen) && (i < destLen); ++i)
            {
                a.SetValue(slice.GetValue(i), i);
            }
            return a;
        }
        static public object Fill(System.Array a, object value)
        {
            for (long i = 0, len = a.GetLongLength(0); i != len; ++i)
            {
                a.SetValue(value, i);
            }
            return a;
        }
        static public object Fill(System.Array a, int lwr, int upr, object value)
        {
            for (long i = lwr, len = a.GetLongLength(0); i < len && i < upr; ++i)
            {
                a.SetValue(value, i);
            }
            return a;
        }
        static public object Fill(System.Array a, object[] values)
        {
            for (long i = 0; i != values.LongLength; ++i)
            {
                a.SetValue(values[i], i);
            }
            return a;
        }
        static public object Fill(System.Array a, int lwr, int upr, object[] values)
        {
            for (long i = lwr, len = a.GetLongLength(0), j = 0;
                 i < len && i < upr && j < values.LongLength;
                 ++i, ++j)
            {
                a.SetValue(values[j], i);
            }
            return a;
        }
        static public object Fill<T>(System.Array a, T value)
        {
            for (long i = 0, len = a.GetLongLength(0); i != len; ++i)
            {
                a.SetValue(value, i);
            }
            return a;
        }
        static public object Fill<T>(System.Array a, T[] values)
        {
            for (long i = 0; i != values.LongLength; ++i)
            {
                a.SetValue(values[i], i);
            }
            return a;
        }
        static public object Fill<T>(System.Array a, int lwr, int upr, T value)
        {
            for (long i = lwr, len = a.GetLongLength(0); i < len && i < upr; ++i)
            {
                a.SetValue(value, i);
            }
            return a;
        }
        static public object Fill<T>(System.Array a, int lwr, int upr, T[] values)
        {
            for (long i = lwr, len = a.GetLongLength(0), j = 0;
                 i < len && i < upr && j < values.LongLength;
                 ++i, ++j)
            {
                a.SetValue(values[j], i);
            }
            return a;
        }
        static public System.ArraySegment<T> Create<T>(T[] a, int lwr, int upr)
        {
            int count = System.Math.Min(upr - lwr, a.Length - lwr);
            //The ctor of ArraySegment throws an exception if upr is out of
            //array bounds. Andrei says that in D, taking a slice past the
            //end of the array is undefined behavior, so we might as well
            //let it throw... I'll think about it some more
            return new System.ArraySegment<T>(a, lwr, count);
        }
        static public void Realloc<T>(int len, ref System.ArraySegment<T> s)
        {
            // make a new array
            T[] a = (T[])s.Array.Clone();
            System.Array.Resize(ref a, s.Offset + len);
            s = new System.ArraySegment<T>(a, s.Offset, len);
        }
        static public void Resize<T>(int len, ref System.ArraySegment<T> s)
        {
            if (s.Array.Length >= s.Offset + len)
            {
                s = new System.ArraySegment<T>(s.Array, s.Offset, len);
            }
            else
            {
                Realloc<T>(len, ref s);
            }
        }
        static public int Size<T>(System.ArraySegment<T> s)
        {
            return s.Count;
        }
        static public System.ArraySegment<T> Concat<T>(System.ArraySegment<T> slice, T[] a)
        {
            int count = slice.Count;
            Realloc<T>(slice.Count + a.Length, ref slice);
            a.CopyTo(slice.Array, slice.Offset + count);
            return slice;
        }
        static public System.ArraySegment<T> Concat<T>(System.ArraySegment<T> slice, T elem)
        {
            int count = slice.Count;
            Realloc<T>(slice.Count + 1, ref slice);
            slice.Array[count] = elem;
            return slice;
        }
        static public System.ArraySegment<T> Concat<T>(System.ArraySegment<T> a1, System.ArraySegment<T> a2)
        {
            int count = a1.Count;
            Realloc<T>(count + a2.Count, ref a1);
            System.Array.Copy(a2.Array, a2.Offset, a1.Array, count, a2.Count);
            return a1;
        }
    } //Slice


    ///<summary>
    ///A collection of helpers for D dynamic and static arrays.
    ///Our D language compiler back-end for .NET generates calls
    ///to the methods of this class.
    ///</summary>
    public class Array
    {
        ///<summary>
        ///Helper for initalizing jagged arrays.
        ///See http://the-free-meme.blogspot.com/2008/12/jagging-away.html for details.
        ///</summary>
        static public void Init<T>(System.Array a, uint[] sizes, int start, int length)
        {
            if (length == 2)
            {
                uint n = sizes[start];
                uint m = sizes[start + 1];
                for (uint i = 0; i != n; ++i)
                {
                    a.SetValue(new T[m], i);
                }
            }
            else
            {
                --length;
                Init<T[]>(a, sizes, start, length);
                uint n = sizes[start];
                for (uint i = 0; i != n; ++i)
                {
                    Init<T>((System.Array)a.GetValue(i), sizes, start + 1, length);
                }
            }
        }

        ///<summary>
        ///Helper for initalizing jagged array, called from the code
        ///generated by our D compiler back-end
        ///</summary>
        static public void Init<T>(System.Array a, uint[] sizes)
        {
            Init<T>(a, sizes, 0, sizes.Length);
        }
        static public T[] Resize<T>(int newSize, T[] array)
        {
            System.Array.Resize(ref array, newSize);
            return array;
        }
        static public T[] Concat<T>(T[] a1, T[] a2)
        {
            long i = (a1 == null) ? 0 : a1.GetLongLength(0);
            T[] a = new T[i + a2.GetLongLength(0)];
            if (a1 != null)
            {
                a1.CopyTo(a, 0);
            }
            a2.CopyTo(a, i);
            return a;
        }
        static public T[] Concat<T>(T[] a1, T elem)
        {
            long len = (a1 == null) ? 0 : a1.GetLongLength(0);
            T[] a = new T[len + 1];
            if (a1 != null)
            {
                a1.CopyTo(a, 0);
            }
            a.SetValue(elem, len);
            return a;
        }
        static public T[] Concat<T>(T[] a1, System.ArraySegment<T> a2)
        {
            long len = a1.GetLongLength(0);
            T[] a = new T[len + a2.Count];
            a1.CopyTo(a, 0);
            System.Array.Copy(a2.Array, a2.Offset, a, len, a2.Count);
            return a;
        }
        static public T[] Reverse<T>(T[] a)
        {
            System.Array.Reverse(a);
            return a;
        }
        static public System.ArraySegment<T> Reverse<T>(System.ArraySegment<T> slice)
        {
            System.Array.Reverse(slice.Array, slice.Offset, slice.Count);
            return slice;
        }
        static public System.ArraySegment<T> Sort<T>(System.ArraySegment<T> slice)
        {
            System.Array.Sort(slice.Array, slice.Offset, slice.Count);
            return slice;
        }
        static public T[] Sort<T>(T[] a)
        {
            System.Array.Sort(a);
            return a;
        }
        static public T[] Clone<T>(T[] a)
        {
            return (T[])a.Clone();
        }
        //static public System.ArraySegment<T> Clone<T>(System.ArraySegment<T> slice)
        static public T[] Clone<T>(System.ArraySegment<T> slice)
        {
            T[] a = new T[slice.Count];
            System.Array.Copy(slice.Array, slice.Offset, a, 0, slice.Count);
            return a;
        }
        static public void Copy(System.Array src, int lower, int upper, System.Array dest)
        {
            for (int i = lower, j = 0; i != upper; ++i, ++j)
            {
                dest.SetValue(src.GetValue(i), j);
            }
        }
        static public void Copy(System.Array src, int srcLower, int srcUpper, System.Array dest, int destLower, int destUpper)
        {
            for (int i = srcLower, j = destLower; i != srcUpper && j != destUpper; ++i, ++j)
            {
                dest.SetValue(src.GetValue(i), j);
            }
        }
#region Array deep comparisons
        static private bool Equals(System.Array a1, System.Array a2, int len, int off1, int off2)
        {
            bool result = true;
            for (int i = 0; i != len; ++i)
            {
                object v1 = a1.GetValue(i + off1);
                object v2 = a2.GetValue(i + off2);
                if (!v1.Equals(v2))
                {
                    result = false;
                    break;
                }
            }
            return result;
        }
        static public bool Equals(System.Array a1, System.Array a2)
        {
            if (a1 == null)
            {
                return a2 == null;
            }
            int len = a1.GetLength(0);
            return (len == a2.GetLength(0)) ? Equals(a1, a2, len, 0, 0) : false;
        }
        static public bool Equals<T>(System.ArraySegment<T> a1, System.Array a2)
        {
            if (a2 == null)
            {
                return a1.Array == null;
            }
            int len = a1.Count;
            return (len == a2.GetLength(0)) ? Equals(a1.Array, a2, len, a1.Offset, 0) : false;
        }
        static public bool Equals<T>(System.ArraySegment<T> a1, System.ArraySegment<T> a2)
        {
            int len = a1.Count;
            return (len == a2.Count) ? Equals(a1.Array, a2.Array, len, a1.Offset, a2.Offset) : false;
        }
#endregion Array deep comparisons

#region Array shallow comparisons
        static private bool Equiv(System.Array a1, System.Array a2, int len, int off1, int off2)
        {
            bool result = true;
            for (int i = 0; i != len; ++i)
            {
                object v1 = a1.GetValue(i + off1);
                object v2 = a2.GetValue(i + off2);
                if (v1 != v2)
                {
                    result = false;
                    break;
                }
            }
            return result;
        }
        static public bool Equiv(System.Array a1, System.Array a2)
        {
            if (a1 == null)
            {
                return a2 == null;
            }
            int len = a1.GetLength(0);
            return (len == a2.GetLength(0)) ? Equiv(a1, a2, len, 0, 0) : false;
        }
        static public bool Equiv<T>(System.ArraySegment<T> a1, System.Array a2)
        {
            if (a2 == null)
            {
                return a1.Array == null;
            }
            int len = a1.Count;
            return (len == a2.GetLength(0)) ? Equiv(a1.Array, a2, len, a1.Offset, 0) : false;
        }
        static public bool Equiv<T>(System.ArraySegment<T> a1, System.ArraySegment<T> a2)
        {
            int len = a1.Count;
            return (len == a2.Count) ? Equiv(a1.Array, a2.Array, len, a1.Offset, a2.Offset) : false;
        }
#endregion Array shallow comparisons
    } //Array


    public class AssocArray
    {
        public delegate int Callback<V>(ref V value);
        public delegate int Callback<K, V>(K key, ref V value);

        ///<summary>
        ///Support foreach(value;myAssocArray) statements in the D language
        ///</summary>
        static public void Foreach<K, V>(Dictionary<K, V> aa, int unused, Callback<V> callback)
        {
            Dictionary<K, V> changed = new Dictionary<K,V>();
            foreach (KeyValuePair<K, V> kvp in aa)
            {
                V temp = kvp.Value;
                int r = callback(ref temp);
                if (!kvp.Value.Equals(temp))
                {
                    changed[kvp.Key] = temp;
                }
                if (r != 0)
                {
                    break;
                }
            }
            foreach (KeyValuePair<K, V> kvp in changed)
            {
                aa[kvp.Key] = kvp.Value;
            }
        }

        ///<summary>
        ///Support foreach(key, value;myAssocArray) statements in the D language
        ///</summary>
        static public void Foreach<K, V>(Dictionary<K, V> aa, int unused, Callback<K, V> callback)
        {
            Dictionary<K, V> changed = new Dictionary<K, V>();
            foreach (KeyValuePair<K, V> kvp in aa)
            {
                V temp = kvp.Value;
                int r = callback(kvp.Key, ref temp);
                if (!kvp.Value.Equals(temp))
                {
                    changed[kvp.Key] = temp;
                }
                if (r != 0)
                {
                    break;
                }
            }
            foreach (KeyValuePair<K, V> kvp in changed)
            {
                aa[kvp.Key] = kvp.Value;
            }
        }


        static public Dictionary<K, V> Init<K, V>(Dictionary<K, V> aa, K[] keys, V[] values)
        {
            for (int i = 0; i != keys.Length; ++i)
            {
                aa.Add(keys[i], values[i]);
            }
            return aa;
        }
    } // AssocArray

} // namespace runtime
