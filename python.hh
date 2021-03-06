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
# include <filesystem>
# include <sstream>

# include <array>
# include <vector>
# include <list>
# include <map>

# include <regex>
# include <locale>
# include <codecvt>

# include "backtrace.hh"

// INCREF/DECREF is any Instance of the object
/// Enable logging of INCREF and DECREF
//# define PYDEBUG_INCREF
//# define PYDEBUG_DECREF

// Construction and Destruction are the creation and release of the Python object
// To be noted, the object can be destroyed, but still held by another object
/// Enable logging of construction and destruction
//# define PYDEBUG_CONST
//# define PYDEBUG_DEST

namespace
{
    [[maybe_unused]] std::string get_typename(const char* s, int skips = 0)
    {
        char* start = const_cast<char*>(s);
        char* end = const_cast<char*>(s);

        for (; *start; start++)
            if (*start == '=')
            {
                if (skips)
                    skips--;
                else
                    break;
            }

        if (*start)
            start++;

        end = start;

        while (*end && *end != ';')
            end++;

        return std::string(start, end);
    }

    # define GET_TYPENAME(I) get_typename(__PRETTY_FUNCTION__, I)

    template <typename T>
    auto is_iterable_type() -> decltype(
        std::begin(std::declval<T&>()) != std::end(std::declval<T&>()), // operator!=
        void(),
        ++std::declval<decltype(std::begin(std::declval<T&>()))&>(),    // operator++
        void(*std::begin(std::declval<T&>())),                          // operator*
        std::true_type{});

    template <typename T>
    using is_iterable = decltype(is_iterable_type<T>());

    std::string escape(const std::string str)
    {
        std::string ret(str);

        for (std::size_t i = 0; i < ret.size(); i++)
            switch (ret[i])
            {
                case '\a':  ret.insert(i, "\\"); ret.replace(++i, 1, "a");  break;
                case '\b':  ret.insert(i, "\\"); ret.replace(++i, 1, "b");  break;
                case '\f':  ret.insert(i, "\\"); ret.replace(++i, 1, "f");  break;
                case '\n':  ret.insert(i, "\\"); ret.replace(++i, 1, "n");  break;
                case '\r':  ret.insert(i, "\\"); ret.replace(++i, 1, "r");  break;
                case '\t':  ret.insert(i, "\\"); ret.replace(++i, 1, "t");  break;
                case '\v':  ret.insert(i, "\\"); ret.replace(++i, 1, "v");  break;
            }

        return ret;
    }

    // disabled as it litteraly *doubles* the duration of the tests
    // const auto kwargs_regex = std::regex("\"([^\"]*)\": ");

    // convert from char_t -> char
    template <typename char_t>
    std::wstring_convert<std::codecvt_utf8<char_t>, char_t> converter;
}

/// Wrapper around PyObject* taking care of INCREF/DECREF and debug
struct PyRef
{
    PyRef(PyObject* ptr, const std::string& name, const bool borrowed = false)
        : ptr(ptr), name(escape(name))
    {
        if (ptr == nullptr)
            return;

        # ifdef PYDEBUG_CONST
        std::cout << "Construction of " << this->name << std::endl;
        # endif

        // increase life duration to outlive it owner
        if (borrowed)
            Py_INCREF(ptr);
    }

    PyRef(void)
        : ptr(nullptr), name("NULL")
    {}

    PyRef(const PyRef& o)
    {
        *this = o;
    }

    PyRef& operator=(const PyRef& o)
    {
        ptr     = o.ptr;
        name    = o.name;

        if (ptr == nullptr)
            return *this;

        Py_INCREF(ptr);

        # ifdef PYDEBUG_INCREF
        std::cout << "Incref of " << name << std::endl;
        # endif

        return *this;
    }

    operator PyObject*() const
    {
        return ptr;
    }

    // WARNING: bool is famous for creating overloading problems as it converts to int
    operator bool() const
    {
        return ptr;
    }

    // NOTE: destruction won't work on borrowed reference with owner still alive
    //       (it will not be logged) like a module's dict or a container's item
    ~PyRef()
    {
        if (ptr == nullptr)
            return;

        # ifdef PYDEBUG_DECREF
            std::cout << "Decref of " << name
                      << " (" << (ptr->ob_refcnt - 1) << " instances remaining)" << std::endl;
        # endif

        # ifdef PYDEBUG_DEST
        if (ptr->ob_refcnt == 1)
            std::cout << "Destruction of " << name << std::endl;
        # endif

        Py_DECREF(ptr);
    }

    PyObject* ptr;
    std::string name;
};

/// Wrapper around PyRef taking care of all the functions and the logic behind
class Python
{
public:
    using utf32_t = std::basic_string<char32_t>;
    using utf16_t = std::basic_string<char16_t>;
//    using utf8_t = std::basic_string<char8_t>;    // C++20
    using utf8_t = std::basic_string<char>;

    enum class Type
    {
        Object,
        Dict,
        Sequence,   // list, array
    };

    /// Error class to indicate any normal error (look at the trace for more info)
    class PythonError : std::exception
    { };

    /// Error class to indicate the end of a "for ... in ..." iteration
    class StopIteration : std::exception
    { };

    // FIXME: refactor the whole class
    /// Intermediary class to recongnize starred expressions
    template <typename T>
    class Starred
    {
    public:
        Starred(T&& ref)
            : ref_(ref)
        {
            static_assert(is_starred<0, Starred<T>>::value);
        }

        Starred(T& ref)
            : ref_(ref)
        {
            static_assert(is_starred<0, Starred<T>>::value);
        }

        // For argument expansion (like tuple(1, 2, *args, 3))
        operator Python()
        {
            Python ref = ref_;

            assert(static_cast<PyObject*>(ref) != nullptr);

            if (PySequence_Check(ref))
                return ref;
            else if (PyDict_Check(ref))
                return ref_.keys();
            else
                throw std::invalid_argument("starred expression must be an iterable");
        }

        auto operator=(Python o)
        {
            return (ref_ = o);
        }

    private:
        T& ref_;
    };

private:
    static void initialize()
    {
        if (initialized_ == false)
        {
            Py_Initialize();
            initialized_ = true;
        }
    }

    static void err(const char* func)
    {
        if (mute_error)
            return;

        if (PyErr_Occurred())
        {
            if (finally_func)
                finally_func();

            std::cerr << "\nIn function \"" << func << "\":" << std::endl;
            PyErr_Print();
            std::cerr << std::endl;

            # ifdef BACKTRACE
                // skip "err" trace
                backtrace(1);
            # endif

            throw PythonError();
        }
    }

    template <typename T>
    static std::string to_string(const T t)
    {
        // char-likes
        if constexpr(std::is_same<T, char>::value || std::is_same<T, wchar_t>::value
                  || std::is_same<T, char16_t>::value || std::is_same<T, char32_t>::value)
            return to_string(std::basic_string<T>(1, t));
        // numbers
        else
        {
            std::stringstream ss;
            ss << t;

            return ss.str();
        }
    }

    // string-likes
    static std::string to_string(const std::string& s)
        { return s; }
    template <typename char_t>
    static std::string to_string(const std::basic_string<char_t>& s)
        { return converter<char_t>.to_bytes(s); }
    template <typename char_t>
    static std::string to_string(const char_t* s)
        { return to_string(std::basic_string<char_t>(s)); }

    // misc
    static std::string to_string(const PyRef& s)
        { return s.name; }
    static std::string to_string(Python& s)
        { return s.name(); }
    static std::string to_string(std::nullptr_t)
        { return "NULL"; }
    static std::string to_string(const std::filesystem::path& s)
        { return s.string(); }

    /// Wrapper around PyRef, taking a key, used to access an element
    template <typename key_t>
    class PyIndexProxy
    {
        /// Force convertion to Python (parent class)
        Python parent()
        {
            return *this;
        }

    public:
        PyIndexProxy(const PyRef& object, const Type type, const key_t& key)
            : object_(object), type_(type), key_(key)
        {
            // restrict types (until constraints in C++20)
            static_assert(std::is_same<key_t, std::string>::value
                       || std::is_same<key_t, char*>::value
                       || std::is_convertible<key_t, PyObject*>::value
                       || std::is_convertible<key_t, int>::value);
        }

        PyIndexProxy(const PyIndexProxy<key_t>& proxy)
            : PyIndexProxy(proxy.object_, proxy.type_, proxy.key_)
        {}

        operator PyObject*()
        {
            assert(object_.ptr != nullptr);

            PyObject* ret = nullptr;

            switch (type_)
            {
                case Type::Object:  ret = PyObject_GetAttr(object_, Python(key_)); break;
                case Type::Dict:
                case Type::Sequence:ret = PyObject_GetItem(object_, Python(key_)); break;
            }
            err("assign");

            return ret;
        }

        // WARNING: as it's a lazy getter, you need to force convertion if you want to keep the object
        /// Implicit convertion to Python
        operator Python()
        {
            PyObject* ret = *this;

            if (type_ == Type::Object)
                return Python(ret, object_.name + "." + to_string(key_), true);
            else if constexpr(std::is_same<key_t, Python>::value)
                return Python(ret, object_.name + "[" + key_.name() + "]");
            else if constexpr(std::is_convertible<key_t, std::string>::value)
                return Python(ret, object_.name + "[\"" + to_string(key_) + "\"]", true);
            else
                return Python(ret, object_.name + "[" + to_string(key_) + "]", true);
        }

        auto& operator=(PyObject* object)
        {
            assert(object_.ptr != nullptr);

            switch (type_)
            {
                case Type::Object:  PyObject_SetAttr(object_, Python(key_), object); break;
                case Type::Dict:
                case Type::Sequence:PyObject_SetItem(object_, Python(key_), object); break;
            }
            err("assign");

            return *this;
        }

        auto& operator=(Python object)
        {
            // explicit convertion to avoid looping on itself
            return operator=(static_cast<PyObject*>(object));
        }

        // FIXME: ambiguous situations, risk of error
        // auto proxy = PyIndexProxy()  -> assignation
        // proxy = PyIndexProxy()       -> item setter
        auto& operator=(PyIndexProxy<key_t> proxy)
        {
            // setter
            if (object_)
                operator=(static_cast<PyObject*>(proxy));
            // assignation
            else
            {
                object_ = proxy.object_;
                type_ = proxy.type_;
                key_ = proxy.key_;
            }

            return *this;
        }

        template <typename T>
        auto& operator=(T t)
        {
            return operator=(Python(t));
        }

        auto name() const
        {
            return object_.name;
        }

        auto key() const
        {
            return key_;
        }

    // Since we can't overload "operator.()", we need to duck-type our proxy class
    auto operator[](const std::string& key) { return parent()[key]; }
    auto operator[](const Py_ssize_t key) { return parent()[key]; }
    auto call(Python args = nullptr, Python kwargs = nullptr)
        { return parent().call(args, kwargs); }
    auto string(void) { return parent().string(); }
    auto is_valid(void) { return parent().is_valid(); }
    auto print(void) { return parent().print(); }
    auto operator()(Python args = nullptr, Python kwargs = nullptr)
    { return parent()(args, kwargs); }

    private:
        PyRef object_;
        Type type_;
        key_t key_;
    };

    template <typename key_t>
    static std::string to_string(PyIndexProxy<key_t>& s) { return s.name() + "[" + to_string(s.key()) + "]"; }

    // Use this with caution, it shouldn't be used in place of another constructor
    // as you can't trace the source
    // Don't use with any Py_* function returning a new reference (leaks)
    Python(PyObject* o)
        : ref_(o, "PyObject*", true)
    {}

public:
    // NOTE: operator[] won't let you access attribute of iterable, you must use attr
    /// Access operators
    # define ACCESS(TYPE) auto operator[](TYPE key) \
    {                                               \
        assert(is_valid());                         \
        return PyIndexProxy(ref_, get_type(), key); \
    }
    ACCESS(const std::string&)
    ACCESS(const Py_ssize_t)
    ACCESS(Python)

    /// Release the ressources of the global Python instance
    static void terminate(void)
    {
        Py_Finalize();
        initialized_ = false;
    }

    /// Disable/Enable error raising
    static void mute_errors(const bool value)
    {
        mute_error = value;
    }

    /// Import the module \var name
    static Python import(const std::string& name)
    {
        initialize();
        auto module = Python(PyImport_ImportModule(name.c_str()), name);
        err("import");

        auto dict = PyModule_GetDict(module);
        err("import");  // a bit careful doesn't hurt, isn't it ?

        return Python(dict, "module " + name, true);
    }

private:
    template <typename ...Args>
    static auto dict_extractor(Python dict, const std::string& name, const Args&... args)
    {
        if constexpr(sizeof...(Args))
            return std::tuple_cat(std::tuple(dict[name]), dict_extractor(dict, args...));

        return std::tuple(dict[name]);
    }

public:
    /// Import object from the module \var name
    template <typename ...Args>
    static auto from_import(const std::string& name, const Args&... args)
    {
        return dict_extractor(import(name), args...);
    }

    /// Return the python builtins
    static Python builtins(void)
    {
        initialize();

        auto ptr = PyEval_GetBuiltins();
        err("builtins");

        return Python(ptr, "builtins", true);
    }

    /// Call a builtin function
    static Python call_builtin(const std::string& name, Python args = nullptr, Python kwargs = nullptr)
    {
        auto main = builtins();
        return main[name].call(args, kwargs);
    }

    /// Evaluate python code
    static Python eval(const std::string& content, int type, Python globals, Python locals)
    {
        PyObject* ret;
        globals["__builtins__"] = PyEval_GetBuiltins();

        ret = PyRun_String(content.c_str(), type, globals, locals);
        err("eval");

        return Python(ret, "eval");
    }

private:
    /// Assign object \var obj at the ith slot of tuple
    template <typename T>
    static std::string tuple_assign(PyObject** ptr, std::size_t& i, T t)
    {
        Python obj = Python(t);

        if constexpr(is_starred<0, T>::value)
        {
            /// Target size (size of the container to fill)
            const std::size_t t_size = PyObject_Size(*ptr);
            /// Container size (size of the container to get items from)
            const std::size_t c_size = PyObject_Size(obj);

            _PyTuple_Resize(ptr, t_size + c_size - 1);
            err("tuple");

            for (std::size_t index = 0; index < c_size; index++, i++)
                PyTuple_SetItem(*ptr, i, PySequence_GetItem(obj, index));
            err("tuple");

            return std::string("*") + obj.name();
        }
        else
        {
            Py_INCREF(obj);
            PyTuple_SetItem(*ptr, i++, obj);
            err("tuple");

            return obj.name();
        }
    }

    /// Collect the arguments of a C++ tuple to put them in a python tuple
    template <std::size_t pos, typename ...Args>
    static std::string tuple_caller(PyObject** ptr, std::size_t& i, const std::tuple<Args...>& args)
    {
        std::string name;

        if constexpr(pos == 0)
            name = tuple_assign(ptr, i, std::get<pos>(args));
        else
            name = ", " + tuple_assign(ptr, i, std::get<pos>(args));

        if constexpr(pos + 1 < sizeof...(Args))
            name += tuple_caller<pos + 1>(ptr, i, args);

        return name;
    }

public:
    /// Create a tuple from C++ tuple
    template <typename ...Args>
    static Python tuple(const std::tuple<Args...>& args)
    {
        initialize();

        constexpr auto size = sizeof...(Args);
        auto ptr = PyTuple_New(size);
        err("tuple");

        if constexpr(sizeof...(Args))
        {
            std::size_t i = 0;
            return Python(ptr, "(" + tuple_caller<0>(&ptr, i, args) + ")");
        }
        else
            return Python(ptr, "()");
    }

    /// Create a tuple from all the passed arguments
    template <typename ...Args>
    static Python tuple(Args... args)
    {
        return tuple(std::tuple(args...));
    }

    /// Create a tuple from C++ iterable
    template <typename Iterable>
    static Python tuple(const Iterable& iter)
    {
        static_assert(is_iterable<Iterable>::value);

        initialize();

        auto ptr = PyTuple_New(iter.size());
        err("tuple");

        std::string name;

        std::size_t i = 0;
        for (auto&& e : iter)
        {
            if (i > 0)
                name += ", ";

            name += tuple_assign(&ptr, i,  e);
        }

        return Python(ptr, "[" + name + "]");
    }

    /// Create a tuple from python iterable
    static Python tuple(Python o)
    {
        assert(o.is_valid());

        auto ptr = PySequence_Tuple(o);
        err("tuple");

        return Python(ptr, "tuple(" + o.name() + ")" );
    }

private:
    /// Assign object \var obj at the ith slot of list
    template <typename T>
    static std::string list_assign(PyObject* ptr, T t)
    {
        Python obj = Python(t);

        if constexpr(is_starred<0, T>::value)
        {
            /// Container size (size of the container to get items from)
            const std::size_t c_size = PyObject_Size(obj);
            for (std::size_t index = 0; index < c_size; index++)
                PyList_Append(ptr, PySequence_GetItem(obj, index));
            err("list");

            return std::string("*") + obj.name();
        }
        else
        {
            Py_INCREF(obj);
            PyList_Append(ptr, obj);
            err("list");

            return obj.name();
        }
    }

    /// Collect the arguments of a C++ tuple to put them in a python list
    template <std::size_t pos, typename ...Args>
    static std::string list_caller(PyObject* ptr, const std::tuple<Args...>& args)
    {
        std::string name;

        if constexpr(pos == 0)
            name = list_assign(ptr, std::get<pos>(args));
        else
            name = ", " + list_assign(ptr, std::get<pos>(args));

        if constexpr(pos + 1 < sizeof...(Args))
            name += list_caller<pos + 1>(ptr, args);

        return name;
    }

public:
    /// Create a list from C++ tuple
    template <typename ...Args>
    static Python list(const std::tuple<Args...>& args)
    {
        initialize();

        auto ptr = PyList_New(0);
        err("list");

        if constexpr(sizeof...(Args))
            return Python(ptr, "[" + list_caller<0>(ptr, args) + "]");
        else
            return Python(ptr, "[]");
    }

    /// Create a list from all the passed arguments
    template <typename ...Args>
    static Python list(Args... args)
    {
        return list(std::tuple(args...));
    }

    /// Create a list from C++ iterable
    template <typename Iterable>
    static Python list(const Iterable& iter)
    {
        static_assert(is_iterable<Iterable>::value);

        initialize();

        auto ptr = PyList_New(0);
        err("list");

        std::string name;

        bool first = false;
        for (auto&& e : iter)
        {
            if (first)
                name += ", ";

            name += list_assign(ptr,  e);
            first = true;
        }

        return Python(ptr, "[" + name + "]");
    }

    /// Create a list from python iterable
    static Python list(Python o)
    {
        assert(o.is_valid());

        auto ptr = PySequence_List(o);
        err("list");

        return Python(ptr, "list(" + o.name() + ")" );
    }

private:
    /// Collect the args (key, value pairs) and put them into the dict
    template <typename K, typename T, typename ...Args>
    static std::string dict_assign(Python& dict, K k, T i, Args... items)
    {
        assert(dict.is_valid());

        auto key = Python(k);
        auto item = Python(i);

        dict[key] = item;
        err("dict");

        std::string name = key.ref_.name + ": " + item.ref_.name;

        if constexpr(sizeof...(Args))
            name += ", " + dict_assign(dict, items...);

        return name;
    }

    /// Overload of dict_assign for pair
    template <typename K, typename T>
    static std::string dict_assign(Python& dict, const std::pair<K, T>& p)
    {
        assert(dict.is_valid());

        auto key = Python(p.first);
        auto item = Python(p.second);

        dict[key] = item;
        err("dict");

        return key.ref_.name + ": " + item.ref_.name;
    }

public:
    /// Create a dictionnary from all the passed objects
    template <typename ...Args>
    static Python dict(Args... items)
    {
        initialize();

        auto ptr = PyDict_New();
        err("dict");

        auto obj = Python(ptr, "dict");
        if constexpr(sizeof...(Args))
            obj.ref_.name = dict_assign(obj, items...);

        return obj;
    }

    /// Overload of dict for iterable
    template <typename Iterable>
    static Python dict(const Iterable& i)
    {
        static_assert(is_iterable<Iterable>::value);

        initialize();

        auto ptr = PyDict_New();
        err("dict");

        auto obj = Python(ptr, "dict");

        bool first = true;
        for (const auto& e : i)
        {
            if (first == false)
                obj.ref_.name += ", ";

            obj.ref_.name += dict_assign(obj, e);
            first = false;
        }

        return obj;
    }

    /// Create a dictionnary from passed \var keys iterable and \var values iterable
    static Python dict(Python keys, Python values)
    {
        assert(keys.size() == values.size());

        auto ptr = PyDict_New();
        err("dict");

        auto obj = Python(ptr, "dict(keys = " + keys.name() + ", values = " + values.name() + ")");

        const std::size_t size = keys.size();
        for (std::size_t i = 0; i < size; i++)
            obj[static_cast<Python>(keys[i])] = values[i];

        return obj;
    }

    /// Create a set from the iterable \var o
    static Python set(Python o)
    {
        assert(PyTuple_Check(o) || PyList_Check(o));

        auto ptr = PySet_New(o);
        err("set");

        return Python(ptr, "set(" + o.name() + ")");
    }

    /*===== CONSTRUCTORS =====*/
    template <typename A, typename B>
    Python(const std::pair<A, B>& p)
    {
        *this = tuple(p.first, p.second);
        assert(is_valid());
    }

    template <typename T>
    Python(const std::vector<T>& v)
    {
        *this = list(v);
        assert(is_valid());
    }

    // avoid ambiguouty with vector-like (like string) structures and vector
    template <typename T>
    Python(const std::initializer_list<T>& v)
    {
        *this = list(v);
        assert(is_valid());
    }

    template <typename T, std::size_t N>
    Python(const std::array<T, N>& v)
    {
        *this = list(v);
        assert(is_valid());
    }

    template <typename T>
    Python(const std::list<T>& v)
    {
        *this = list(v);
        assert(is_valid());
    }

    template <typename K, typename T>
    Python(const std::map<K, T>& v)
    {
        *this = dict(v);
        assert(is_valid());
    }

    template <typename ...Args>
    Python(const std::tuple<Args...>& v)
    {
        *this = tuple(v);
        assert(is_valid());
    }

    template <typename T>
    Python(const std::optional<T>& t)
    {
        if (t.has_value())
            *this = Python(t.value());
        else
            *this = None;
    }

    // Generic constructor working with any type of string (as long as python support them)
    template <typename charT>
    Python(const std::basic_string<charT>& t)
    {
        initialize();
        ref_ = PyRef(PyUnicode_FromKindAndData(sizeof(charT), t.c_str(), t.size()), "\"" + to_string(t) + "\"");
        err("Python");
    }

    template <typename charT>
    explicit Python(const charT* t)
        : Python(std::basic_string<charT>(t))
    {
        static_assert(std::is_same<charT, char>::value
                   || std::is_same<charT, wchar_t>::value
                   || std::is_same<charT, char16_t>::value
                   || std::is_same<charT, char32_t>::value);
    }

    template <typename T>
    Python(PyIndexProxy<T> t)
    {
        *this = t;
    }

    template <typename T>
    Python(Starred<T> t)
    {
        *this = t;
    }

    Python(const PyRef& ref)
        : ref_(ref)
    {}

    /// Default constructor
    Python(void)
    {}

    /// Default constructor for nullptr (seamlessly pass nullptr)
    Python(std::nullptr_t)
    {}

    /// Copy constructor
    Python(const Python& o)
        : ref_(o.ref_)
    {}

    explicit Python(const std::filesystem::path& t)
    {
        initialize();
        ref_ = PyRef(PyUnicode_FromKindAndData(sizeof(std::filesystem::path::value_type), t.c_str(), t.string().size()), "\"" + to_string(t) + "\"");
        err("Python");
    }

    // unified interface to get rid of ambiguous overloads
    template <typename T, typename B = typename std::remove_cv<T>::type>
    explicit Python(const T t)
    {
        // forbid this contructor to supplant the other ones
        static_assert(std::is_same<B, PyRef>::value == false
                   && std::is_same<B, Python>::value == false
                   && std::is_same<B, std::string>::value == false
                   && std::is_same<B, std::nullptr_t>::value == false
                   && std::is_same<B, PyIndexProxy<std::string>>::value == false
                   && std::is_same<B, PyIndexProxy<Python>>::value == false
                   && std::is_same<B, PyIndexProxy<Py_ssize_t>>::value == false);

        initialize();
        // char
        if constexpr(std::is_same<B, char>::value || std::is_same<B, wchar_t>::value || std::is_same<B, char16_t>::value || std::is_same<B, char32_t>::value)
            ref_ = PyRef(PyUnicode_FromKindAndData(sizeof(B), &t, 1), to_string(t));
        // floatant
        else if constexpr(std::is_same<B, float>::value)
            ref_ = PyRef(PyFloat_FromDouble(t), to_string(t) + "f");
        else if constexpr(std::is_same<B, double>::value)
            ref_ = PyRef(PyFloat_FromDouble(t), to_string(t));
        else if constexpr(std::is_same<B, long double>::value)
            ref_ = PyRef(PyFloat_FromDouble(t), to_string(t) + "l");
        // signed
        else if constexpr(std::is_same<B, Py_ssize_t>::value)
            ref_ = PyRef(PyLong_FromSsize_t(t), to_string(t) + "ssz");
        else if constexpr(std::is_same<B, int8_t>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t) + "ss");
        else if constexpr(std::is_same<B, short>::value || std::is_same<B, int16_t>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t) + "s");
        else if constexpr(std::is_same<B, int>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t));
        else if constexpr(std::is_same<B, long>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t) + "l");
        else if constexpr(std::is_same<B, long long>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t) + "ll");
        // unsigned
        else if constexpr(std::is_same<B, std::size_t>::value)
            ref_ = PyRef(PyLong_FromSize_t(t), to_string(t) + "sz");
        else if constexpr(std::is_same<B, uint8_t>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t) + "uss");
        else if constexpr(std::is_same<B, unsigned short>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t) + "us");
        else if constexpr(std::is_same<B, unsigned int>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t) + "u");
        else if constexpr(std::is_same<B, unsigned long>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t) + "ul");
        else if constexpr(std::is_same<B, unsigned long long>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t) + "ull");

        // bool was too tedious to let alone because of multiple convertions to it
        else if constexpr(std::is_same<B, bool>::value)
            ref_ = t ? True : False;
        // in case you didn't activate unused variables
        else throw std::logic_error("Tried to convert unimplemented type to Python type:" + GET_TYPENAME(0));

        err("Python");

        // verify validity after (they must all be valid)
        assert(is_valid());
    }

    Python(PyObject* ptr, const std::string& name, bool borrowed = false)
        : ref_(ptr, name, borrowed)
    {}

    /*===== OPERATORS =====*/
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

    Python operator()(Python args = nullptr, Python kwargs = nullptr)
    {
        return call(args, kwargs);
    }

    # define COMPARISON(OP, CMP)                                                \
        Python OP(Python o)                                                     \
        {                                                                       \
            assert(is_valid() && o.is_valid());                                 \
            auto ptr = PyObject_RichCompare(ref_, o, Py_##CMP);                 \
            err(#OP);                                                           \
            auto ret = Python(ptr, name() + " " + (#OP + 8) + " " + o.name());  \
                                                                                \
            return ret;                                                         \
        }

    // Comparison operators
    COMPARISON(operator<,   LT)
    COMPARISON(operator<=,  LE)
    COMPARISON(operator==,  EQ)
    COMPARISON(operator!=,  NE)
    COMPARISON(operator>,   GT)
    COMPARISON(operator>=,  GE)

    # define OPERATION(OP, FUNC)                                        \
        Python OP(Python o)                                             \
        {                                                               \
            assert(is_valid());                                         \
            auto ret = import("operator")[#FUNC](tuple(ref_, o));  \
            err(#OP);                                                   \
            ret.ref_.name = name() + " " + (#OP + 8) + " " + o.name();  \
                                                                        \
            return ret;                                                 \
        }

    // Arithmetic operators
    OPERATION(operator+, __add__)
    OPERATION(operator-, __sub__)
    OPERATION(operator*, __mul__)
    OPERATION(operator/, __truediv__)
    OPERATION(operator%, __mod__)
    OPERATION(operator>>, __rshift__)
    OPERATION(operator<<, __lshift__)
    OPERATION(operator&, __and__)
    OPERATION(operator^, __xor__)
    OPERATION(operator|, __or__)

    // Inplace arithmetic operators
    // Can't use the __i*__ function, since immutable objets don't handle them
    Python operator+=(Python o) {   return (*this = *this + o); }
    Python operator-=(Python o) {   return (*this = *this - o); }
    Python operator*=(Python o) {   return (*this = *this * o); }
    Python operator/=(Python o) {   return (*this = *this / o); }
    Python operator%=(Python o) {   return (*this = *this % o); }
    Python operator>>=(Python o) {  return (*this = *this >> o); }
    Python operator<<=(Python o) {  return (*this = *this << o); }
    Python operator&=(Python o) {   return (*this = *this & o); }
    Python operator^=(Python o) {   return (*this = *this ^ o); }
    Python operator|=(Python o) {   return (*this = *this | o); }

    /*===== MISC =====*/
    Python contains(Python o)
    {
        assert(is_valid());
        auto ret = import("operator")["__contains__"](tuple(ref_, o));
        err("contains");
        ret.ref_.name = o.name() + " in " + name();

        return ret;
    }

    Python in(Python o)
    {
        return o.contains(*this);
    }

    Python count_of(Python o)
    {
        assert(is_valid());
        auto ret = import("operator")["__countOf__"](tuple(ref_, o));
        err("count_of");
        ret.ref_.name = o.name() + ".countOf(" + name() + ")";

        return ret;
    }

    Python index_of(Python o)
    {
        assert(is_valid());
        auto ret = import("operator")["__indexOf__"](tuple(ref_, o));
        err("index_of");
        ret.ref_.name = o.name() + ".indexOf(" + name() + ")";

        return ret;
    }

private:
    template <std::size_t i, typename ...Args>
    class is_starred
    {
    public:
        using type = typename std::tuple_element<i, std::tuple<Args...>>::type;
        static constexpr bool value = std::is_same<type, Starred<Python>>::value
                                   || std::is_same<type, Starred<PyIndexProxy<std::string>>>::value
                                   || std::is_same<type, Starred<PyIndexProxy<Py_ssize_t>>>::value
                                   || std::is_same<type, Starred<PyIndexProxy<Python>>>::value;
    };

    template <typename T, typename ...Args>
    class starred_count
    {
    public:
        static constexpr std::size_t value = is_starred<0, T>::value
                                           + starred_count<Args...>::value;
    };


    template <typename T>
    class starred_count<T>
    {
    public:
        static constexpr std::size_t value = is_starred<0, T>::value;
    };

    static void assign_error_not_enough(const std::size_t expected, const std::size_t got)
    {
        std::stringstream msg;
        msg << "not enough values to unpack (expected "
            << expected << ", got " << got << ")";
        PyErr_SetString(PyExc_ValueError, msg.str().c_str());
        err("assign");
    }

    static void assign_error_too_many(const std::size_t expected, const std::size_t got)
    {
        std::stringstream msg;
        msg << "too many values to unpack (expected "
            << expected << ", got " << got << ")";
        PyErr_SetString(PyExc_ValueError, msg.str().c_str());
        err("assign");
    }

    template <int way, std::size_t i, std::size_t tend, typename ...To>
    static void one_way_assign(Python from, const std::size_t fend, std::tuple<To&...> to)
    {
        if constexpr(way > 0)
        {
            // [0, tend] = [0, fend]
            if constexpr(i == tend)
                // starred part
                std::get<i>(to) = static_cast<Python>(from.slice(Python(i)));  // i to fend
            else
            {
                std::get<i>(to) = from[i];
                one_way_assign<way, i + 1, tend>(from, tend, to);
            }
        }
        else
        {
            if constexpr(i == 0)
                // starred part
                std::get<0>(to) = static_cast<Python>(from.slice(Python(0), Python(fend + 1))); // 0 to i
            else
            {
                std::get<i>(to) = from[fend];
                one_way_assign<way, i - 1, tend>(from, fend - 1, to);
            }
        }
    }

    template <std::size_t b, std::size_t e, std::size_t tend, typename ...To>
    static void two_way_assign(Python from, const std::size_t fend, [[maybe_unused]] std::tuple<To&...> to)
    {
        if constexpr(b < e)
        {
            constexpr bool bs = is_starred<b, To...>::value;
            constexpr bool es = is_starred<e, To...>::value;

            // shouldn't raise at all
            assert((bs && es) == false && "two starred expressions in same assign");

            if (bs == false)
                std::get<b>(to) = from[b];
            if (es == false)
                std::get<e>(to) = from[fend];

            // b is starred (freeze b in position, while e keep going)
            if constexpr(bs)
                two_way_assign<b, e - 1, tend>(from, fend - 1, to);
            // e is starred (freeze e in position, while b keep going)
            else if constexpr(es)
                two_way_assign<b + 1, e, tend>(from, fend, to);
            // no starred (yet) - b and e keep going
            else
                two_way_assign<b + 1, e - 1, tend>(from, fend - 1, to);
        }
        // slice or odd size
        if constexpr(b == e)
        {
            // starred expression
            if constexpr(is_starred<b, To...>::value)
                std::get<b>(to) = from.slice(Python(b), Python(fend + 1));
            // odd size without starred
            else if (e == fend)
                std::get<b>(to) = from[b];
            else
                assign_error_not_enough(tend + 1, tend + fend - e + 1);
        }
        // even size without slice (fend > e means unequal size for assign)
        else if (b > e && fend > e)
            assign_error_too_many(tend + 1, tend + fend - e + 1);
    }

public:
    template <typename ...To>
    static void assign(Python from, To&&... to)
    {
        // ensures there is less than 2 starred expressions
        static_assert(starred_count<To...>::value < 2);

        assert(PyTuple_Check(from) || PyList_Check(from) || PyDict_Check(from));

        // handle dict's case (return keys)
        from = static_cast<PyObject*>(Starred(from));

        /// Index of last element of to
        constexpr std::size_t tend = sizeof...(To) - 1;
        /// Index of last element of from
        const std::size_t fend = from.size() - 1;

        // starred at begin (*a, b, c)
        if constexpr(is_starred<0, To...>::value)
            one_way_assign<-1, tend, tend>(from, fend, std::forward_as_tuple(to...));
        // starred at end (a, b, *c)
        else if constexpr(is_starred<tend, To...>::value)
            one_way_assign<1, 0, tend>(from, fend, std::forward_as_tuple(to...));
        // starred in-between or none (a, b, c or a, *b, c)
        else
            two_way_assign<0, tend, tend>(from, fend, std::forward_as_tuple(to...));
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
        assert(is_valid());

        if (PyDict_Check(ref_))
            return Type::Dict;
        else if (PySequence_Check(ref_))
            return Type::Sequence;
        else
            return Type::Object;
    }

    /*=====  DICT  =====*/
    /// Return a slice of a Sequence
    auto slice(Python start = None, Python stop = None, Python step = None)
    {
        std::string name = this->name() + "["
                         + (start == None ? "" : start.name()) + ":"
                         + (stop == None ? "" : stop.name()) + ":"
                         + (step == None ? "" : step.name()) + "]";
        Python slice_obj = Python(PySlice_New(start, stop, step), name);

        return (*this)[slice_obj];
    }

    /// Return an iterator on the iterable
    auto iter(void)
    {
        assert(is_valid());

        auto ptr = PyObject_GetIter(ref_.ptr);
        err("iter");

        return Python(ptr, name() + ".__iter__()");
    }

    auto next(void)
    {
        assert(is_valid() && PyIter_Check(ref_.ptr));

        auto ptr = PyIter_Next(ref_.ptr);
        if (PyErr_Occurred() && PyErr_ExceptionMatches(PyExc_StopIteration))
            throw StopIteration();
        else
            err("next");

        return Python(ptr, name() + ".__next__()");
    }

    /// Return the size of an object (must be an iterable)
    Py_ssize_t size(void) const
    {
        assert(is_valid());

        auto ret = PyObject_Size(ref_.ptr);
        err("size");

        return ret;
    }

    /// Return current dict's keys
    Python keys(void)
    {
        assert(PyDict_Check(ref_.ptr))

        auto ptr = PyDict_Keys(ref_.ptr);
        err("keys");

        return Python(ptr, name() + ".keys()");
    }

    /// Return current dict's values
    Python values(void)
    {
        assert(PyDict_Check(ref_.ptr))

        auto ptr = PyDict_Values(ref_.ptr);
        err("values");

        return Python(ptr, name() + ".values()");
    }

    /*===== OBJECT =====*/
    /// Return true if the object has attribute \var name
    bool hasattr(const std::string& name)
    {
        assert(is_valid());

        auto ret = PyObject_HasAttrString(ref_, name.c_str());
        err("hasattr");

        return ret;
    }

    /// Get attribute of object (it's the only way to get attributes of iterables)
    # define ATTR(TYPE) auto attr(TYPE key)             \
    {                                                   \
        assert(is_valid());                             \
        return PyIndexProxy(ref_, Type::Object, key);   \
    }
    ATTR(const std::string&)
    ATTR(Python&)

    /// Delete the attribute \var name of the current object
    bool delattr(const std::string& name)
    {
        assert(is_valid());

        auto ret = PyObject_DelAttrString(ref_, name.c_str()) + 1;
        err("delattr");

        return ret;
    }

    /// Print the object on the file \var fp, print the representation if \var repr is True
    void print(const bool repr = false, FILE* fp = stdout) const
    {
        PyObject_Print(ref_.ptr, fp, repr ? Py_PRINT_RAW : 0);
        fprintf(fp, "\n");
        err("print");
    }

    /*===== FUNCTION =====*/
    /// Call the object with \var args as arguments and \var kwargs as keywords
    Python call(Python args = nullptr, Python kwargs = nullptr)
    {
        assert(is_valid());

        bool tup = args.is_valid();

        // args must be at least an empty tuple
        // FYI, not being an iterable object raise a MemoryError
        if (args.is_valid() == false)
            args = tuple();

        auto ret = PyObject_Call(ref_, args, kwargs);
        err("call");

        auto nargs = args.name();
        if (kwargs)
        {
            nargs.pop_back();       // remove ending parenthesis

            if (tup)                // add "," to make distinction with args
                nargs += ", ";

            // add kwargs
            const auto kwarg = kwargs.name().substr(1, kwargs.name().size() - 1);
            nargs += kwarg + ")";// std::regex_replace(kwarg, kwargs_regex, "$1 = ") + ")";
        }

        return Python(ret, name() + nargs);
    }

    /// Call the function \var name in object with \var args as arguments and \var kwargs as keywords
    Python call(const std::string& name, Python args = nullptr, Python kwargs = nullptr)
    {
        assert(is_valid());

        return attr(name).call(args, kwargs);
    }

    /*===== MISC =====*/
    static void set_finally(void (*func)())
    {
        finally_func = func;
    }

    /*===== CONVERSIONS =====*/
    bool to_bool(void)
    {
        assert(is_valid());

        const auto ret = PyObject_IsTrue(ref_);
        err("to_bool");

        return ret;
    }

    Py_ssize_t to_ssize_t(void)
    {
        assert(is_valid());

        auto val = PyLong_AsSsize_t(ref_);
        err("to_ssize_t");

        return val;
    }

    std::size_t to_size_t(void)
    {
        assert(is_valid());

        auto val = PyLong_AsSize_t(ref_);
        err("to_size_t");

        return val;
    }

    double to_double(void)
    {
        assert(is_valid());

        auto val = PyFloat_AsDouble(ref_);
        err("to_double");

        return val;
    }

    Py_UCS4* ucs4(void)
    {
        assert(is_valid());

        constexpr int len = 200;

        Py_UCS4 buffer[len];
        auto ret = PyUnicode_AsUCS4(ref_, buffer, len, true);
        err("UCS4");

        return ret;
    }

    /*===== Narrowing string convertions =====*/
    // for non-narrowing, use the converter
    utf32_t utf32(void)
    {
        utf32_t string;

        for (auto c = ucs4(); *c; c++)
            string.push_back(*c & std::numeric_limits<utf32_t::value_type>::max());

        return string;
    }

    utf16_t utf16(void)
    {
        utf16_t string;

        for (auto c = ucs4(); *c; c++)
            string.push_back(*c & std::numeric_limits<utf16_t::value_type>::max());

        return string;
    }

    utf8_t utf8(void)
    {
        utf8_t string;

        for (auto c = ucs4(); *c; c++)
            string.push_back(*c & std::numeric_limits<utf8_t::value_type>::max());

        return string;
    }

    std::string string(void)
    {
        std::string string;

        for (auto c = ucs4(); *c; c++)
            string.push_back(*c & std::numeric_limits<std::string::value_type>::max());

        return string;
    }

    std::wstring wstring(void)
    {
        std::wstring string;

        for (auto c = ucs4(); *c; c++)
            string.push_back(*c & std::numeric_limits<std::wstring::value_type>::max());

        return string;
    }

private:
    static inline bool initialized_ = false;
    static inline bool mute_error = false;
    static inline void (*finally_func)() = nullptr;

    PyRef ref_;

public:
    static inline PyRef True = PyRef(Py_True, "True", true);
    static inline PyRef False = PyRef(Py_False, "False", true);
    static inline PyRef None = PyRef(Py_None, "None", true);
    static inline PyRef Ellipsis = PyRef(Py_Ellipsis, "Ellipsis", true);
};