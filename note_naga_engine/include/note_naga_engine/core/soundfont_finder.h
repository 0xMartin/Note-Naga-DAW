#pragma once

#include <string>
#include <vector>

class SoundFontFinder {
public:
  /**
   * @brief Searches for available SoundFont files in the system
   * @return Path to the first found SoundFont file or an empty string
   * if none is found
   */
  static std::string findSoundFont();

  /**
   * @brief Gets a list of all SoundFont files available in the system
   * @param includeUserDirs Whether to include user directories
   * @return List of paths to found files
   */
  static std::vector<std::string> getAllSoundFonts(bool includeUserDirs = true);

private:
  /**
   * @brief Checks if a file exists
   * @param path Path to the file
   * @return true if the file exists
   */
  static bool fileExists(const std::string &path);

  /**
   * @brief Recursively searches for SoundFont files in a directory
   * @param directory Path to the directory
   * @param maxDepth Maximum recursion depth
   * @return List of found SoundFont files
   */
  static std::vector<std::string>
  findSoundFontsInDirectory(const std::string &directory, int maxDepth = 3);

  /**
   * @brief Checks if the path has a SoundFont file extension (.sf2, .sf3)
   * @param path Path to the file
   * @return true if the file is a SoundFont
   */
  static bool isSoundFontFile(const std::string &path);

  /**
   * @brief Checks if a string ends with the given suffix
   * @param str String to check
   * @param suffix Suffix to search for
   * @return true if the string ends with the given suffix
   */
  static bool endsWith(const std::string &str, const std::string &suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
  }
};