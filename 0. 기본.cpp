#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
using namespace std;

// 한 줄(라인)의 구성요소 구조체
struct Line {
    int loc;          // 주소 (Location Counter)
    string label;     // 라벨
    string opcode;    // 명령어
    string operand;   // 피연산자
};

int main() {
    unordered_map<string, string> OPTAB; // 명령어 테이블
    unordered_map<string, int> SYMTAB;   // 심볼 테이블
    vector<Line> intermediate;           // 중간파일용 벡터

    // OPTAB 파일 읽기
    ifstream optFile("OPTAB.txt");
    if (!optFile) {
        cerr << "OPTAB.txt 파일을 열 수 없습니다.\n";
        return 1;
    }

    string key, code;
    while (optFile >> key >> code) OPTAB[key] = code; // OPTAB에 명령어와 기계어 코드 저장
    optFile.close();

    // 소스 프로그램 파일 읽기
    ifstream src("SRCFILE.txt");
    if (!src) {
        cerr << "SRCFILE.txt 파일을 열 수 없습니다.\n";
        return 1;
    }

    string line;
    int LOCCTR = 0, startAddr = 0;
    bool started = false; // START 명령어 확인 여부

    // PASS1: 주소 계산 및 SYMTAB 생성
    while (getline(src, line)) {
        if (line.empty() || line[0] == '.') continue; // 주석 라인은 무시
        line.erase(remove(line.begin(), line.end(), '\r'), line.end());

        // 공백 기준으로 토큰 분리
        istringstream iss(line);
        vector<string> tokens;
        string temp;
        while (iss >> temp) tokens.push_back(temp);
        if (tokens.empty()) continue;

        string label = "", opcode = "", operand = "";

        // 토큰 개수에 따라 label, opcode, operand 구분
        if (tokens.size() == 1)
            opcode = tokens[0];
        else if (tokens.size() == 2) {
            opcode = tokens[0];
            operand = tokens[1];
        }
        else if (tokens.size() >= 3) {
            // 첫 토큰이 명령어나 지시어인 경우
            if (OPTAB.find(tokens[0]) != OPTAB.end() || tokens[0] == "START" ||
                tokens[0] == "END" || tokens[0] == "WORD" ||
                tokens[0] == "RESW" || tokens[0] == "RESB" ||
                tokens[0] == "BYTE") {
                opcode = tokens[0];
                operand = tokens[1];
            }
            // 첫 토큰이 라벨인 경우
            else {
                label = tokens[0];
                opcode = tokens[1];
                operand = tokens[2];
            }
        }

        // START 지시어 처리
        if (opcode == "START") {
            startAddr = stoi(operand, nullptr, 16); // 시작 주소 설정
            LOCCTR = startAddr;
            started = true;
            intermediate.push_back({ LOCCTR, label, opcode, operand });
            continue;
        }

        // 라벨이 존재하면 SYMTAB에 추가
        if (!label.empty()) {
            if (SYMTAB.find(label) != SYMTAB.end()) {
                cerr << "Error: 중복된 심볼 " << label << endl;
            }
            else {
                SYMTAB[label] = LOCCTR;
            }
        }

        // 중간 파일용 벡터에 현재 라인 정보 저장
        intermediate.push_back({ LOCCTR, label, opcode, operand });

        // 명령어나 지시어에 따라 LOCCTR 증가
        if (OPTAB.find(opcode) != OPTAB.end()) LOCCTR += 3;
        else if (opcode == "WORD") LOCCTR += 3;
        else if (opcode == "RESW") LOCCTR += 3 * stoi(operand);
        else if (opcode == "RESB") LOCCTR += stoi(operand);
        else if (opcode == "BYTE") {
            if (operand[0] == 'C')
                LOCCTR += operand.length() - 3; // 문자 상수 길이
            else
                LOCCTR += (operand.length() - 3) / 2; // 16진 상수 길이
        }
        else if (opcode == "END")
            break;
    }
    src.close();

    // 중간파일(INTFILE) 출력
    ofstream intFile("INTFILE.txt");
    for (auto& l : intermediate) {
        intFile << hex << setw(4) << setfill('0') << l.loc << " "
            << left << setw(10) << l.label
            << setw(10) << l.opcode
            << setw(10) << l.operand << endl;
    }
    intFile.close();

    cout << "Pass1 완료: SYMTAB" << endl;
    for (auto& s : SYMTAB)
        cout << setw(10) << s.first << " : " << hex << s.second << endl;

    // PASS2: 오브젝트 코드 생성
    ofstream objOut("OBJFILE.txt");

    string programName;
    int programLength = LOCCTR - startAddr;

    // Header Record (프로그램 이름, 시작주소, 길이)
    for (auto& l : intermediate) {
        if (l.opcode == "START") {
            programName = l.label.empty() ? "------" : l.label;
            objOut << "H "
                << left << setw(6) << setfill(' ') << programName.substr(0, 6)
                << right << setfill('0') << uppercase
                << setw(6) << hex << startAddr << ' '
                << setw(6) << hex << programLength
                << endl;
            break;
        }
    }

    // Text Record 생성
    const int MAX_TEXT_LEN = 30; // 한 Text Record 최대 길이(30바이트)
    string textRecord = "";
    int textStart = 0;
    int textLength = 0;

    for (auto& l : intermediate) {
        if (l.opcode == "START" || l.opcode == "END") continue;

        string objCode = "";
        int objLen = 0;

        // 일반 명령어 (OPTAB에 존재)
        if (OPTAB.find(l.opcode) != OPTAB.end()) {
            string opcodeVal = OPTAB[l.opcode];
            int operandAddr = 0;
            if (!l.operand.empty() && SYMTAB.find(l.operand) != SYMTAB.end())
                operandAddr = SYMTAB[l.operand];

            stringstream ss;
            ss << opcodeVal << setw(4) << setfill('0') << hex << operandAddr;
            objCode = ss.str();
            objLen = 3;
        }
        // WORD 상수 처리
        else if (l.opcode == "WORD") {
            int val = stoi(l.operand);
            stringstream ss;
            ss << setw(6) << setfill('0') << hex << val;
            objCode = ss.str();
            objLen = 3;
        }
        // BYTE 상수 처리
        else if (l.opcode == "BYTE") {
            if (l.operand[0] == 'C') { // 문자 상수
                for (size_t i = 2; i < l.operand.size() - 1; ++i) {
                    stringstream ss;
                    ss << hex << uppercase << (int)l.operand[i];
                    objCode += ss.str();
                }
                objLen = (int)(l.operand.size() - 3);
            }
            else if (l.operand[0] == 'X') { // 16진 상수
                objCode = l.operand.substr(2, l.operand.size() - 3);
                objLen = (int)((objCode.size() + 1) / 2);
            }
        }

        // Text Record 작성
        if (!objCode.empty()) {
            if (textRecord.empty()) {
                textStart = l.loc;
                textLength = 0;
                textRecord = "";
            }

            // 30바이트 초과 시 새 레코드 시작
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
            // RESW, RESB 시 현재 레코드 종료
            if (!textRecord.empty()) {
                objOut << "T " << setw(6) << setfill('0') << hex << textStart << ' '
                    << setw(2) << setfill('0') << hex << textLength << ' '
                    << textRecord << endl;
                textRecord.clear();
                textLength = 0;
            }
        }
    }

    // 마지막 Text Record 출력
    if (!textRecord.empty()) {
        objOut << "T " << setw(6) << setfill('0') << hex << textStart << ' '
            << setw(2) << setfill('0') << hex << textLength << ' '
            << textRecord << endl;
    }

    // End Record (프로그램 시작 주소)
    objOut << "E " << setw(6) << setfill('0') << hex << startAddr << endl;

    objOut.close();
    cout << "Pass2 완료: OBJFILE.txt 생성됨" << endl;
    return 0;
}
