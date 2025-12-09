// Source/Sprite.cpp
#include "Sprite.h"
#include <iostream>
#include "Header/stb_image.h"

#define STB_IMAGE_IMPLEMENTATION

bool Sprite::load(const std::string& path)
{
    // okretanje da ne budu naopak
    stbi_set_flip_vertically_on_load(1);

    int w, h, ch;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data) {
        std::cout << "Nisam uspeo da ucitam teksturu: " << path << "\n";
        return false;
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // OVDE: clamp da ostali sprite-ovi ne tile-uju
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(data);
    return true;
}

bool Sprite::loadRepeated(const std::string& path)
{
    stbi_set_flip_vertically_on_load(1);

    int w, h, ch;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data) {
        std::cout << "Greska: ne mogu ucitati " << path << "\n";
        return false;
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // --- VAŽNO: ponavljanje (tiling) ---
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // --- linearni filtering + mipmap ---
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // --- upload ---
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    return true;
}

void Sprite::draw(GLuint spriteShader)
{
    if (textureID == 0) return;

    glUseProgram(spriteShader);
    glBindTexture(GL_TEXTURE_2D, textureID);

    float left = x - width * 0.5f;
    float right = x + width * 0.5f;
    float bottom = y - height * 0.5f;
    float top = y + height * 0.5f;

    // 6 temena (2 trougla), svako ima (pos.x, pos.y, uv.x, uv.y)
    float vertices[] = {
        // prvi trougao
        left,  bottom,  0.0f,        0.0f,
        right, bottom,  uvRepeatX,   0.0f,
        right, top,     uvRepeatX,   uvRepeatY,
        // drugi trougao
        left,  bottom,  0.0f,        0.0f,
        right, top,     uvRepeatX,   uvRepeatY,
        left,  top,     0.0f,        uvRepeatY
    };

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0); // pozicija
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1); // UV
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
}
