/* danielsinkin97@gmail.com */

#include <SDL.h>
#include <glad/glad.h>

#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl.h"
#include "imgui.h"

#include "stb_image.h"
#include "stb_image_write.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

using Color = glm::vec3;
using Position = glm::vec2; // (y, x)
using gl_VAO = GLuint;
using gl_VBO = GLuint;
using gl_EBO = GLuint;
using gl_Shader = GLuint;
using gl_ShaderProgram = GLuint;
using gl_UBO = GLuint;

auto panic(const std::string &message) -> void {
    std::cerr << "PANIC: " << message << std::endl;
    std::exit(EXIT_FAILURE);
}

struct Constants {
    static constexpr std::string_view window_title = "Breakout";
    static constexpr int window_width = 1280;
    static constexpr int window_height = 720;
    static constexpr float aspect_ratio = static_cast<float>(window_width) / window_height;

    static constexpr float path_marker_width = 0.05f / aspect_ratio;
    static constexpr float path_marker_height = 0.05f;

    static constexpr std::array<float, 12> square_vertices = {
        1.0f, -1.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f};

    static constexpr std::array<unsigned int, 6> square_indices = {
        0, 1, 3,
        1, 2, 3};

    static constexpr std::array<float, 51> circle_vertices = {
        0.000000f, 0.000000f, 0.000000f,
        0.500000f, 0.000000f, 0.000000f,
        0.461940f, 0.191342f, 0.000000f,
        0.353553f, 0.353553f, 0.000000f,
        0.191342f, 0.461940f, 0.000000f,
        0.000000f, 0.500000f, 0.000000f,
        -0.191342f, 0.461940f, 0.000000f,
        -0.353553f, 0.353553f, 0.000000f,
        -0.461940f, 0.191342f, 0.000000f,
        -0.500000f, 0.000000f, 0.000000f,
        -0.461940f, -0.191342f, 0.000000f,
        -0.353553f, -0.353553f, 0.000000f,
        -0.191342f, -0.461940f, 0.000000f,
        -0.000000f, -0.500000f, 0.000000f,
        0.191342f, -0.461940f, 0.000000f,
        0.353553f, -0.353553f, 0.000000f,
        0.461940f, -0.191342f, 0.000000f};

    // Triangle-fan indices  → 16 triangles = 48 indices
    static constexpr std::array<unsigned int, 48> circle_indices = {
        0, 1, 2, 0, 2, 3, 0, 3, 4,
        0, 4, 5, 0, 5, 6, 0, 6, 7,
        0, 7, 8, 0, 8, 9, 0, 9, 10,
        0, 10, 11, 0, 11, 12, 0, 12, 13,
        0, 13, 14, 0, 14, 15, 0, 15, 16,
        0, 16, 1};

    struct Color {
        static constexpr auto white = glm::vec3(1.0f, 1.0f, 1.0f);
        static constexpr auto black = glm::vec3(0.0f, 0.0f, 0.0f);
        static constexpr auto red = glm::vec3(1.0f, 0.0f, 0.0f);
        static constexpr auto green = glm::vec3(0.0f, 1.0f, 0.0f);
        static constexpr auto blue = glm::vec3(0.0f, 0.0f, 1.0f);
    };

    static constexpr const char *fp_shader_dir = "assets/shaders/";
    static constexpr const char *fp_vertex_shader = "assets/shaders/vertex.glsl";
    static constexpr const char *fp_fragment_shader = "assets/shaders/fragment.glsl";
};

auto window_normalized_to_ndc(const Position &norm_pos) -> Position {
    return Position{
        norm_pos.x * 2.0f - 1.0f,
        1.0f - norm_pos.y * 2.0f};
}

auto ndc_to_window_normalized(const Position &ndc_pos) -> Position {
    return Position{
        (ndc_pos.x + 1.0f) * 0.5f,
        (1.0f - ndc_pos.y) * 0.5f};
}

struct Box {
    Position position;
    float width;
    float height;
};

enum class CollisionDirection {
    None,
    Left,
    Right,
    Top,
    Bottom
};

auto collision_box_box_directional(const Box &b1, const Box &b2) -> CollisionDirection {
    float left1 = b1.position.x;
    float right1 = b1.position.x + b1.width;
    float top1 = b1.position.y;
    float bottom1 = b1.position.y - b1.height;

    float left2 = b2.position.x;
    float right2 = b2.position.x + b2.width;
    float top2 = b2.position.y;
    float bottom2 = b2.position.y - b2.height;

    bool xcoll = (left1 < right2) &&
                 (right1 > left2);
    bool ycoll = (top1 > bottom2) &&
                 (bottom1 < top2);
    if (!(xcoll && ycoll)) {
        return CollisionDirection::None;
    }

    float c1x = (left1 + right1) * 0.5f;
    float c1y = (top1 + bottom1) * 0.5f;
    float c2x = (left2 + right2) * 0.5f;
    float c2y = (top2 + bottom2) * 0.5f;

    float dx = c2x - c1x;
    float dy = c2y - c1y;

    float penX = (b1.width * 0.5f + b2.width * 0.5f) - std::abs(dx);
    float penY = (b1.height * 0.5f + b2.height * 0.5f) - std::abs(dy);

    if (penX < penY) {
        return (dx > 0) ? CollisionDirection::Left : CollisionDirection::Right;
    } else {
        return (dy > 0) ? CollisionDirection::Bottom : CollisionDirection::Top;
    }
}

auto collision_box_box(const Box b1, const Box b2) -> bool {
    bool xcoll = b1.position.x < b2.position.x + b2.width &&
                 b1.position.x + b1.width > b2.position.x;

    bool ycoll = b1.position.y > b2.position.y - b2.height &&
                 b1.position.y - b1.height < b2.position.y;

    return xcoll && ycoll;
}

struct Block {
    Box box;
    Color color;
    int value;
    bool active;
    bool special;
};
struct Enemy {
    bool is_active;
    int hp;
    int hp_max;
    Box box;
    int pathfinding_target = -1;

    auto death() -> void {
        this->is_active = false;
    }
    auto take_damage(int amount) -> void {
        this->hp -= amount;
    }
};

enum class TowerType {
    Fire,
    Ice,
    Buff,
    NumTowerType
};
struct Tower {
    bool is_active;
    TowerType type;
    Box box;
    int level;
};

struct GameState {
    int score = 0;
};

struct s_UBO {
    gl_UBO time;
    gl_UBO pos;
    gl_UBO width;
    gl_UBO height;
    gl_UBO color;
};
struct s_Color {
    Color background{0.2f, 0.2f, 0.31f};
    Color path_marker{1.0f, 0.0f, 1.0f};
    Color enemy{1.0f, 0.0f, 0.0f};
    Color tower{0.1f, 0.3f, 0.9f};
    Color tower_radius{0.1f, 0.8f, 0.0f};
    Color projectile{1.0f, 1.0f, 1.0f};
};

struct Global {
    SDL_Window *window = nullptr;
    bool running = false;

    ImGuiIO imgui_io;
    SDL_GLContext gl_context;

    gl_ShaderProgram shader_program;
    gl_VAO vao_square;
    gl_VAO vao_circle;
    s_UBO ubo;

    s_Color color;

    Position mouse_pos;

    int frame_counter = 0;
    std::chrono::system_clock::time_point run_start_time;
    std::chrono::system_clock::time_point frame_start_time;
    std::chrono::milliseconds delta_time;
    std::chrono::milliseconds runtime;

    std::array<Box, 15> path_markers = {
        Box{window_normalized_to_ndc(Position{0.131f, 0.931f}), Constants::path_marker_width, Constants::path_marker_height},
        Box{window_normalized_to_ndc(Position{0.133f, 0.729f}), Constants::path_marker_width, Constants::path_marker_height},
        Box{window_normalized_to_ndc(Position{0.173f, 0.573f}), Constants::path_marker_width, Constants::path_marker_height},
        Box{window_normalized_to_ndc(Position{0.243f, 0.436f}), Constants::path_marker_width, Constants::path_marker_height},
        Box{window_normalized_to_ndc(Position{0.350f, 0.204f}), Constants::path_marker_width, Constants::path_marker_height},
        Box{window_normalized_to_ndc(Position{0.411f, 0.163f}), Constants::path_marker_width, Constants::path_marker_height},
        Box{window_normalized_to_ndc(Position{0.441f, 0.227f}), Constants::path_marker_width, Constants::path_marker_height},
        Box{window_normalized_to_ndc(Position{0.477f, 0.355f}), Constants::path_marker_width, Constants::path_marker_height},
        Box{window_normalized_to_ndc(Position{0.524f, 0.583f}), Constants::path_marker_width, Constants::path_marker_height},
        Box{window_normalized_to_ndc(Position{0.596f, 0.820f}), Constants::path_marker_width, Constants::path_marker_height},
        Box{window_normalized_to_ndc(Position{0.667f, 0.786f}), Constants::path_marker_width, Constants::path_marker_height},
        Box{window_normalized_to_ndc(Position{0.710f, 0.558f}), Constants::path_marker_width, Constants::path_marker_height},
        Box{window_normalized_to_ndc(Position{0.716f, 0.368f}), Constants::path_marker_width, Constants::path_marker_height},
        Box{window_normalized_to_ndc(Position{0.774f, 0.226f}), Constants::path_marker_width, Constants::path_marker_height},
        Box{window_normalized_to_ndc(Position{0.939f, 0.166f}), Constants::path_marker_width, Constants::path_marker_height}};

    std::vector<Enemy> enemies = {
        Enemy{true, 100, 100, Box{Position{0.0f, 0.0f}, 0.05f / Constants::aspect_ratio, 0.05f}},
        Enemy{true, 100, 500, Box{Position{0.2f, 0.0f}, 0.05f / Constants::aspect_ratio, 0.05f}},
        Enemy{true, 300, 300, Box{Position{-0.2f, 0.0f}, 0.05f / Constants::aspect_ratio, 0.05f}}};

    std::vector<Tower> towers = {
        Tower{true, TowerType::Fire, Box{window_normalized_to_ndc(Position{0.371f, 0.616f}), 0.1f / Constants::aspect_ratio, 0.05f}},
        Tower{true, TowerType::Fire, Box{window_normalized_to_ndc(Position{0.897f, 0.465f}), 0.1f / Constants::aspect_ratio, 0.05f}},
    };

    int gl_success;
    char gl_error_buffer[512];

    GameState game;
};
Global global;

auto on_tick_enemy(Enemy *enemy) -> void {
    if (!enemy->is_active) return;
    auto &target = global.path_markers[enemy->pathfinding_target];
    float dist_to_target = glm::distance(target.position, enemy->box.position);
    if (dist_to_target < 0.01f) {
        enemy->pathfinding_target = (enemy->pathfinding_target + 1) % global.path_markers.size();
    }

    glm::vec2 dir = normalize(target.position - enemy->box.position);
    dir.y *= Constants::aspect_ratio;

    // TODO: Might need to normalize for aspect ratio
    enemy->box.position += dir * 0.007f;

    if (enemy->is_active) {
        if (enemy->hp <= 0) enemy->death();
    }
}

auto handle_gl_error(const char *reason) -> void {
    std::cerr << reason << "\n"
              << global.gl_error_buffer << "\n";
    panic("GL Error");
}

auto format_time(std::chrono::system_clock::time_point tp) -> const char * {
    static char buffer[64];

    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm *tm_ptr = std::localtime(&time);

    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_ptr);
    return buffer;
}

auto format_duration(std::chrono::milliseconds duration) -> const char * {
    static char buffer[32];
    using namespace std::chrono;

    auto hrs = duration_cast<hours>(duration);
    duration -= hrs;
    auto mins = duration_cast<minutes>(duration);
    duration -= mins;
    auto secs = duration_cast<seconds>(duration);
    duration -= secs;
    auto millis = duration_cast<milliseconds>(duration);

    std::snprintf(
        buffer, sizeof(buffer),
        "%02lld:%02lld:%02lld.%03lld",
        static_cast<long long>(hrs.count()),
        static_cast<long long>(mins.count()),
        static_cast<long long>(secs.count()),
        static_cast<long long>(millis.count()));

    return buffer;
}

auto _main_imgui() -> void {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(global.window);
    ImGui::NewFrame();
    { // Debug
        ImGui::Begin("Debug");
        ImGui::ColorEdit3("Background", glm::value_ptr(global.color.background));
        ImGui::Text("Frame Counter: %d", global.frame_counter);
        ImGui::Text("Run Start: %s", format_time(global.run_start_time));
        ImGui::Text("Runtime: %s", format_duration(global.runtime));
        ImGui::Text("Runtime Count: %lld", global.runtime.count());
        ImGui::Text("Delta Time (ms): %lld", global.delta_time.count());
        ImGui::Text("Score: %d", global.game.score);
        ImGui::Text("Mouse Position: (%.3f, %.3f)", global.mouse_pos.x, global.mouse_pos.y);
        for (size_t enemy_idx = 0; enemy_idx < global.enemies.size(); ++enemy_idx) {
            ImGui::Text("Enemy %zu target: %d", enemy_idx, global.enemies[enemy_idx].pathfinding_target);
        }
        ImGui::End();
    } // Debug
    ImGui::Render();
}

auto _main_handle_inputs() -> void {
    int mouse_x, mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    global.mouse_pos = Position{
        static_cast<float>(mouse_x) / Constants::window_width,
        static_cast<float>(mouse_y) / Constants::window_height};

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        if (event.type == SDL_QUIT)
            global.running = false;

        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
                global.running = false;
                break;
            }
        }

        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            std::cout << "Mouse Clicked at: (" << global.mouse_pos.x << ", " << global.mouse_pos.y << ")\n";
        }
    }
}

auto _gl_set_box_ubo(const Box box) -> void {
    glUniform2f(global.ubo.pos, box.position.x, box.position.y);
    glUniform1f(global.ubo.width, box.width);
    glUniform1f(global.ubo.height, box.height);
}
auto _gl_set_color_ubo(const Color color) -> void {
    glUniform3f(global.ubo.color, color.r, color.g, color.b);
}

auto _main_render() -> void {
    glViewport(0, 0, (int)global.imgui_io.DisplaySize.x, (int)global.imgui_io.DisplaySize.y);
    glClearColor(global.color.background.r, global.color.background.g, global.color.background.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(global.shader_program);
    glUniform1f(global.ubo.time, static_cast<float>(global.runtime.count()));

    { // Square VAO
        glBindVertexArray(global.vao_square);
        _gl_set_color_ubo(global.color.path_marker);
        for (size_t marker_idx = 0; marker_idx < global.path_markers.size(); ++marker_idx) {
            _gl_set_box_ubo(global.path_markers[marker_idx]);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }

        for (size_t enemy_idx = 0; enemy_idx < global.enemies.size(); ++enemy_idx) {
            Enemy &enemy = global.enemies[enemy_idx];
            if (!enemy.is_active) continue;

            float health_pct = static_cast<float>(enemy.hp) / enemy.hp_max;
            _gl_set_color_ubo(health_pct * global.color.enemy + (1 - health_pct) * Constants::Color::black);
            _gl_set_box_ubo(enemy.box);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }

        for (size_t tower_idx = 0; tower_idx < global.towers.size(); ++tower_idx) {
            Tower &tower = global.towers[tower_idx];
            if (!tower.is_active) continue;

            _gl_set_color_ubo(global.color.tower);
            _gl_set_box_ubo(tower.box);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
    } // Square VAO

    { // Circle VAO
        glBindVertexArray(global.vao_square);
        for (size_t tower_idx = 0; tower_idx < global.towers.size(); ++tower_idx) {
            Tower &tower = global.towers[tower_idx];
            if (!tower.is_active) continue;

            _gl_set_color_ubo(global.color.tower);
            _gl_set_box_ubo(tower.box);
            glDrawElements(GL_TRIANGLES, 48, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
    } // Circle VAO
}

/*
Handles the SDL, ImGUI, OpenGL init and linking. Returns true if setup successful, false otherwise
*/
auto setup() -> bool {
    // Initialize SDL2 with video and timer subsystems
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "Error: SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);

    // Create window with OpenGL context
    global.window = SDL_CreateWindow(
        Constants::window_title.data(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        Constants::window_width, Constants::window_height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!global.window) {
        std::cerr << "Error: SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return false;
    }

    global.gl_context = SDL_GL_CreateContext(global.window);
    if (!global.gl_context) {
        std::cerr << "Error: SDL_GL_CreateContext failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(global.window);
        SDL_Quit();
        return false;
    }
    SDL_GL_MakeCurrent(global.window, global.gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Initialize GL loader
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return false;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    global.imgui_io = ImGui::GetIO();
    ImGui::StyleColorsDark();

    // Initialize ImGui SDL2 + OpenGL3 backends
    ImGui_ImplSDL2_InitForOpenGL(global.window, global.gl_context);
    const char *glsl_version = nullptr;
    glsl_version = "#version 410 core";
    ImGui_ImplOpenGL3_Init(glsl_version);

    return true;
}

auto compile_shader_from_file(const char *filepath, GLenum shader_type) -> gl_Shader {
    std::ifstream in(filepath);
    if (!in) {
        std::cerr << "Couldn't open file " << filepath << "\n";
        return 0;
    }
    std::string shader_source_str;
    {
        std::ostringstream ss;
        ss << in.rdbuf();
        shader_source_str = ss.str();
    }
    in.close();
    const char *vertex_shader_source = shader_source_str.c_str();
    gl_Shader shader;
    shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &vertex_shader_source, nullptr);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &global.gl_success);
    if (!global.gl_success) {
        glGetShaderInfoLog(shader, 512, nullptr, global.gl_error_buffer);
        handle_gl_error("Shader Compilation Failed.");
    }
    return shader;
}

auto setup_shader_program() -> void {
    gl_Shader vertex_shader = compile_shader_from_file(Constants::fp_vertex_shader, GL_VERTEX_SHADER);
    if (vertex_shader == 0) panic("Failed to compile vertex shader.");
    gl_Shader fragment_shader = compile_shader_from_file(Constants::fp_fragment_shader, GL_FRAGMENT_SHADER);
    if (fragment_shader == 0) panic("Failed to compile fragment shader.");

    global.shader_program = glCreateProgram();

    glAttachShader(global.shader_program, vertex_shader);
    glAttachShader(global.shader_program, fragment_shader);

    glLinkProgram(global.shader_program);
    // Check link errors:
    glGetProgramiv(global.shader_program, GL_LINK_STATUS, &global.gl_success);
    if (!global.gl_success) {
        glGetProgramInfoLog(global.shader_program, 512, nullptr, global.gl_error_buffer);
        panic(std::string("Shader Program Link Failed: ") + global.gl_error_buffer);
    }

    glUseProgram(global.shader_program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    global.ubo.time = glGetUniformLocation(global.shader_program, "u_Time");
    global.ubo.pos = glGetUniformLocation(global.shader_program, "u_Pos");
    global.ubo.width = glGetUniformLocation(global.shader_program, "u_Width");
    global.ubo.height = glGetUniformLocation(global.shader_program, "u_Height");
    global.ubo.color = glGetUniformLocation(global.shader_program, "u_Color");
}

auto setup_vao_square() -> void {
    // 1) Generate and bind the VAO up front
    glGenVertexArrays(1, &global.vao_square);
    glBindVertexArray(global.vao_square);

    // 2) Create VBO, upload vertex data, set attribute pointers
    gl_VBO vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
        sizeof(Constants::square_vertices),
        Constants::square_vertices.data(),
        GL_STATIC_DRAW);
    glVertexAttribPointer(
        0, // location = 0 in your vertex shader
        3, // 3 floats per vertex
        GL_FLOAT, GL_FALSE,
        3 * sizeof(float),
        (void *)0);
    glEnableVertexAttribArray(0);

    // 3) Create EBO *while the VAO is still bound*, so the binding is stored in it
    gl_EBO ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        sizeof(Constants::square_indices),
        Constants::square_indices.data(),
        GL_STATIC_DRAW);

    // 4) Unbind VAO (optional but good practice)
    glBindVertexArray(0);
}

auto setup_vao_circle() -> void {
    // 1) Generate and bind the VAO up front
    glGenVertexArrays(1, &global.vao_circle);
    glBindVertexArray(global.vao_circle);

    // 2) Create VBO, upload vertex data, set attribute pointers
    gl_VBO vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(Constants::circle_vertices),
        Constants::circle_vertices.data(),
        GL_STATIC_DRAW);
    glVertexAttribPointer(
        0, // location = 0 in your vertex shader
        3, // 3 floats per vertex (x, y, z)
        GL_FLOAT,
        GL_FALSE,
        3 * sizeof(float), // stride
        (void *)0          // offset
    );
    glEnableVertexAttribArray(0);

    // 3) Create EBO *while the VAO is still bound*, so the binding is stored in it
    gl_EBO ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        sizeof(Constants::circle_indices),
        Constants::circle_indices.data(),
        GL_STATIC_DRAW);

    // 4) Unbind VAO (optional but good practice)
    glBindVertexArray(0);
}

auto cleanup() -> void {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(global.gl_context);
    SDL_DestroyWindow(global.window);
    SDL_Quit();
}

auto main(int argc, char **argv) -> int {
    if (!setup()) panic("Setup failed!");

    setup_shader_program();
    setup_vao_square();

    for (size_t enemy_idx = 0; enemy_idx < global.enemies.size(); ++enemy_idx) {
        Enemy &enemy = global.enemies[enemy_idx];
        float min_dist = 100000.0f;
        int min_idx = -1;
        if (enemy.pathfinding_target == -1) {
            for (size_t marker_idx = 0; marker_idx < global.path_markers.size(); ++marker_idx) {
                // make this center to center distance instead
                auto &marker = global.path_markers[marker_idx];
                float dist = glm::distance(enemy.box.position, marker.position);
                if (dist < min_dist) {
                    min_dist = dist;
                    min_idx = marker_idx;
                }
            }
        }
        enemy.pathfinding_target = min_idx;
    }

    global.run_start_time = std::chrono::system_clock::now();
    global.running = true;
    // For initial delta time computation
    global.frame_start_time = std::chrono::system_clock::now();
    while (global.running) {
        global.delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - global.frame_start_time);
        global.frame_start_time = std::chrono::system_clock::now();
        global.runtime = std::chrono::duration_cast<std::chrono::milliseconds>(global.frame_start_time - global.run_start_time);

        _main_handle_inputs();
        for (size_t enemy_idx = 0; enemy_idx < global.enemies.size(); ++enemy_idx) {
            on_tick_enemy(&global.enemies[enemy_idx]);
        }

        _main_imgui();
        _main_render();

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(global.window);

        global.frame_counter += 1;
    }

    cleanup();

    return EXIT_SUCCESS;
}