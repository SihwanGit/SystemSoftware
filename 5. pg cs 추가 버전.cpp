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
    string block;
    string csect;
};

struct Literal {
    string value;
    int address;
    string block;
    string csect;
};

struct Symbol {
    string name;
    int address;
    int value;
    string block;
    string csect;
};

struct Block {
    string name;
    int startAddr;
    int LOCCTR;
    int length;
};

struct Csect {
    string name;
    int startAddr;
    int LOCCTR;
    int length;
    unordered_map<string, Block> blocks;
    vector<Literal> literals;
    unordered_map<string, Symbol> symbols;
    vector<string> extdef;
    vector<string> extref;
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

int evaluateExpression(const string& expr, const unordered_map<string, Symbol>& SYMTAB) {
    string e = expr;
    e.erase(remove(e.begin(), e.end(), ' '), e.end());
    int result = 0, sign = +1;
    string token;
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
                if (first) { result = value; first = false; }
                else result += sign * value;
            }
            if (i < e.size()) sign = (e[i] == '+') ? +1 : -1;
            token.clear();
        }
        else token += e[i];
    }
    return result;
}

int main() {
    unordered_map<string, string> OPTAB;
    unordered_map<string, Symbol> SYMTAB;
    unordered_map<string, Csect> CSECTTAB;
    vector<Line> intermediate;

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

    ifstream src("SRCFILE.txt");
    if (!src) {
        cerr << "SRCFILE.txt 파일을 열 수 없습니다.\n";
        return 1;
    }

    string line;
    int LOCCTR = 0, startAddr = 0;
    static int savedLOCCTR = 0;

    string currentBlock = "DEFAULT";   // 현재 프로그램 블록 이름
    string currentCsect = "DEFAULT";   // 현재 제어 섹션 이름

    // 제어 섹션(CSECT) 및 블록 초기화
    CSECTTAB[currentCsect] = { currentCsect, 0, 0, 0 };
    CSECTTAB[currentCsect].blocks[currentBlock] = { currentBlock, 0, 0, 0 };

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
                tokens[0] == "ORG" || tokens[0] == "USE" ||
                tokens[0] == "CSECT" || tokens[0] == "EXTDEF" || tokens[0] == "EXTREF") {
                opcode = tokens[0];
                operand = tokens[1];
            }
            else {
                label = tokens[0];
                opcode = tokens[1];
                operand = tokens[2];
            }
        }

        // ---------- 제어 섹션(CSECT) ----------
        if (opcode == "CSECT") {
            // 이전 CSECT의 길이 계산
            int maxLOC = 0;
            for (auto& b : CSECTTAB[currentCsect].blocks)
                maxLOC = max(maxLOC, b.second.LOCCTR);
            CSECTTAB[currentCsect].LOCCTR = maxLOC;
            CSECTTAB[currentCsect].length = maxLOC;

            // 새로운 제어 섹션 시작
            currentCsect = operand.empty() ? "DEFAULT" : operand;
            CSECTTAB[currentCsect] = { currentCsect, 0, 0, 0 };

            // 기본 블록 초기화
            currentBlock = "DEFAULT";
            CSECTTAB[currentCsect].blocks[currentBlock] = { currentBlock, 0, 0, 0 };
            LOCCTR = 0;

            intermediate.push_back({ LOCCTR, label, opcode, operand, currentBlock, currentCsect });
            continue;
        }

        // ---------- 외부 정의/참조(EXTDEF, EXTREF) ----------
        if (opcode == "EXTDEF") {
            istringstream ss(operand);
            string sym;
            while (getline(ss, sym, ',')) {
                sym = trim(sym);
                CSECTTAB[currentCsect].extdef.push_back(sym);
            }
            continue;
        }
        if (opcode == "EXTREF") {
            istringstream ss(operand);
            string sym;
            while (getline(ss, sym, ',')) {
                sym = trim(sym);
                CSECTTAB[currentCsect].extref.push_back(sym);
            }
            continue;
        }

        // ---------- 프로그램 블록(USE) ----------
        if (opcode == "USE") {
            string newBlock = operand.empty() ? "DEFAULT" : operand;
            // 현재 블록의 LOCCTR 저장
            CSECTTAB[currentCsect].blocks[currentBlock].LOCCTR = LOCCTR;

            // 새로운 블록이 존재하지 않으면 생성
            if (CSECTTAB[currentCsect].blocks.find(newBlock) == CSECTTAB[currentCsect].blocks.end())
                CSECTTAB[currentCsect].blocks[newBlock] = { newBlock, 0, 0, 0 };

            // 현재 블록 변경 및 LOCCTR 갱신
            currentBlock = newBlock;
            LOCCTR = CSECTTAB[currentCsect].blocks[currentBlock].LOCCTR;

            intermediate.push_back({ LOCCTR, label, opcode, operand, currentBlock, currentCsect });
            continue;
        }

        // START
        if (opcode == "START") {
            startAddr = stoi(operand, nullptr, 16);
            LOCCTR = startAddr;
            CSECTTAB[currentCsect].blocks[currentBlock].LOCCTR = startAddr;
            intermediate.push_back({ LOCCTR, label, opcode, operand, currentBlock, currentCsect });
            continue;
        }

        // EQU
        if (opcode == "EQU") {
            int value = 0;
            if (operand == "*") value = LOCCTR;
            else if (isNumber(operand)) value = stoi(operand);
            else if (SYMTAB.find(operand) != SYMTAB.end())
                value = SYMTAB[operand].value ? SYMTAB[operand].value : SYMTAB[operand].address;
            else
                value = evaluateExpression(operand, SYMTAB);

            SYMTAB[label] = { label, LOCCTR, value, currentBlock, currentCsect };
            intermediate.push_back({ LOCCTR, label, opcode, operand, currentBlock, currentCsect });
            continue;
        }

        // ORG
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
            intermediate.push_back({ LOCCTR, label, opcode, operand, currentBlock, currentCsect });
            continue;
        }

        // 일반 심볼
        if (!label.empty() && opcode != "EQU" && opcode != "ORG") {
            if (SYMTAB.find(label) != SYMTAB.end())
                cerr << "Error: 중복된 심볼 " << label << endl;
            else
                SYMTAB[label] = { label, LOCCTR, 0, currentBlock, currentCsect };
        }

        intermediate.push_back({ LOCCTR, label, opcode, operand, currentBlock, currentCsect });

        // LOCCTR 증가
        if (OPTAB.find(opcode) != OPTAB.end()) LOCCTR += 3;
        else if (opcode == "WORD") LOCCTR += 3;
        else if (opcode == "RESW") LOCCTR += 3 * stoi(operand);
        else if (opcode == "RESB") LOCCTR += stoi(operand);
        else if (opcode == "BYTE") {
            if (operand[0] == 'C') LOCCTR += operand.length() - 3;
            else LOCCTR += (operand.length() - 3) / 2;
        }

        if (opcode == "END") break;
    }
    src.close();

    // ---------- INTFILE ----------
    ofstream intFile("INTFILE.txt");
    intFile << left << setw(6) << "LOC" << setw(12) << "LABEL"
        << setw(12) << "OPCODE" << setw(15) << "OPERAND"
        << setw(10) << "BLOCK" << setw(10) << "CSECT" << endl;
    intFile << string(65, '-') << endl;
    for (auto& l : intermediate)
        intFile << setw(6) << hex << l.loc
        << setw(12) << l.label
        << setw(12) << l.opcode
        << setw(15) << l.operand
        << setw(10) << l.block
        << setw(10) << l.csect << endl;
    intFile.close();

    // ---------- OBJFILE ----------
    ofstream objFile("OBJFILE.txt");
    for (auto& c : CSECTTAB) {
        string name = (c.first.empty() ? "DEFAULT" : c.first);
        objFile << "H " << left << setw(6) << name.substr(0, 6)
            << setw(6) << hex << c.second.startAddr
            << setw(6) << hex << c.second.length << endl;

        if (!c.second.extdef.empty()) {
            objFile << "D ";
            for (auto& d : c.second.extdef) {
                if (SYMTAB.find(d) != SYMTAB.end())
                    objFile << left << setw(6) << d.substr(0, 6)
                    << setw(6) << hex << SYMTAB[d].address;
            }
            objFile << endl;
        }

        if (!c.second.extref.empty()) {
            objFile << "R ";
            for (auto& r : c.second.extref)
                objFile << left << setw(6) << r.substr(0, 6);
            objFile << endl;
        }

        for (auto& l : intermediate) {
            if (l.csect == c.first && (l.opcode == "WORD" || l.opcode == "BYTE")) {
                objFile << "T " << setw(6) << hex << l.loc << " ";
                if (isNumber(l.operand))
                    objFile << setw(6) << hex << stoi(l.operand);
                else objFile << "000000";
                objFile << endl;
            }
        }

        objFile << "E " << setw(6) << hex << c.second.startAddr << endl;
    }
    objFile.close();

    // ---------- 콘솔 출력 ----------
    cout << "\n===== SYMTAB =====" << endl;
    cout << left << setw(12) << "Name" << " | "
        << setw(8) << "Address" << " | "
        << setw(8) << "Value" << " | "
        << setw(10) << "Block" << " | "
        << setw(10) << "Csect" << endl;
    cout << string(56, '-') << endl;
    for (auto& s : SYMTAB) {
        cout << left << setw(12) << s.second.name << " | "
            << setw(8) << hex << s.second.address << " | ";
        if (s.second.value != 0)
            cout << setw(8) << hex << s.second.value;
        else
            cout << setw(8) << " ";
        cout << " | " << setw(10) << s.second.block
            << " | " << setw(10) << s.second.csect << endl;
    }

    cout << "\n===== BLOCKTAB =====" << endl;
    for (auto& c : CSECTTAB)
        for (auto& b : c.second.blocks)
            cout << left << setw(10) << b.second.name << " : start="
            << setw(6) << hex << b.second.startAddr
            << " length=" << setw(6) << hex << b.second.LOCCTR
            << " (" << c.first << ")" << endl;

    cout << "\n===== CSECTTAB =====" << endl;
    for (auto& c : CSECTTAB)
        cout << left << setw(10) << c.second.name << " : length="
        << setw(6) << hex << c.second.LOCCTR << endl;

    cout << "\n===== LITTAB =====" << endl;
    for (auto& c : CSECTTAB)
        for (auto& lit : c.second.literals)
            cout << setw(10) << lit.value << " : "
            << hex << lit.address << " (" << lit.block << ", " << lit.csect << ")" << endl;

    cout << "\nPass2 완료: OBJFILE.txt, INTFILE.txt 생성됨" << endl;
    return 0;
}
