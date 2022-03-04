// Copyright 2019, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Yi-Chun Lin <circle40191@gmail.com>

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <ctime>

using std::string;
using std::vector;

inline vector<string> tokenlize(const string& in, const char del) {
  std::istringstream iss(in);
  string token;
  vector<string> tokenList;
  while (std::getline(iss, token, del)) {
    tokenList.push_back(token);
  }
  return tokenList;
}

inline void tokenlize(const string& in, const char del,
    vector<string>& tokenList) {
  std::istringstream iss(in);
  string token;
  tokenList.clear();
  while (std::getline(iss, token, del)) {
    tokenList.push_back(token);
  }
}

inline string join(const vector<string>& tokens, const char del) {
  string output("");
  for (auto token : tokens) {
    output += token + del;
  }
  output.erase(output.end() - 1);
  return output;
}

inline string lowercase(const string orig) {
  string lower = orig;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  return lower;
}

inline void printProgress(const uint64_t cur, const uint64_t total) {
  uint64_t freq = total / 1000 + 1;
  if (cur % freq != 0) {
    return;
  }

  double percent = cur * 100.0 / total;
  printf("Processing... %.1f%s\r", percent, "%");
  fflush(stdout);
}

string getDate() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y_%m_%d");
    return ss.str();
}

inline string getDuration(
    const std::chrono::high_resolution_clock::time_point& t1,
    const std::chrono::high_resolution_clock::time_point& t2) {
  size_t time = std::chrono::duration_cast<std::chrono::seconds>
    (t2 - t1).count();
  std::stringstream ss;
  ss << time / 3600 << "h " << (time % 3600) / 60 << "m " << time % 60 << "s";
  return ss.str();
}

inline string printStat(const string& key, const string& value) {
  std::stringstream ss;
  ss << "  \"" << key << "\": \"" << value << "\",\n";
  return ss.str();
}

inline string getFileSize(std::ifstream& f) {
  if (f.tellg() == -1) {
    f.clear();
    f.seekg(0, f.end);
  }
  return to_string(f.tellg());
}

inline string getFileSize(std::ofstream& f) {
  if (f.tellp() == -1) {
    f.clear();
    f.seekp(0, f.end);
  }
  return to_string(f.tellp());
}
inline string getFileName(const string& f) {
  vector<string> fields = tokenlize(f, '/');
  auto index = fields.size() - 1;
  return fields[index];
}
