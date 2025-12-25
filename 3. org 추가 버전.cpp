#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
using namespace std;

struct Line {
    int loc;
    string label;
    string opcode;
    string operand;
};

struct Literal {
    string value;
    int address;
};

string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

string toUpper(string s) {
    transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

bool isNumber(const string& s) {
    return all_of(s.begin(), s.end(), ::isdigit);
}

int main() {
    unordered_map<string, string> OPTAB;
    unordered_map<string, int> SYMTAB;
    vector<Literal> literalPool;
    vector<Line> intermediate;

    // ---------- OPTAB ----------
    ifstream optFile("OPTAB.txt");
    if (!optFile) {
        cerr << "OPTAB.txt 파일을 열 수 없습니다.\n";
        return 1;
    }

    string key, code;
    while (optFile >> key >> code) {
        key = toUpper(trim(key));
        OPTAB[key] = trim(code);
    }
    optFile.close();

    // ---------- SRCFILE ----------
    ifstream src("SRCFILE6.txt");
    if (!src) {
        cerr << "SRCFILE.txt 파일을 열 수 없습니다.\n";
        return 1;
    }

    string line;
    int LOCCTR = 0, startAddr = 0;
    static int savedLOCCTR = 0; // ORG 복귀 기능용 변수

    // ---------- PASS 1 ----------
    while (getline(src, line)) {
        if (line.empty() || line[0] == '.') continue;
        line.erase(remove(line.begin(), line.end(), '\r'), line.end());
        line = trim(line);

        istringstream iss(line);
        vector<string> tokens;
        string temp;
        while (iss >> temp) tokens.push_back(temp);
        if (tokens.empty()) continue;

        for (auto& t : tokens) t = toUpper(trim(t));

        string label = "", opcode = "", operand = "";
        if (tokens.size() == 1)
            opcode = tokens[0];
        else if (tokens.size() == 2) {
            opcode = tokens[0];
            operand = tokens[1];
        }
        else if (tokens.size() >= 3) {
            if (OPTAB.find(tokens[0]) != OPTAB.end() ||
                tokens[0] == "START" || tokens[0] == "END" ||
                tokens[0] == "WORD" || tokens[0] == "RESW" ||
                tokens[0] == "RESB" || tokens[0] == "BYTE" ||
                tokens[0] == "LTORG" || tokens[0] == "EQU" ||
                tokens[0] == "ORG") {
                opcode = tokens[0];
                operand = tokens[1];
            }
            else {
                label = tokens[0];
                opcode = tokens[1];
                operand = tokens[2];
            }
        }

        // START
        if (opcode == "START") {
            startAddr = stoi(operand, nullptr, 16);
            LOCCTR = startAddr;
            intermediate.push_back({ LOCCTR, label, opcode, operand });
            continue;
        }

        // EQU (기호 상수 정의)
        if (opcode == "EQU") {
            int value = 0;
            if (operand == "*") value = LOCCTR; // 현재 주소
            else if (isNumber(operand)) value = stoi(operand); // 숫자 상수
            else if (SYMTAB.find(operand) != SYMTAB.end())
                value = SYMTAB[operand]; // 기존 심볼 참조
            else
                cerr << "Error: EQU 해석 실패 (" << operand << ")" << endl;

            SYMTAB[label] = value; // 심볼 테이블에 값 저장
            intermediate.push_back({ LOCCTR, label, opcode, operand });
            continue;
        }

        // ORG (주소 재지정)
        if (opcode == "ORG") {
            if (operand.empty()) {
                // 피연산자 없음 → 이전 LOCCTR 복귀
                if (savedLOCCTR != 0)
                    LOCCTR = savedLOCCTR;
            }
            else {
                // 현재 LOCCTR 저장
                savedLOCCTR = LOCCTR;

                if (operand == "*") {
                    // 현재 위치 유지
                }
                else if (isNumber(operand)) {
                    LOCCTR = stoi(operand, nullptr, 16); // 숫자 주소 지정
                }
                else if (SYMTAB.find(operand) != SYMTAB.end()) {
                    LOCCTR = SYMTAB[operand]; // 기존 심볼 주소로 이동
                }
                else {
                    cerr << "Error: ORG 해석 실패 (" << operand << ")" << endl;
                }
            }
            intermediate.push_back({ LOCCTR, label, opcode, operand });
            continue;
        }

        // SYMTAB 등록
        if (!label.empty() && opcode != "EQU" && opcode != "ORG") {
            if (SYMTAB.find(label) != SYMTAB.end())
                cerr << "Error: 중복된 심볼 " << label << endl;
            else
                SYMTAB[label] = LOCCTR;
        }

        // 리터럴 등록
        if (!operand.empty() && operand[0] == '=') {
            bool exists = false;
            for (auto& lit : literalPool)
                if (lit.value == operand) exists = true;
            if (!exists)
                literalPool.push_back({ operand, -1 });
        }

        intermediate.push_back({ LOCCTR, label, opcode, operand });

        // LOCCTR 증가
        if (OPTAB.find(opcode) != OPTAB.end()) LOCCTR += 3;
        else if (opcode == "WORD") LOCCTR += 3;
        else if (opcode == "RESW") LOCCTR += 3 * stoi(operand);
        else if (opcode == "RESB") LOCCTR += stoi(operand);
        else if (opcode == "BYTE") {
            if (operand[0] == 'C') LOCCTR += operand.length() - 3;
            else LOCCTR += (operand.length() - 3) / 2;
        }

        // LTORG / END 처리
        if (opcode == "LTORG" || opcode == "END") {
            for (auto& lit : literalPool) {
                if (lit.address == -1) {
                    lit.address = LOCCTR;
                    string val = lit.value;
                    if (val[1] == 'C') LOCCTR += val.length() - 4;
                    else if (val[1] == 'X') LOCCTR += (val.length() - 4) / 2;
                    intermediate.push_back({ lit.address, "*", "BYTE", val.substr(1) });
                }
            }
        }

        if (opcode == "END") break;
    }
    src.close();

    // ---------- INTFILE ----------
    ofstream intFile("INTFILE.txt");
    for (auto& l : intermediate) {
        intFile << setw(4) << setfill('0') << hex << l.loc << "  "
            << left << setw(10) << l.label
            << setw(10) << l.opcode
            << setw(10) << l.operand << endl;
    }
    intFile.close();

    // ---------- SYMTAB, LITTAB ----------
    cout << "\n===== SYMTAB =====" << endl;
    for (auto& s : SYMTAB)
        cout << setw(10) << s.first << " : " << hex << s.second << endl;

    cout << "\n===== LITTAB =====" << endl;
    for (auto& lit : literalPool)
        cout << setw(10) << lit.value << " : " << hex << lit.address << endl;

    // ---------- PASS 2 ----------
    ofstream objOut("OBJFILE.txt");
    string programName;
    int programLength = LOCCTR - startAddr;

    for (auto& l : intermediate) {
        if (l.opcode == "START") {
            programName = l.label.empty() ? "------" : l.label;
            objOut << "H "
                << left << setw(6) << setfill(' ') << programName.substr(0, 6)
                << right << setfill('0') << uppercase
                << setw(6) << hex << startAddr << ' '
                << setw(6) << hex << programLength << endl;
            break;
        }
    }

    const int MAX_TEXT_LEN = 30;
    string textRecord = "";
    int textStart = 0;
    int textLength = 0;

    for (auto& l : intermediate) {
        if (l.opcode == "START" || l.opcode == "END" || l.opcode == "LTORG" || l.opcode == "EQU" || l.opcode == "ORG") continue;

        string objCode = "";
        int objLen = 0;

        if (OPTAB.find(l.opcode) != OPTAB.end()) {
            string opcodeVal = OPTAB[l.opcode];
            int operandAddr = 0;
            if (l.operand == "*")
                operandAddr = l.loc;
            else if (!l.operand.empty() && SYMTAB.find(l.operand) != SYMTAB.end())
                operandAddr = SYMTAB[l.operand];
            else {
                for (auto& lit : literalPool)
                    if (lit.value == l.operand)
                        operandAddr = lit.address;
            }

            stringstream ss;
            ss << uppercase << opcodeVal << setw(4) << setfill('0') << hex << operandAddr;
            objCode = ss.str();
            objLen = 3;
        }
        else if (l.opcode == "WORD") {
            int val = stoi(l.operand);
            stringstream ss;
            ss << setw(6) << setfill('0') << hex << val;
            objCode = ss.str();
            objLen = 3;
        }
        else if (l.opcode == "BYTE") {
            string op = l.operand;
            if (op[0] == 'C') {
                for (size_t i = 2; i < op.size() - 1; ++i) {
                    stringstream ss;
                    ss << hex << uppercase << (int)op[i];
                    objCode += ss.str();
                }
                objLen = (int)(op.size() - 3);
            }
            else if (op[0] == 'X') {
                objCode = op.substr(2, op.size() - 3);
                objLen = (int)((objCode.size() + 1) / 2);
            }
        }

        if (!objCode.empty()) {
            if (textRecord.empty()) {
                textStart = l.loc;
                textLength = 0;
                textRecord = "";
            }
            if (textLength + objLen > MAX_TEXT_LEN) {
                objOut << "T " << setw(6) << setfill('0') << hex << textStart << ' '
                    << setw(2) << setfill('0') << hex << textLength << ' '
                    << textRecord << endl;
                textRecord.clear();
                textStart = l.loc;
                textLength = 0;
            }
            if (!textRecord.empty()) textRecord += ' ';
            textRecord += objCode;
            textLength += objLen;
        }
        else {
            if (!textRecord.empty()) {
                objOut << "T " << setw(6) << setfill('0') << hex << textStart << ' '
                    << setw(2) << setfill('0') << hex << textLength << ' '
                    << textRecord << endl;
                textRecord.clear();
                textLength = 0;
            }
        }
    }

    if (!textRecord.empty()) {
        objOut << "T " << setw(6) << setfill('0') << hex << textStart << ' '
            << setw(2) << setfill('0') << hex << textLength << ' '
            << textRecord << endl;
    }

    objOut << "E " << setw(6) << setfill('0') << hex << startAddr << endl;
    objOut.close();

    cout << "\nPass2 완료: ORG 복귀 기능 포함 OBJFILE.txt, INTFILE.txt 생성됨" << endl;
    return 0;
}
