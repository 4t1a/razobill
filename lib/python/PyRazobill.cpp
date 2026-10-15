// PyRazobill: Pybind11 Python bindings for librazobill.
// Provides zero-allocation C++20 parsing, exact lossless joining, and multi-core batch processing.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "librazobill/Slots.hpp"
#include "librazobill/ThreadPool.hpp"
#include "librazobill/Version.hpp"

namespace py = pybind11;
using namespace razobill;

namespace {

core::ScanMode parseMode(const std::string& mode) {
    if (mode == "semantic") return core::ScanMode::Semantic;
    if (mode == "exact") return core::ScanMode::Exact;
    throw py::value_error("[razobill] What happened: Invalid scan mode. "
                          "Why: Expected 'semantic' or 'exact', got '" +
                          mode +
                          "'. "
                          "What to do: Pass mode='semantic' (default) or mode='exact'.");
}

std::string truncateOffending(std::string_view sv, size_t maxLen = 80) {
    if (sv.size() <= maxLen) return std::string(sv);
    return std::string(sv.substr(0, maxLen - 3)) + "...";
}

std::string formatJoinError(Status st, std::string_view tmpl) {
    std::string why;
    switch (st) {
    case Status::SlotUnderflow:
        why = "Count mismatch (template has more or fewer placeholders than provided values)";
        break;
    case Status::Malformed:
        why = "Malformed template syntax or unclosed brace";
        break;
    case Status::TooLarge:
        why = "Reconstructed message exceeds maximum permitted size (16 MB)";
        break;
    default:
        why = std::string(razobill::describe(st));
        break;
    }
    return "[razobill] What happened: Failed to join log components. "
           "Why: " +
           why +
           ". "
           "What to do: Verify that {i} and {s} placeholders match the provided integers and strings. "
           "Offending template: '" +
           truncateOffending(tmpl) + "'.";
}

py::tuple splitOne(const py::object& input, const std::string& modeStr) {
    const core::ScanMode mode = parseMode(modeStr);

    if (py::isinstance<py::bytes>(input)) {
        py::bytes b = input.cast<py::bytes>();
        char* buf = nullptr;
        Py_ssize_t len = 0;
        PYBIND11_BYTES_AS_STRING_AND_SIZE(b.ptr(), &buf, &len);
        std::string_view sv(buf, static_cast<size_t>(len));

        if (sv.empty()) {
            return py::make_tuple(py::bytes(""), py::list(), py::list());
        }

        core::SlotRecord slots;
        Status st = core::parse(sv, slots, mode);
        if (st != Status::Ok) {
            throw py::value_error("[razobill] What happened: Parsing failed. Why: " +
                                  std::string(razobill::describe(st)) + ". What to do: Check input data integrity.");
        }

        py::bytes tmpl(slots.templateStr.data(), slots.templateStr.size());
        py::list intList;
        for (int64_t v : slots.ints)
            intList.append(v);
        py::list strList;
        for (const auto& s : slots.strs)
            strList.append(py::bytes(s.data(), s.size()));

        return py::make_tuple(tmpl, intList, strList);
    }

    if (py::isinstance<py::str>(input)) {
        PyObject* encoded = PyUnicode_AsEncodedString(input.ptr(), "utf-8", "surrogateescape");
        if (!encoded) throw py::error_already_set();

        char* buf = PyBytes_AS_STRING(encoded);
        Py_ssize_t len = PyBytes_GET_SIZE(encoded);
        std::string_view sv(buf, static_cast<size_t>(len));

        if (sv.empty()) {
            Py_DECREF(encoded);
            return py::make_tuple(py::str(""), py::list(), py::list());
        }

        core::SlotRecord slots;
        Status st = core::parse(sv, slots, mode);
        if (st != Status::Ok) {
            Py_DECREF(encoded);
            throw py::value_error("[razobill] What happened: Parsing failed. Why: " +
                                  std::string(razobill::describe(st)) + ". What to do: Check input data integrity.");
        }

        PyObject* pyTmpl = PyUnicode_DecodeUTF8(slots.templateStr.data(),
                                                static_cast<Py_ssize_t>(slots.templateStr.size()), "surrogateescape");
        if (!pyTmpl) {
            Py_DECREF(encoded);
            throw py::error_already_set();
        }
        py::object tmplObj = py::reinterpret_steal<py::object>(pyTmpl);

        py::list intList;
        for (int64_t v : slots.ints)
            intList.append(v);

        py::list strList;
        for (const auto& s : slots.strs) {
            PyObject* pyS = PyUnicode_DecodeUTF8(s.data(), static_cast<Py_ssize_t>(s.size()), "surrogateescape");
            if (!pyS) {
                Py_DECREF(encoded);
                throw py::error_already_set();
            }
            strList.append(py::reinterpret_steal<py::object>(pyS));
        }

        Py_DECREF(encoded);
        return py::make_tuple(tmplObj, intList, strList);
    }

    std::string typeName = py::str(input.get_type());
    throw py::type_error("[razobill] What happened: Unsupported input type. "
                         "Why: Expected str or bytes, got " +
                         typeName +
                         ". "
                         "What to do: Pass a str or bytes object.");
}

py::object templateOne(const py::object& input, const std::string& modeStr) {
    const core::ScanMode mode = parseMode(modeStr);

    if (py::isinstance<py::bytes>(input)) {
        py::bytes b = input.cast<py::bytes>();
        char* buf = nullptr;
        Py_ssize_t len = 0;
        PYBIND11_BYTES_AS_STRING_AND_SIZE(b.ptr(), &buf, &len);
        std::string_view sv(buf, static_cast<size_t>(len));

        if (sv.empty()) return py::bytes("");

        core::SlotRecord slots;
        Status st = core::parse(sv, slots, mode);
        if (st != Status::Ok) {
            throw py::value_error("[razobill] What happened: Parsing failed. Why: " +
                                  std::string(razobill::describe(st)) + ". What to do: Check input data integrity.");
        }
        return py::bytes(slots.templateStr.data(), slots.templateStr.size());
    }

    if (py::isinstance<py::str>(input)) {
        PyObject* encoded = PyUnicode_AsEncodedString(input.ptr(), "utf-8", "surrogateescape");
        if (!encoded) throw py::error_already_set();

        char* buf = PyBytes_AS_STRING(encoded);
        Py_ssize_t len = PyBytes_GET_SIZE(encoded);
        std::string_view sv(buf, static_cast<size_t>(len));

        if (sv.empty()) {
            Py_DECREF(encoded);
            return py::str("");
        }

        core::SlotRecord slots;
        Status st = core::parse(sv, slots, mode);
        if (st != Status::Ok) {
            Py_DECREF(encoded);
            throw py::value_error("[razobill] What happened: Parsing failed. Why: " +
                                  std::string(razobill::describe(st)) + ". What to do: Check input data integrity.");
        }

        PyObject* pyTmpl = PyUnicode_DecodeUTF8(slots.templateStr.data(),
                                                static_cast<Py_ssize_t>(slots.templateStr.size()), "surrogateescape");
        Py_DECREF(encoded);
        if (!pyTmpl) throw py::error_already_set();
        return py::reinterpret_steal<py::object>(pyTmpl);
    }

    std::string typeName = py::str(input.get_type());
    throw py::type_error("[razobill] What happened: Unsupported input type. "
                         "Why: Expected str or bytes, got " +
                         typeName +
                         ". "
                         "What to do: Pass a str or bytes object.");
}

py::object joinOne(const py::object& tmplObj, const py::sequence& intsSeq, const py::sequence& strsSeq) {
    const bool isBytes = py::isinstance<py::bytes>(tmplObj);
    const bool isStr = py::isinstance<py::str>(tmplObj);

    if (!isBytes && !isStr) {
        std::string typeName = py::str(tmplObj.get_type());
        throw py::type_error("[razobill] What happened: Unsupported template type. "
                             "Why: Expected str or bytes, got " +
                             typeName +
                             ". "
                             "What to do: Pass a str or bytes template.");
    }

    std::string tmplStorage;
    std::string_view tmplView;

    PyObject* encodedTmpl = nullptr;
    if (isBytes) {
        char* buf = nullptr;
        Py_ssize_t len = 0;
        PYBIND11_BYTES_AS_STRING_AND_SIZE(tmplObj.ptr(), &buf, &len);
        tmplView = std::string_view(buf, static_cast<size_t>(len));
    } else {
        encodedTmpl = PyUnicode_AsEncodedString(tmplObj.ptr(), "utf-8", "surrogateescape");
        if (!encodedTmpl) throw py::error_already_set();
        tmplView = std::string_view(PyBytes_AS_STRING(encodedTmpl), static_cast<size_t>(PyBytes_GET_SIZE(encodedTmpl)));
    }

    std::vector<int64_t> ints;
    ints.reserve(py::len(intsSeq));
    for (auto item : intsSeq) {
        ints.push_back(item.cast<int64_t>());
    }

    std::vector<PyObject*> encodedStrs;
    std::vector<std::string_view> strViews;
    strViews.reserve(py::len(strsSeq));

    auto cleanup = [&]() {
        if (encodedTmpl) Py_DECREF(encodedTmpl);
        for (PyObject* obj : encodedStrs) {
            Py_DECREF(obj);
        }
    };

    for (auto item : strsSeq) {
        if (py::isinstance<py::bytes>(item)) {
            char* buf = nullptr;
            Py_ssize_t len = 0;
            PYBIND11_BYTES_AS_STRING_AND_SIZE(item.ptr(), &buf, &len);
            strViews.emplace_back(buf, static_cast<size_t>(len));
        } else if (py::isinstance<py::str>(item)) {
            PyObject* enc = PyUnicode_AsEncodedString(item.ptr(), "utf-8", "surrogateescape");
            if (!enc) {
                cleanup();
                throw py::error_already_set();
            }
            encodedStrs.push_back(enc);
            strViews.emplace_back(PyBytes_AS_STRING(enc), static_cast<size_t>(PyBytes_GET_SIZE(enc)));
        } else {
            cleanup();
            std::string tn = py::str(item.get_type());
            throw py::type_error("[razobill] What happened: Unsupported string slot element type. "
                                 "Why: Expected str or bytes in strs list, got " +
                                 tn +
                                 ". "
                                 "What to do: Ensure all elements in strs are str or bytes.");
        }
    }

    std::string result;
    Status st = core::reconstruct(tmplView, ints, strViews, result);
    if (st != Status::Ok) {
        std::string err = formatJoinError(st, tmplView);
        cleanup();
        throw py::value_error(err);
    }

    if (isBytes) {
        cleanup();
        return py::bytes(result.data(), result.size());
    } else {
        PyObject* pyRes =
            PyUnicode_DecodeUTF8(result.data(), static_cast<Py_ssize_t>(result.size()), "surrogateescape");
        cleanup();
        if (!pyRes) throw py::error_already_set();
        return py::reinterpret_steal<py::object>(pyRes);
    }
}

py::list splitMany(const py::sequence& inputs, const std::string& modeStr) {
    const core::ScanMode mode = parseMode(modeStr);
    const size_t total = py::len(inputs);

    struct InputItem {
        std::string bytes;
        bool isBytes;
    };

    std::vector<InputItem> rawInputs;
    rawInputs.reserve(total);

    for (size_t i = 0; i < total; ++i) {
        py::object item = inputs[i];
        if (py::isinstance<py::bytes>(item)) {
            char* buf = nullptr;
            Py_ssize_t len = 0;
            PYBIND11_BYTES_AS_STRING_AND_SIZE(item.ptr(), &buf, &len);
            rawInputs.push_back(InputItem{std::string(buf, static_cast<size_t>(len)), true});
        } else if (py::isinstance<py::str>(item)) {
            PyObject* enc = PyUnicode_AsEncodedString(item.ptr(), "utf-8", "surrogateescape");
            if (!enc) throw py::error_already_set();
            rawInputs.push_back(
                InputItem{std::string(PyBytes_AS_STRING(enc), static_cast<size_t>(PyBytes_GET_SIZE(enc))), false});
            Py_DECREF(enc);
        } else {
            std::string tn = py::str(item.get_type());
            throw py::type_error("[razobill] What happened: Unsupported item type in split_many at index " +
                                 std::to_string(i) + ". Why: Expected str or bytes, got " + tn +
                                 ". "
                                 "What to do: Pass an iterable of str or bytes.");
        }
    }

    std::vector<core::SlotRecord> records(total);

    // Release GIL for multi-threaded parsing
    {
        py::gil_scoped_release release;
        if (total > 128) {
            static core::ThreadPool pool;
            pool.parallelFor(total, 256, [&](size_t start, size_t end) {
                for (size_t idx = start; idx < end; ++idx) {
                    if (!rawInputs[idx].bytes.empty()) {
                        (void)core::parse(rawInputs[idx].bytes, records[idx], mode);
                    }
                }
            });
        } else {
            for (size_t idx = 0; idx < total; ++idx) {
                if (!rawInputs[idx].bytes.empty()) {
                    (void)core::parse(rawInputs[idx].bytes, records[idx], mode);
                }
            }
        }
    }

    // Build Python return list with GIL reacquired
    py::list outList;
    for (size_t i = 0; i < total; ++i) {
        const auto& rec = records[i];
        const bool itemIsBytes = rawInputs[i].isBytes;

        py::list intList;
        for (int64_t v : rec.ints)
            intList.append(v);

        if (itemIsBytes) {
            py::bytes tmpl(rec.templateStr.data(), rec.templateStr.size());
            py::list strList;
            for (const auto& s : rec.strs) {
                strList.append(py::bytes(s.data(), s.size()));
            }
            outList.append(py::make_tuple(tmpl, intList, strList));
        } else {
            PyObject* pyTmpl = PyUnicode_DecodeUTF8(rec.templateStr.data(),
                                                    static_cast<Py_ssize_t>(rec.templateStr.size()), "surrogateescape");
            if (!pyTmpl) throw py::error_already_set();
            py::object tmplObj = py::reinterpret_steal<py::object>(pyTmpl);

            py::list strList;
            for (const auto& s : rec.strs) {
                PyObject* pyS = PyUnicode_DecodeUTF8(s.data(), static_cast<Py_ssize_t>(s.size()), "surrogateescape");
                if (!pyS) throw py::error_already_set();
                strList.append(py::reinterpret_steal<py::object>(pyS));
            }
            outList.append(py::make_tuple(tmplObj, intList, strList));
        }
    }

    return outList;
}

} // namespace

PYBIND11_MODULE(Core, m) {
    m.doc() = "Razobill native C++20 SIMD core module";

    m.def("split", &splitOne, py::arg("input"), py::arg("mode") = "semantic",
          "Splits a single log line (str or bytes) into (template, ints, strs)");
    m.def("template", &templateOne, py::arg("input"), py::arg("mode") = "semantic",
          "Extracts template from a single log line (str or bytes) without allocating slot lists");
    m.def("join", &joinOne, py::arg("template"), py::arg("ints"), py::arg("strs"),
          "Reconstructs original log line from template, ints, and strs");
    m.def("split_many", &splitMany, py::arg("inputs"), py::arg("mode") = "semantic",
          "Splits an iterable of log lines with GIL release across worker threads");
    m.def("version", []() { return std::string(LIBRAZOBILL_VERSION_STRING); }, "Returns semantic library version");
}
