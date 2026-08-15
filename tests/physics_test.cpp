#include "physics.hpp"

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main() {
    try {
        require(gh::scaled_toy_speed16(120, 2000, 1200, 800) == 480, "full speed changed");
        require(gh::scaled_toy_speed16(120, 1600, 1200, 800) == 240, "slowdown changed");
        require(gh::scaled_toy_speed16(120, 1200, 1200, 800) == 0, "decay cutoff changed");
        require(
            gh::vector_from_original_heading(480, 0).x16 == 480 &&
                gh::vector_from_original_heading(480, 0).y16 == 0,
            "heading zero changed"
        );
        require(
            gh::vector_from_original_heading(480, 16).x16 == 0 &&
                gh::vector_from_original_heading(480, 16).y16 == 480,
            "quarter-turn heading changed"
        );
        require(
            gh::approach_original_velocity(-8, -1, 5) == -6,
            "signed arithmetic shift behavior changed"
        );
        {
            const gh::ContactMotionBody bodies[]{
                {16, 8, 32, 0, 8},
                {-8, -4, -16, 0, 24},
            };
            const gh::MotionVector velocity = gh::integrate_original_contact_group(bodies, 5);
            require(
                velocity.x16 == -2 && velocity.y16 == 0,
                "contact-group mass/friction integration changed"
            );
        }
        {
            const gh::ContactMotionBody bodies[]{
                {16, 8, 32, 0, 8, true},
                {-8, -4, -16, 0, 24},
            };
            const gh::MotionVector velocity = gh::integrate_original_contact_group(bodies, 5);
            require(
                velocity.x16 == 0 && velocity.y16 == 0,
                "motion-blocking contact group did not stop"
            );
        }
        {
            gh::CollisionBody first{0, 0, 16, 0, 8};
            gh::CollisionBody second{32, 0, -8, 0, 8};
            require(gh::resolve_original_collision(first, second), "head-on collision rejected");
            require(
                first.velocity_x16 == -8 && second.velocity_x16 == 16,
                "equal-mass velocities were not exchanged"
            );
        }
        {
            gh::CollisionBody first{0, 0, 16, 8, 8};
            gh::CollisionBody second{32, 0, -8, -4, 24};
            require(gh::resolve_original_collision(first, second), "unequal collision rejected");
            require(
                first.velocity_x16 == -20 && first.velocity_y16 == 8 &&
                    second.velocity_x16 == 4 && second.velocity_y16 == -4,
                "mass-weighted normal response or tangent preservation changed"
            );
        }
        {
            gh::CollisionBody first{0, 0, -8, 3, 8};
            gh::CollisionBody second{32, 0, 8, -2, 24};
            require(!gh::resolve_original_collision(first, second), "separating pair was resolved");
            require(
                first.velocity_x16 == -8 && second.velocity_x16 == 8,
                "rejected pair was mutated"
            );
        }
        {
            gh::CollisionBody moving{0, 0, 16, 5, 8};
            gh::CollisionBody wall{32, 0, 0, 0, 1, true, true};
            require(gh::resolve_original_collision(moving, wall), "blocking collision rejected");
            require(
                moving.velocity_x16 == -16 && moving.velocity_y16 == 5 &&
                    wall.velocity_x16 == 0 && wall.velocity_y16 == 0,
                "motion-blocking branch changed"
            );
        }
        {
            gh::CollisionBody first{0, 0, 16, 0, 8};
            gh::CollisionBody second{0, 0, -16, 0, 8};
            require(!gh::resolve_original_collision(first, second), "coincident pair was resolved");
        }
        std::cout << "validated original fixed-point collision solver\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
