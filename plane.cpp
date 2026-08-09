#include "plane.h"

void Airplane::setFlightData(vec2 latLon, float altitude, float heading)
{
    this->latLon = latLon;
    this->altitude = altitude;
    this->heading = heading;
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
    float r = planetRadius + altitude;

    vec3 pos {
        r * std::cos(latRad) * std::cos(lonRad),
        r * std::sin(latRad),
        r * std::cos(latRad) * std::sin(lonRad),
    };

    vec3 up = glm::normalize(pos);
    vec3 east = glm::normalize(glm::cross(worldNorth, up));
    vec3 north = glm::cross(up, east);

    float headingRad = glm::radians(heading);
    vec3 forward = glm::normalize(north * std::cos(headingRad) + east * std::sin(headingRad));
    vec3 right = glm::cross(up, forward);

    mat4 model {
        vec4(forward, 0.0f), // lokalne X
        vec4(right, 0.0f), // lokalne Y
        vec4(up, 0.0f), // lokalne Z
        vec4(pos, 1.0f), // przesunięcie
    };

    cachedModelMatrix = glm::scale(model, vec3(scale));
    changed = false;
    return cachedModelMatrix;
}