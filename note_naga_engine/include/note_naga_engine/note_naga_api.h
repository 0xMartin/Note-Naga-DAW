#pragma once

#ifdef _WIN32
#ifdef NOTE_NAGA_ENGINE_EXPORTS
#define NOTE_NAGA_ENGINE_API __declspec(dllexport)
#else
#define NOTE_NAGA_ENGINE_API __declspec(dllimport)
#endif
#else
#define NOTE_NAGA_ENGINE_API __attribute__((visibility("default")))
#endif