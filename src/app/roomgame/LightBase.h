#pragma once

#include <glm/gtx/transform.hpp>

class LightBase
{
public:
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    LightBase(glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular) {
        this->ambient = ambient;
        this->diffuse = diffuse;
        this->specular = specular;
    }
    ~LightBase() {

    }
};

class DirLight : public LightBase {
public:
    glm::vec3 direction;
    DirLight(glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, glm::vec3 direction) :
        LightBase(ambient, diffuse, specular) {
        this->direction = direction;
    }
}; 

class PointLight : public LightBase {
public:
    glm::vec3 position;
    PointLight(glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, float constant, float linear, float quadratic) :
        LightBase(ambient, diffuse, specular) {
        this->constant = constant;
        this->linear = linear;
        this->quadratic = quadratic;
    }
    float constant;
    float linear;
    float quadratic;
};