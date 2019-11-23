/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2019, Arthus Leroy <arthus.leroy@epita.fr>
 * On https://github.com/arthus-leroy/Python
 * All rights reserved. */

# pragma once

// WARNING: requires use of pkg-config (pkg-config --cflags --libs python3)
# include <Python.h>

# include <iostream>
# include <memory>
# include <cassert>

# ifndef NDEBUG
//# define PYDEBUG
# endif

/** TODO
 *
 *  Copy all the methods of Python in Proxy in the Pythonway than operator[] and remove get()
 *  Move GetAttr and SetAttr to their respective functions ?
 *  Add more types to constructor, like vector, map
 *  Add more types to convert from Python
 *  Quote name in operator[] for access with string ?
 *  add operator() instead of call(args, kwargs)
 *
 *  Replace the exits by exceptions
 */

/** FIXME
 *
 *  No pending issue
 */
struct PyRef
{
    using ref_counter_t = std::shared_ptr<std::size_t>;

    PyRef(PyObject* ptr, ref_counter_t counter, const std::string& name, const bool is_ref)
        : ptr(ptr), counter(counter), name(name), is_ref(is_ref)
    {
        // mute NULL INCREF
        if (ptr)
        {
            if (counter)
                (*counter)++;

            # ifdef PYDEBUG
            std::cerr << "Incref of " << name << std::endl;
            # endif
        }
    }

    PyRef(PyObject* ptr, const std::string& name, const bool is_ref)
        : PyRef(ptr, std::make_shared<std::size_t>(), name, is_ref)
    {}

    PyRef(void)
        : ptr(nullptr), counter(nullptr), name("NULL"), is_ref(false)
    {}

    PyRef(const PyRef& o)
        : PyRef(o.ptr, o.counter, o.name, o.is_ref)
    {}

    PyRef& operator=(const PyRef& o)
    {
        ptr     = o.ptr;
        counter = o.counter;
        name    = o.name;
        is_ref  = o.is_ref;

        if (counter)
        {
            (*counter)++;

            # ifdef PYDEBUG
            std::cerr << "Incref of " << name << std::endl;
            # endif
        }

        return *this;
    }

    operator PyObject*()
    {
        return ptr;
    }

    // WARNING: bool is famous for creating overloading problems as it converts to int
    operator bool() const
    {
        return ptr;
    }

    // "counter" check are here to remove log of NULL incref and decref
    ~PyRef()
    {
        if (counter)
            (*counter)--;

        # ifdef PYDEBUG
        if (counter)
        {
            std::cerr << "Decref of " << name;
            std::cerr << " (" << *counter << " instances remaining)";
            std::cerr << std::endl;
        }
        # endif

        if (is_ref && *counter == 0)
        {
            Py_DECREF(ptr);

            # ifdef PYDEBUG
            std::cerr << "Destruction of " << name << std::endl;
            # endif
        }
    }

    PyObject* ptr;
    ref_counter_t counter;
    std::string name;
    bool is_ref;
};

class Python
{
public:
    using utf32_t = std::basic_string<int32_t>;
    using utf16_t = std::basic_string<int16_t>;
    using utf8_t = std::basic_string<int8_t>;
    enum class Type
    {
        Object,
        Dict,
        Sequence,   // list, array
        Function,
    };

private:
    static void initialize()
    {
        if (initialized_ == false)
        {
            Py_Initialize();
            initialized_ = true;
        }

        instances_++;
    }

    static void release()
    {
        instances_--;
    }

    static void err(const char* func)
    {
        if (PyErr_Occurred())
        {
            std::cerr << "\nIn function \"" << func << "\":" << std::endl;
            PyErr_Print();
            exit(EXIT_FAILURE);
        }
    }

    static std::string to_string(const char* s) { return s; }
    static std::string to_string(const std::string& s) { return s; }
    static std::string to_string(const PyRef& s) { return s.name; }
    static std::string to_string(Python& s) { return s.name(); }
    static std::string to_string(nullptr_t) { return "NULL"; }
    template <typename T>
    static std::string to_string(const T t) { return std::to_string(t); }

    template <typename key_t>
    class PyIndexProxy
    {
        /// Force convertion to Python (parent class)
        Python parent()
        {
            return *this;
        }

    public:
        PyIndexProxy(PyRef& object, const Type type, const key_t& key)
            : object_(object), type_(type), key_(key)
        {
            // restrict types (until constraints in C++20)
            static_assert(std::is_same<key_t, std::string>::value
                       || std::is_same<key_t, char*>::value
                       || std::is_convertible<key_t, PyObject*>::value
                       || std::is_convertible<key_t, int>::value);
        }

        /// Implicit convertion to Python
        operator Python()
        {
            PyObject* ret = nullptr;

            switch (type_)
            {
                case Type::Object:  ret = PyObject_GetAttr(object_, Python(key_)); break;
                // check for presence (as it's not checked ?)
                case Type::Dict:    ret = PyDict_GetItemWithError(object_, Python(key_)); break;
                case Type::Sequence:
                    if constexpr(std::is_convertible<key_t, Py_ssize_t>::value)
                        ret = PySequence_GetItem(object_.ptr, key_);
                    else
                        PyErr_SetString(PyExc_TypeError, "list indices must be integers or slices");
                    break;
                case Type::Function: PyErr_SetString(PyExc_TypeError, "'function' object is not subscriptable"); break;
            }
            err("assign");

            return Python(ret, object_.name + "[" + to_string(key_) + "]", false);
        }

        bool operator=(PyObject* object)
        {
            bool ret = false;
            auto key = Python(key_);

            switch (type_)
            {
                case Type::Object:  ret = PyObject_SetAttr(object_.ptr, Python(key_), object); break;
                case Type::Dict:    ret = PyDict_SetItem(object_.ptr, Python(key_), object); break;
                case Type::Sequence:
                    if constexpr(std::is_convertible<key_t, Py_ssize_t>::value)
                        ret = PySequence_GetItem(object_.ptr, key_);
                    else
                        PyErr_SetString(PyExc_TypeError, "list indices must be integers or slices");
                    break;
                case Type::Function: PyErr_SetString(PyExc_TypeError, "'function' object is not subscriptable"); break;
            }
            err("assign");

            return ret;
        }

        bool operator=(Python object)
        {
            // explicit convertion to avoid looping on itself
            return operator=(static_cast<PyObject*>(object));
        }

        template <typename T>
        bool operator=(T t)
        {
            return operator=(Python(t));
        }

    // Since we can't overload "operator.()", we need to duck-type our proxy class
    auto operator[](const std::string& key) { return parent().operator[](key); }
    auto operator[](const Py_ssize_t key) { return parent().operator[](key); }
    auto call(Python args = nullptr, Python kwargs = nullptr)
        { return parent().call(args, kwargs); }
    auto string(void) { return parent().string(); }

    private:
        PyRef object_;
        const Type type_;
        const key_t key_;
    };

public:
    /// Access operators
    # define ACCESS(TYPE) auto operator[](TYPE key) { return PyIndexProxy(ref_, get_type(), key); }
    ACCESS(const std::string&)
    ACCESS(const Py_ssize_t)
    ACCESS(const Python&)

    /// Import the module \var name
    static Python import(const std::string& name)
    {
        initialize();
        auto ret = Python(PyImport_ImportModule(name.c_str()), name, true);
        err("import");
        release();

        return Python(PyModule_GetDict(ret), "module " + name, false);
    }

    /// Evaluate python code
    static Python eval(const std::string& content, int type, Python globals, Python locals)
    {
        PyObject* ret;
        globals["__builtins__"] = PyEval_GetBuiltins();

        ret = PyRun_String(content.c_str(), type, globals, locals);
        err("eval");

        return Python(ret, "eval", true);
    }

private:
    /// Collect the args (values) and put them into the tuple
    template <typename T, typename ...Args>
    static void tuple_collect(PyObject* tuple, Py_ssize_t i, T& item, Args... items)
    {
        // disable DECREF since SetItem steal the reference of obj
        auto obj = Python(item, false);
        PyTuple_SetItem(tuple, i, obj);
        err("tuple_collect");

        if constexpr(sizeof...(Args))
            list_collect(tuple, i + 1, items...);
    }

public:
    /// Create a tuple from all the passed objects
    template <typename ...Args>
    static Python tuple(Args... items)
    {
        auto ptr = PyTuple_New(sizeof...(Args));
        err("args");

        auto tuple = Python(ptr, "tuple", true);

        if constexpr(sizeof...(Args))
            tuple_collect(tuple, 0, items...);

        return tuple;
    }

private:
    /// Collect the args (values) and put them into the list
    template <typename T, typename ...Args>
    static void list_collect(PyObject* list, Py_ssize_t i, T& item, Args... items)
    {
        // disable DECREF since SetItem steal the reference of obj
        auto obj = Python(item, false);
        PyList_SetItem(list, i, obj);
        err("list_collect");

        if constexpr(sizeof...(Args))
            list_collect(tuple, i + 1, items...);
    }

public:
    /// Create a tuple from all the passed objects
    template <typename ...Args>
    static Python list(Args... items)
    {
        auto ptr = PyList_New(sizeof...(Args));
        err("list");

        auto list = Python(ptr, "list", true);

        if constexpr(sizeof...(Args))
            list_collect(list, 0, items...);

        return list;
    }

private:
    /// Collect the args (key, value pairs) and put them into the dict
    template <typename K, typename T, typename ...Args>
    static void dict_collect(PyObject* dict, K& key, T& item, Args... items)
    {
        // disable DECREF since SetItem steal the reference of obj
        auto obj = Python(item, false);
        auto k = Python(key);
        PyDict_SetItem(dict, k, obj);
        err("dict_collect");

        if constexpr(sizeof...(Args))
            dict_collect(dict, items...);
    }

public:
    /// Create a tuple from all the passed objects
    template <typename ...Args>
    static Python dict(Args... items)
    {
        auto ptr = PyDict_New();
        err("dict");

        auto dict = Python(ptr, "dict", true);

        if constexpr(sizeof...(Args))
            dict_collect(dict, items...);

        return dict;
    }

    /// Create Python value from format
    template <typename ...Args>
    static Python build_format(const std::string& format, Args... args)
    {
        auto ret = Py_BuildValue(format.c_str(), args...);
        err("build_format");

        return Python(ret, "built_value \"" + format + "\"", true);
    }

    /// Create Python String from format (printf-like)
    template <typename ...Args>
    static Python format(const std::string& format, Args... args)
    {
        initialize();
        const auto ret = PyUnicode_FromFormat(format.c_str(), args...);
        err("Python");
        release();

        return Python(ret, "format \"" + format + "\"", true);
    }

    /// convertion constructors
    # define PYWRAPPER(TYPE, EXPR)                  \
        Python(TYPE t, const bool is_ref = true) \
        {                                           \
            initialize();                           \
                                                    \
            PyObject* ret = EXPR;                   \
            err("Python");                       \
                                                    \
            release();                              \
                                                    \
            ref_ = PyRef(ret, "built", is_ref);     \
        }

    // string
             PYWRAPPER(const std::string&, PyUnicode_FromString(t.c_str()))
    // floatant
    explicit PYWRAPPER(const float, PyFloat_FromDouble(t))
    explicit PYWRAPPER(const double, PyFloat_FromDouble(t))
    // unsigned
    explicit PYWRAPPER(const std::size_t, PyLong_FromSize_t(t))
    // signed
    explicit PYWRAPPER(const int, PyLong_FromLong(t))
    explicit PYWRAPPER(const long int, PyLong_FromLong(t))
    explicit PYWRAPPER(const long long int, PyLong_FromLongLong(t))

/*    /// Convert a C++ type to a Python type
    template <typename T, typename V = typename std::remove_const<T>::type>
    static Python build(const T t)
    {
        initialize();

        PyObject* ret = nullptr;
        // string
        if constexpr(std::is_same<V, std::string>::value)
            ret = PyUnicode_FromString(t.c_str());
        // string
        else if constexpr(std::is_same<V, char*>::value)
            ret = PyUnicode_FromString(t);
        // string (should probably set a const* remover)
        else if constexpr(std::is_same<V, const char*>::value)
            ret = PyUnicode_FromString(t);
        // float
        else if constexpr(std::is_floating_point<V>::value)
            ret = PyFloat_FromDouble(t);
        // unsigned int
        else if constexpr(std::is_unsigned<V>::value)
            ret = PyLong_FromSize_t(t);
        // signed int
        else if constexpr(std::is_integral<V>::value)
            ret = PyLong_FromLongLong(t);
        // Python
        else if constexpr(std::is_same<V, Python>::value)
            return t;
        // nullptr
        else if constexpr(std::is_null_pointer<V>::value)
            ret = t;
        else
            assert(false && "No compatible type for Python constructor");
        err("build");

        release();

        return Python(ret, "built", true);
    }*/

    /// Default constructor
    Python(void)
    {}

    /// Default constructor for nullptr (seamlessly pass nullptr)
    Python(nullptr_t)
    {}

    /// Copy constructor
    Python(const Python& o)
        : ref_(o.ref_)
    {}

    Python(PyObject* ptr, const std::string& name, const bool is_ref)
        : ref_(ptr, name, is_ref)
    {}

    /// Copy operator
    Python& operator=(const Python& o)
    {
        ref_ = o.ref_;
        return *this;
    }

    /// Implicit convertion to PyObject*
    operator PyObject*()
    {
        return ref_;
    }

    ~Python(void)
    {
        release();
    }

    /// Release the ressources of the global Python instance
    static void terminate(void)
    {
        assert(instances_ == 0 && "Instances still running, kill them before shutting down");
        Py_Finalize();
        initialized_ = false;
    }

    bool is_valid(void) const
    {
        return ref_;
    }

    std::string name(void) const
    {
        return ref_.name;
    }

    Type get_type(void)
    {
        if (PyDict_Check(ref_))
            return Type::Dict;
        else if (PySequence_Check(ref_))
            return Type::Sequence;
        else if (PyFunction_Check(ref_))
            return Type::Function;
        else
            return Type::Object;
    }

    /*===== object =====*/
    bool hasattr(const std::string& name)
    {
        auto ret = PyObject_HasAttrString(ref_, name.c_str());
        err("hasattr");

        return ret;
    }

    std::size_t size(void) const
    {
        auto ret = PyObject_Size(ref_.ptr);
        err("size");

        return ret;
    }

    /*===== function =====*/
    Python call(Python args = nullptr, Python kwargs = nullptr)
    {
        // args must be at least an empty tupple
        // FYI, not being an iterable object raise a MemoryError
        if (args.is_valid() == false)
            args = list();

        auto ret = PyObject_Call(ref_, args, kwargs);
        err("call");

        return Python(ret, "return of " + name(), true);
    }

    Python call(const std::string& name, Python args = nullptr, Python kwargs = nullptr)
    {
        return operator[](name).call(args, kwargs);
    }

    template <typename ...Args>
    Python call_format(const std::string& format, Args... args) const
    {
        auto py_args = build_value(format, args...);
        return call(py_args);
    }

    /*===== conversions =====*/
    ssize_t to_ssize_t(void)
    {
        auto val = PyNumber_AsSsize_t(ref_, nullptr);
        err("ssize_t");

        return val;
    }

    Py_UCS4* ucs4(void)
    {
        constexpr int len = 200;

        Py_UCS4 buffer[len];
        auto ret = PyUnicode_AsUCS4(ref_, buffer, len, true);
        err("UCS4");

        return ret;
    }

    utf32_t utf32(void)
    {
        utf32_t string;

        for (auto c = ucs4(); *c; c++)
            string.push_back(*c & 0xFFFFFFFF);

        return string;
    }

    utf16_t utf16(void)
    {
        utf16_t string;

        for (auto c = ucs4(); *c; c++)
            string.push_back(*c & 0xFFFF);

        return string;
    }

    utf8_t utf8(void)
    {
        utf8_t string;

        for (auto c = ucs4(); *c; c++)
            string.push_back(*c & 0xFF);

        return string;
    }

    std::string string(void)
    {
        std::string string;

        for (auto c = ucs4(); *c; c++)
            string.push_back(*c & 0xFF);

        return string;
    }

private:
    static inline bool initialized_;
    static inline std::size_t instances_;

    PyRef ref_;

public:
    const static inline PyRef True = PyRef(Py_True, "True", false);
    const static inline PyRef False = PyRef(Py_False, "False", false);
    const static inline PyRef None = PyRef(Py_None, "None", false);
};