// Header/Sprite.h
#pragma once
#include <GL/glew.h>
#include <string>

class Sprite {
public:
    GLuint textureID = 0;

    float x = 0.0f;
    float y = 0.0f;
    float width = 0.15f;
    float height = 0.15f;
    float uvRepeatX = 1.0f;
    float uvRepeatY = 1.0f;


    Sprite() = default;

    bool load(const std::string& path);
    bool loadRepeated(const std::string& path);
    void draw(GLuint spriteShader);
};
