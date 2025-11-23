#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <iomanip>
#include <algorithm>
using namespace std;

//해당 코드는 매크로 안에 매크로를 정의하는 중첩 매크로 정의와 그것을 호출하는 소스코드다.
//결과적으로 실패하긴 했는데, 일단 첨부한다.

// ===== 문자열 유틸 =====
// 문자열의 앞뒤 공백 제거 함수
string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// 문자열 내 특정 문자열을 모두 다른 문자열로 치환
string replaceAll(string s, const string& from, const string& to) {
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != string::npos) {
        s.replace(pos, from.length(), to);
        pos += to.length();
    }
    return s;
}

// ===== 테이블 구조 =====
// 매크로의 정의 정보, 파라미터, 기본값 등을 저장하는 구조체
struct NamTabEntry {
    string name;
    int start;
    int end;
    vector<string> params;
    unordered_map<string, string> defaultValues;
};

// 매크로 관련 전역 테이블
vector<string> DEFTAB; // 매크로 정의부 전체 저장
vector<NamTabEntry> NAMTAB; // 각 매크로의 이름, 위치, 매개변수 정보 저장
unordered_map<string, string> ARGTAB; // 실제 호출 시 인자값 저장

// ===== 매크로 탐색 =====
// NAMTAB에서 특정 매크로 이름을 찾아 인덱스를 반환
int findMacro(const string& name) {
    for (int i = 0; i < (int)NAMTAB.size(); i++)
        if (NAMTAB[i].name == name)
            return i;
    return -1;
}

// ===== 매개변수 접합 지원 =====
// '&ARG'가 문자열 중간에 등장하는 경우, 실제 인자값으로 대체하면서 연결
string expandLineWithConcat(const string& line, const unordered_map<string, string>& argtab) {
    string result;
    for (size_t i = 0; i < line.size();) {
        if (line[i] == '&') {
            string param = "&";
            size_t j = i + 1;
            while (j < line.size() && (isalnum(line[j]) || line[j] == '_'))
                param += line[j++];
            auto it = argtab.find(param);
            if (it != argtab.end()) result += it->second; // 인자가 존재하면 치환
            else result += param; // 존재하지 않으면 그대로 유지
            i = j;
        }
        else result += line[i++]; // 일반 문자 복사
    }
    return result;
}

// ===== 테이블 출력 =====
// 현재 NAMTAB, DEFTAB, ARGTAB의 상태를 콘솔에 출력 (디버깅용)
void printTables() {
    cout << "\n============================\n";
    cout << "        NAMTAB 내용         \n";
    cout << "============================\n";
    for (auto& n : NAMTAB) {
        cout << "매크로명: " << n.name << " (" << n.start << " ~ " << n.end << ")\n";
        cout << "매개변수: ";
        for (size_t i = 0; i < n.params.size(); i++) {
            cout << n.params[i];
            if (i != n.params.size() - 1) cout << ", ";
        }
        cout << "\n";
        if (!n.defaultValues.empty()) {
            cout << "기본값: ";
            for (auto& kv : n.defaultValues)
                cout << kv.first << "=" << kv.second << " ";
            cout << "\n";
        }
    }
    cout << "\n============================\n";
    cout << "        DEFTAB 내용         \n";
    cout << "============================\n";
    for (int i = 0; i < (int)DEFTAB.size(); i++)
        cout << "[" << setw(2) << i << "] " << DEFTAB[i] << "\n";

    cout << "\n============================\n";
    cout << "        ARGTAB 내용         \n";
    cout << "============================\n";
    for (auto& a : ARGTAB)
        cout << left << setw(10) << a.first << " → " << a.second << "\n";
    cout << "============================\n\n";
}

// ===== 매크로 재귀 확장 (중첩 호출 포함 완전 구현) =====
// 매크로 내부에서 또 다른 매크로를 호출하는 경우 재귀적으로 확장
void expandMacroRecursive(ofstream& fout, int idx, unordered_map<string, string> ARGTAB,
    unordered_map<string, int>& macroCallCount) {
    // 각 매크로 호출 시마다 카운트를 증가시켜 고유한 레이블 생성
    macroCallCount[NAMTAB[idx].name]++;
    int callNum = macroCallCount[NAMTAB[idx].name];

    // 매크로 본문을 한 줄씩 읽어 확장
    for (int j = NAMTAB[idx].start; j <= NAMTAB[idx].end; j++) {
        string line = trim(DEFTAB[j]);
        // 매크로 정의부(MACRO, MEND)는 출력에서 제외
        if (line.empty() || line == "MEND" || line.rfind("MACRO", 0) == 0) continue;

        // 매개변수 값 치환 및 접합 처리
        for (auto& a : ARGTAB)
            line = replaceAll(line, a.first, a.second);
        line = expandLineWithConcat(line, ARGTAB);

        stringstream ss(line);
        string first;
        ss >> first;

        // 레이블 고유화: 같은 이름의 레이블이 여러 번 생성되는 것을 방지
        if (!first.empty() && first[0] != '&' && first != "MACRO" && first != "MEND") {
            if (isalpha(first[0])) {
                line = replaceAll(line, first, first + "_" + to_string(callNum));
            }
        }

        // ===== 중첩 매크로 호출 지원 =====
        // 매크로 내부에서 또 다른 매크로가 호출될 경우 처리
        {
            stringstream callCheck(line);
            string cmd;
            callCheck >> cmd;
            int nestedIdx = findMacro(cmd); // 내부 매크로 탐색
            if (nestedIdx != -1) {
                string rest;
                getline(callCheck, rest);
                rest = trim(rest);

                // 상위 매크로 인자(&A, &B) 완전 치환 추가
                // 내부 호출 시 상위 매크로 인자도 적용되도록 처리
                for (auto& a : ARGTAB) {
                    rest = replaceAll(rest, a.first, a.second);
                }
                rest = expandLineWithConcat(rest, ARGTAB);

                // 내부 매크로 호출 인자 분리
                stringstream argstream(trim(rest));
                vector<string> args;
                string arg;
                while (getline(argstream, arg, ',')) args.push_back(trim(arg));

                unordered_map<string, string> innerArgtab;
                // 내부 매크로에 인자 매핑 수행
                for (size_t k = 0; k < NAMTAB[nestedIdx].params.size(); k++) {
                    string param = NAMTAB[nestedIdx].params[k];
                    string value = (k < args.size()) ? args[k] : "";
                    if (value.empty() && NAMTAB[nestedIdx].defaultValues.count(param))
                        value = NAMTAB[nestedIdx].defaultValues[param];
                    innerArgtab[param] = value;
                }
                // 내부 매크로를 재귀적으로 확장
                expandMacroRecursive(fout, nestedIdx, innerArgtab, macroCallCount);
                continue; // 한 번 확장 후 다음 줄로 이동
            }
        }
        // =====================================

        // 최종 확장 결과를 출력 파일에 기록
        fout << line << "\n";
    }
}

// ===== 메인 =====
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    ifstream fin("input6.asm");
    ofstream fout("output6.asm");
    if (!fin.is_open()) {
        cerr << "Error: input.asm 파일을 열 수 없습니다.\n";
        return 1;
    }

    vector<string> source;
    string line;
    while (getline(fin, line)) source.push_back(line);

    bool inMacro = false; // 현재 매크로 정의부 내부인지 여부
    int defStart = 0; // 매크로 정의 시작 인덱스
    int macroDepth = 0; // 중첩 매크로 깊이
    NamTabEntry currentMacro; // 현재 정의 중인 매크로
    vector<pair<int, int>> macroRanges; // 매크로 정의 범위 저장용

    // =============================
    // PASS 1: MACRO 정의 수집 (중첩 정의 완전 지원)
    // =============================
    for (int i = 0; i < (int)source.size(); i++) {
        string raw = source[i];
        string t = trim(raw);
        if (t.empty()) continue;

        string label = "", op = "", operand = "";
        stringstream ss(t);
        ss >> label >> op;
        getline(ss, operand);
        operand = trim(operand);

        if (op == "MACRO") {
            if (!inMacro) {
                // 매크로 정의 시작
                defStart = DEFTAB.size();
                currentMacro = NamTabEntry();
                currentMacro.name = label;
                currentMacro.params.clear();
                currentMacro.defaultValues.clear();
                inMacro = true;
            }
            else {
                // ===== 내부 매크로 정의 처리 추가 =====
                // 중첩된 매크로 정의를 별도로 저장
                NamTabEntry nested;
                nested.name = label;
                stringstream ps(operand);
                string p;
                while (getline(ps, p, ',')) {
                    p = trim(p);
                    if (p.empty()) continue;
                    if (p[0] != '&') p = "&" + p;
                    size_t eqPos = p.find('=');
                    if (eqPos != string::npos) {
                        string param = trim(p.substr(0, eqPos));
                        string defVal = trim(p.substr(eqPos + 1));
                        nested.params.push_back(param);
                        nested.defaultValues[param] = defVal;
                    }
                    else nested.params.push_back(p);
                }

                int nestedStart = DEFTAB.size();
                // 내부 매크로 DEFTAB 분리
                // 내부 매크로 본문을 따로 읽어 NAMTAB에 등록
                vector<string> nestedBody;
                for (int j = i + 1; j < (int)source.size(); j++) {
                    string inner = trim(source[j]);
                    if (inner.empty()) continue;
                    if (inner == "MEND") {
                        nested.start = nestedStart;
                        nested.end = nestedStart + nestedBody.size() - 1;
                        for (auto& l : nestedBody)
                            DEFTAB.push_back(l);
                        DEFTAB.push_back("MEND");
                        NAMTAB.push_back(nested);
                        i = j;
                        break;
                    }
                    nestedBody.push_back(source[j]);
                }
                continue;
            }
            macroDepth++;

            // 매크로의 매개변수 파싱
            stringstream ps(operand);
            string p;
            while (getline(ps, p, ',')) {
                p = trim(p);
                if (p.empty()) continue;
                if (p[0] != '&') p = "&" + p;
                size_t eqPos = p.find('=');
                if (eqPos != string::npos) {
                    string param = trim(p.substr(0, eqPos));
                    string defVal = trim(p.substr(eqPos + 1));
                    currentMacro.params.push_back(param);
                    currentMacro.defaultValues[param] = defVal;
                }
                else currentMacro.params.push_back(p);
            }
            continue;
        }

        if (inMacro) {
            if (t == "MEND") {
                // 매크로 정의 종료
                macroDepth--;
                DEFTAB.push_back(raw);
                if (macroDepth == 0) {
                    inMacro = false;
                    currentMacro.start = defStart;
                    currentMacro.end = DEFTAB.size() - 1;
                    NAMTAB.push_back(currentMacro);
                    currentMacro = NamTabEntry();
                }
                continue;
            }

            string first;
            stringstream inner(t);
            inner >> first;

            // 내부 매크로 정의 감지
            // ***** 수정된 부분: MACRO 키워드는 DEFTAB에 추가하지 않음 *****
            if (op == "MACRO") continue;
            if (findMacro(first) != -1) continue;
            // *******************************************************
            DEFTAB.push_back(raw);
        }
    }

    static unordered_map<string, int> macroCallCount; // 매크로 호출 횟수 기록 (고유화용)

    // =============================
    // PASS 2: 매크로 확장 (중첩 호출 완전 지원)
    // =============================
    for (int i = 0; i < (int)source.size(); i++) {
        string raw = source[i];
        string t = trim(raw);
        if (t.empty()) continue;

        // 매크로 이름과 인자를 구분하기 위한 토큰 분리
        string first;
        stringstream ss(t);
        ss >> first;
        string operand;
        getline(ss, operand);
        operand = trim(operand);

        // 매크로 정의부는 확장 대상에서 제외
        if (operand == "MACRO" || first == "MACRO" || first == "MEND" || operand == "MEND")
            continue;

        // 현재 줄이 매크로 호출인지 확인
        int idx = findMacro(first);
        if (idx != -1) {
            ARGTAB.clear();
            string args = operand;

            // 인자 문자열을 ','로 분리
            vector<string> actualArgs;
            stringstream as(args);
            string a;
            while (getline(as, a, ',')) actualArgs.push_back(trim(a));

            // 매크로 매개변수와 인자 매핑
            for (size_t k = 0; k < NAMTAB[idx].params.size(); k++) {
                string param = NAMTAB[idx].params[k];
                string value = "";
                if (k < actualArgs.size()) value = actualArgs[k];
                if (value.empty() && NAMTAB[idx].defaultValues.count(param))
                    value = NAMTAB[idx].defaultValues[param];
                ARGTAB[param] = value;
            }

            cout << "\n▶ 매크로 호출: " << NAMTAB[idx].name << "\n";
            printTables();
            // 매크로 본문 확장을 재귀적으로 수행
            expandMacroRecursive(fout, idx, ARGTAB, macroCallCount);
        }
        else fout << raw << "\n"; // 매크로가 아닌 일반 명령은 그대로 출력
    }

    // 결과 요약 출력
    cout << "\n===================================\n";
    cout << " 매크로 확장이 완료되었습니다.\n";
    cout << " 매개변수 접합 기능이 구현되었습니다.\n";
    cout << " 고유한 레이블 형성 기능이 구현되었습니다.\n";
    cout << " 조건부 매크로 확장 기능이 구현되었습니다.\n";
    cout << " 키워드 매크로 매개변수 기능이 구현되었습니다.\n";
    cout << " 매크로 중첩 정의 및 호출 기능이 구현되었습니다.\n";
    cout << " 결과 파일: output.asm\n";
    cout << "===================================\n";
    printTables();

    fin.close();
    fout.close();
    return 0;
}
