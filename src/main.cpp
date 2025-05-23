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
using glm::vec2;
using glm::vec3;

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_map>

struct Position {
    float x;
    float y;

    Position() = default;
    Position(float x_, float y_) : x(x_), y(y_) {}
    Position(const vec2 &v) : x(v.x), y(v.y) {}
    operator vec2() const { return {x, y}; }

    Position operator+(const vec2 &v) const { return Position{x + v.x, y + v.y}; }
    Position operator-(const vec2 &v) const { return Position{x - v.x, y - v.y}; }
    Position &operator+=(const vec2 &v) {
        x += v.x;
        y += v.y;
        return *this;
    }
    Position &operator-=(const vec2 &v) {
        x -= v.x;
        y -= v.y;
        return *this;
    }

    vec2 to_glm() const { return vec2(x, y); }
};

inline std::ostream &operator<<(std::ostream &os, const Position &p) {
    return os
           << "Position("
           << p.x << ", "
           << p.y
           << ")";
}

inline float distance(const Position &a, const Position &b) {
    return glm::distance(a.to_glm(), b.to_glm());
}

struct Color {
    float r, g, b;

    Color() = default;
    Color(float r_, float g_, float b_) : r(r_), g(g_), b(b_) {}
    Color(const vec3 &v) : r(v.r), g(v.g), b(v.b) {}
    static Color from_u8(uint8_t r, uint8_t g, uint8_t b) {
        return Color(r / 255.0f, g / 255.0f, b / 255.0f);
    }
    operator vec3() const { return {r, g, b}; }

    Color operator*(float f) const { return {r * f, g * f, b * f}; }
    Color operator+(const Color &c) const { return {r + c.r, g + c.g, b + c.b}; }
    vec3 to_glm() const { return {r, g, b}; }
    float *data() { return &r; }

    static Color mix(const Color &a, const Color &b, float t) {
        return {
            a.r * (1.0f - t) + b.r * t,
            a.g * (1.0f - t) + b.g * t,
            a.b * (1.0f - t) + b.b * t};
    }
};

inline std::ostream &operator<<(std::ostream &os, const Color &c) {
    return os
           << "Color("
           << c.r << ", "
           << c.g << ", "
           << c.b
           << ")";
}

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
constexpr std::array<float, 51> make_circle_vertices() {
    std::array<float, 51> v = {
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
    for (float &f : v) {
        f *= 2.0f;
    }
    return v;
}
struct Constants {
    static constexpr std::string_view window_title = "Tower Defense";
    static constexpr int window_width = 1280;
    static constexpr int window_height = 720;
    static constexpr float aspect_ratio = static_cast<float>(window_width) / window_height;

    static constexpr float path_marker_width = 0.025f;
    static constexpr float path_marker_height = 0.025f;

    static constexpr std::chrono::duration<float> projectile_life_time = std::chrono::duration<float>(1.0f);

    static constexpr std::array<float, 12> square_vertices = {
        1.0f, -1.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f};

    static constexpr std::array<unsigned int, 6> square_indices = {
        0, 1, 3,
        1, 2, 3};

    static constexpr std::array<float, 9> triangle_vertices = {
        0.5f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f};

    static constexpr std::array<unsigned int, 3> triangle_indices = {
        0, 1, 2};

    static constexpr auto circle_vertices = make_circle_vertices();

    // Triangle-fan indices  → 16 triangles = 48 indices
    static constexpr std::array<unsigned int, 48> circle_indices = {
        0, 1, 2, 0, 2, 3, 0, 3, 4,
        0, 4, 5, 0, 5, 6, 0, 6, 7,
        0, 7, 8, 0, 8, 9, 0, 9, 10,
        0, 10, 11, 0, 11, 12, 0, 12, 13,
        0, 13, 14, 0, 14, 15, 0, 15, 16,
        0, 16, 1};

    struct Color {
        static constexpr auto white = vec3(1.0f, 1.0f, 1.0f);
        static constexpr auto black = vec3(0.0f, 0.0f, 0.0f);
        static constexpr auto red = vec3(1.0f, 0.0f, 0.0f);
        static constexpr auto green = vec3(0.0f, 1.0f, 0.0f);
        static constexpr auto blue = vec3(0.0f, 0.0f, 1.0f);
    };

    static constexpr int max_tower_level = 5;

    static constexpr const char *fp_shader_dir = "assets/shaders/";
    static constexpr const char *fp_vertex_shader = "assets/shaders/vertex.glsl";
    static constexpr const char *fp_fragment_shader = "assets/shaders/fragment.glsl";
    static constexpr const char *fp_fragment_tower_range_shader = "assets/shaders/fragment_tower_range.glsl";
};

auto window_normalized_to_ndc(const Position &norm_pos) -> Position {
    auto pos = Position{
        norm_pos.x * 2.0f - 1.0f,
        1.0f - norm_pos.y * 2.0f};
    pos.x *= Constants::aspect_ratio;
    return pos;
}

auto ndc_to_window_normalized(const Position &ndc_pos) -> Position {
    auto pos = Position{
        (ndc_pos.x / Constants::aspect_ratio + 1.0f) * 0.5f,
        (1.0f - ndc_pos.y) * 0.5f};
    return pos;
}

struct Box {
    Position position;
    float width;
    float height;
    auto get_center() const -> Position {
        return Position{position.x + width / 2.0f, position.y - height / 2.0f};
    }
    auto is_point_inside(Position pos) const -> bool {
        return pos.x >= position.x &&
               pos.x <= position.x + width &&
               pos.y >= position.y - height &&
               pos.y <= position.y;
    }
};
inline float distance(const Box &a, const Box &b) {
    return distance(a.get_center(), b.get_center());
}

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

struct Enemy {
    int id;
    bool is_active;
    int hp;
    int hp_max;
    Box box;
    int pathfinding_target = -1;

    auto death() -> void {
        this->is_active = false;
    }
    auto take_damage(int amount) -> void {
        hp -= amount;
        if (hp <= 0) death();
    }
};

enum class TowerType {
    Fire,
    Ice,
    Buff,
    NumTowerType
};
struct Tower;

struct Projectile {
    int tower_idx;
    bool is_active = false;
    std::chrono::steady_clock::time_point spawn_time;
    Box box;
    vec2 dir;
};
struct EnemyInRange {
    int id;
    float distance;
};
struct Tower {
    int id;
    bool is_active;
    TowerType type;
    Box box;
    int level;
    std::vector<EnemyInRange> enemies_in_range;
    std::array<Projectile, 6> projectiles;
    std::chrono::steady_clock::time_point time_of_last_shot;

    auto find_closest_enemy() const -> std::optional<int> {
        if (enemies_in_range.empty()) return std::nullopt;
        auto min_it = std::min_element(
            enemies_in_range.begin(), enemies_in_range.end(),
            [](const EnemyInRange &a, const EnemyInRange &b) {
                return a.distance < b.distance;
            });

        return min_it->id;
    }
};

struct ShaderProgram {
    gl_ShaderProgram id;
    std::unordered_map<std::string, gl_UBO> ubos;
    auto activate() -> void {
        if (id == GL_ZERO) panic("Trying to activate uninitialized ShaderProgram!");
        glUseProgram(id);
    }
};

struct s_Color {
    Color background = Color::from_u8(15, 15, 21);
    Color path_marker{1.0f, 0.0f, 1.0f};
    Color enemy{1.0f, 0.0f, 0.0f};
    Color tower_fire{0.9f, 0.1f, 0.3f};
    Color tower_ice{0.5f, 0.5f, 0.9f};
    Color tower_buff{0.3f, 1.0f, 0.4f};
    Color tower_radius{0.1f, 0.8f, 0.0f};
    Color projectile{1.0f, 1.0f, 1.0f};
};

struct GameState {
    int score = 0;
    int life = 10;

    std::vector<Enemy> enemies = {
        Enemy{0, true, 100, 100, Box{window_normalized_to_ndc(Position{0.441f, 0.467f}), 0.05f, 0.05f}},
        Enemy{1, true, 250, 500, Box{window_normalized_to_ndc(Position{0.271f, 0.768f}), 0.05f, 0.05f}},
        Enemy{2, true, 300, 300, Box{window_normalized_to_ndc(Position{0.668f, 0.160f}), 0.05f, 0.05f}},
        Enemy{3, true, 300, 300, Box{window_normalized_to_ndc(Position{0.339844f, 0.452778f}), 0.05f, 0.05f}},
        Enemy{4, true, 300, 300, Box{window_normalized_to_ndc(Position{0.386719f, 0.255556f}), 0.05f, 0.05f}},
        Enemy{5, true, 300, 300, Box{window_normalized_to_ndc(Position{0.514063f, 0.126389f}), 0.05f, 0.05f}},
        Enemy{6, true, 300, 300, Box{window_normalized_to_ndc(Position{0.760156f, 0.658333f}), 0.05f, 0.05f}},
        Enemy{7, true, 300, 300, Box{window_normalized_to_ndc(Position{0.721875f, 0.851389f}), 0.05f, 0.05f}},
        Enemy{8, true, 300, 300, Box{window_normalized_to_ndc(Position{0.49375f, 0.866667f}), 0.05f, 0.05f}},
        Enemy{9, true, 300, 300, Box{window_normalized_to_ndc(Position{0.464844f, 0.690278f}), 0.05f, 0.05f}}};

    std::vector<Tower> towers = {
        Tower{0, true, TowerType::Fire, Box{window_normalized_to_ndc(Position{0.146f, 0.516f}), 0.1f, 0.1f}, 1},
        Tower{1, true, TowerType::Ice, Box{window_normalized_to_ndc(Position{0.827f, 0.276f}), 0.1f, 0.1f}, 3},
        Tower{2, true, TowerType::Buff, Box{window_normalized_to_ndc(Position{0.55f, 0.400f}), 0.1f, 0.1f}, 4}};

    /*
    auto GameState::serialise() -> void {
        std::ofstream out("gamestate.json");
        if (!out) panic("Failed to open gamestate.json for writing");
        out << std::setw(2) << json(*this) << "\n";
    }

    auto GameState::deserialise() -> void {
        std::ifstream in("gamestate.json");
        if (!in) panic("Failed to open gamestate.json for reading");
        json j;
        in >> j;
        *this = j.get<GameState>();
    }
    */
};

struct Global {
    SDL_Window *window = nullptr;
    bool running = false;

    ImGuiIO imgui_io;
    SDL_GLContext gl_context;

    ShaderProgram shader_program_single_color;
    ShaderProgram shader_program_tower_range;

    gl_VAO vao_square;
    gl_VAO vao_circle;
    gl_VAO vao_triangle;
    gl_VAO vao_NONE = GL_ZERO; // TODO: Maybe move this to Constants

    s_Color color;

    Position mouse_pos;

    int frame_counter = 0;
    std::chrono::steady_clock::time_point run_start_time;
    std::chrono::steady_clock::time_point frame_start_time;
    std::chrono::duration<float> delta_time;
    std::chrono::duration<float> runtime;

    // Index into those with the tower level
    std::array<float, Constants::max_tower_level> table_tower_range = {0.25f, 0.3f, 0.35f, 0.4f, 0.45f};
    std::array<float, Constants::max_tower_level> table_tower_damage = {5, 10, 20, 40, 50};
    std::array<float, Constants::max_tower_level> table_tower_firing_delay = {1.0f, 0.9f, 0.8f, 0.7f, 0.5f};

    std::array<Box, 15>
        path_markers = {
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

    int gl_success;
    char gl_error_buffer[512];

    GameState game;
};
Global global;

auto tower_init_projectiles(Tower &tower) -> void {
    tower.time_of_last_shot = global.frame_start_time;
    for (auto &p : tower.projectiles) {
        p.tower_idx = tower.id;
    }
}
auto init_global() -> void {
    for (auto &tower : global.game.towers) {
        tower_init_projectiles(tower);
    }
}

auto spawn_tower_at_position(const Position &position) -> void {
    auto box = Box{position, 0.1f, 0.1f};
    int tower_id = global.game.towers.size();
    auto tower = Tower{tower_id, true, TowerType::Fire, box, 0};
    tower_init_projectiles(tower);
    global.game.towers.push_back(tower);
}

auto emplace_enemy(Enemy enemy) -> void {
    enemy.id = global.game.enemies.size();
    global.game.enemies.push_back(enemy);
}

auto spawn_enemy_at_position(const Position &position) -> void {
    auto box = Box{position, 0.05f, 0.05f};
    emplace_enemy(Enemy{0, true, 100, 100, box});
}

auto advance_pathfinding_target(Enemy &enemy) -> void {
    if (enemy.pathfinding_target == -1) panic("Trying to advance not initialised pathfinding target");
    enemy.pathfinding_target += 1;
    bool has_reached_end = (enemy.pathfinding_target == global.path_markers.size());
    if (has_reached_end) {
        global.game.life -= 1;
        enemy.box.position = global.path_markers[0].position;
        enemy.pathfinding_target = 0;
        enemy.hp = enemy.hp_max;
    }
}

auto on_tick_enemy(Enemy &enemy) -> void {
    if (!enemy.is_active) return;

    bool no_target = enemy.pathfinding_target == -1;
    if (no_target) {
        float min_dist = 100000.0f;
        int min_idx = -1;
        if (enemy.pathfinding_target == -1) {
            for (size_t marker_idx = 0; marker_idx < global.path_markers.size(); ++marker_idx) {
                // make this center to center distance instead
                auto &marker = global.path_markers[marker_idx];
                float dist = distance(enemy.box, marker);
                if (dist < min_dist) {
                    min_dist = dist;
                    min_idx = marker_idx;
                }
            }
        }
        enemy.pathfinding_target = min_idx;
    }
    { // Movement
        auto &target = global.path_markers[enemy.pathfinding_target];
        float dist_to_target = distance(target.position, enemy.box.position);
        if (dist_to_target < 0.01f) {
            advance_pathfinding_target(enemy);
        }
        vec2 dir = normalize((target.position - enemy.box.position).to_glm());
        enemy.box.position += dir * 0.001f;
    }
    { // Combining enemies

        for (auto &other : global.game.enemies) {
            if (!other.is_active) continue;
            if (enemy.id == other.id) continue;
            if (collision_box_box(enemy.box, other.box)) {
                { // height
                    float big = std::max(enemy.box.height, other.box.height);
                    float small = std::min(enemy.box.height, other.box.height);
                    enemy.box.height = big + small / 5.0f;
                }
                { // width
                    float big = std::max(enemy.box.width, other.box.width);
                    float small = std::min(enemy.box.width, other.box.width);
                    enemy.box.width = big + small / 5.0f;
                }
                { // max HP
                    int big = std::max(enemy.hp_max, other.hp_max);
                    int small = std::min(enemy.hp_max, other.hp_max);
                    enemy.hp_max = big + small / 5.0f;
                }
                { // current HP
                    int big = std::max(enemy.hp, other.hp);
                    int small = std::min(enemy.hp, other.hp);
                    enemy.hp = big + small / 5.0f;
                    if (enemy.hp > enemy.hp_max)
                        enemy.hp = enemy.hp_max;
                }

                other.is_active = false;
            }
        }
    }

    if (enemy.is_active) {
        if (enemy.hp <= 0) enemy.death();
    }
}

auto shoot_at(Tower &tower, Position pos) -> void {
    vec2 dir = pos - tower.box.get_center();
    for (auto &proj : tower.projectiles) {
        if (!proj.is_active) {
            proj.box = Box{tower.box.get_center(), 0.02f, 0.02f};
            proj.dir = glm::normalize(dir);
            proj.is_active = true;
            proj.spawn_time = global.frame_start_time;

            tower.time_of_last_shot = global.frame_start_time;
            return;
        }
    }
}

auto proj_get_tower(const Projectile &proj) -> const Tower & {
    return global.game.towers[proj.tower_idx];
}

auto on_tick_projectile(Projectile &proj) -> void {
    if (!proj.is_active) return;

    auto duration = std::chrono::duration<float>(global.frame_start_time - proj.spawn_time);
    if (duration.count() >= 1.0f) {
        proj.is_active = false;
        return;
    }
    proj.box.position += 0.01f * proj.dir;
    for (auto &enemy : global.game.enemies) {
        if (collision_box_box(proj.box, enemy.box)) {
            const Tower &tower = proj_get_tower(proj);
            enemy.take_damage(global.table_tower_damage[tower.level]);
            proj.is_active = false;
            break;
        }
    }
}

auto on_tick_tower(Tower &tower) -> void {
    if (!tower.is_active) return;
    tower.enemies_in_range.clear();

    for (size_t enemy_idx = 0; enemy_idx < global.game.enemies.size(); ++enemy_idx) {
        auto &enemy = global.game.enemies[enemy_idx];
        if (!enemy.is_active) continue;
        float dist = distance(tower.box, enemy.box);
        if (dist < global.table_tower_range[tower.level]) {
            tower.enemies_in_range.push_back(EnemyInRange{
                static_cast<int>(enemy_idx), dist});
        }
    }

    auto tower_firing_delay = std::chrono::duration<float>(global.table_tower_firing_delay[tower.level]);
    bool ready_to_shoot = (global.frame_start_time - tower.time_of_last_shot) > tower_firing_delay;
    if (ready_to_shoot) {
        if (auto closest = tower.find_closest_enemy()) {
            Enemy enemy = global.game.enemies[*closest];
            shoot_at(tower, enemy.box.get_center());
        }
    }
    for (auto &proj : tower.projectiles) {
        on_tick_projectile(proj);
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

auto format_duration(std::chrono::duration<float> duration) -> const char * {
    static char buffer[32];
    auto total_ms = static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());

    auto hrs = total_ms / (60 * 60 * 1000);
    total_ms %= (60 * 60 * 1000);
    auto mins = total_ms / (60 * 1000);
    total_ms %= (60 * 1000);
    auto secs = total_ms / 1000;
    auto millis = total_ms % 1000;

    std::snprintf(
        buffer, sizeof(buffer),
        "%02lld:%02lld:%02lld.%03lld",
        hrs, mins, secs, millis);

    return buffer;
}

auto _main_imgui() -> void {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(global.window);
    ImGui::NewFrame();
    { // Debug
        ImGui::Begin("Debug");
        ImGui::ColorEdit3("Background", global.color.background.data());
        ImGui::Text("Frame Counter: %d", global.frame_counter);
        ImGui::Text("Runtime: %s", format_duration(global.runtime));
        ImGui::Text("Delta Time (ms): %f", global.delta_time.count());
        ImGui::Text("Score: %d", global.game.score);
        ImGui::Text("Life: %d", global.game.life);
        ImGui::Text("Mouse Position: (%.3f, %.3f)", global.mouse_pos.x, global.mouse_pos.y);
        for (size_t enemy_idx = 0; enemy_idx < global.game.enemies.size(); ++enemy_idx) {
            auto enemy = global.game.enemies[enemy_idx];
            ImGui::Text("Enemy %zu (%.3f, %.3f) target: %d", enemy_idx, enemy.box.position.x, enemy.box.position.y, enemy.pathfinding_target);
        }
        for (size_t tower_idx = 0; tower_idx < global.game.towers.size(); ++tower_idx) {
            auto &tower = global.game.towers[tower_idx];
            for (auto &eir : tower.enemies_in_range) {
                ImGui::Text("Tower %zu -> Enemy %d (dist=%.3f)", tower_idx, eir.id, eir.distance);
            }
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
            case SDLK_e:
                Position mouse_pos_ndc = window_normalized_to_ndc(global.mouse_pos);
                spawn_enemy_at_position(mouse_pos_ndc);
                break;
            }
        }

        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            auto mouse_pos = Position{global.mouse_pos.x, global.mouse_pos.y};
            std::cout << "Mouse Clicked at: " << mouse_pos << "\n";
            spawn_tower_at_position(
                window_normalized_to_ndc(mouse_pos) - vec2{0.05f, -0.05f});
        }
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
            Position mouse_pos_ndc = window_normalized_to_ndc(global.mouse_pos);
            for (auto &tower : global.game.towers) {
                if (tower.box.is_point_inside(mouse_pos_ndc)) {
                    std::cout << "Disabling tower\n";
                    tower.is_active = false;
                }
            }
        }
    }
}

namespace gl {
auto set_box_ubo(ShaderProgram sp, const Box box) -> void {
    glUniform2f(sp.ubos["u_Pos"], box.position.x, box.position.y);
    glUniform1f(sp.ubos["u_Width"], box.width);
    glUniform1f(sp.ubos["u_Height"], box.height);
}
auto set_color_ubo(ShaderProgram sp, const Color color) -> void {
    glUniform3f(sp.ubos["u_Color"], color.r, color.g, color.b);
}
auto draw_square() -> void {
    glDrawElements(GL_TRIANGLES, Constants::square_indices.size(), GL_UNSIGNED_INT, 0);
}
auto draw_triangle() -> void {
    glDrawElements(GL_TRIANGLES, Constants::triangle_indices.size(), GL_UNSIGNED_INT, 0);
}
auto draw_circle() -> void {
    glDrawElements(GL_TRIANGLES, Constants::circle_indices.size(), GL_UNSIGNED_INT, 0);
}
} // namespace gl

auto _main_render() -> void {
    glViewport(0, 0, (int)global.imgui_io.DisplaySize.x, (int)global.imgui_io.DisplaySize.y);
    glClearColor(global.color.background.r, global.color.background.g, global.color.background.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    { // Single Color Shader Program
        ShaderProgram &shader = global.shader_program_single_color;
        shader.activate();
        glUniform1f(shader.ubos["u_Time"], static_cast<float>(global.runtime.count()));
        // shader.set_ubo1f("u_Time", static_cast<float>(global.runtime.count()));

        { // Triangle VAO
            glBindVertexArray(global.vao_triangle);
            for (size_t tower_idx = 0; tower_idx < global.game.towers.size(); ++tower_idx) {
                Tower &tower = global.game.towers[tower_idx];
                if (!tower.is_active) continue;

                switch (tower.type) {
                case TowerType::Fire:
                    gl::set_color_ubo(global.shader_program_single_color, global.color.tower_fire);
                    break;
                case TowerType::Ice:
                    gl::set_color_ubo(global.shader_program_single_color, global.color.tower_ice);
                    break;
                case TowerType::Buff:
                    gl::set_color_ubo(global.shader_program_single_color, global.color.tower_buff);
                    break;
                default:
                    panic("Unknown Tower Type!");
                    break;
                }
                gl::set_box_ubo(shader, tower.box);

                gl::draw_triangle();
            }
            glBindVertexArray(global.vao_NONE);
        } // Triangle VAO

        { // Square VAO
            glBindVertexArray(global.vao_square);
            gl::set_color_ubo(shader, global.color.path_marker);
            for (size_t marker_idx = 0; marker_idx < global.path_markers.size(); ++marker_idx) {
                gl::set_box_ubo(shader, global.path_markers[marker_idx]);
                gl::draw_square();
            }

            for (size_t enemy_idx = 0; enemy_idx < global.game.enemies.size(); ++enemy_idx) {
                Enemy &enemy = global.game.enemies[enemy_idx];
                if (!enemy.is_active) continue;

                float health_pct = static_cast<float>(enemy.hp) / enemy.hp_max;
                gl::set_color_ubo(shader, Color::mix(Constants::Color::black, global.color.enemy, health_pct));
                gl::set_box_ubo(shader, enemy.box);
                gl::draw_square();
            }

            gl::set_color_ubo(shader, global.color.projectile);
            for (auto &tower : global.game.towers) {
                if (!tower.is_active) continue;
                for (auto &proj : tower.projectiles) {
                    if (!proj.is_active) continue;
                    gl::set_box_ubo(shader, proj.box);
                    gl::draw_square();
                }
            }
            glBindVertexArray(global.vao_NONE);
        } // Square VAO
    }
    { // Tower Range Shader Program
        ShaderProgram &shader = global.shader_program_tower_range;
        shader.activate();
        glUniform1f(shader.ubos["u_Time"], static_cast<float>(global.runtime.count()));

        { // Circle VAO
            glBindVertexArray(global.vao_circle);
            for (size_t tower_idx = 0; tower_idx < global.game.towers.size(); ++tower_idx) {
                Tower &tower = global.game.towers[tower_idx];
                if (!tower.is_active) continue;

                gl::set_color_ubo(shader, global.color.tower_radius);

                float tower_range = global.table_tower_range[tower.level];
                auto box_shifted = Box{tower.box.get_center(), tower_range, tower_range};
                gl::set_box_ubo(shader, box_shifted);
                glUniform1f(shader.ubos["u_Radius"], tower_range);
                glUniform2f(shader.ubos["u_Pos"], tower.box.get_center().x, tower.box.get_center().y);
                gl::draw_circle();
            }
            glBindVertexArray(global.vao_NONE);
        } // Circle VAO
    }
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
        SDL_WINDOW_OPENGL);
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
    // Enables Blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

auto compile_shader_program_single_color() -> void {
    gl_Shader vertex_shader = compile_shader_from_file(Constants::fp_vertex_shader, GL_VERTEX_SHADER);
    if (vertex_shader == 0) panic("Failed to compile vertex shader.");
    gl_Shader fragment_shader = compile_shader_from_file(Constants::fp_fragment_shader, GL_FRAGMENT_SHADER);
    if (fragment_shader == 0) panic("Failed to compile fragment shader.");

    global.shader_program_single_color.id = glCreateProgram();

    glAttachShader(global.shader_program_single_color.id, vertex_shader);
    glAttachShader(global.shader_program_single_color.id, fragment_shader);

    glLinkProgram(global.shader_program_single_color.id);
    glGetProgramiv(global.shader_program_single_color.id, GL_LINK_STATUS, &global.gl_success);
    if (!global.gl_success) {
        glGetProgramInfoLog(global.shader_program_single_color.id, 512, nullptr, global.gl_error_buffer);
        panic(std::string("Shader Program Link Failed: ") + global.gl_error_buffer);
    }

    global.shader_program_single_color.activate();

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    std::vector<std::string> uniformNames = {
        "u_Time",
        "u_Pos",
        "u_Width",
        "u_Height",
        "u_Color",
        "u_AspectRatio"};

    for (const std::string &name : uniformNames) {
        global.shader_program_single_color.ubos[name] = glGetUniformLocation(global.shader_program_single_color.id, name.c_str());
    }
    glUniform1f(global.shader_program_single_color.ubos["u_AspectRatio"], Constants::aspect_ratio);
}
auto compile_shader_program_tower_radius() -> void {
    gl_Shader vertex_shader = compile_shader_from_file(Constants::fp_vertex_shader, GL_VERTEX_SHADER);
    if (vertex_shader == 0) panic("Failed to compile vertex shader.");
    gl_Shader fragment_shader = compile_shader_from_file(Constants::fp_fragment_tower_range_shader, GL_FRAGMENT_SHADER);
    if (fragment_shader == 0) panic("Failed to compile fragment shader.");

    global.shader_program_tower_range.id = glCreateProgram();

    glAttachShader(global.shader_program_tower_range.id, vertex_shader);
    glAttachShader(global.shader_program_tower_range.id, fragment_shader);

    glLinkProgram(global.shader_program_tower_range.id);
    glGetProgramiv(global.shader_program_tower_range.id, GL_LINK_STATUS, &global.gl_success);
    if (!global.gl_success) {
        glGetProgramInfoLog(global.shader_program_tower_range.id, 512, nullptr, global.gl_error_buffer);
        panic(std::string("Shader Program Link Failed: ") + global.gl_error_buffer);
    }

    global.shader_program_tower_range.activate();

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    std::vector<std::string> uniformNames = {
        "u_Time",
        "u_Pos",
        "u_Width",
        "u_Height",
        "u_Color",
        "u_Radius",
        "u_AspectRatio"};

    for (const std::string &name : uniformNames) {
        global.shader_program_tower_range.ubos[name] = glGetUniformLocation(global.shader_program_tower_range.id, name.c_str());
    }
    glUniform1f(global.shader_program_tower_range.ubos["u_AspectRatio"], Constants::aspect_ratio);
}

auto create_vao_square() -> void {
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

    glBindVertexArray(global.vao_NONE);
}

auto create_vao_triangle() -> void {
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

    glBindVertexArray(global.vao_NONE);
}

auto creat_vao_triangle() -> void {
    // 1) Generate and bind the VAO for the triangle
    glGenVertexArrays(1, &global.vao_triangle);
    glBindVertexArray(global.vao_triangle);

    // 2) Create VBO, upload triangle vertex data, set attribute pointers
    gl_VBO triangle_vbo;
    glGenBuffers(1, &triangle_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, triangle_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(Constants::triangle_vertices),
        Constants::triangle_vertices.data(),
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

    // 3) Create EBO while VAO is still bound, so the binding is stored in it
    gl_EBO triangle_ebo;
    glGenBuffers(1, &triangle_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangle_ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        sizeof(Constants::triangle_indices),
        Constants::triangle_indices.data(),
        GL_STATIC_DRAW);

    glBindVertexArray(global.vao_NONE);
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

    compile_shader_program_single_color();
    compile_shader_program_tower_radius();

    create_vao_square();
    create_vao_triangle();
    creat_vao_triangle();

    global.running = true;
    global.run_start_time = std::chrono::steady_clock::now();
    // For initial delta time computation
    global.frame_start_time = global.run_start_time;

    init_global();
    while (global.running) {
        auto now = std::chrono::steady_clock::now();
        global.delta_time = now - global.frame_start_time;
        global.frame_start_time = now;
        global.runtime = now - global.run_start_time;

        _main_handle_inputs();
        for (auto &enemy : global.game.enemies) {
            on_tick_enemy(enemy);
        }
        for (auto &tower : global.game.towers) {
            on_tick_tower(tower);
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