#include "plane.h"
#include "glm/trigonometric.hpp"
#include <cmath>

void Airplane::setFlightData(vec2 latLon, float velocity, float heading)
{
    this->latLon = latLon;
    this->velocity = velocity;
    this->heading = heading;
    changed = true;
}

void Airplane::fly()
{
    float latRad = glm::radians(latLon.x);
    float lonRad = glm::radians(latLon.y);
    float headingRad = glm::radians(heading);
    float d = velocity / FRAMES / 1000.0f;
    float q = d / PLANET_RADIUS;
    float lat2Rad = glm::asin(sin(latRad) * cos(q) + cos(latRad) * sin(q) * cos(headingRad));
    float y = std::sin(headingRad) * std::sin(q) * std::cos(latRad);
    float x = std::cos(q) - std::sin(latRad) * std::sin(lat2Rad);
    float lon2Rad = lonRad + glm::atan(y, x);
    latLon = {glm::degrees(lat2Rad), glm::degrees(lon2Rad)};
    changed = true;
}

void Airplane::fly(vec2 delta)
{
    latLon += delta;
    changed = true;
}

void Airplane::setScale(float scale)
{
    this->scale = scale;
    changed = true;
}

mat4 Airplane::getModelMatrix()
{
    if (!changed)
    {
        return cachedModelMatrix;
    }

    float latRad = glm::radians(latLon.x);
    float lonRad = glm::radians(latLon.y);

    vec3 pos {
        PLANET_RADIUS * std::cos(latRad) * std::cos(lonRad),
        PLANET_RADIUS * std::sin(latRad),
        PLANET_RADIUS * std::cos(latRad) * std::sin(lonRad),
    };

    vec3 up = glm::normalize(pos);
    
    vec3 east = glm::normalize(glm::cross(up, WORLD_NORTH));
    vec3 north = glm::cross(east, up);

    float headingRad = glm::radians(heading);
    vec3 forward = glm::normalize(north * std::cos(headingRad) + east * std::sin(headingRad));
    
    vec3 right = glm::cross(forward, up);

    mat4 model {
        vec4(forward, 0.0f), // lokalna oś X modelu (Dziób)
        vec4(right, 0.0f),   // lokalna oś Y modelu (Prawe skrzydło)
        vec4(up, 0.0f),      // lokalna oś Z modelu (Góra)
        vec4(pos, 1.0f),     // Pozycja
    };

    cachedModelMatrix = glm::scale(model, vec3(scale));
    changed = false;
    return cachedModelMatrix;
}