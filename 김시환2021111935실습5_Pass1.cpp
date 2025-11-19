#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    unordered_map<string, string> OPTAB; // 명령어 테이블
    unordered_map<string, int> SYMTAB;   // 심볼 테이블

    // OPTAB 파일 열기
    ifstream optFile("Optab.txt");
    if (!optFile) {
        cerr << "Optab.txt 파일을 열 수 없습니다." << endl;
        return 1;
    }

    string key, code;
    while (optFile >> key >> code) {
        OPTAB[key] = code;
    }
    optFile.close();

    // 어셈블리 소스 파일 열기
    ifstream src("SRCFILE.txt");
    if (!src) {
        cerr << "SRCFILE.txt 파일을 열 수 없습니다." << endl;
        return 1;
    }

    string line;
    int LOCCTR = 0;     // 위치 카운터
    int startAddr = 0;  // START 주소
    bool started = false;

    cout << left << setw(10) << "LOC" << setw(10) << "LABEL"
        << setw(10) << "OPCODE" << setw(10) << "OPERAND" << endl;
    cout << "-------------------------------------------" << endl;

    while (getline(src, line)) {
        if (line.empty() || line[0] == '.') continue; // 주석 무시

        // 개행 및 CR 제거
        line.erase(remove(line.begin(), line.end(), '\r'), line.end());

        istringstream iss(line);
        vector<string> tokens;
        string temp;
        while (iss >> temp) tokens.push_back(temp);
        if (tokens.empty()) continue;

        string label = "";
        string opcode = "";
        string operand = "";

        // 토큰 수에 따라 구분
        if (tokens.size() == 1) {
            opcode = tokens[0];
        }
        else if (tokens.size() == 2) {
            opcode = tokens[0];
            operand = tokens[1];
        }
        else if (tokens.size() >= 3) {
            // 첫 단어가 명령어면 라벨 없음
            if (OPTAB.find(tokens[0]) != OPTAB.end() || tokens[0] == "START" || tokens[0] == "END" ||
                tokens[0] == "WORD" || tokens[0] == "RESW" || tokens[0] == "RESB" || tokens[0] == "BYTE") {
                opcode = tokens[0];
                operand = tokens[1];
            }
            else {
                // 첫 단어가 라벨
                label = tokens[0];
                opcode = tokens[1];
                // operand 병합 (쉼표 포함 처리)
                operand = tokens[2];
                for (size_t i = 3; i < tokens.size(); ++i) {
                    operand += " " + tokens[i];
                }
            }
        }

        // START 처리
        if (opcode == "START") {
            startAddr = stoi(operand, nullptr, 16);
            LOCCTR = startAddr;
            started = true;
            cout << hex << setw(10) << LOCCTR << setw(10) << label
                << setw(10) << opcode << setw(10) << operand << endl;
            continue;
        }

        // SYMTAB 등록
        if (!label.empty()) {
            if (SYMTAB.find(label) != SYMTAB.end()) {
                cerr << "Error: 중복된 심볼 " << label << endl;
            }
            else {
                SYMTAB[label] = LOCCTR;
            }
        }

        // LOCCTR 계산
        if (OPTAB.find(opcode) != OPTAB.end()) {
            LOCCTR += 3;
        }
        else if (opcode == "WORD") {
            LOCCTR += 3;
        }
        else if (opcode == "RESW") {
            LOCCTR += 3 * stoi(operand);
        }
        else if (opcode == "RESB") {
            LOCCTR += stoi(operand);
        }
        else if (opcode == "BYTE") {
            if (operand[0] == 'C')
                LOCCTR += operand.length() - 3;
            else
                LOCCTR += (operand.length() - 3) / 2;
        }
        else if (opcode == "END") {
            cout << hex << setw(10) << LOCCTR << setw(10) << label
                << setw(10) << opcode << setw(10) << operand << endl;
            break;
        }
        else {
            cerr << "Error: 알 수 없는 명령어 " << opcode << endl;
        }

        cout << hex << setw(10) << LOCCTR << setw(10) << label
            << setw(10) << opcode << setw(10) << operand << endl;
    }

    src.close();

    cout << "\n===== SYMTAB =====" << endl;
    for (auto& sym : SYMTAB) {
        cout << setw(10) << sym.first << " : " << hex << sym.second << endl;
    }

    cout << "\nPass 1 완료. 시작주소: " << hex << startAddr
        << "  종료주소: " << LOCCTR << endl;

    return 0;
}
