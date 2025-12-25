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

// 문자열 트리밍 함수
string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// 대문자 변환
string toUpper(string s) {
    transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

int main() {
    unordered_map<string, string> OPTAB;
    unordered_map<string, int> SYMTAB;
    vector<Literal> literalPool;   // 리터럴 풀 (리터럴 목록)
    vector<Line> intermediate;     // 중간파일용 벡터

    // ---------- OPTAB 읽기 ----------
    ifstream optFile("OPTAB.txt");
    if (!optFile) {
        cerr << "OPTAB.txt 파일을 열 수 없습니다.\n";
        return 1;
    }

    string key, code;
    while (optFile >> key >> code) {
        key = toUpper(trim(key));
        code = trim(code);
        OPTAB[key] = code;
    }
    optFile.close();

    // ---------- SRCFILE 읽기 ----------
    ifstream src("SRCFILE2.txt");
    if (!src) {
        cerr << "SRCFILE.txt 파일을 열 수 없습니다.\n";
        return 1;
    }

    string line;
    int LOCCTR = 0, startAddr = 0;

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
                tokens[0] == "LTORG") {
                opcode = tokens[0];
                operand = tokens[1];
            }
            else {
                label = tokens[0];
                opcode = tokens[1];
                operand = tokens[2];
            }
        }

        if (opcode == "START") {
            startAddr = stoi(operand, nullptr, 16);
            LOCCTR = startAddr;
            intermediate.push_back({ LOCCTR, label, opcode, operand });
            continue;
        }

        if (!label.empty()) {
            if (SYMTAB.find(label) != SYMTAB.end())
                cerr << "Error: 중복된 심볼 " << label << endl;
            else
                SYMTAB[label] = LOCCTR;
        }

        // 리터럴
        // 피연산자가 '='로 시작하는 경우, 이는 리터럴 상수임
        // 예: LDA =C'EOF' 리터럴 풀에 저장
        if (!operand.empty() && operand[0] == '=') {
            bool exists = false;

            // 이미 동일한 리터럴이 등록되어 있는지 확인
            for (auto& lit : literalPool)
                if (lit.value == operand) exists = true;

            // 아직 등록되지 않은 리터럴이면 literalPool에 추가
            if (!exists)
                literalPool.push_back({ operand, -1 });  // 주소는 미정(-1)
        }

        intermediate.push_back({ LOCCTR, label, opcode, operand });

        if (OPTAB.find(opcode) != OPTAB.end()) LOCCTR += 3;
        else if (opcode == "WORD") LOCCTR += 3;
        else if (opcode == "RESW") LOCCTR += 3 * stoi(operand);
        else if (opcode == "RESB") LOCCTR += stoi(operand);
        else if (opcode == "BYTE") {
            if (operand[0] == 'C') LOCCTR += operand.length() - 3;
            else LOCCTR += (operand.length() - 3) / 2;
        }

        // LTORG 또는 END 지시어 처리
        // 리터럴 풀을 실제 메모리에 배치
        // 아직 주소가 없는 리터럴(-1)들을 LOCCTR 기준으로 할당
        if (opcode == "LTORG" || opcode == "END") {
            for (auto& lit : literalPool) {
                if (lit.address == -1) {
                    lit.address = LOCCTR;  // 현재 LOCCTR을 리터럴 시작 주소로 설정
                    string val = lit.value;

                    // 리터럴의 종류에 따라 메모리 크기 계산
                    if (val[1] == 'C')         // 문자형 리터럴 (=C'EOF')
                        LOCCTR += val.length() - 4; // 'C'와 따옴표 제외
                    else if (val[1] == 'X')    // 16진 리터럴 (=X'F1')
                        LOCCTR += (val.length() - 4) / 2; // 2자리가 1바이트

                    // 중간 파일(intermediate)에 BYTE 형태로 기록
                    // 예: *   BYTE   C'EOF'
                    intermediate.push_back({ lit.address, "*", "BYTE", val.substr(1) });
                }
            }
        }

        if (opcode == "END") break;
    }
    src.close();

    cout << "===== SYMTAB =====" << endl;
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
        if (l.opcode == "START" || l.opcode == "END" || l.opcode == "LTORG") continue;

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
                // PASS2 리터럴 참조 처리
                // 피연산자가 리터럴이면 literalPool에서 주소 검색
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

    cout << "\nPass2 완료: OBJFILE.txt 생성됨" << endl;
    return 0;
}
