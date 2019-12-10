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

struct PyRef
{
    using ref_counter_t = std::shared_ptr<std::size_t>;

    PyRef(PyObject* ptr, ref_counter_t counter, const std::string& name, const bool is_ref)
        : ptr(ptr), counter(counter), name(escape(name)), is_ref(is_ref)
    {
        // mute NULL INCREF
        if (ptr)
        {
            if (counter)
            {
                # ifdef PYDEBUG_CONST
                if (*counter == 0)
                    std::cout << "Construction of " << this->name << std::endl;
                # endif

                (*counter)++;
            }

            # ifdef PYDEBUG_INCREF
            std::cout << "Incref of " << this->name << std::endl;
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

            # ifdef PYDEBUG_INCREF
            std::cout << "Incref of " << name << std::endl;
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

        # ifdef PYDEBUG_DECREF
        if (counter)
        {
            std::cout << "Decref of " << name;
            std::cout << " (" << *counter << " instances remaining)" << std::endl;
        }
        # endif

        if (is_ref && *counter == 0)
        {
            Py_XDECREF(ptr);

            # ifdef PYDEBUG_DEST
            std::cout << "Destruction of " << name << std::endl;
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
        if (PyErr_Occurred())
        {
            std::cerr << "\nIn function \"" << func << "\":" << std::endl;
            PyErr_Print();
            std::cerr << std::endl;

            // skip "err" trace
            backtrace(1);
        }
    }

    // char-likes
    template <typename T>
    static std::string to_string(const T t, [[maybe_unused]] const bool quotes = true)
    {
        if constexpr(std::is_same<T, char>::value || std::is_same<T, wchar_t>::value
                  || std::is_same<T, char16_t>::value || std::is_same<T, char32_t>::value)
            return quotes ? to_string(std::basic_string<T>(1, t)) : std::string(1, t);
        else
            return std::to_string(t);
    }

    // string-likes
    static std::string to_string(const std::string& s, const bool quotes = true)
        { return quotes ? s : s; }
    template <typename char_t>
    static std::string to_string(const std::basic_string<char_t>& s, const bool quotes = true)
        { return quotes ? converter<char_t>.to_bytes(s) : converter<char_t>.to_bytes(s); }
    template <typename char_t>
    static std::string to_string(const char_t* s, const bool quotes = true)
        { return to_string(std::basic_string<char_t>(s), quotes); }

    // misc
    static std::string to_string(const PyRef& s, const bool quotes = false)
        { (void) quotes; return s.name; }
    static std::string to_string(Python& s, const bool quotes = false)
        { (void) quotes; return s.name(); }
    static std::string to_string(std::nullptr_t, const bool quotes = false)
        { (void) quotes; return "NULL"; }
    static std::string to_string(const std::filesystem::path& s, const bool quotes = false)
        { (void) quotes; return s.string(); }

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
                // check for presence (as it's not checked ?)
                case Type::Object:  ret = PyObject_GetAttr(object_, Python(key_)); break;
                case Type::Dict:    ret = PyDict_GetItemWithError(object_, Python(key_)); break;
                case Type::Sequence:
                    if constexpr(std::is_convertible<key_t, Py_ssize_t>::value)
                        ret = PySequence_GetItem(object_.ptr, key_);
                    else
                        PyErr_SetString(PyExc_TypeError, "list indices must be integers or slices");
                    break;
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
                return Python(ret, object_.name + "." + to_string(key_, false), false);
            else
                return Python(ret, object_.name + "[" + to_string(key_, true) + "]", false);
        }

        auto& operator=(PyObject* object)
        {
            assert(object_.ptr != nullptr);

            switch (type_)
            {
                case Type::Object: PyObject_SetAttr(object_.ptr, Python(key_), object); break;
                case Type::Dict:    PyDict_SetItem(object_.ptr, Python(key_), object); break;
                case Type::Sequence:
                    if constexpr(std::is_convertible<key_t, Py_ssize_t>::value)
                        PySequence_SetItem(object_.ptr, key_, object);
                    else
                        PyErr_SetString(PyExc_TypeError, "list indices must be integers or slices");
                    break;
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

        auto name()
        {
            return object_.name;
        }

        auto key()
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

    private:
        PyRef object_;
        Type type_;
        key_t key_;
    };

    template <typename key_t>
    static std::string to_string(PyIndexProxy<key_t>& s) { return s.name() + "[" + to_string(s.key()) + "]"; }

    // Use this with caution, it should be used in place of another constructor
    Python(PyObject* o)
        : ref_(o, "PyObject*", false)
    {}

public:
    /// Access operators
    # define ACCESS(TYPE) auto operator[](TYPE key) \
    {                                               \
        assert(is_valid());                         \
        return PyIndexProxy(ref_, get_type(), key); \
    }
    ACCESS(const std::string&)
    ACCESS(const Py_ssize_t)
    ACCESS(Python&)

    /// Import the module \var name
    static Python import(const std::string& name)
    {
        initialize();
        auto module = Python(PyImport_ImportModule(name.c_str()), name, true);
        err("import");

        auto dict = PyModule_GetDict(module);
        err("import");  // a bit careful doesn't hurt, isn't it ?

        return Python(dict, "module " + name, false);
    }

    /// Return the python builtins
    static Python builtins(void)
    {
        initialize();

        auto ptr = PyEval_GetBuiltins();
        err("builtins");

        return Python(ptr, "builtins", false);
    }

    /// Call a builtin function
    static Python call_builtin(const std::string& name, Python args = nullptr, Python kwargs = nullptr)
    {
        auto main = builtins();
        return main.call(name, args, kwargs);
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
    static std::string tuple_collect(PyObject* tuple, Py_ssize_t i, T& item, Args... items)
    {
        assert(tuple != nullptr);

        // disable DECREF since SetItem steals the reference of obj
        auto obj = Python(item, false);
        PyTuple_SetItem(tuple, i, obj);
        err("tuple_collect");

        std::string name = obj.ref_.name;

        if constexpr(sizeof...(Args))
            name += ", " + tuple_collect(tuple, i + 1, items...);

        return name;
    }

    template <std::size_t size, std::size_t i = 0, typename ...Args>
    static std::string tuple_collect(PyObject* ptr, const std::tuple<Args...>& tuple)
    {
        auto obj = Python(std::get<i>(tuple), false);
        PyTuple_SetItem(ptr, i, obj);
        err("tuple");

        auto name = obj.name();
        if constexpr(i + 1 < size)
            name += ", " + tuple_collect<size, i + 1>(ptr, tuple);

        return name;
    }

public:
    /// Create a tuple from all the passed objects
    template <typename ...Args>
    static Python tuple(Args... items)
    {
        initialize();

        auto ptr = PyTuple_New(sizeof...(Args));
        err("tuple");

        std::string name;

        if constexpr(sizeof...(Args))
            name = tuple_collect(ptr, 0, items...);

        return Python(ptr, "(" + name + ")", true);
    }

    /// Create a tuple from all the passed object
    template <typename ...Args>
    static Python tuple(const std::tuple<Args...>& t)
    {
        initialize();

        constexpr auto size = std::tuple_size<std::tuple<Args...>>::value;
        auto ptr = PyTuple_New(size);
        err("tuple");

        std::string name;
        if (size)
            name = tuple_collect<size>(ptr, t);

        return Python(ptr, "(" + name + ")", true);
    }

private:
    /// Collect the args (values) and put them into the list
    template <typename T, typename ...Args>
    static std::string list_collect(PyObject* list, Py_ssize_t i, T& item, Args... items)
    {
        assert(list != nullptr);

        // disable DECREF since SetItem steal the reference of obj
        auto obj = Python(item, false);
        PyList_SetItem(list, i, obj);
        err("list_collect");

        std::string name = obj.ref_.name;

        if constexpr(sizeof...(Args))
            name += ", " + list_collect(list, i + 1, items...);

        return name;
    }

public:
    /// Create a list from all the passed objects
    template <typename ...Args>
    static Python list(Args... items)
    {
        initialize();

        auto ptr = PyList_New(sizeof...(Args));
        err("list");

        std::string name;

        if constexpr(sizeof...(Args))
            name = list_collect(ptr, 0, items...);

        return Python(ptr, "[" + name + "]", true);
    }

    // Overload of list for iterable
    template <typename Iterable>
    static Python list(const Iterable& i)
    {
        static_assert(is_iterable<Iterable>::value);

        initialize();

        auto ptr = PyList_New(i.size());
        err("dict");

        std::string name;

        std::size_t c = 0;
        bool first = true;
        for (const auto& e : i)
        {
            if (first == false)
                name += ", ";

            name += list_collect(ptr, c++,  e);
            first = false;
        }

        return Python(ptr, "[" + name + "]", true);
    }

private:
    /// Collect the args (key, value pairs) and put them into the dict
    template <typename K, typename T, typename ...Args>
    static std::string dict_collect(Python& dict, K k, T i, Args... items)
    {
        assert(dict != nullptr);

        auto key = Python(k);
        auto item = Python(i);

        dict[key] = item;
        err("dict_collect");

        std::string name = key.ref_.name + ": " + item.ref_.name;

        if constexpr(sizeof...(Args))
            name += ", " + dict_collect(dict, items...);

        return name;
    }

    /// Overload of dict_collect for pair
    template <typename K, typename T>
    static std::string dict_collect(Python& dict, const std::pair<K, T>& p)
    {
        assert(dict != nullptr);

        auto key = Python(p.first);
        auto item = Python(p.second);

        dict[key] = item;
        err("dict_collect");

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

        auto obj = Python(ptr, "dict", true);
        if constexpr(sizeof...(Args))
            obj.ref_.name = dict_collect(obj, items...);

        return obj;
    }

    // Overload of dict for iterable
    template <typename Iterable>
    static Python dict(const Iterable& i)
    {
        static_assert(is_iterable<Iterable>::value);

        initialize();

        auto ptr = PyDict_New();
        err("dict");

        auto obj = Python(ptr, "dict", true);

        bool first = true;
        for (const auto& e : i)
        {
            if (first == false)
                obj.ref_.name += ", ";

            obj.ref_.name += dict_collect(obj, e);
            first = false;
        }

        return obj;
    }

    /// Create Python value from format
    template <typename ...Args>
    static Python build_format(const std::string& format, Args... args)
    {
        initialize();

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

        return Python(ret, "format \"" + format + "\"", true);
    }

    /// Release the ressources of the global Python instance
    static void terminate(void)
    {
        Py_Finalize();
        initialized_ = false;
    }

    /*===== CONSTRUCTORS =====*/
    template <typename A, typename B>
    Python(const std::pair<A, B>& p, const bool is_ref = true)
    {
        *this = tuple(p.first, p.second);
        assert(is_valid());

        if (is_ref == false)
            ref_.is_ref = false;
    }

    template <typename T>
    Python(const std::vector<T>& v, const bool is_ref = true)
    {
        *this = list(v);
        assert(is_valid());

        if (is_ref == false)
            ref_.is_ref = false;
    }

    // avoid ambiguouty with vector-like (like string) structures and vector
    template <typename T>
    Python(const std::initializer_list<T>& v, const bool is_ref = true)
    {
        *this = list(v);
        assert(is_valid());

        if (is_ref == false)
            ref_.is_ref = false;
    }

    template <typename T, std::size_t N>
    Python(const std::array<T, N>& v, const bool is_ref = true)
    {
        *this = list(v);
        assert(is_valid());

        if (is_ref == false)
            ref_.is_ref = false;
    }

    template <typename T>
    Python(const std::list<T>& v, const bool is_ref = true)
    {
        *this = list(v);
        assert(is_valid());

        if (is_ref == false)
            ref_.is_ref = false;
    }

    template <typename K, typename T>
    Python(const std::map<K, T>& v, const bool is_ref = true)
    {
        *this = dict(v);
        assert(is_valid());

        if (is_ref == false)
            ref_.is_ref = false;
    }

    template <typename ...Args>
    Python(const std::tuple<Args...>& v, const bool is_ref = true)
    {
        *this = tuple(v);
        assert(is_valid());

        if (is_ref == false)
            ref_.is_ref = false;
    }

    template <typename T>
    Python(const std::optional<T>& t, const bool is_ref = true)
    {
        if (t.has_value())
            *this = Python(t.value(), is_ref);
        else
            *this = None;
    }

    // Generic constructor working with any type of string (as long as python support them)
    template <typename charT>
    Python(const std::basic_string<charT>& t, const bool is_ref = true)
    {
        initialize();
        ref_ = PyRef(PyUnicode_FromKindAndData(sizeof(charT), t.c_str(), t.size()), "\"" + to_string(t) + "\"", is_ref);
        err("Python");
    }

    template <typename charT>
    explicit Python(const charT* t, const bool is_ref = true)
        : Python(std::basic_string<charT>(t), is_ref)
    {
        static_assert(std::is_same<charT, char>::value
                   || std::is_same<charT, wchar_t>::value
                   || std::is_same<charT, char16_t>::value
                   || std::is_same<charT, char32_t>::value);
    }

    template <typename T>
    Python(PyIndexProxy<T> t, const bool is_ref = true)
    {
        *this = t;
        (void) is_ref;
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

    explicit Python(const std::filesystem::path& t, const bool is_ref = true)
    {
        initialize();
        ref_ = PyRef(PyUnicode_FromKindAndData(sizeof(std::filesystem::path::value_type), t.c_str(), t.string().size()), "\"" + t.string() + "\"", is_ref);
        err("Python");
    }

    // unified interface to get rid of ambiguous overloads
    template <typename T, typename B = typename std::remove_cv<T>::type>
    explicit Python(const T t, const bool is_ref = true)
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
            ref_ = PyRef(PyUnicode_FromKindAndData(sizeof(B), &t, 1), to_string(t), is_ref);
        // floatant
        else if constexpr(std::is_same<B, float>::value)
            ref_ = PyRef(PyFloat_FromDouble(t), to_string(t) + "f", is_ref);
        else if constexpr(std::is_same<B, double>::value)
            ref_ = PyRef(PyFloat_FromDouble(t), to_string(t), is_ref);
        else if constexpr(std::is_same<B, long double>::value)
            ref_ = PyRef(PyFloat_FromDouble(t), to_string(t) + "l", is_ref);
        // signed
        else if constexpr(std::is_same<B, Py_ssize_t>::value)
            ref_ = PyRef(PyLong_FromSsize_t(t), to_string(t) + "ssz", is_ref);
        else if constexpr(std::is_same<B, int8_t>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t) + "ss", is_ref);
        else if constexpr(std::is_same<B, short>::value || std::is_same<B, int16_t>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t) + "s", is_ref);
        else if constexpr(std::is_same<B, int>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t), is_ref);
        else if constexpr(std::is_same<B, long>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t) + "l", is_ref);
        else if constexpr(std::is_same<B, long long>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t) + "ll", is_ref);
        // unsigned
        else if constexpr(std::is_same<B, std::size_t>::value)
            ref_ = PyRef(PyLong_FromSize_t(t), to_string(t) + "sz", is_ref);
        else if constexpr(std::is_same<B, uint8_t>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t) + "uss", is_ref);
        else if constexpr(std::is_same<B, unsigned short>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t) + "us", is_ref);
        else if constexpr(std::is_same<B, unsigned int>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t) + "u", is_ref);
        else if constexpr(std::is_same<B, unsigned long>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t) + "ul", is_ref);
        else if constexpr(std::is_same<B, unsigned long long>::value)
            ref_ = PyRef(PyLong_FromLong(t), to_string(t) + "ull", is_ref);

        // bool was too tedious to let alone because of multiple convertions to it
        else if constexpr(std::is_same<B, bool>::value)
            ref_ = t ? True : False;
        // in case you didn't activate unused variables
        else throw std::logic_error("Tried to convert unimplemented type to Python type:" + GET_TYPENAME(0));

        // mute unused is_ref if not used (like in bool's case)
        (void) is_ref;

        err("Python");

        // verify validity after (they must all be valid)
        assert(is_valid());
    }

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

    bool is_valid(void) const
    {
        return ref_;
    }

    std::string name(void) const
    {
        return ref_.name;
    }

    // FIXME: add an additional detection for classes that implements __get_item__
    //        these classes seems like Sequence, but don't accept always Numeric key
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

    /*===== object =====*/
    bool hasattr(const std::string& name)
    {
        assert(is_valid());

        auto ret = PyObject_HasAttrString(ref_, name.c_str());
        err("hasattr");

        return ret;
    }

    auto size(void) const
    {
        assert(is_valid());

        auto ret = PyObject_Size(ref_.ptr);
        err("size");

        return ret;
    }

    void print(const bool repr = false, FILE* fp = stdout) const
    {
        PyObject_Print(ref_.ptr, fp, repr ? Py_PRINT_RAW : 0);
        fprintf(fp, "\n");
        err("print");
    }

    /*===== function =====*/
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

        return Python(ret, name() + nargs, true);
    }

    Python call(const std::string& name, Python args = nullptr, Python kwargs = nullptr)
    {
        assert(is_valid());

        return operator[](name).call(args, kwargs);
    }

    /*===== conversions =====*/
    ssize_t to_ssize_t(void)
    {
        assert(is_valid());

        auto val = PyNumber_AsSsize_t(ref_, nullptr);
        err("ssize_t");

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

    PyRef ref_;

public:
    static inline PyRef True = PyRef(Py_True, "True", false);
    static inline PyRef False = PyRef(Py_False, "False", false);
    static inline PyRef None = PyRef(Py_None, "None", false);
};