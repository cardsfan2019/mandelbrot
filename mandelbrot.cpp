#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.hpp"
#include "GLShader.hpp"
#include "Texture2D.hpp"
#include "VAO.hpp"
#include "VBO.hpp"
#include "EBO.hpp"

#include <vector>
#include <iostream>
#include <string>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);

// settings
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

float windowDim[] = {
    -1.0f, -1.0f, 2.0f, 2.0f
};

float zoomSpeed = 0.1f;

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Ant Simulation", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetScrollCallback(window, scroll_callback); 
    glfwSetCursorPosCallback(window, mouse_callback);  

    if (!gladLoadGL(glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    int work_grp_cnt[3];
    work_grp_cnt[0] = 0;
    work_grp_cnt[1] = 0;
    work_grp_cnt[2] = 0;

    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &work_grp_cnt[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &work_grp_cnt[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &work_grp_cnt[2]);

    printf("max global (total) work group counts x:%i y:%i z:%i\n",
    work_grp_cnt[0], work_grp_cnt[1], work_grp_cnt[2]);

{
    Shader shader("shader.vs", "shader.fs");
    shader.use();

    // Compile fragment shader
    GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);

    // Read shaders

    std::string compShaderStr = readFile("shader.compute");
    const char *computeShaderSrc = compShaderStr.c_str();

    GLint result = GL_FALSE;
    int logLength;
    std::cout << "Compiling fragment shader." << std::endl;
    glShaderSource(computeShader, 1, &computeShaderSrc, NULL);
    glCompileShader(computeShader);

    // Check fragment shader

    glGetShaderiv(computeShader, GL_COMPILE_STATUS, &result);
    glGetShaderiv(computeShader, GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> computeShaderError((logLength > 1) ? logLength : 1);
    glGetShaderInfoLog(computeShader, logLength, NULL, &computeShaderError[0]);
    std::cout << &computeShaderError[0] << std::endl;

    std::cout << "Linking program" << std::endl;
    GLuint computeShaderProgram = glCreateProgram();
    glAttachShader(computeShaderProgram, computeShader);
    glLinkProgram(computeShaderProgram);

    glGetProgramiv(computeShaderProgram, GL_LINK_STATUS, &result);
    glGetProgramiv(computeShaderProgram, GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> programError( (logLength > 1) ? logLength : 1 );
    glGetProgramInfoLog(computeShaderProgram, logLength, NULL, &programError[0]);
    std::cout << &programError[0] << std::endl;

    glDeleteShader(computeShader);
    const GLchar* shader_source = "shader.compute";

    GLuint ssbo;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(windowDim), windowDim, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo);

    const int tex_w = 512, tex_h = 512;
    GLuint tex_output;
    glGenTextures(1, &tex_output);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_output);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, tex_w, tex_h, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindImageTexture(0, tex_output, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

    float vertices[] = {
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f,  // top right
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,  // bottom right
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,  // bottom left
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f   // top left 
    };

    unsigned int indices[] = {  // note that we start from 0!
        0, 1, 3,  // first Triangle
        1, 2, 3   // second Triangle
    };

    VAO quad_vao;
    VBO quad_vbo;
    EBO quad_ebo;

    quad_vao.bind();
    quad_vbo.bind();
    quad_ebo.bind();

    quad_vbo.init(vertices, sizeof(vertices), GL_STATIC_DRAW);
    quad_ebo.init(indices, sizeof(indices), GL_STATIC_DRAW);
    quad_vao.set(0, 3, GL_FLOAT, 5 * sizeof(float), (void*)0);
    quad_vao.set(1, 2, GL_FLOAT, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    shader.use();
    shader.setInt("texture1", 0);

    while (!glfwWindowShouldClose(window))
    {
        processInput(window);
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;        
        
        glfwPollEvents();
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(computeShaderProgram);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(windowDim), windowDim, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo);
        glDispatchCompute((GLuint)tex_w, (GLuint)tex_h, 1);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        shader.use();
        {
            glClear(GL_COLOR_BUFFER_BIT);
            quad_vao.bind();
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex_output);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
        
        glfwSwapBuffers(window);
    }
}

    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        windowDim[1] += 0.02f * zoomSpeed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        windowDim[1] -= 0.02f * zoomSpeed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        windowDim[0] -= 0.02f * zoomSpeed;    
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        windowDim[0] += 0.02f * zoomSpeed;

}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{

}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    float lx = windowDim[2];
    float ly = windowDim[3];

    windowDim[2] -= yoffset * zoomSpeed;
    windowDim[3] -= yoffset * zoomSpeed;

    windowDim[0] += (lx - windowDim[2]) / 2.0f;
    windowDim[1] += (ly - windowDim[3]) / 2.0f;

    if(yoffset > 0) {
        zoomSpeed *= 0.95f;
    } else {
        zoomSpeed /= 0.95f;
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    lastX = xpos;
    lastY = ypos; 
}