//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#include <algorithm>
#include <iostream>
#include <errno.h>
#include <string.h>
#include "cstring.h"
#include "ltype.h"
#include "parallel/api.h"
#include "progress/progress_manager.h"
#include "python/obj.h"
#include "python/python.h"
#include "python/string.h"
#include "python/tuple.h"
#include "stype.h"
#include "utils/assert.h"
#include "utils/exceptions.h"


// Singleton, used to write the current "errno" into the stream
CErrno Errno;

static PyObject* DtExc_ImportError           = PyExc_Exception;
static PyObject* DtExc_IndexError            = PyExc_Exception;
static PyObject* DtExc_InvalidOperationError = PyExc_Exception;
static PyObject* DtExc_IOError               = PyExc_Exception;
static PyObject* DtExc_KeyError              = PyExc_Exception;
static PyObject* DtExc_MemoryError           = PyExc_Exception;
static PyObject* DtExc_NotImplementedError   = PyExc_Exception;
static PyObject* DtExc_OverflowError         = PyExc_Exception;
static PyObject* DtExc_TypeError             = PyExc_Exception;
static PyObject* DtExc_ValueError            = PyExc_Exception;
static PyObject* DtWrn_DatatableWarning      = PyExc_Exception;
static PyObject* DtWrn_IOWarning             = PyExc_Exception;

void init_exceptions() {
  auto dx = py::oobj::import("datatable", "exceptions");
  DtExc_ImportError           = dx.get_attr("ImportError").release();
  DtExc_IndexError            = dx.get_attr("IndexError").release();
  DtExc_InvalidOperationError = dx.get_attr("InvalidOperationError").release();
  DtExc_IOError               = dx.get_attr("IOError").release();
  DtExc_KeyError              = dx.get_attr("KeyError").release();
  DtExc_MemoryError           = dx.get_attr("MemoryError").release();
  DtExc_NotImplementedError   = dx.get_attr("NotImplementedError").release();
  DtExc_OverflowError         = dx.get_attr("OverflowError").release();
  DtExc_TypeError             = dx.get_attr("TypeError").release();
  DtExc_ValueError            = dx.get_attr("ValueError").release();
  DtWrn_DatatableWarning      = dx.get_attr("DatatableWarning").release();
  DtWrn_IOWarning             = dx.get_attr("IOWarning").release();
}



//==============================================================================

static bool is_string_empty(const char* msg) noexcept {
  if (!msg) return true;
  char c;
  while ((c = *msg) != 0) {
    if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r'))
      return false;
    msg++;
  }
  return true;
}


void exception_to_python(const std::exception& e) noexcept {
  wassert(dt::num_threads_in_team() == 0);
  // dt::progress::manager->update_view();
  const Error* error = dynamic_cast<const Error*>(&e);
  if (error) {
    error->to_python();
  }
  else if (!PyErr_Occurred()) {
    const char* msg = e.what();
    if (is_string_empty(msg)) {
      PyErr_SetString(PyExc_Exception, "unknown error");
    } else {
      PyErr_SetString(PyExc_Exception, msg);
    }
  }
}


/**
  * If `str` contains any backticks or backslashes, they will be
  * escaped by prepending each such character with a backslash.
  * If there are no backticks/backslahes in `str`, then a simple copy
  * of the string is returned.
  */
std::string escape_backticks(const std::string& str) {
  size_t count = 0;
  for (char c : str) {
    count += (c == '`' || c == '\\');
  }
  if (count == 0) return str;
  std::string out;
  out.reserve(str.size() + count);
  for (char c : str) {
    if (c == '`' || c == '\\') out += '\\';
    out += c;
  }
  return out;
}




//==============================================================================

Error::Error(PyObject* exception_class)
  : pycls_(exception_class) {}

Error::Error(const Error& other) {
  error << other.error.str();
  pycls_ = other.pycls_;
}

Error::Error(Error&& other) : Error(nullptr) {
  *this = std::move(other);
}

Error& Error::operator=(Error&& other) {
  #if defined(__GNUC__) && __GNUC__ < 5
    // In gcc4.8 string stream was not moveable
    error.str(other.error.str());
  #else
    std::swap(error, other.error);
  #endif
  std::swap(pycls_, other.pycls_);
  return *this;
}

void Error::to_stderr() const {
  std::cerr << error.str() << "\n";
}

std::string Error::to_string() const {
  return error.str();
}


Error& Error::operator<<(const std::string& v) { error << v; return *this; }
Error& Error::operator<<(const char* v)        { error << v; return *this; }
Error& Error::operator<<(const void* v)        { error << v; return *this; }
Error& Error::operator<<(int64_t v)            { error << v; return *this; }
Error& Error::operator<<(int32_t v)            { error << v; return *this; }
Error& Error::operator<<(int8_t v)             { error << v; return *this; }
Error& Error::operator<<(size_t v)             { error << v; return *this; }
Error& Error::operator<<(uint32_t v)           { error << v; return *this; }
Error& Error::operator<<(float v)              { error << v; return *this; }
Error& Error::operator<<(double v)             { error << v; return *this; }
#ifdef __APPLE__
  Error& Error::operator<<(uint64_t v)         { error << v; return *this; }
  Error& Error::operator<<(ssize_t v)          { error << v; return *this; }
#endif

Error& Error::operator<<(const dt::CString& str) {
  return *this << str.to_string();
}

Error& Error::operator<<(const py::_obj& o) {
  return *this << o.to_borrowed_ref();
}

Error& Error::operator<<(const py::ostring& o) {
  PyObject* ptr = o.to_borrowed_ref();
  Py_ssize_t size;
  const char* chardata = PyUnicode_AsUTF8AndSize(ptr, &size);
  if (chardata) {
    error << std::string(chardata, static_cast<size_t>(size));
  } else {
    error << "<unknown>";
    PyErr_Clear();
  }
  return *this;
}

Error& Error::operator<<(PyObject* v) {
  PyObject* repr = PyObject_Repr(v);
  Py_ssize_t size;
  const char* chardata = PyUnicode_AsUTF8AndSize(repr, &size);
  if (chardata) {
    error << std::string(chardata, static_cast<size_t>(size));
  } else {
    error << "<unknown>";
    PyErr_Clear();
  }
  Py_XDECREF(repr);
  return *this;
}

Error& Error::operator<<(PyTypeObject* v) {
  return *this << reinterpret_cast<PyObject*>(v);
}

Error& Error::operator<<(const CErrno&) {
  error << "[errno " << errno << "] " << strerror(errno);
  return *this;
}

Error& Error::operator<<(dt::SType stype) {
  error << dt::stype_name(stype);
  return *this;
}

Error& Error::operator<<(dt::LType ltype) {
  error << dt::ltype_name(ltype);
  return *this;
}

Error& Error::operator<<(char c) {
  uint8_t uc = static_cast<uint8_t>(c);
  if (uc < 0x20 || uc >= 0x80 || uc == '`' || uc == '\\') {
    error << '\\';
    if (c == '\n') error << 'n';
    else if (c == '\r') error << 'r';
    else if (c == '\t') error << 't';
    else if (c == '\\') error << '\\';
    else if (c == '`')  error << '`';
    else {
      uint8_t d1 = uc >> 4;
      uint8_t d2 = uc & 15;
      error << "\\x" << static_cast<char>((d1 <= 9? '0' : 'a' - 10) + d1)
                     << static_cast<char>((d2 <= 9? '0' : 'a' - 10) + d2);
    }
  } else {
    error << c;
  }
  return *this;
}


void Error::to_python() const noexcept {
  // The pointer returned by errstr.c_str() is valid until errstr gets out
  // of scope. By contrast, `error.str().c_str()` returns a dangling pointer,
  // which usually works but sometimes doesn't...
  // See https://stackoverflow.com/questions/1374468
  try {
    const std::string errstr = error.str();
    PyErr_SetString(pycls_, errstr.c_str());
  } catch (const std::exception& e) {
    PyErr_SetString(PyExc_RuntimeError, e.what());
  }
}


bool Error::is_keyboard_interrupt() const noexcept {
  return false;
}



//==============================================================================

PyError::PyError() : Error(nullptr) {
  PyErr_Fetch(&exc_type, &exc_value, &exc_traceback);
  if (is_keyboard_interrupt()) {
    dt::progress::manager->set_status_cancelled();
  }
}

PyError::PyError(PyError&& other) : Error(std::move(other)) {
  exc_type = other.exc_type;
  exc_value = other.exc_value;
  exc_traceback = other.exc_traceback;
  other.exc_type = nullptr;
  other.exc_value = nullptr;
  other.exc_traceback = nullptr;
}

PyError::~PyError() {
  Py_XDECREF(exc_type);
  Py_XDECREF(exc_value);
  Py_XDECREF(exc_traceback);
}

void PyError::to_python() const noexcept {
  PyErr_Restore(exc_type, exc_value, exc_traceback);
  exc_type = nullptr;
  exc_value = nullptr;
  exc_traceback = nullptr;
}

bool PyError::is_keyboard_interrupt() const noexcept {
  return exc_type == PyExc_KeyboardInterrupt;
}

bool PyError::is_assertion_error() const noexcept {
  return exc_type == PyExc_AssertionError;
}

std::string PyError::message() const {
  return py::robj(exc_value).to_pystring_force().to_string();
}



//==============================================================================

Error AssertionError()        { return Error(PyExc_AssertionError); }
Error RuntimeError()          { return Error(PyExc_RuntimeError); }
Error ImportError()           { return Error(DtExc_ImportError); }
Error IndexError()            { return Error(DtExc_IndexError); }
Error IOError()               { return Error(DtExc_IOError); }
Error KeyError()              { return Error(DtExc_KeyError); }
Error MemoryError()           { return Error(DtExc_MemoryError); }
Error NotImplError()          { return Error(DtExc_NotImplementedError); }
Error OverflowError()         { return Error(DtExc_OverflowError); }
Error TypeError()             { return Error(DtExc_TypeError); }
Error ValueError()            { return Error(DtExc_ValueError); }
Error InvalidOperationError() { return Error(DtExc_InvalidOperationError); }




//==============================================================================

Warning::Warning(PyObject* cls) : Error(cls) {}

void Warning::emit() {
  const std::string errstr = error.str();
  // Normally, PyErr_WarnEx returns 0. However, when the `warnings` module is
  // configured in such a way that all warnings are converted into errors,
  // then PyErr_WarnEx will return -1. At that point we should throw
  // an exception too, the error message is already set in Python.
  int ret = PyErr_WarnEx(pycls_, errstr.c_str(), 1);
  if (ret) throw PyError();
}


Warning DeprecationWarning() { return Warning(PyExc_FutureWarning); }
Warning DatatableWarning()   { return Warning(DtWrn_DatatableWarning); }
Warning IOWarning()          { return Warning(DtWrn_IOWarning); }




//------------------------------------------------------------------------------
// HidePythonError
//------------------------------------------------------------------------------

HidePythonError::HidePythonError() {
  if (PyErr_Occurred()) {
    PyErr_Fetch(&ptype_, &pvalue_, &ptraceback_);
  } else {
    ptype_ = nullptr;
    pvalue_ = nullptr;
    ptraceback_ = nullptr;
  }
}

HidePythonError::~HidePythonError() {
  if (ptype_) {
    PyErr_Restore(ptype_, pvalue_, ptraceback_);
  }
}
