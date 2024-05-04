#ifndef PHYSICS_HPP
#define PHYSICS_HPP

#include "../input_snapshot/input_snapshot.hpp"
#include <glm/glm.hpp>
#include <functional>

std::function<void(double)> physics_step_closure(InputSnapshot *input_snapshot, glm::vec3 *position);

#endif // PHYSICS_HPP
