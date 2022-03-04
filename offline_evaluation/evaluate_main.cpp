// Copyright 2019, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Yi-Chun Lin <circle40191@gmail.com>

#include <unordered_map>
#include <iterator>
#include <tuple>
#include <sys/stat.h>
#include "utils.hpp"

using std::cout;
using std::to_string;

#define NERNED_CORRECT 0
#define NERNED_WRONG 1
#define NERNED_MISMATCH 2

std::unordered_map<string, unsigned int> flagBitMap({
    {"S_fp", (1 << 0)},
    {"B_fp", (1 << 1)},
    {"I_fp", (1 << 2)},
    {"E_fp", (1 << 3)},
    {"O_fp", (1 << 4)},
    {"S_fn", (1 << 5)},
    {"B_fn", (1 << 6)},
    {"I_fn", (1 << 7)},
    {"E_fn", (1 << 8)},
    {"O_fn", (1 << 9)}
    });  // Add new flags from here

bool getNextLine(std::ifstream& f, vector<string>& algWords,
    vector<string>& truthWords, uint64_t& lineIdx, size_t& pos) {
  string line;

  pos = f.tellg();
  if (!std::getline(f, line)) {
    return false;
  }

  vector<string> lineFields = tokenlize(line, '\t');
  lineIdx = stoull(lineFields[0]);
  tokenlize(lineFields[1], ' ', truthWords);
  tokenlize(lineFields[2], ' ', algWords);
  return true;
}

string getBIOES(const string& word, const string& nextWord,
    vector<string>& wordFields, vector<string>& nextWordFields) {
  tokenlize(word, '\\', wordFields);
  string BIOES = wordFields.size() < 3 ? "O" : wordFields[2];
  tokenlize(nextWord, '\\', nextWordFields);
  string nextBIOES = nextWordFields.size() < 3 ? "O" : nextWordFields[2];

  if (BIOES == "O") {
    return BIOES;
  }

  if (BIOES == "I") {
    return nextBIOES == "I" ? "I" : "E";
  }

  return nextBIOES == "I" ? "B" : "S";
}

inline double computeF1(const uint64_t& tp,
    const uint64_t& fp, const uint64_t& fn) {
  if (tp == 0 && fp == 0 && fn == 0) {
    return 1.0;
  }

  if (tp == 0) {
    return 0.0;
  }

  double p = static_cast<double>(tp) / (tp + fp);
  double r = static_cast<double>(tp) / (tp + fn);
  return p * r * 2 / (p + r);
}

void evaluate(const string& algFile, const string& benchmarkType,
    const string& statFile, const string& NerNedFile, const string& NerFile) {
  std::ifstream fAlg(algFile.c_str());
  std::ofstream fStat(statFile.c_str());
  std::ofstream fNerNed(NerNedFile.c_str());
  std::ofstream fNer(NerFile.c_str());

  uint64_t lineIdx;
  size_t linePos;
  vector<string> algWords;
  vector<string> truthWords;
  vector<string> wordFields;
  vector<string> nextWordFields;

  unsigned int flags = 0;

  std::unordered_map<string, std::unordered_map<string, int>> statsBIOES;
  statsBIOES["B"] = {{"tp", 0}, {"fp", 0}, {"fn", 0}};
  statsBIOES["I"] = {{"tp", 0}, {"fp", 0}, {"fn", 0}};
  statsBIOES["O"] = {{"tp", 0}, {"fp", 0}, {"fn", 0}};
  statsBIOES["E"] = {{"tp", 0}, {"fp", 0}, {"fn", 0}};
  statsBIOES["S"] = {{"tp", 0}, {"fp", 0}, {"fn", 0}};

  std::unordered_map<string, uint64_t> statsSentence({
      {"num_total", 0},
      {"num_correct", 0},
      {"num_wrong", 0},
      {"num_mismatch", 0},
      });

  uint64_t microTp = 0;
  uint64_t microFp = 0;
  uint64_t microFn = 0;
  double microF1InKB = 0.0;
  double macroF1InKB = 0.0;

  auto time1 = std::chrono::high_resolution_clock::now();

  while (getNextLine(fAlg, algWords, truthWords, lineIdx, linePos)) {
    uint64_t macroTp = 0;
    uint64_t macroFp = 0;
    uint64_t macroFn = 0;

    flags = 0;
    statsSentence["num_total"]++;

    if (algWords.size() != truthWords.size()) {
      fNerNed << lineIdx << "\t" << linePos << "\t" << NERNED_MISMATCH << "\n";
      statsSentence["num_mismatch"]++;
      continue;
    }

    // Add dummy tail
    algWords.push_back("du\\mm\\y");
    truthWords.push_back("du\\mm\\y");

    bool sentenceCorrect = true;
    std::tuple<int, int, string> truthEntity(0, 0, "");
    std::tuple<int, int, string> algEntity(0, 0, "");
    vector<std::tuple<int, int, string>> truths;
    vector<std::tuple<int, int, string>> algs;

    // For each word in the sentence
    for (size_t i = 0; i < algWords.size() - 1; i++) {
      string algBIOES = getBIOES(algWords[i], algWords[i+1],
          wordFields, nextWordFields);
      string truthBIOES = getBIOES(truthWords[i], truthWords[i+1],
          wordFields, nextWordFields);
      string algId = tokenlize(algWords[i], '\\')[2];
      string truthId = tokenlize(truthWords[i], '\\')[2];

      // update NER stats
      if (algBIOES == truthBIOES) {
        statsBIOES[algBIOES]["tp"] += 1;
      } else {
        statsBIOES[algBIOES]["fp"] += 1;
        statsBIOES[truthBIOES]["fn"] += 1;
        flags |= flagBitMap[string(algBIOES + "_fp")];
        flags |= flagBitMap[string(truthBIOES + "_fn")];
      }

      // update NER_NED stats
      if (truthBIOES == "B" || truthBIOES == "S") {
        std::get<0>(truthEntity) = i;
        std::get<2>(truthEntity) = truthId;
      }

      if (truthBIOES == "E" || truthBIOES == "S") {
        std::get<1>(truthEntity) = i;
        truths.push_back(truthEntity);
      }

      if (algBIOES == "B" || algBIOES == "S") {
        std::get<0>(algEntity) = i;
        std::get<2>(algEntity) = algId;
      }

      if (algBIOES == "E" || algBIOES == "S") {
        std::get<1>(algEntity) = i;
        algs.push_back(algEntity);
      }
    }

    fNer << lineIdx << "\t" << linePos << "\t" << flags << "\n";

    size_t j = 0;
    for (auto e : algs) {
      int head = std::get<0>(e);
      int tail = std::get<1>(e);
      string id = std::get<2>(e);
      bool inKB = id.substr(0, 1) == "Q";

      if (!inKB) {
        continue;
      }

      if (truths.size() == 0) {
        macroFp++;
        sentenceCorrect = false;
        continue;
      }

      // forward truths to an overlap with algs
      while (std::get<1>(truths[j]) < head && j < truths.size() - 1) {
        j++;
      }

      if (std::get<0>(truths[j]) == head &&
          std::get<1>(truths[j]) == tail &&
          std::get<2>(truths[j]) == id) {
        macroTp++;
      } else if (std::get<0>(truths[j]) <= head &&
          std::get<1>(truths[j]) >= tail &&
          std::get<2>(truths[j]).substr(0, 1) != "Q") {
        // outKB
      } else {
        macroFp++;
        sentenceCorrect = false;
      }
    }

    // update recall counts: fn
    j = 0;
    int debug = 0;
    for (auto e : truths) {
      int head = std::get<0>(e);
      int tail = std::get<1>(e);
      string id = std::get<2>(e);
      bool inKB = id.substr(0, 1) == "Q";

      if (!inKB) {
        continue;
      }

      if (algs.size() == 0) {
        macroFn++;
        sentenceCorrect = false;
        continue;
      }

      // forward algs to an overlap with truths
      while (std::get<1>(algs[j]) < head && j < algs.size() - 1) {
        j++;
      }

      if (std::get<0>(algs[j]) == head &&
          std::get<1>(algs[j]) == tail &&
          std::get<2>(algs[j]) == id) {
        debug++;
      } else {
        macroFn++;
        sentenceCorrect = false;
      }
    }

    if (sentenceCorrect) {
      statsSentence["num_correct"]++;
      fNerNed << lineIdx << "\t" << linePos << "\t" << NERNED_CORRECT << "\n";
    } else {
      statsSentence["num_wrong"]++;
      fNerNed << lineIdx << "\t" << linePos << "\t" << NERNED_WRONG << "\n";
    }

    microTp += macroTp;
    microFp += macroFp;
    microFn += macroFn;

    macroF1InKB += computeF1(macroTp, macroFp, macroFn);
  }

  microF1InKB = computeF1(microTp, microFp, microFn);
  macroF1InKB /= (statsSentence["num_total"] - statsSentence["num_mismatch"]);

  auto time2 = std::chrono::high_resolution_clock::now();

  fStat << "{\n";

  fStat << printStat("duration", getDuration(time1, time2));
  fStat << printStat(
      "alg_filename", benchmarkType + "/" + getFileName(algFile));
  fStat << printStat("filesize_ner_ned", getFileSize(fNerNed));
  fStat << printStat("filesize_ner", getFileSize(fNer));
  fStat << printStat("micro_F1_InKB", to_string(microF1InKB));
  fStat << printStat("macro_F1_InKB", to_string(macroF1InKB));
  fStat << printStat("micro_Tp", to_string(microTp));
  fStat << printStat("micro_Fp", to_string(microFp));
  fStat << printStat("micro_Fn", to_string(microFn));

  for (const auto& elem : statsSentence) {
    fStat << printStat(elem.first, to_string(elem.second));
  }

  for (const auto& elem : statsBIOES) {
    string key = elem.first + "_";
    for (const auto& elem2 : elem.second) {
      fStat << printStat(key + elem2.first, to_string(elem2.second));
    }
  }

  fStat << "  \"dummy\": \"tail\"\n}\n";

  fAlg.close();
  fStat.close();
  fNerNed.close();
  fNer.close();
}

int main(int argc, char** argv) {
  if (argc < 3) {
    cout << "\nUsage: \n" <<
      "  evaluate_main <algorithm_iob_file> <eval_results_dir>\n\n";
    return 1;
  }

  vector<string> types = {"clueweb", "manual", "conll"};
  string benchmarkType = "others";
  for (auto type : types) {
      std::size_t pos = string(argv[1]).find(type);
      if (pos != string::npos) {
          benchmarkType = type;
          break;
      }
  }

  string outputDir = string(argv[2]) + "/" + benchmarkType;

  std::size_t posAlg = string(argv[1]).find("alg");
  vector<string> fields = tokenlize(string(argv[1]).substr(posAlg), '.');
  outputDir += "-" + fields[0].substr(4);
  if (fields.size() > 1) {
    outputDir += "-" + fields[1];
  }

  if (mkdir(outputDir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
    if (errno != EEXIST) {
        cout << "Cannot create result folder " << outputDir << "\n";
        return 1;
    }
  }

  string statFilepath = outputDir + "/stat";
  string NerNedFilepath = outputDir + "/detail_ner_ned";
  string NerFilepath = outputDir + "/detail_ner";
  cout << "\nOutput path:\n" << statFilepath << "\n" << NerNedFilepath << "\n"
    << NerFilepath << "\n";
  evaluate(argv[1], benchmarkType, statFilepath, NerNedFilepath, NerFilepath);
  cout << "\nDone!\n\n";
  return 0;
}

