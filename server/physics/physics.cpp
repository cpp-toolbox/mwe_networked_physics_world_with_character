#include "physics.hpp"
#include <cstdio>
#include <iostream>
#include <ostream>

std::function<void(double)> physics_step_closure(InputSnapshot *input_snapshot, glm::vec3 *position) {
    return [input_snapshot, position](double time_since_last_step) {
        float forward_component = (float)input_snapshot->forward_pressed - (float)input_snapshot->backward_pressed;
        float strafe_component = (float)input_snapshot->right_pressed - (float)input_snapshot->left_pressed;

        printf("physics step\n");

        glm::vec3 input_vector(strafe_component, forward_component, 0);

        if (glm::length(input_vector) != 0) {
            input_vector = glm::normalize(input_vector);
        }

        input_vector = input_vector * (float)time_since_last_step;

        *position += input_vector;

        std::cout << "{" << position->x << " " << position->y << " " << position->z << "}";
    };
}
