#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <iomanip>
#include <algorithm>
using namespace std;

// ===== 문자열 유틸 =====
string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

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
struct NamTabEntry {
    string name;
    int start;
    int end;
    vector<string> params;
};

vector<string> DEFTAB;
vector<NamTabEntry> NAMTAB;
unordered_map<string, string> ARGTAB;

// ===== 매크로 탐색 =====
int findMacro(const string& name) {
    for (int i = 0; i < (int)NAMTAB.size(); i++)
        if (NAMTAB[i].name == name)
            return i;
    return -1;
}

// ===== 테이블 출력 =====
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

    ifstream fin("input.asm");
    ofstream fout("output.asm");
    if (!fin.is_open()) {
        cerr << "Error: input.asm 파일을 열 수 없습니다.\n";
        return 1;
    }

    vector<string> source;
    string line;
    while (getline(fin, line)) source.push_back(line);

    bool inMacro = false;
    int defStart = 0;
    NamTabEntry currentMacro;
    vector<pair<int, int>> macroRanges; // MACRO~MEND 구간 저장

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

        // MACRO 시작
        if (op == "MACRO") {
            inMacro = true;
            defStart = i; // 원본에서 MACRO 시작 줄 저장
            currentMacro.name = label;
            currentMacro.params.clear();

            // 인자 분리
            stringstream ps(operand);
            string p;
            while (getline(ps, p, ',')) {
                p = trim(p);
                if (p[0] != '&') p = "&" + p;
                currentMacro.params.push_back(p);
            }
            continue;
        }

        // MACRO 내부
        if (inMacro) {
            string first;
            stringstream inner(t);
            inner >> first;
            if (first == "MEND") {
                DEFTAB.push_back(raw);
                inMacro = false;
                currentMacro.start = DEFTAB.size() - (DEFTAB.empty() ? 0 : 6);
                currentMacro.end = DEFTAB.size() - 1;
                NAMTAB.push_back(currentMacro);
                macroRanges.push_back({ defStart, i }); // MACRO~MEND 구간 기록
                currentMacro = NamTabEntry();
                continue;
            }
            DEFTAB.push_back(raw);
        }
    }

    // =============================
    // PASS 2: 매크로 확장
    // =============================
    for (int i = 0; i < (int)source.size(); i++) {

        // --- MACRO 정의 구간 건너뛰기 ---
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

        // MACRO/MEND는 무시
        if (second == "MACRO" || first == "MACRO" || first == "MEND" || second == "MEND")
            continue;

        // 호출문 판단
        int idx = findMacro(first);
        if (idx == -1) idx = findMacro(second);
        if (idx != -1) {
            ARGTAB.clear();

            // 인자 추출
            string args = operand;
            if (args.empty() && idx == findMacro(first)) {
                size_t pos = t.find(first) + first.size();
                args = trim(t.substr(pos));
            }
            else if (args.empty() && idx == findMacro(second)) {
                size_t pos = t.find(second) + second.size();
                args = trim(t.substr(pos));
            }

            vector<string> actualArgs;
            stringstream as(args);
            string a;
            while (getline(as, a, ',')) actualArgs.push_back(trim(a));

            // ARGTAB 매핑
            for (size_t k = 0; k < NAMTAB[idx].params.size() && k < actualArgs.size(); k++)
                ARGTAB[NAMTAB[idx].params[k]] = actualArgs[k];

            cout << "\n▶ 매크로 호출: " << NAMTAB[idx].name << "\n";
            printTables();

            // 본문 확장
            for (int j = NAMTAB[idx].start; j < NAMTAB[idx].end; j++) {
                string expanded = DEFTAB[j];
                for (auto& a : ARGTAB)
                    expanded = replaceAll(expanded, a.first, a.second);
                fout << expanded << "\n";
            }
        }
        else {
            fout << raw << "\n";
        }
    }

    cout << "\n===================================\n";
    cout << " 매크로 확장이 완료되었습니다.\n";
    cout << " 결과 파일: output.asm\n";
    cout << "===================================\n";
    printTables();

    fin.close();
    fout.close();
    return 0;
}
