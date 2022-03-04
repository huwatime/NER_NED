// Copyright 2019, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Yi-Chun Lin <circle40191@gmail.com>

#include <fstream>
#include "utils.hpp"

using std::cout;

const char OUTPUT_FILE_PREFIX[] = "clueweb-freebase-iob-annotations";
const uint64_t RECORD_NUM = 1499211974;

inline void getNextWord(
    std::ifstream& f, vector<string>& fields, vector<string>& remaining) {
  // Due to unknown reasons, wordsfile sometimes contains spaces in a word,
  // which breaks our assumption in docsfile as we use " " as delimeter.
  // So we use " " to further splits the word in wordsfile just in case.
  string line;
  if (remaining.empty()) {
    if (std::getline(f, line)) {
      fields = tokenlize(line, '\t');
      remaining = tokenlize(fields[0], ' ');
      std::reverse(remaining.begin(), remaining.end());
    } else {
      fields = {"", "-1", "-1"};
      remaining = {""};
    }
  }
  fields[0] = remaining.back();
  remaining.pop_back();
}

void quickSeek(std::ifstream& f, const uint64_t goal, const int tokenPos) {
  uint64_t curIdx = 0;
  string line;
  std::streampos left, right, pos;

  left = 0;
  f.seekg(0, f.end);
  right = f.tellg();
  f.seekg(0);

  if (goal == 0) {
    std::getline(f, line);
    return;
  }

  while (curIdx != goal) {
    pos = (left + right) / 2;
    f.seekg(pos);
    std::getline(f, line);  // The first line may be incomplete
    std::getline(f, line);
    curIdx = stoi(tokenlize(line, '\t')[tokenPos]);
    // cout << "jumping to line " << curIdx << "\n";
    if (curIdx > goal) {
      right = pos;
    } else {
      left = pos;
    }
  }
}

/*
 * Generate clueweb IOB file with freebase_id for NER_NED.
 *
 * One sentence per line in the format of:
 * LINE_NO <TAB> WORD1\TAG1\[IOB] <SPACE> WORD2\TAG2\[IOB] <SPACE> ...
 *
 * Note:
 * 1) In the case of B, replace B by freebase ID.
 * 2) Since clueweb benchmark doesn't contain tagging information,
 *    all TAGs are replaced by "?".
 */
void genCluewebFreebaseIOB(
    const string& docsFile, const string& wordsFile, const string& outFile,
    const uint64_t beginIdx, const uint64_t endIdx) {
  std::ifstream fDocs(docsFile.c_str());
  std::ifstream fWords(wordsFile.c_str());
  std::ofstream fOut(outFile.c_str());

  string line;
  vector<string> lineFields;
  uint64_t lineIdx = 0;

  vector<string> wordFields;
  vector<string> remainingWords;

  vector<string> lastEntityIds = {"", ""};

  const string defaultTextPostfix = "\\?\\O";

  // Seek starting position
  // We want to goto the previous line of our goal.
  if (beginIdx > 0) {
    cout << "Seeking starting position [" << beginIdx << "] in docsFile...\n";
    quickSeek(fDocs, beginIdx - 1, 0);

    cout << "Seeking starting position [" << beginIdx << "] in wordsFile...\n";
    quickSeek(fWords, beginIdx - 1, 2);
  }

  // Init wordFields and remaining
  getNextWord(fWords, wordFields, remainingWords);

  // (1) Loop each sentence in the desired range of docsFile
  while (std::getline(fDocs, line) && lineIdx < endIdx) {
    bool endOfLine = false;
    vector<string> textList;
    unsigned int textIdx = 0;

    lineFields = tokenlize(line, '\t');
    lineIdx = stoull(lineFields[0]);
    printProgress(lineIdx - beginIdx, endIdx - beginIdx);

    // Check current line index in wordsFile. Advance in wordsFile until
    // it's sync with line index in docsFile.
    while (std::stoull(wordFields[2]) < lineIdx) {
      getNextWord(fWords, wordFields, remainingWords);
    }

    // (2) Seperate each sentence by space into words.
    textList = tokenlize(lineFields[1], ' ');

    // (3) Add proper postfix to all texts in this sentence,
    //     by looking at all words beloning to this sentence in wordsFile.
    //     Since wordsFile doesn't contain punctuations but do repeat words
    //     when it's an entity, it's not 1-on-1 mapping to the text. We need
    //     to advance in word and text at different pace. Be careful.
    while (!endOfLine) {
      string& text = textList[textIdx];
      const string& word = wordFields[0];
      bool wordIsEntity = wordFields[1] == "1";
      string entityId("");
      bool wordMatched = wordIsEntity || lowercase(word) == lowercase(text);

      // Note: we need textIdx > 0, otherwise, no previous text to modify.
      if (wordIsEntity && textIdx > 0) {
        entityId = word.size() > 29 ? word.substr(28, word.size() - 29) : "B";
        // Modify previous postfix, no advance in text
        string& lastText = textList[textIdx - 1];
        lastText.erase(lastText.end() - 1);
        lastText += lastEntityIds[0] == entityId ? "I" : entityId;
      } else {
        // Add default postfix to text and advance to next text
        text = text + defaultTextPostfix;
        textIdx++;
        endOfLine = textIdx == textList.size();
      }

      // As in wordsfile, it takes two lines to represent one entity,
      // we need to keep track of two last IDs to really tracking an entity of
      // more than one word.
      lastEntityIds[0] = lastEntityIds[1];
      lastEntityIds[1] = entityId;

      if (wordMatched) {
        // If word matched with text, advance to next word
        getNextWord(fWords, wordFields, remainingWords);
      }
    }
    // (4) End of line reached, write processed text to file
    fOut << lineIdx << '\t' << join(textList, ' ') << '\n';
  }

  fDocs.close();
  fWords.close();
  fOut.close();
}

int main(int argc, char** argv) {
  if (argc < 4) {
    cout << "\nUsage: \n" <<
      "  gen_clueweb_freebase_iob_main <docsfile> <wordsfile> " <<
      "<output_dir> [ <from> ] [ <to> ]\n" <<
      "\nDescription: \n" <<
      "  Generate the IOB ground truth of clueweb with freebase_id for " <<
      "NER_NED usage. \n\n" <<
      "  <docsfile> <wordsfile>\n" <<
      "    See https://github.com/ad-freiburg/QLever/blob/master/docs/" <<
      "sparql_plus_text.md\n\n" <<
      "  <output_dir>\n" <<
      "    Specify the directory you want to store the output.\n\n" <<
      "  <from> <to>\n" <<
      "    Specify the range of the record id(stated in docsfile) that " <<
      "you want to generate.\n" <<
      "    Default from 0 to " << RECORD_NUM << ".\n\n";
    return 1;
  }

  uint64_t from = argc > 5 ? atoll(argv[4]) : 0;
  uint64_t to = argc > 5 ? atoll(argv[5]) : RECORD_NUM;
  from = from > RECORD_NUM ? RECORD_NUM : from;
  to = to > RECORD_NUM || to < from ? RECORD_NUM : to;

  char outputPath[512] = "\0";
  snprintf(outputPath, sizeof(outputPath),
      "%s/%s.%lu-%lu", argv[3], OUTPUT_FILE_PREFIX, from, to);
  cout << "\nOutput path: " << outputPath << "\n";

  genCluewebFreebaseIOB(argv[1], argv[2], outputPath, from, to);
  cout << "\nDone!\n\n";
  return 0;
}
