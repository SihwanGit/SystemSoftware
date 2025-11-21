#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <cctype>
using namespace std;

// 헥사문자 1개를 4비트 값으로 변환하는 함수
unsigned int hexCharToValue(char hex) {
    hex = toupper(hex); // 대문자로 통일
    if (hex >= '0' && hex <= '9') return hex - '0';      // 숫자 범위
    if (hex >= 'A' && hex <= 'F') return 10 + (hex - 'A'); // A~F 처리
    return 0; // 잘못된 문자 처리
}

// 16진 문자열을 이진 문자열로 변환 (shift-left, OR 연산 사용)
string toBinary(string hexStr) {
    string result = "";
    for (char c : hexStr) { // 각 문자 순회
        unsigned int val = hexCharToValue(c); // 문자 → 정수값 변환
        unsigned int bits = 0;

        // shift-left와 OR 연산을 통해 4비트 구성
        for (int i = 3; i >= 0; --i)
            bits = (bits << 1) | ((val >> i) & 1);

        // 4비트를 이진 문자열로 변환
        for (int i = 3; i >= 0; --i)
            result += ((val >> i) & 1) ? '1' : '0';
        result += " "; // 각 nibble 간 공백 추가
    }
    return result;
}

int main() {
    ifstream obj("OBJFILE.txt"); // OBJFILE 읽기
    if (!obj.is_open()) { // 파일 열기 실패 시 에러 출력
        cerr << "OBJFILE.txt 파일을 열 수 없습니다." << endl;
        return 1;
    }

    string line;
    cout << "======================== OBJECT FILE BINARY CONVERSION ========================\n\n";

    // OBJFILE의 각 줄 읽기
    while (getline(obj, line)) {
        if (line.empty()) continue; // 빈 줄은 무시

        string tag, addr, length, rest;
        stringstream ss(line);
        ss >> tag; // 첫 단어(T, H, E 등) 추출
        if (tag != "T") continue; // T 레코드만 처리

        ss >> addr >> length; // 주소와 길이 추출
        getline(ss, rest); // 나머지 부분(CODES) 읽기
        if (rest.empty()) continue;

        // 앞부분의 공백 제거
        while (!rest.empty() && isspace(rest[0])) rest.erase(0, 1);

        // CODES 부분의 공백 제거 후 연결
        string codes = "";
        for (char c : rest)
            if (!isspace(c)) codes += c;

        // 코드 부분을 이진수로 변환
        string binary = toBinary(codes);

        // 결과 출력
        cout << left << setw(8) << "TAG:" << tag << "\n"
            << setw(8) << "ADDR:" << addr << "\n"
            << setw(8) << "LENGTH:" << length << "\n";

        cout << setw(8) << "CODES:" << "\n" << codes << "\n";
        cout << setw(8) << "InCODES:" << "\n" << binary << "\n";
        cout << string(80, '-') << "\n"; // 구분선 출력
    }

    obj.close(); // 파일 닫기
    return 0;
}
