#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::mat4;

constexpr float PLANET_RADIUS = 100.0f;
constexpr vec3 WORLD_NORTH {0.0f, 1.0f, 0.0f};
constexpr float FRAMES = 60.0;

class Airplane
{
    public:
        Airplane(){};
        void setFlightData(vec2 latLon, float velocity, float heading);
        void fly();
        void fly(vec2 delta);
        void setScale(float scale);
        mat4 getModelMatrix();
    private:
        vec2 latLon {0.0f};
        float velocity = 0.0f;
        float heading = 0.0f;
        float scale = 0.1f;
        mat4 cachedModelMatrix {1.0f};
        bool changed = true;
};