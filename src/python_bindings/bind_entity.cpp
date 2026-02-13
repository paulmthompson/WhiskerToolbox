#include "bind_module.hpp"

#include <pybind11/operators.h>
#include <pybind11/stl.h>

#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityTypes.hpp"

#include <functional>
#include <string>

void init_entity(py::module_ & m) {

    // --- EntityId ---
    py::class_<EntityId>(m, "EntityId",
        "Opaque identifier for a discrete entity (point, line, event, etc.)")
        .def(py::init<>())
        .def(py::init<uint64_t>(), py::arg("value"))
        .def_readonly("id", &EntityId::id)
        .def("__repr__", [](EntityId const & e) {
            return "EntityId(" + std::to_string(e.id) + ")";
        })
        .def("__hash__", [](EntityId const & e) {
            return std::hash<EntityId>{}(e);
        })
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def(py::self < py::self);

    // --- GroupDescriptor ---
    py::class_<GroupDescriptor>(m, "GroupDescriptor",
        "Metadata for an entity group")
        .def_readonly("id", &GroupDescriptor::id)
        .def_readonly("name", &GroupDescriptor::name)
        .def_readonly("description", &GroupDescriptor::description)
        .def_readonly("entity_count", &GroupDescriptor::entity_count)
        .def("__repr__", [](GroupDescriptor const & g) {
            return "GroupDescriptor('" + g.name + "', entities="
                   + std::to_string(g.entity_count) + ")";
        });

    // --- EntityGroupManager ---
    py::class_<EntityGroupManager>(m, "EntityGroupManager",
        "Manages named groups of entities")
        .def("createGroup", &EntityGroupManager::createGroup,
             py::arg("name"), py::arg("description") = "",
             "Create a new group, returning its ID")
        .def("deleteGroup", &EntityGroupManager::deleteGroup,
             py::arg("group_id"))
        .def("hasGroup", &EntityGroupManager::hasGroup,
             py::arg("group_id"))
        .def("getGroupDescriptor", &EntityGroupManager::getGroupDescriptor,
             py::arg("group_id"))
        .def("getAllGroupDescriptors", &EntityGroupManager::getAllGroupDescriptors,
             "Get descriptors for all groups")
        .def("addEntityToGroup", &EntityGroupManager::addEntityToGroup,
             py::arg("group_id"), py::arg("entity_id"))
        .def("removeEntityFromGroup", &EntityGroupManager::removeEntityFromGroup,
             py::arg("group_id"), py::arg("entity_id"))
        .def("getEntitiesInGroup", &EntityGroupManager::getEntitiesInGroup,
             py::arg("group_id"),
             "Get all entity IDs in a group")
        .def("isEntityInGroup", &EntityGroupManager::isEntityInGroup,
             py::arg("group_id"), py::arg("entity_id"))
        .def("getGroupsContainingEntity", &EntityGroupManager::getGroupsContainingEntity,
             py::arg("entity_id"))
        .def("getGroupSize", &EntityGroupManager::getGroupSize,
             py::arg("group_id"))
        .def("getGroupCount", &EntityGroupManager::getGroupCount)
        .def("clearGroup", &EntityGroupManager::clearGroup,
             py::arg("group_id"));
}
