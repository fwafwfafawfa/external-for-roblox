#pragma once
#include "../main.hpp"

struct vector2d {
    float x, y;

    vector2d ( ) : x ( 0 ), y ( 0 ) {}
    vector2d ( float x, float y ) : x ( x ), y ( y ) {}

    vector2d operator+(const vector2d& other) const { return vector2d(x + other.x, y + other.y); }
    vector2d operator-(const vector2d& other) const { return vector2d(x - other.x, y - other.y); }
    vector2d operator*(float scalar) const { return vector2d(x * scalar, y * scalar); }
};

class c_core {
public:
    std::string read_string ( uintptr_t address );
    std::string length_read_string ( uintptr_t string );
    std::string get_instance_name ( uintptr_t instance_address );
    std::string get_instance_classname ( uintptr_t instance_address );
    uintptr_t find_first_child ( uintptr_t instance_address, const std::string &child_name );
    uintptr_t find_first_child_class ( uintptr_t instance_address, const std::string &child_class );
    uintptr_t get_model_instance ( uintptr_t instance_address );
    std::vector < uintptr_t > children ( uintptr_t instance_address );
    std::vector < uintptr_t > get_players ( uintptr_t datamodel_address );
    uintptr_t get_local_humanoid();
    bool world_to_screen( const vector& world_pos, vector2d& screen_pos, const view_matrix_t& view_matrix );
    int get_screen_width( ) { if ( !screen_width ) screen_width = GetSystemMetrics( SM_CXSCREEN ); return screen_width; }
    int get_screen_height( ) { if ( !screen_height ) screen_height = GetSystemMetrics( SM_CYSCREEN ); return screen_height; }
private:
    int screen_width = 0;
    int screen_height = 0;
};

inline c_core core;
