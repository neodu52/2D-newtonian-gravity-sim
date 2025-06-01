// main.cpp
#define _USE_MATH_DEFINES
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <GL/freeglut.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <cmath>
#include <vector>
#include <chrono>
#include <string>
#include <sstream>
#include <iostream>

const unsigned int INIT_WIDTH = 800;
const unsigned int INIT_HEIGHT = 600;

const float G = 6.67430e-11f;
const float RESTITUTION = 0.8f;
const float IMPULSE_STRENGTH = 2.5f;

int windowWidth = INIT_WIDTH;
int windowHeight = INIT_HEIGHT;
bool isPaused = false;

struct Body {
    float x, y;
    float vx, vy;
    float mass;
    float radius;
    bool isStatic;
    float r, g, b;
};

std::vector<Body> bodies;
int selectedBodyIndex = -1;

Body createBody(float x, float y, float vx, float vy, float mass_kg, float radius, bool isStatic, float r, float g, float b) {
    return Body{ x, y, vx, vy, mass_kg, radius, isStatic, r, g, b };
}
// c le gui
void ShowBodyEditor(bool isPaused) {
    if (!isPaused || selectedBodyIndex < 0 || selectedBodyIndex >= bodies.size()) return;
    Body& b = bodies[selectedBodyIndex];
    ImGui::Begin("Edit Selected Body");
    ImGui::Text("Editing body #%d", selectedBodyIndex);
    ImGui::Checkbox("Static", &b.isStatic);
    ImGui::SliderFloat("Mass (kg)", &b.mass, 0.1f, 1.0e10);
    ImGui::SliderFloat("Radius", &b.radius, 0.005f, 0.2f);
    ImGui::InputFloat2("Position (x,y)", &b.x);
    ImGui::InputFloat2("Velocity (vx,vy)", &b.vx);
    ImGui::ColorEdit3("Color", &b.r);
    if (ImGui::Button("Delete Planet")) {
        bodies.erase(bodies.begin() + selectedBodyIndex);
        selectedBodyIndex = -1;
        ImGui::End();
        return;
    }
    ImGui::End();
}

// c pour les shaders
GLuint createCircleVAO(int segments) {
    std::vector<float> vertices = { 0.0f, 0.0f };
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * M_PI * i / segments;
        vertices.push_back(cos(angle));
        vertices.push_back(sin(angle));
    }
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    return VAO;
}
// c le shader
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}
// c la fenetre openGL
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    windowWidth = width;
    windowHeight = height;
    glViewport(0, 0, width, height);
}
// c la fonction freeglut pour les touches
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (action != GLFW_PRESS) return;

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);

    float x = ((float)mx / windowWidth) * 2.0f - 1.0f;
    float y = -(((float)my / windowHeight) * 2.0f - 1.0f);
    // c pour spawn des cercles
    if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        bodies.push_back(createBody(x, y, 0.0f, 0.0f, 1.0f, 0.03f, false, 1.0f, 1.0f, 1.0f));
    }
    // c pour ouvrire l'ui
    else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        selectedBodyIndex = -1;
        for (size_t i = 0; i < bodies.size(); ++i) {
            float dx = x - bodies[i].x;
            float dy = y - bodies[i].y;
            if (dx * dx + dy * dy <= bodies[i].radius * bodies[i].radius) {
                selectedBodyIndex = static_cast<int>(i);
                break;
            }
        }
    }
}

void key_callback(GLFWwindow* window, int key, int, int action, int) {
    if (action == GLFW_PRESS && key == GLFW_KEY_SPACE) isPaused = !isPaused;
}
// c tout les math de la gravité
void applyGravitationalForces(float dt) {
    for (size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].isStatic) continue;
        float ax = 0, ay = 0;
        for (size_t j = 0; j < bodies.size(); ++j) {
            if (i == j) continue;
            float dx = bodies[j].x - bodies[i].x;
            float dy = bodies[j].y - bodies[i].y;
            float dist2 = dx * dx + dy * dy;
            float dist = std::sqrt(dist2) + 1e-6f;
            float force = G * bodies[i].mass * bodies[j].mass / dist2;
            ax += force * dx / dist / bodies[i].mass;
            ay += force * dy / dist / bodies[i].mass;
        }
        bodies[i].vx += ax * dt;
        bodies[i].vy += ay * dt;
    }
}
// ca c pr update la positions des corps
void updateBodies(float dt) {
    for (Body& b : bodies) {
        if (b.isStatic) continue;
        b.x += b.vx * dt;
        b.y += b.vy * dt;
        if (b.y - b.radius < -1.0f) {
            b.y = -1.0f + b.radius;
            b.vy *= -RESTITUTION;
        }
        else if (b.y + b.radius > 1.0f) {
            b.y = 1.0f - b.radius;
            b.vy *= -RESTITUTION;
        }
        if (b.x - b.radius < -1.0f) {
            b.x = -1.0f + b.radius;
            b.vx *= -RESTITUTION;
        }
        else if (b.x + b.radius > 1.0f) {
            b.x = 1.0f - b.radius;
            b.vx *= -RESTITUTION;
        }
    }
}
// ba c le main, c la boucle principale en gros
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(INIT_WIDTH, INIT_HEIGHT, "Gravity Simulation", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        uniform vec2 offset;
        uniform float aspect;
        uniform float scale;
        void main() {
            vec2 pos = aPos * scale;
            gl_Position = vec4(vec2(pos.x * aspect, pos.y) + offset, 0.0, 1.0);
        })");

    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, R"(
        #version 330 core
        out vec4 FragColor;
        uniform vec3 color;
        void main() {
            FragColor = vec4(color, 1.0);
        })");

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLuint circleVAO = createCircleVAO(60);

    //x, y, vx, vy, mass_kg, radius, isStatic, r, g, b
    bodies.push_back(createBody(-0.3f, 0.0f, 0.0f, 0.0f, 5.0f, 0.05f, false, 0.0f, 0.7f, 1.0f));
    bodies.push_back(createBody(0.3f, 0.0f, 0.0f, 0.0f, 50.0f, 0.1f, true, 1.0f, 0.9f, 0.2f));

    auto lastTime = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window)) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        if (!isPaused) {
            applyGravitationalForces(deltaTime);
            updateBodies(deltaTime);
        }

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        float aspect = (float)windowHeight / (float)windowWidth;
        int aspectLoc = glGetUniformLocation(shaderProgram, "aspect");
        int offsetLoc = glGetUniformLocation(shaderProgram, "offset");
        int scaleLoc = glGetUniformLocation(shaderProgram, "scale");
        int colorLoc = glGetUniformLocation(shaderProgram, "color");
        glUniform1f(aspectLoc, aspect);
        glBindVertexArray(circleVAO);

        for (const Body& b : bodies) {
            glUniform2f(offsetLoc, b.x, b.y);
            glUniform1f(scaleLoc, b.radius);
            glUniform3f(colorLoc, b.r, b.g, b.b);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 62);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ShowBodyEditor(isPaused);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}