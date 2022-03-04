// Copyright 2020, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Yi-Chun Lin <circle40191@gmail.com>

#include <fstream>
#include <unordered_map>
#include "utils.hpp"

using std::cout;

const char INPUT_AIDA[] = "AIDA-YAGO2-annotations.tsv";
const char INPUT_TRAIN[] = "eng.train";
const char INPUT_TESTA[] = "eng.testa";
const char INPUT_TESTB[] = "eng.testb";
const char OUTPUT_FILE_NAME[] = "conll-wikidata-iob-annotations";

/*
 * Generate CoNLL-2003 IOB file with wikidata annotations for NER_NED.
 *
 * wikipedia_url of each entity can be obtained by aido-yago2-annotations,
 * which can be further mapped to wikidata_id by the mapping file provided.
 *
 * Output: One corpus per line in the format of
 * CORPUS_NO <TAB> WORD1\TAG1\[IOB] <SPACE> WORD2\TAG2\[IOB] <SPACE> ...
 *
 * Note: In the case of B, replace B by wikidata / wikipedia_url if exist.
 */
void genConllWikidataIOB(
    const string& annotationFile, const vector<string>& datasetFiles,
    const string& wikiMapFile, const string& freebaseMapFile,
    const string& outFile) {
  std::ifstream fAnnot(annotationFile.c_str());
  std::ifstream fWikiMap(wikiMapFile.c_str());
  std::ifstream fFreebaseMap(freebaseMapFile.c_str());
  std::ofstream fOut(outFile.c_str());
  int corpusIdx = 0;
  vector<string> wordList;

  string mapLine;
  vector<string> mapTokens;
  std::unordered_map<string, string> wikiMap;
  std::unordered_map<string, string> freebaseMap;

  cout << "\nLoading wikipedia url mapping file ...";
  // Line format:
  // <https://en.wikipedia.org/wiki/xxx>,<http://www.wikidata.org/entity/xxx>
  while (std::getline(fWikiMap, mapLine)) {
    string wikidataId, wikipediaUrl;
    tokenlize(mapLine, ',', mapTokens);

    if (mapTokens.size() != 2 ||
        mapTokens[0].size() < 8 ||
        mapTokens[1].size() < 34) {
      continue;
    }

    mapTokens[0].erase(mapTokens[0].end() - 1);
    mapTokens[1].erase(mapTokens[1].end() - 1);
    wikipediaUrl = "http" + mapTokens[0].substr(6);
    wikidataId = mapTokens[1].substr(32);
    wikiMap[wikipediaUrl] = wikidataId;
  }

  cout << "\nLoading freebase id mapping file ...";
  // Line format: <http://www.wikidata.org/entity/xxx>,"/m/xxx"
  while (std::getline(fFreebaseMap, mapLine)) {
    string wikidataId, freebaseId;
    tokenlize(mapLine, ',', mapTokens);

    if (mapTokens.size() != 2 ||
        mapTokens[0].size() < 34 ||
        mapTokens[1].size() < 4) {
      continue;
    }

    mapTokens[0].erase(mapTokens[0].end() - 1);
    mapTokens[1].erase(mapTokens[1].end() - 1);
    wikidataId = mapTokens[0].substr(32);
    freebaseId = mapTokens[1].substr(1);
    freebaseMap[freebaseId] = wikidataId;
  }

  for (const string& filename : datasetFiles) {
    std::ifstream fData(filename.c_str());
    string dataLine;
    string annotLine;
    vector<string> dataTokens;
    vector<string> annotTokens;
    string curWordType = "O";
    string prevWordType = "O";
    string IBO;

    cout << "\nProcessing " << filename << " ...";
    while (std::getline(fData, dataLine)) {
      string word;
      tokenlize(dataLine, ' ', dataTokens);

      if (dataTokens.size() == 0) {
        continue;
      }

      if (dataTokens[0] == "-DOCSTART-") {
        if (corpusIdx > 0) {
          fOut << corpusIdx << '\t' << join(wordList, ' ') << '\n';
          wordList.clear();
          prevWordType = "O";

          // Read the empty line at the end of each corpus in annotFile
          std::getline(fAnnot, annotLine);
        }
        corpusIdx++;

        // Read next annotFile -DOCSTART- line.
        if (std::getline(fAnnot, annotLine)) {
          if (annotLine.substr(0, 10) != "-DOCSTART-") {
            cout << corpusIdx << '\t' << "Unexpected format in annotFile. "
              << "expect -DOCSTART- line, get[" << annotLine << "]\n";
            return;
          }
        }

        continue;
      }

      if (dataTokens.size() != 4) {
        cout << "Unexpected format at corpus " << corpusIdx << " word "
          << wordList.size() - 1 << ": " << dataLine << "\n";
        continue;
      }

      curWordType = dataTokens[3];
      if (curWordType == "O") {
        IBO = "O";
      } else {
        if (curWordType != prevWordType && std::getline(fAnnot, annotLine)) {
            tokenlize(annotLine, '\t', annotTokens);
        }

        if (curWordType == prevWordType) {
          IBO = "I";
        } else if (annotTokens.size() >= 5 &&
            freebaseMap.find(annotTokens[4]) != freebaseMap.end()) {
          IBO = freebaseMap[annotTokens[4]];
        } else if (annotTokens.size() >=3 &&
            wikiMap.find(annotTokens[2]) != wikiMap.end()) {
          IBO = wikiMap[annotTokens[2]];
        } else {
          IBO = "B";
        }
      }

      // Handle &amp;
      std::size_t pos = dataTokens[0].find("&amp;");
      if (pos != string::npos) {
        dataTokens[0] = dataTokens[0].substr(0, pos+1) +
          dataTokens[0].substr(pos+5);
      }

      word = dataTokens[0] + "\\?\\" + IBO;
      wordList.push_back(word);
      prevWordType = curWordType.substr(0, 1) == "B" ?
        "I" + curWordType.substr(1) : curWordType;
    }

    fData.close();
  }

  // Write the last line
  fOut << corpusIdx << '\t' << join(wordList, ' ') << '\n';

  fAnnot.close();
  fWikiMap.close();
  fFreebaseMap.close();
  fOut.close();
}

int main(int argc, char** argv) {
  if (argc < 5) {
    cout << "\nUsage: \n" <<
      "  gen_conll_wikidata_iob_main <dataset_dir> " <<
      "<wikipedia_url_map_file> <freebase_id_map_file> <output_dir>\n" <<
      "\nDescription: \n" <<
      "  Generate the IOB ground truth of CoNLL-2003 dataset with " <<
      "wikidata annotations for NER_NED usage. \n\n" <<
      "  <dataset_dir>\n" <<
      "    Should include the following files:\n" <<
      "    1. " << INPUT_AIDA << " from aida-yago2-dataset.\n" <<
      "       See https://www.mpi-inf.mpg.de/departments/databases-and-inf" <<
      "ormation-systems/research/yago-naga/aida/downloads for details.\n" <<
      "    2. " << INPUT_TRAIN << ", " << INPUT_TESTA <<
      ", " << INPUT_TESTB << " from CoNLL-2003 dataset.\n\n" <<
      "  <wikipedia_url_map_file> \n" <<
      "    mappings between wikipedia URL and wikidata ID,\n" <<
      "    generated by qLever at http://qlever.informatik.uni-freiburg.de/"
      "Wikidata_Full\n" <<
      "    of line format <https://en.wikipedia.org/wiki/xxx>," <<
      "<http://www.wikidata.org/entity/xxx>\n\n" <<
      "  <id_mapping_csv> \n" <<
      "    mappings between freebase and wikidata IDs,\n" <<
      "    generated by qLever at http://qlever.informatik.uni-freiburg.de/"
      "Wikidata_Full\n" <<
      "    of line format <http://www.wikidata.org/entity/xxx>," <<
      "\"/m/xxx\"\n\n" <<
      "  <output_dir>\n" <<
      "    Specify the directory you want to store the output.\n\n";
    return 1;
  }

  char outputPath[512] = "\0";
  snprintf(outputPath, sizeof(outputPath), "%s/%s",
      argv[4], OUTPUT_FILE_NAME);
  cout << "\nOutput path: " << outputPath << "\n";

  string annotationFile = string(argv[1]) + "/" + INPUT_AIDA;
  vector<string> datasetFiles = {
    string(argv[1]) + "/" + INPUT_TRAIN,
    string(argv[1]) + "/" + INPUT_TESTA,
    string(argv[1]) + "/" + INPUT_TESTB
  };
  genConllWikidataIOB(
      annotationFile, datasetFiles, argv[2], argv[3], outputPath);
  cout << "\nDone!\n\n";
  return 0;
}
