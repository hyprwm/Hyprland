#pragma once

#include <array>
#include <string_view>

// clang-format off
constexpr std::array<const char*, 35> CURSOR_SHAPE_NAMES = {
    "invalid",
    "default",
    "context-menu",
    "help",
    "pointer",
    "progress",
    "wait",
    "cell",
    "crosshair",
    "text",
    "vertical-text",
    "alias",
    "copy",
    "move",
    "no-drop",
    "not-allowed",
    "grab",
    "grabbing",
    "e-resize",
    "n-resize",
    "ne-resize",
    "nw-resize",
    "s-resize",
    "se-resize",
    "sw-resize",
    "w-resize",
    "ew-resize",
    "ns-resize",
    "nesw-resize",
    "nwse-resize",
    "col-resize",
    "row-resize",
    "all-scroll",
    "zoom-in",
    "zoom-out",
};

constexpr std::array<std::string_view, 47> CURSOR_SHAPE_NAMES_FINAL_SHADER = {
    "invalid",             //0
    "default",             //1
    "context-menu",        //2
    "help",                //3
    "pointer",             //4
    "progress",            //5
    "wait",                //6
    "cell",                //7
    "crosshair",           //8
    "text",                //9
    "vertical-text",       //10
    "alias",               //11
    "copy",                //12
    "move",                //13
    "no-drop",             //14
    "not-allowed",         //15
    "grab",                //16
    "grabbing",            //17
    "e-resize",            //18
    "n-resize",            //19
    "ne-resize",           //20
    "nw-resize",           //21
    "s-resize",            //22
    "se-resize",           //23
    "sw-resize",           //24
    "w-resize",            //25
    "ew-resize",           //26
    "ns-resize",           //27
    "nesw-resize",         //28
    "nwse-resize",         //29
    "col-resize",          //30
    "row-resize",          //31
    "all-scroll",          //32
    "zoom-in",             //33
    "zoom-out",            //34
    "dnd-ask",             //35
    "all-resize",          //36

    "left_ptr",            //37
    "top_side",            //38
    "bottom_side",         //39
    "left_side",           //40
    "right_side",          //41
    "top_left_corner",     //42
    "bottom_left_corner",  //43
    "top_right_corner",    //44
    "bottom_right_corner", //45

    "killing",             //46
};
// clang-format on
