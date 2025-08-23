#include <note_naga_engine/core/soundfont_finder.h>

#include <note_naga_engine/logger.h>
#include <cstdlib>

namespace fs = std::filesystem;

std::string SoundFontFinder::findSoundFont() {
  auto soundfonts = getAllSoundFonts();

  if (!soundfonts.empty()) {
    NOTE_NAGA_LOG_INFO("Found SoundFont: " + soundfonts[0]);
    return soundfonts[0];
  }

  // Pokud nebyl nalezen žádný SoundFont, vráť výchozí
  NOTE_NAGA_LOG_WARNING("No SoundFont found in system, using default");
  return "./FluidR3_GM.sf2";
}

std::vector<std::string>
SoundFontFinder::getAllSoundFonts(bool includeUserDirs) {
  std::vector<std::string> soundfontPaths;
  std::vector<std::string> checkPaths;

// Cesty specifické pro macOS
#ifdef __APPLE__
  checkPaths.push_back("/Library/Audio/Sounds/Banks");
  checkPaths.push_back("/Library/Audio/Sounds/SF2");
  checkPaths.push_back(
      "/System/Library/Components/CoreAudio.component/Contents/Resources");

  if (includeUserDirs) {
    // Získání domovského adresáře uživatele
    const char *homeDir = std::getenv("HOME");
    if (homeDir) {
      std::string userHome(homeDir);
      checkPaths.push_back(userHome + "/Library/Audio/Sounds/Banks");
      checkPaths.push_back(userHome +
                           "/Music/Audio Music Apps/Sampler Instruments");
    }
  }
#endif

// Cesty specifické pro Linux
#ifdef __linux__
  checkPaths.push_back("/usr/share/sounds/sf2");
  checkPaths.push_back("/usr/share/soundfonts");
  checkPaths.push_back("/usr/local/share/soundfonts");

  if (includeUserDirs) {
    // Získání domovského adresáře uživatele
    const char *homeDir = std::getenv("HOME");
    if (homeDir) {
      std::string userHome(homeDir);
      checkPaths.push_back(userHome + "/.local/share/soundfonts");
      checkPaths.push_back(userHome + "/.soundfonts");
    }
  }
#endif

  // Kontrola všech cest
  for (const auto &path : checkPaths) {
    if (!fs::exists(path))
      continue;

    try {
      // Nejprve kontrola přímých souborů v adresáři
      for (const auto &entry : fs::directory_iterator(path)) {
        if (entry.is_regular_file() && isSoundFontFile(entry.path().string())) {
          soundfontPaths.push_back(entry.path().string());
        }
      }

      // Pokud je adresář, zkus rekurzivní hledání (omezená hloubka)
      auto recursiveResults = findSoundFontsInDirectory(path);
      soundfontPaths.insert(soundfontPaths.end(), recursiveResults.begin(),
                            recursiveResults.end());
    } catch (const std::exception &e) {
      NOTE_NAGA_LOG_ERROR("Error searching directory " + path + ": " +
                          e.what());
    }
  }

  // Zkontroluj také aktuální adresář a adresář aplikace
  try {
    auto currentDirSoundfonts = findSoundFontsInDirectory(".", 1);
    soundfontPaths.insert(soundfontPaths.end(), currentDirSoundfonts.begin(),
                          currentDirSoundfonts.end());
  } catch (const std::exception &e) {
    NOTE_NAGA_LOG_ERROR("Error searching current directory: " +
                        std::string(e.what()));
  }

  return soundfontPaths;
}

bool SoundFontFinder::fileExists(const std::string &path) {
  std::ifstream f(path.c_str());
  return f.good();
}

std::vector<std::string>
SoundFontFinder::findSoundFontsInDirectory(const std::string &directory,
                                           int maxDepth) {
  std::vector<std::string> results;

  if (maxDepth <= 0)
    return results;

  try {
    for (const auto &entry : fs::directory_iterator(directory)) {
      if (entry.is_regular_file() && isSoundFontFile(entry.path().string())) {
        results.push_back(entry.path().string());
      } else if (entry.is_directory()) {
        auto subResults =
            findSoundFontsInDirectory(entry.path().string(), maxDepth - 1);
        results.insert(results.end(), subResults.begin(), subResults.end());
      }
    }
  } catch (const std::exception &e) {
    // Tiché ignorování chyb - některé adresáře nemusí být přístupné
  }

  return results;
}

bool SoundFontFinder::isSoundFontFile(const std::string& path) {
    std::string lowercasePath = path;
    std::transform(lowercasePath.begin(), lowercasePath.end(), lowercasePath.begin(), 
                  [](unsigned char c){ return std::tolower(c); });
    
    return endsWith(lowercasePath, ".sf2") || endsWith(lowercasePath, ".sf3");
}