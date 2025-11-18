#include <iostream>
#include <fstream>
#include <sstream>   // istringstream 사용을 위해 추가
#include <string>
#include <unordered_map>
using namespace std;

int main() {
    // OPTAB을 저장할 해시맵
    unordered_map<string, string> optab;
    
    ifstream file("Optab.txt");
    if (!file) { //에러 처리
        cerr << "파일을 열 수 없습니다." << endl;
        return 1;
    }

    string line, key, code;
    // 한 줄씩 읽기
    while (getline(file, line)) {
        if (line.empty()) continue; // 빈 줄이면 건너뜀
        istringstream iss(line);    // 문자열 스트림 생성
        iss >> key >> code;         // key와 code 분리
        if (!key.empty() && !code.empty()) {
            optab[key] = code;      // map에 저장
        }
    }
    file.close();

    // 사용자 입력 받기
    cout << "찾을 Key를 입력하세요: ";
    string input;
    cin >> input;

    // 탐색
    if (optab.find(input) != optab.end()) {
        cout << "Code: " << optab[input] << endl;
    }
    else {
        cout << "Error: 명령어가 OPTAB에 존재하지 않습니다." << endl;
    }

    return 0;
}
