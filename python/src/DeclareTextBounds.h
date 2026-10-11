#pragma once
#include "TextBounds/TextBounds.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

inline void declare_text_bounds(pybind11::module_ &m) {
  namespace py = pybind11;
  using Engine = NAMESPACE_PSAPI::TextBoundsEngine;
  py::class_<Engine>(m, "TextBoundsEngine")
      .def(py::init([](py::bytes data, unsigned face_index) {
             return std::make_unique<Engine>(static_cast<std::string>(data),
                                             face_index);
           }),
           py::arg("font_data"), py::arg("face_index") = 0)
      .def("measure", &Engine::measure, py::arg("text"), py::arg("size"),
           py::arg("position"), py::arg("leading") = 1.2,
           py::arg("mode") = "metrics", py::arg("aa") = "smooth",
           py::arg("baseline_direction") = 1,
           py::arg("stylistic_alternates") = false,
           py::arg("old_style") = false, py::arg("orientation") = "vertical",
           py::call_guard<py::gil_scoped_release>())
      .def("optical_adjustment", &Engine::optical_adjustment,
           py::arg("left_glyph"), py::arg("right_glyph"), py::arg("size"),
           py::call_guard<py::gil_scoped_release>());
}
