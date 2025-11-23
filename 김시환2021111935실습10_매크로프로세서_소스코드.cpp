#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <iomanip>
#include <algorithm>
using namespace std;

//구현 기능
// - 접합
// - 고유레이블
// - 조건부 매크로
// - 키워드 매크로 매개변수



// ===== 문자열 유틸 =====
// 문자열의 앞뒤 공백을 제거하는 함수
string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// 문자열 내 특정 패턴을 모두 다른 문자열로 치환
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
// 매크로 이름, 정의 위치, 매개변수, 기본값을 저장하는 구조체
struct NamTabEntry {
    string name;                               // 매크로 이름
    int start;                                 // DEFTAB 내 매크로 시작 인덱스
    int end;                                   // DEFTAB 내 매크로 끝 인덱스
    vector<string> params;                     // 매크로의 매개변수 목록
    unordered_map<string, string> defaultValues; // [키워드 매크로 매개변수 처리 추가] 기본값 저장
};

// 전역 테이블 선언
vector<string> DEFTAB; // 매크로 정의부 전체를 저장
vector<NamTabEntry> NAMTAB; // 매크로 이름, 범위, 매개변수 정보를 저장
unordered_map<string, string> ARGTAB; // 매크로 호출 시 전달된 인자를 저장

// ===== 매크로 탐색 =====
// NAMTAB에서 특정 매크로 이름을 찾아 인덱스를 반환
int findMacro(const string& name) {
    for (int i = 0; i < (int)NAMTAB.size(); i++)
        if (NAMTAB[i].name == name)
            return i;
    return -1;
}

// ===== 매개변수 접합 지원 =====
// 매크로 본문 내에서 '&ARG'가 문자열 중간에 포함될 경우, 실제 인자와 이어붙여 새로운 심볼을 생성
string expandLineWithConcat(const string& line, const unordered_map<string, string>& argtab) {
    string result;
    for (size_t i = 0; i < line.size(); ) {
        if (line[i] == '&') {
            // '&' 이후의 매개변수 이름 추출
            string param = "&";
            size_t j = i + 1;
            while (j < line.size() && (isalnum(line[j]) || line[j] == '_'))
                param += line[j++];

            // ARGTAB에 존재하면 인자값으로 치환 (접합 처리)
            auto it = argtab.find(param);
            if (it != argtab.end()) {
                result += it->second; // 접합 발생
            }
            else {
                result += param; // 정의되지 않으면 그대로 유지
            }
            i = j;
        }
        else {
            result += line[i++]; // 일반 문자 복사
        }
    }
    return result;
}

// ===== 테이블 출력 =====
// 현재 NAMTAB, DEFTAB, ARGTAB 내용을 콘솔에 출력 (디버깅용)
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
        // [키워드 매크로 매개변수 처리 추가]
        // 매크로 정의 시 기본값이 설정된 경우 출력
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

// ===== 메인 =====
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    ifstream fin("input5.asm");
    ofstream fout("output5.asm");
    if (!fin.is_open()) {
        cerr << "Error: input.asm 파일을 열 수 없습니다.\n";
        return 1;
    }

    vector<string> source;
    string line;
    while (getline(fin, line)) source.push_back(line);

    bool inMacro = false; // 현재 MACRO 정의부 안에 있는지 여부
    int defStart = 0; // DEFTAB 내 매크로 시작 인덱스
    NamTabEntry currentMacro; // 현재 정의 중인 매크로
    vector<pair<int, int>> macroRanges; // 매크로 정의 범위 저장용

    // =============================
    // PASS 1: MACRO 정의 수집
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

        // MACRO 시작 시점
        if (op == "MACRO") {
            inMacro = true;
            defStart = DEFTAB.size();
            currentMacro.name = label;
            currentMacro.params.clear();
            currentMacro.defaultValues.clear(); // [키워드 매크로 매개변수 처리 추가]

            // 매개변수 목록 파싱 (&ARG1=VALUE 형태 지원)
            stringstream ps(operand);
            string p;
            while (getline(ps, p, ',')) {
                p = trim(p);
                if (p.empty()) continue;
                if (p[0] != '&') p = "&" + p;

                // '='이 포함된 경우 기본값 등록 (키워드 매크로 매개변수)
                size_t eqPos = p.find('=');
                if (eqPos != string::npos) {
                    string param = trim(p.substr(0, eqPos));
                    string defVal = trim(p.substr(eqPos + 1));
                    currentMacro.params.push_back(param);
                    currentMacro.defaultValues[param] = defVal;
                }
                else {
                    currentMacro.params.push_back(p);
                }
            }
            continue;
        }

        // MACRO 내부 내용 수집
        if (inMacro) {
            string first;
            stringstream inner(t);
            inner >> first;
            if (first == "MEND") {
                // 매크로 정의 종료
                DEFTAB.push_back(raw);
                inMacro = false;

                currentMacro.start = defStart;
                currentMacro.end = DEFTAB.size() - 1;
                NAMTAB.push_back(currentMacro); // NAMTAB에 매크로 등록
                macroRanges.push_back({ i - (int)(DEFTAB.size() - defStart), i });
                currentMacro = NamTabEntry();
                continue;
            }
            // 매크로 본문은 DEFTAB에 저장
            DEFTAB.push_back(raw);
        }
    }

    // ===== 고유한 레이블 생성용 카운터 =====
    // 각 매크로가 호출될 때마다 번호를 부여하여 중복 방지
    static unordered_map<string, int> macroCallCount;

    // =============================
    // PASS 2: 매크로 확장
    // =============================
    for (int i = 0; i < (int)source.size(); i++) {
        bool skip = false;
        for (auto& r : macroRanges) {
            if (i >= r.first && i <= r.second) { skip = true; break; }
        }
        if (skip) continue;

        string raw = source[i];
        string t = trim(raw);
        if (t.empty()) continue;

        string first, second, operand = "";
        stringstream ss(t);
        ss >> first >> second;
        getline(ss, operand);
        operand = trim(operand);

        // 매크로 정의 구문은 확장 대상에서 제외
        if (second == "MACRO" || first == "MACRO" || first == "MEND" || second == "MEND")
            continue;

        // 매크로 이름 탐색
        int idx = findMacro(first);
        if (idx == -1) idx = findMacro(second);
        if (idx != -1) {
            ARGTAB.clear();

            // 호출문에서 인자 부분 추출
            string args = operand;
            if (args.empty() && idx == findMacro(first)) {
                size_t pos = t.find(first) + first.size();
                args = trim(t.substr(pos));
            }
            else if (args.empty() && idx == findMacro(second)) {
                size_t pos = t.find(second) + second.size();
                args = trim(t.substr(pos));
            }

            // 인자를 ','로 분리
            vector<string> actualArgs;
            stringstream as(args);
            string a;
            while (getline(as, a, ',')) actualArgs.push_back(trim(a));

            // [키워드 매크로 매개변수 처리 추가]
            // 호출 시 일부 인자가 비어있거나 '=' 형식으로 전달될 경우 처리
            for (size_t k = 0; k < NAMTAB[idx].params.size(); k++) {
                string param = NAMTAB[idx].params[k];
                string value = "";

                if (k < actualArgs.size()) {
                    string arg = actualArgs[k];
                    if (arg.find('=') != string::npos) {
                        // 키워드 인자 형식 (&ARG=VALUE)
                        size_t eqPos = arg.find('=');
                        string key = "&" + trim(arg.substr(0, eqPos));
                        string val = trim(arg.substr(eqPos + 1));
                        ARGTAB[key] = val;
                        continue;
                    }
                    if (!arg.empty()) value = arg;
                }

                // 인자가 비어 있으면 기본값 사용
                if (value.empty() && NAMTAB[idx].defaultValues.count(param))
                    value = NAMTAB[idx].defaultValues[param];

                ARGTAB[param] = value;
            }

            cout << "\n▶ 매크로 호출: " << NAMTAB[idx].name << "\n";
            printTables();

            // ===== 고유한 레이블 형성 =====
            // 각 매크로 호출마다 고유 번호를 부여해 중복 레이블 방지
            macroCallCount[NAMTAB[idx].name]++;
            int callNum = macroCallCount[NAMTAB[idx].name];

            for (int j = NAMTAB[idx].start; j <= NAMTAB[idx].end; j++) {
                string expanded = DEFTAB[j];

                // ===== MEND 출력 생략 추가 =====
                if (trim(expanded) == "MEND") continue;

                // 고유 레이블 자동 부여: 라벨 뒤에 _번호 추가
                stringstream s2(expanded);
                string maybeLabel;
                s2 >> maybeLabel;
                if (!maybeLabel.empty() && maybeLabel.back() != ':' && maybeLabel != "MEND" && maybeLabel != "MACRO") {
                    if (maybeLabel[0] != '&' && isalpha(maybeLabel[0])) {
                        expanded = replaceAll(expanded, maybeLabel, maybeLabel + "_" + to_string(callNum));
                    }
                }

                bool processedIf = false; // 조건부 매크로가 처리되었는지 표시

                // ===== 조건부 매크로 확장 =====
                // IF, ELSE, ENDIF 구조 처리 (매개변수 값에 따라 분기)
                string token1;
                stringstream condCheck(expanded);
                condCheck >> token1;

                if (token1.rfind("IF", 0) == 0) {
                    processedIf = true;
                    string var, op, val;
                    condCheck >> var >> op >> val;
                    string actual = ARGTAB.count(var) ? ARGTAB[var] : "";

                    // 조건 비교 (EQ, NE 지원)
                    bool cond = false;
                    if (op == "EQ" && actual == val) cond = true;
                    else if (op == "NE" && actual != val) cond = true;

                    vector<string> trueBlock, falseBlock;
                    bool inElse = false;

                    // IF~ENDIF 블록 추출
                    j++;
                    for (; j <= NAMTAB[idx].end; j++) {
                        string inner = trim(DEFTAB[j]);
                        if (inner.rfind("ENDIF", 0) == 0) break;
                        if (inner.rfind("ELSE", 0) == 0) { inElse = true; continue; }
                        if (!inElse) trueBlock.push_back(inner);
                        else falseBlock.push_back(inner);
                    }

                    // 조건 결과에 따라 출력할 블록 선택
                    vector<string>& chosen = cond ? trueBlock : falseBlock;
                    for (string& x : chosen) {
                        // 블록 내 매개변수 치환 및 접합 처리
                        for (auto& a : ARGTAB) x = replaceAll(x, a.first, a.second);
                        x = expandLineWithConcat(x, ARGTAB);
                        fout << x << "\n";
                    }
                }

                if (processedIf) continue;

                // 일반 매크로 본문 치환 및 출력
                for (auto& a : ARGTAB)
                    expanded = replaceAll(expanded, a.first, a.second);
                expanded = expandLineWithConcat(expanded, ARGTAB);
                fout << expanded << "\n";
            }
        }
        else {
            fout << raw << "\n"; // 매크로가 아닌 일반 명령은 그대로 출력
        }
    }

    // ===== 실행 결과 요약 출력 =====
    cout << "\n===================================\n";
    cout << " 매크로 확장이 완료되었습니다.\n";
    cout << " 매개변수 접합 기능이 구현되었습니다.\n";
    cout << " 고유한 레이블 형성 기능이 구현되었습니다.\n";
    cout << " 조건부 매크로 확장 기능이 구현되었습니다.\n";
    cout << " 키워드 매크로 매개변수 기능이 구현되었습니다.\n";
    cout << " 결과 파일: output.asm\n";
    cout << "===================================\n";
    printTables();

    fin.close();
    fout.close();
    return 0;
}
