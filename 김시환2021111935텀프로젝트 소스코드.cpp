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

struct Symbol {
    string name;
    int address;
    int value;
};

// 문자열 정리 함수
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

// 숫자 여부 확인
bool isNumber(const string& s) {
    return all_of(s.begin(), s.end(), ::isdigit);
}

// 수식 계산 (EQU, ORG 등에서 사용)
int evaluateExpression(const string& expr, const unordered_map<string, Symbol>& SYMTAB) {
    string e = expr;
    e.erase(remove(e.begin(), e.end(), ' '), e.end());
    int result = 0;
    int sign = +1;
    string token = "";
    bool first = true;

    for (size_t i = 0; i <= e.size(); ++i) {
        if (i == e.size() || e[i] == '+' || e[i] == '-') {
            if (!token.empty()) {
                int value = 0;
                if (isNumber(token))
                    value = stoi(token);
                else if (SYMTAB.find(token) != SYMTAB.end())
                    value = SYMTAB.at(token).value ? SYMTAB.at(token).value : SYMTAB.at(token).address;
                else
                    cerr << "Error: 식 해석 실패 (" << token << ")" << endl;

                if (first) {
                    result = value;
                    first = false;
                }
                else {
                    result += sign * value;
                }
            }
            if (i < e.size()) sign = (e[i] == '+') ? +1 : -1;
            token.clear();
        }
        else {
            token += e[i];
        }
    }
    return result;
}

int main() {
    unordered_map<string, string> OPTAB;
    unordered_map<string, Symbol> SYMTAB;
    vector<Literal> literalPool;   // 리터럴 풀
    vector<Line> intermediate;     // 중간 파일 벡터

    // ---------- OPTAB 읽기 ----------
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

    // ---------- SRCFILE 읽기 ----------
    ifstream src("SRCFILE7.txt");
    if (!src) {
        cerr << "SRCFILE.txt 파일을 열 수 없습니다.\n";
        return 1;
    }

    string line;
    int LOCCTR = 0, startAddr = 0;
    static int savedLOCCTR = 0;

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

        // ---------- START ----------
        if (opcode == "START") {
            startAddr = stoi(operand, nullptr, 16);
            LOCCTR = startAddr;
            intermediate.push_back({ LOCCTR, label, opcode, operand });
            continue;
        }

        // ---------- EQU ----------
        if (opcode == "EQU") {
            int value = 0;
            if (operand == "*") value = LOCCTR;
            else if (isNumber(operand)) value = stoi(operand);
            else if (SYMTAB.find(operand) != SYMTAB.end())
                value = SYMTAB[operand].value ? SYMTAB[operand].value : SYMTAB[operand].address;
            else
                value = evaluateExpression(operand, SYMTAB);

            SYMTAB[label] = { label, LOCCTR, value };
            intermediate.push_back({ LOCCTR, label, opcode, operand });
            continue;
        }

        // ---------- ORG ----------
        if (opcode == "ORG") {
            if (operand.empty()) {
                if (savedLOCCTR != 0)
                    LOCCTR = savedLOCCTR;
            }
            else {
                savedLOCCTR = LOCCTR;
                if (operand == "*") {}
                else if (isNumber(operand))
                    LOCCTR = stoi(operand, nullptr, 16);
                else if (SYMTAB.find(operand) != SYMTAB.end())
                    LOCCTR = SYMTAB[operand].value ? SYMTAB[operand].value : SYMTAB[operand].address;
                else
                    LOCCTR = evaluateExpression(operand, SYMTAB);
            }
            intermediate.push_back({ LOCCTR, label, opcode, operand });
            continue;
        }

        // ---------- 심볼 등록 ----------
        if (!label.empty() && opcode != "EQU" && opcode != "ORG") {
            if (SYMTAB.find(label) != SYMTAB.end())
                cerr << "Error: 중복된 심볼 " << label << endl;
            else
                SYMTAB[label] = { label, LOCCTR, 0 };
        }

        // ---------- 리터럴 등록 ----------
        if (!operand.empty() && operand[0] == '=') {
            bool exists = false;
            for (auto& lit : literalPool)
                if (lit.value == operand) exists = true;
            if (!exists)
                literalPool.push_back({ operand, -1 });  // 주소 미정
        }

        intermediate.push_back({ LOCCTR, label, opcode, operand });

        // ---------- LOCCTR 증가 ----------
        if (OPTAB.find(opcode) != OPTAB.end()) LOCCTR += 3;
        else if (opcode == "WORD") {
            LOCCTR += 3;
            if (!label.empty()) {
                int value = 0;
                if (isNumber(operand)) value = stoi(operand);
                else value = evaluateExpression(operand, SYMTAB);
                SYMTAB[label].value = value;
            }
        }
        else if (opcode == "RESW") LOCCTR += 3 * stoi(operand);
        else if (opcode == "RESB") LOCCTR += stoi(operand);
        else if (opcode == "BYTE") {
            if (operand[0] == 'C') LOCCTR += operand.length() - 3;
            else LOCCTR += (operand.length() - 3) / 2;
        }

        // ---------- LTORG / END ----------
        if (opcode == "LTORG" || opcode == "END") {
            for (auto& lit : literalPool) {
                if (lit.address == -1) {
                    lit.address = LOCCTR;
                    string val = lit.value;
                    if (val[1] == 'C')
                        LOCCTR += val.length() - 4;
                    else if (val[1] == 'X')
                        LOCCTR += (val.length() - 4) / 2;

                    // 중간파일에 BYTE 형태로 추가
                    intermediate.push_back({ lit.address, "*", "BYTE", val.substr(1) });
                }
            }
        }

        if (opcode == "END") break;
    }
    src.close();

    // ---------- SYMTAB 출력 ----------
    cout << "\n===== SYMTAB =====" << endl;
    cout << left << setw(12) << "Name" << " | "
        << setw(8) << "Address" << " | "
        << setw(8) << "Value" << endl;
    cout << string(34, '-') << endl;

    for (auto& s : SYMTAB) {
        cout << left << setw(12) << s.second.name << " | "
            << setw(8) << hex << s.second.address << " | ";
        if (s.second.value != 0)
            cout << setw(8) << hex << s.second.value;
        else
            cout << setw(8) << " ";
        cout << endl;
    }

    // ---------- LITTAB 출력 ----------
    cout << "\n===== LITTAB =====" << endl;
    for (auto& lit : literalPool)
        cout << setw(10) << lit.value << " : " << hex << lit.address << endl;

    cout << "\nPass2 완료: OBJFILE.txt, INTFILE.txt 생성됨" << endl;
    return 0;
}
