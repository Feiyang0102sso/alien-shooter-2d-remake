/** @file windows_entry.cpp
 * @brief Provide the Windows desktop entry point for Release builds.
 */
#include <Windows.h>
#include <cstdlib>

int main(int argc, char** argv);

// Release starts as a desktop application; Debug retains its console entry point.
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main(__argc, __argv);
}
