#include <SDL2/SDL.h>
#include <SDL_events.h>
#include <SDL_render.h>
#include <cstdlib>
#include <glm/ext/quaternion_geometric.hpp>
#include <glm/geometric.hpp>
#include <string>
#include <glm/glm.hpp>
#include <vector>
#include <print.h>

#include "color.h"
#include "intersect.h"
#include "object.h"
#include "sphere.h"
#include "cube.h"
#include "light.h"
#include "camera.h"

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;
const float ASPECT_RATIO = static_cast<float>(SCREEN_WIDTH) / static_cast<float>(SCREEN_HEIGHT);
const int MAX_RECURSION = 5;
const float BIAS = 0.0001f;

SDL_Renderer *renderer;
std::vector<Object *> objects;
Light light(glm::vec3(4, 5, 0), 100, Color(253, 158, 0));
Camera camera(glm::vec3(8.0, 8.0, 8.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 10.0f);

void point(glm::vec2 position, Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawPoint(renderer, position.x, position.y);
}
float castShadow(const glm::vec3 &shadowOrigin, const glm::vec3 &lightDir, Object *hitObject, float shadowSoftness = 0.5f)
{
    float shadowIntensity = 1; // Fully lit by default

    for (auto &obj : objects)
    {
        if (obj != hitObject)
        {
            Intersect shadowIntersect = obj->rayIntersect(shadowOrigin, lightDir);
            if (shadowIntersect.isIntersecting && shadowIntersect.dist > 0)
            {
                // Calculate shadow intensity based on distance to the object casting the shadow
                float shadowFactor = glm::clamp(shadowIntersect.dist * shadowSoftness, 0.0f, 1.0f);
                shadowIntensity = glm::min(shadowIntensity, shadowFactor);
            }
        }
    }
    return shadowIntensity; // Returns a value between 0 and 1
}

Color castRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, const short recursion = 0)
{
    float zBuffer = 99999;
    Object *hitObject = nullptr;
    Intersect intersect;

    for (const auto &object : objects)
    {
        Intersect i = object->rayIntersect(rayOrigin, rayDirection);
        if (i.isIntersecting && i.dist < zBuffer)
        {
            zBuffer = i.dist;
            hitObject = object;
            intersect = i;
        }
    }

    if (!intersect.isIntersecting || recursion == MAX_RECURSION)
    {
        return Color(127, 169, 255); // Background color
    }

    glm::vec3 lightDir = glm::normalize(light.position - intersect.point);
    glm::vec3 viewDir = glm::normalize(rayOrigin - intersect.point);
    // glm::vec3 reflectDir = glm::reflect(-lightDir, intersect.normal);

    float shadowIntensity = castShadow(intersect.point + intersect.normal * BIAS, lightDir, hitObject);
    float diffuseLightIntensity = std::max(0.0f, glm::dot(intersect.normal, lightDir)) * shadowIntensity;
    // float specReflection = glm::dot(viewDir, reflectDir);

    Material mat = hitObject->material;

    // Ambient component
    float ambientStrength = 0.4f; // Base ambient light in the scene
    Color ambientLight = mat.diffuse * ambientStrength;

    // Diffuse component
    float diffuseIntensity = std::max(0.0f, glm::dot(intersect.normal, lightDir)) * shadowIntensity;
    Color diffuseLight = mat.diffuse * light.intensity * diffuseIntensity * mat.albedo;

    // Specular component
    glm::vec3 reflectDir = glm::reflect(-lightDir, intersect.normal);
    float specReflection = glm::dot(viewDir, reflectDir);
    float specLightIntensity = std::pow(std::max(0.0f, specReflection), mat.specularCoefficient);
    Color specularLight = light.color * light.intensity * specLightIntensity * mat.specularAlbedo;

    // Reflection
    Color reflectedColor(0.0f, 0.0f, 0.0f);
    if (mat.reflectivity > 0 && recursion < MAX_RECURSION)
    {
        glm::vec3 reflectOrigin = intersect.point + intersect.normal * BIAS;
        glm::vec3 reflectDir = glm::reflect(rayDirection, intersect.normal);
        reflectedColor = castRay(reflectOrigin, reflectDir, recursion + 1) * mat.reflectivity;
    }

    // Refraction
    Color refractedColor(0.0f, 0.0f, 0.0f);
    if (mat.transparency > 0 && recursion < MAX_RECURSION)
    {
        glm::vec3 refractOrigin = intersect.point - intersect.normal * BIAS;
        glm::vec3 refractDir = glm::refract(rayDirection, intersect.normal, mat.refractionIndex);
        refractedColor = castRay(refractOrigin, refractDir, recursion + 1) * mat.transparency;
    }

    // Combine components
    Color color = ambientLight + diffuseLight + specularLight + reflectedColor + refractedColor;
    return color;
}

void addGrassAndDirtBlock(const glm::vec3 &basePosition, const Material &dirtMaterial, const Material &grassMaterial)
{
    glm::vec3 dirtPosition = basePosition + glm::vec3(0.0f, -0.2f, 0.0f);
    glm::vec3 grassPosition = basePosition;

    objects.push_back(new Cube(dirtPosition, 1.0f, dirtMaterial));
    objects.push_back(new Cube(grassPosition, 1.0f, grassMaterial));
}

void createCentered3x3Grid(const glm::vec3 &center, const Material &dirtMaterial, const Material &grassMaterial)
{
    // Calculate the starting position of the grid based on the center
    glm::vec3 startPosition = center - glm::vec3(1.0f, 0.0f, 1.0f); // Adjust to center the grid

    for (int x = 0; x < 3; ++x)
    {
        for (int z = 0; z < 3; ++z)
        {
            glm::vec3 position = startPosition + glm::vec3(x, 0.0f, z);
            addGrassAndDirtBlock(position, dirtMaterial, grassMaterial);
        }
    }
}

void setUp()
{
    Material grass = {
        Color(0, 230, 0), // diffuse color
        0.2f,             // albedo (proportion of incident light that is diffusely reflected)
        0.2f,             // specularAlbedo (no specular reflection)
        0.0f,             // specularCoefficient (irrelevant when specularAlbedo is 0)
        0.0f,             // reflectivity (no mirror-like reflection)
        0.0f,             // transparency (opaque, not transparent)
        1.0f              // refractionIndex (irrelevant when transparency is 0)
    };

    Material dirt = {
        Color(125, 84, 41), // diffuse color
        0.2f,               // albedo (proportion of incident light that is diffusely reflected)
        0.2f,               // specularAlbedo (no specular reflection)
        1.0f,               // specularCoefficient (irrelevant when specularAlbedo is 0)
        0.0f,               // reflectivity (no mirror-like reflection)
        0.0f,               // transparency (opaque, not transparent)
        1.0f                // refractionIndex (irrelevant when transparency is 0)
    };

    Material water = {
        Color(173, 216, 230), // diffuse color (light blue, but can be adjusted)
        0.1f,                 // albedo (low diffuse reflectivity)
        0.5f,                 // specularAlbedo (moderate specular reflection)
        10.0f,                // specularCoefficient (smooth water surface)
        0.5f,                 // reflectivity (moderate mirror-like reflection)
        0.1f,                 // transparency (highly transparent)
        1.33f                 // refractionIndex (approximate for water)
    };

    Material wood = {
        Color(224, 199, 174), // diffuse color (light blue, but can be adjusted)
        0.2f,                 // albedo (proportion of incident light that is diffusely reflected)
        0.2f,                 // specularAlbedo (no specular reflection)
        0.0f,                 // specularCoefficient (irrelevant when specularAlbedo is 0)
        0.0f,                 // reflectivity (no mirror-like reflection)
        0.0f,                 // transparency (opaque, not transparent)
        1.0f                  // refractionIndex (irrelevant when transparency is 0)
    };

    Material cherryBlossomLeaves = {
        Color(255, 183, 197), // A soft pink color typical of cherry blossoms
        0.6f,                 // Albedo: Leaves are not highly reflective, so a moderate value
        0.8f,                 // SpecularAlbedo: Low specular reflection
        10.0f,                // SpecularCoefficient: A low value for a slightly matte finish
        0.0f,                 // Reflectivity: Leaves are not reflective
        0.0f,                 // Transparency: Leaves are opaque
        1.0f                  // RefractionIndex: Irrelevant for opaque material
    };

    Material stone = {
        Color(128, 128, 128), // diffuse color (grey)
        0.6f,                 // albedo (moderate amount of light is diffusely reflected)
        0.3f,                 // specularAlbedo (some specular highlights)
        30.0f,                // specularCoefficient (for a slightly glossy look)
        0.05f,                // reflectivity (just a bit reflective)
        0.0f,                 // transparency (stone is not transparent)
        1.0f                  // refractionIndex (irrelevant for non-transparent materials)
    };

    Material rubber = {
        Color(80, 0, 0), // diffuse
        0.9,
        0.1,
        10.0f,
        0.0f,
        0.0f};

    Material ivory = {
        Color(100, 100, 80),
        0.5,
        0.5,
        50.0f,
        0.4f,
        0.0f};

    Material mirror = {
        Color(255, 255, 255),
        0.0f,
        10.0f,
        1425.0f,
        0.9f,
        0.0f};

    Material glass = {
        Color(255, 255, 255),
        0.0f,
        10.0f,
        1425.0f,
        0.2f,
        1.0f,

    };
    createCentered3x3Grid(glm::vec3(-2.0f, -1.0f, 2.0f), stone, stone);
    createCentered3x3Grid(glm::vec3(1.0f, -1.0f, 0.0f), stone, stone);
    createCentered3x3Grid(glm::vec3(0, -2.0f, 2), stone, stone);

    createCentered3x3Grid(glm::vec3(0.0f, 0.0f, 0.0f), dirt, grass);
    createCentered3x3Grid(glm::vec3(-1.0f, 1.0f, -1.0f), dirt, grass);
    createCentered3x3Grid(glm::vec3(-2.0f, 0.0f, 1.0f), dirt, grass);

    objects.push_back(new Cube(glm::vec3(-1.0f, 2.0f, -1.0f), 1.0f, wood));
    objects.push_back(new Cube(glm::vec3(-1.0f, 3.0f, -1.0f), 1.0f, wood));
    objects.push_back(new Cube(glm::vec3(-1.0f, 4.0f, -1.0f), 1.0f, wood));

    createCentered3x3Grid(glm::vec3(-1.0f, 4.0f, -1.0f), cherryBlossomLeaves, cherryBlossomLeaves);
    objects.push_back(new Cube(glm::vec3(-1.0f, 5.0f, -1.0f), 1.0f, cherryBlossomLeaves));

    objects.push_back(new Cube(glm::vec3(0.0f, 0.0f, 2.0f), 1.0f, water));

    objects.push_back(new Cube(glm::vec3(0.0f, -1.0f, 2.0f), 1.0f, water));
    objects.push_back(new Cube(glm::vec3(0.0f, -2.0f, 2.0f), 1.0f, water));
    objects.push_back(new Cube(glm::vec3(0.0f, 1.0f, 1.0f), 1.0f, water));
}

void render()
{
    float fov = 3.1415 / 3;
    for (int y = 0; y < SCREEN_HEIGHT; y++)
    {
        for (int x = 0; x < SCREEN_WIDTH; x++)
        {
            /*
            float random_value = static_cast<float>(std::rand())/static_cast<float>(RAND_MAX);
            if (random_value < 0.0) {
                continue;
            }
            */

            float screenX = (2.0f * (x + 0.5f)) / SCREEN_WIDTH - 1.0f;
            float screenY = -(2.0f * (y + 0.5f)) / SCREEN_HEIGHT + 1.0f;
            screenX *= ASPECT_RATIO;
            screenX *= tan(fov / 2.0f);
            screenY *= tan(fov / 2.0f);

            glm::vec3 cameraDir = glm::normalize(camera.target - camera.position);

            glm::vec3 cameraX = glm::normalize(glm::cross(cameraDir, camera.up));
            glm::vec3 cameraY = glm::normalize(glm::cross(cameraX, cameraDir));
            glm::vec3 rayDirection = glm::normalize(
                cameraDir + cameraX * screenX + cameraY * screenY);

            Color pixelColor = castRay(camera.position, rayDirection);
            /* Color pixelColor = castRay(glm::vec3(0,0,20), glm::normalize(glm::vec3(screenX, screenY, -1.0f))); */

            point(glm::vec2(x, y), pixelColor);
        }
    }
}

int main(int argc, char *argv[])
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    // Create a window
    SDL_Window *window = SDL_CreateWindow("Hello World - FPS: 0",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH, SCREEN_HEIGHT,
                                          SDL_WINDOW_SHOWN);

    if (!window)
    {
        SDL_Log("Unable to create window: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create a renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (!renderer)
    {
        SDL_Log("Unable to create renderer: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;

    int frameCount = 0;
    Uint32 startTime = SDL_GetTicks();
    Uint32 currentTime = startTime;

    setUp();

    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = false;
            }

            if (event.type == SDL_KEYDOWN)
            {
                switch (event.key.keysym.sym)
                {
                case SDLK_UP:
                    camera.move(1.0f);
                    break;
                case SDLK_DOWN:
                    camera.move(-1.0f);
                    break;
                case SDLK_LEFT:
                    print("left");
                    camera.rotate(-1.0f, 0.0f);
                    break;
                case SDLK_RIGHT:
                    print("right");
                    camera.rotate(1.0f, 0.0f);
                    break;
                }
            }
        }

        // Clear the screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        render();

        // Present the renderer
        SDL_RenderPresent(renderer);

        frameCount++;

        // Calculate and display FPS
        if (SDL_GetTicks() - currentTime >= 1000)
        {
            currentTime = SDL_GetTicks();
            std::string title = "Hello World - FPS: " + std::to_string(frameCount);
            SDL_SetWindowTitle(window, title.c_str());
            frameCount = 0;
        }
    }

    // Cleanup
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
